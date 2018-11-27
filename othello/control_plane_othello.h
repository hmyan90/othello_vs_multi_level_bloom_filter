#pragma "once"

#include <vector>
#include <iostream>
#include <array>
#include <queue>
#include <cstring>
#include <list>
#include "disjointset.h"
#include <set>
#include <algorithm>
#include <cassert>
#include "common.h"

#ifdef P4_CONCURY
#include "p4/hash.h"
#else
#include "hash.h"
#endif

using namespace std;

template<class keyType, class valueType, uint8_t valueLength>
class DataPlaneOthello;

/**
 * control plane Othello can track connections (Add [amortized], Delete, Membership Judgment) in O(1) time,
 * can iterate on the keys and values in exactly n elements.
 * 
 * implementation: just add an array indMem to be maintained. always ensure that registered keys can
 * be queried to get the index of it in the keys array
 * 
 * how to ensure: 
 * add to tail when add, and store the value as well as the index to othello
 * when delete, move key-value and update corresponding index
 */
template<class keyType, class valueType, uint8_t valueLength = 0>
class ControlPlaneOthello {
  static_assert(sizeof(valueType)*8>=valueLength, "sizeof(valueType)*8 < valueLength");

  friend class DataPlaneOthello<keyType, valueType, valueLength> ;
private:
  //*******builtin values
  const static int MAX_REHASH = 5000; //!< Maximum number of rehash tries before report an error. If this limit is reached, Othello build fails.
  const static uint32_t L = valueLength ? valueLength : sizeof(valueType) * 8; //!< the bit length of return value.
  const static uint64_t LMASK = ((L == 64) ? (~0ULL) : ((1ULL << L) - 1));
public:
  ControlPlaneOthello(vector<keyType>& _keys = 0, uint32_t keycount = 0, vector<valueType>& _values = 0) {
    resizeKey(keycount);

    for (int i = 0; i < keycount; ++i) {
        kvs[i].first = _keys[i];
        kvs[i].second = _values[i];
    } 
    
    resetBuildState();
    
    build();
  }
  
  //****************************************
  //*************DATA Plane
  //****************************************
private:
  vector<valueType> mem; //!< actual memory space for arrayA and arrayB.
  vector<uint32_t> indMem;
  uint32_t ma = 0; //!< length of arrayA.
  uint32_t mb = 0; //!< length of arrayB
  uint32_t hashSizeReserve = 0;
  Hasher32<keyType> Ha; //<! hash function Ha
  Hasher32<keyType> Hb; //<! hash function Hb
  
  //inline int getMemSize() {
  //  if (valueLength) return (hashSizeReserve + 1) * 3 / 4;
  //  else return hashSizeReserve;
  //}
  
  void inline getIndexA(const keyType &k, uint32_t &ret1) {
    ret1 = (Ha)(k) & (ma - 1);
  }
  
  void inline getIndexB(const keyType &k, uint32_t &ret1) {
    ret1 = (Hb)(k) & (mb - 1);
    ret1 += ma;
  }
  
  void inline getIndexAB(const keyType &k, uint32_t &ret1, uint32_t &ret2) {
    getIndexA(k, ret1);
    getIndexB(k, ret2);
  }
  
  void inline memSet(int index, valueType value) {
    static_assert(valueLength == 0 || (valueLength == 12 && sizeof(valueType)==sizeof(uint16_t)), "");
    
    if (valueLength) {
      const static uint16_t MASK[] = { 0xF000, 0x000F };
      value &= 0x0FFF;
      uint16_t* addr = (uint16_t*) ((uint8_t*) &mem + index * 3 / 2);
      
      if (index & 1) {
        value <<= 4;
        *addr &= MASK[1];
        *addr |= value;
      } else {
        *addr &= MASK[0];
        *addr |= value;
      }
    } else {
      mem[index] = value;
    }
  }
  
public:
  inline int getMemSize() {
    if (valueLength) return (hashSizeReserve + 1) * 3 / 4;
    else return hashSizeReserve;
  }

  valueType inline memGet(int index) {
    static_assert(valueLength == 0 || (valueLength == 12 && sizeof(valueType)==sizeof(uint16_t)), "");
    
    if (valueLength) {
      uint16_t res = *(uint16_t*) ((uint8_t*) &mem + index * 3 / 2); ///
      if (index & 1) res >>= 4;
      
      return res & 0x0FFF;
    } else {
      return mem[index];
    }
  }
  
  uint32_t getMa() const {
    return ma;
  }
  uint32_t getMb() const {
    return mb;
  }
  Hasher32<keyType> getHa() const {
    return Ha;
  }
  Hasher32<keyType> getHb() const {
    return Hb;
  }
  
  /*!
   \brief returns a 64-bit integer query value for a key.
   */
  inline valueType query(const keyType &k) {
    uint32_t ha, hb;
    getIndexAB(k, ha, hb);
    valueType aa = memGet(ha);
    valueType bb = memGet(hb);
    ////printf("%llx   [%x] %x ^ [%x] %x = %x\n", k,ha,aa&LMASK,hb,bb&LMASK,(aa^bb)&LMASK);
    return aa ^ bb;
  }
  
  /*!
   \brief returns a 64-bit integer query value for a key.
   */
  inline uint32_t queryIndex(const keyType &k) {
    uint32_t ha, hb;
    getIndexAB(k, ha, hb);
    uint32_t aa = indMem[ha];
    uint32_t bb = indMem[hb];
    return aa ^ bb;
  }
  
  //****************************************
  //*************CONTROL plane
  //****************************************
private:
  uint32_t keyCnt = 0, keyCntReserve = 0;
  // ******input of control plane
  vector<pair<keyType, valueType>> kvs;

  inline valueType randVal(int i = 0) {
    valueType v = rand();
    
    if (sizeof(valueType) > 4) {
      *(((int *) &v) + 1) = rand();
    }
    if (sizeof(valueType) > 8) {
      *(((int *) &v) + 2) = rand();
    }
    if (sizeof(valueType) > 12) {
      *(((int *) &v) + 3) = rand();
    }
    return v;
  }
  
  //! resize key and value related memory.
  //!
  //! Side effect: will change keyCnt, and if hash size is changed, will incur a rebuild
  void resizeKey(int keycount) {
    int hl1 = 7; //start from ma=64
    int hl2 = 8; //start from mb=64
    
    while ((1UL << hl1) < keycount * 1.333334)
      hl1++;
    while ((1UL << hl2) < keycount * 1)
      hl2++;
    
    int nextMa = 1U << hl1;
    int nextMb = 1U << hl2;
    
    if (keyCntReserve == 0 || keycount > keyCntReserve) {
      keyCntReserve = max(256, keycount * 2);
      kvs.resize(keyCntReserve);
      nextKeyOfThisKeyAtPartA.resize(keyCntReserve);
      nextKeyOfThisKeyAtPartB.resize(keyCntReserve);
    }
    
    if (nextMa > ma || nextMb > mb) {
      hashSizeReserve = nextMa + nextMb;
      ma = nextMa;
      mb = nextMb;
      mem.resize(getMemSize());
      free(filled);
      filled = (bool*) malloc(getFilledSize());
      indMem.resize(hashSizeReserve);
      keyIndicesOfThisNode.resize(hashSizeReserve);
      disj.resize(hashSizeReserve);
      
      build();
    }
    
    keyCnt = keycount;
    
//    cout << human(keyCnt) << " Keys, ma/mb = " << human(ma) << "/" << human(mb) << endl;
  }
  
  void resetBuildState() {
    for (int i = 0; i < hashSizeReserve; ++i) {
      memSet(i, randVal(i));
      indMem[i] = rand();
    }
    // _ind needn't to be initialized
    memset(filled, 0, getFilledSize());
    fill(keyIndicesOfThisNode.begin(), keyIndicesOfThisNode.end(), -1);
    fill(nextKeyOfThisKeyAtPartA.begin(), nextKeyOfThisKeyAtPartA.end(), -1);
    fill(nextKeyOfThisKeyAtPartB.begin(), nextKeyOfThisKeyAtPartB.end(), -1);
    disj.reset();
  }
  
  bool built = false; //!< true if Othello is successfully built.
  uint32_t tryCount = 0; //!< number of rehash before a valid hash pair is found.
  /*! multiple keys may share a same end (hash value)
   first and next1, next2 maintain linked lists,
   each containing all keys with the same hash in either of their ends
   */
  vector<int32_t> keyIndicesOfThisNode;         //!< subscript: hashValue, value: keyIndex
  vector<int32_t> nextKeyOfThisKeyAtPartA;         //!< subscript: keyIndex, value: keyIndex
  vector<int32_t> nextKeyOfThisKeyAtPartB;         //! h2(keys[i]) = h2(keys[next2[i]]);
  
  DisjointSet disj;                     //!< store the hash values that are connected by key edges
  
  bool* filled = (bool*) malloc(1);                  //!< remember filled nodes
      
  inline void setFilled(int index) {
    filled[index] = true;
  }
  
  inline void clearFilled(int index) {
    filled[index] = false;
  }
  
  inline bool isFilled(int index) {
    return filled[index];
  }
  
  inline int getFilledSize() {
    return hashSizeReserve * sizeof(bool);
  }
  
  //! gen new hash seed pair, cnt ++
  void newHash() {
    int s1 = rand();
    int s2 = rand();
    Ha.setSeed(s1);
    Hb.setSeed(s2);
    tryCount++;
    if (tryCount > 1) {
      //printf("NewHash for the %d time\n", tryCount);
    }
  }
  
  //! update the disjoint set and the connected forest so that
  //! include all the old keys and the newly inserted key
  //! Warning: this method won't change the node value and the filled vector
  void addEdge(int key, uint32_t ha, uint32_t hb) {
    nextKeyOfThisKeyAtPartA[key] = keyIndicesOfThisNode[ha];
    keyIndicesOfThisNode[ha] = key;
    nextKeyOfThisKeyAtPartB[key] = keyIndicesOfThisNode[hb];
    keyIndicesOfThisNode[hb] = key;
    disj.merge(ha, hb);
  }
  
  //! test if this hash pair is acyclic, and build:
  //! the connected forest and the disjoint set of connected relation
  //! the disjoint set will be only useful to determine the root of a connected component
  //!
  //! Assume: all build related memory are cleared
  //! Side effect: the disjoint set and the connected forest are properly set
  bool testHash() {
    uint32_t ha, hb;
//    cout << "********\ntesting hash" << endl;
    for (int i = 0; i < keyCnt; i++) {
      getIndexAB(kvs[i].first, ha, hb);
      
//      cout << i << "th key: " << keys[i] << ", ha: " << ha << ", hb: " << hb << endl;
      
      if (disj.sameSet(ha, hb)) {  // if two indices are in the same disjoint set, means the corresponding key will incur circle.
        //printf("Conflict key %d: %llx\n", i, *(unsigned long long*) &(keys[i]));
        return false;
      }
      addEdge(i, ha, hb);
    }
    return true;
  }
  
  //! Fill a connected tree from the root.
  //! the value of root is set, and set all its children according to root values
  //! Assume: values are present, and the connected forest are properly set
  //! Side effect: all node in this tree is set and if updateToFilled, the filled vector will record filled values
  template<bool updateToFilled, bool fillValue, bool fillIndex>
  void fillTreeBFS(int root) {
    if (updateToFilled) setFilled(root);
    
    list<uint32_t> Q;
    Q.clear();
    Q.push_back(root);
    
    std::set<uint32_t> visited;
    visited.insert(root);
    
    while (!Q.empty()) {
      uint32_t nodeid = (*Q.begin());
      Q.pop_front();
      
      // // find all the opposite side node to be filled
      
      // search all the edges of this node, to fill and enqueue the opposite side, and record the fill
      vector<int32_t> *nextKeyOfThisKey;
      nextKeyOfThisKey = (nodeid < ma) ? &nextKeyOfThisKeyAtPartA : &nextKeyOfThisKeyAtPartB;
      
      for (int32_t currKeyIndex = keyIndicesOfThisNode[nodeid]; currKeyIndex >= 0; currKeyIndex = nextKeyOfThisKey->at(currKeyIndex)) {
        uint32_t ha, hb;
        getIndexAB(kvs[currKeyIndex].first, ha, hb);
        
        // now the opposite side node needs to be filled
        // fill and enqueue all next element of
        
        // ha xor hb must have been filled, find the opposite side
        bool aFilled = visited.find(ha) != visited.end();
        int toBeFilled = aFilled ? hb : ha;
        int hasBeenFilled = aFilled ? ha : hb;
        
        if (visited.find(toBeFilled) != visited.end()) {
          continue;
        }
        
        if (fillValue) {
          valueType &value = kvs[currKeyIndex].second;
          valueType valueToFill = value ^ memGet(hasBeenFilled);
          memSet(toBeFilled, valueToFill);
        }
        
        if (fillIndex) {
          uint32_t indexToFill = currKeyIndex ^ indMem[hasBeenFilled];
          indMem[toBeFilled] = indexToFill;
        }
        
        Q.push_back(toBeFilled);
        if (updateToFilled) setFilled(toBeFilled);
        visited.insert(toBeFilled);
      }
    }
  }
  
  //! Fill *Othello* so that the query returns values as defined
  //!
  //! Assume: edges and disjoint set are properly set up, filled is clear.
  //! Side effect: filled vector and all values are properly set
  void fillValue() {
    for (int i = 0; i < ma + mb; i++)
      if (disj.isRoot(i)) {  // we can only fix one end's value in a cc of keys, then fix the roots'
        memSet(i, randVal());
        fillTreeBFS<true, true, true>(i);
      }
  }
  
  //! Begin a new build
  //!
  //! Side effect: 1) discard all memory except keys and values. 2) build fail, or
  //! all the values, filled vector, and disjoint set are properly set
  bool trybuild() {
    resetBuildState();
    
    if (keyCnt == 0) {
      return true;
    }
    
    bool succ;
    if ((succ = testHash())) {
      fillValue();
    }
    
    disj.reset();  // won't be maintained during key deletion
    return succ;
  }
  
  //! try really hard to build, until success or tryCount >= MAX_REHASH
  //!
  //! Side effect: 1) discard all memory except keys and values. 2) build fail, or
  //! all the values, filled vector, and disjoint set are properly set
  bool build() {
    tryCount = 0;
    do {
      newHash();
      if (tryCount > 20 && !(tryCount & (tryCount - 1))) {
        cout << "Another try: " << tryCount << " " << human(keyCnt) << " Keys, ma/mb = " << human(ma) << "/" << human(mb)    //
             << " keyT" << sizeof(keyType) * 8 << "b  valueT" << sizeof(valueType) * 8 << "b"     //
             << " L=" << (int) L << endl;
      }
      built = trybuild();
    } while ((!built) && (tryCount < MAX_REHASH));
    
    //printf("%08x %08x\n", Ha.s, Hb.s);
    if (built) {
      if (tryCount > 20) {
        cout << "Succ " << human(keyCnt) << " Keys, ma/mb = " << human(ma) << "/" << human(mb)    //
             << " keyT" << sizeof(keyType) * 8 << "b  valueT" << sizeof(valueType) * 8 << "b"     //
             << " L=" << (int) L << " After " << tryCount << "tries" << endl;
      }
    } else {
      cout << "rebuild fail! " << endl;
      throw new exception();
    }
    
    return built;
  }
  
  bool testConnected(int32_t ha0, int32_t hb0) {
    list<int32_t> q;
    int t = keyIndicesOfThisNode[ha0];
    while (t >= 0) {
      q.push_back(t); //edges A -> B: >=0;  B -> A : <0;
      t = nextKeyOfThisKeyAtPartA[t];
    }
    
    while (!q.empty()) {
      int kid = q.front();
      bool isAtoB = (kid >= 0);
      if (kid < 0) kid = -kid - 1;
      q.pop_front();
      uint32_t ha, hb;
      getIndexAB(kvs[kid].first, ha, hb);
      if (hb == hb0) return true;
      
      if (isAtoB) {
        int t = keyIndicesOfThisNode[hb];
        while (t >= 0) {
          if (t != kid) q.push_back(-t - 1);
          t = nextKeyOfThisKeyAtPartB[t];
        }
      } else {
        int t = keyIndicesOfThisNode[ha];
        while (t >= 0) {
          if (t != kid) q.push_back(t);
          t = nextKeyOfThisKeyAtPartA[t];
        }
      }
    }
    return false;
  }
  
public:
  inline bool insert(pair<keyType, valueType> &&kv) {
    resizeKey(keyCnt + 1);
    
    int lastIndex = keyCnt - 1;
    this->kvs[lastIndex] = kv;
    
    uint32_t ha, hb;
    getIndexAB(kv.first, ha, hb);
    
    if (testConnected(ha, hb)) {  // circle, rehash, tricky: takes all added keys together, rather than add one by one
      if (!build()) {
        keyCnt -= 1;
        throw new exception();
        return false;
      }
    } else {  // acyclic, just add
      addEdge(lastIndex, ha, hb);
      fillTreeBFS<false, true, true>(ha);
    }
//    assert(checkIntegrity());
    return true;
  }
  
  /*!
   \brief remove one key with the particular index from the keylist.
   \param [in] uint32_t kid.
   \note after this option, the number of keys, *keyCnt* decrease by 1. The key currently stored in *keys[kid]* will be replaced by the last key in *keys[]*. \n *sizeof(valueType)* is the number of bytes of each value.
   \note remember to adjust the value[] array if necessary.
   */
  void eraseAt(uint32_t kid) {
    uint32_t ha, hb;
    getIndexAB(kvs[kid].first, ha, hb);
    keyCnt--;
    
    // delete the edges of kid
    uint32_t headA = keyIndicesOfThisNode[ha];
    if (headA == kid) {
      keyIndicesOfThisNode[ha] = nextKeyOfThisKeyAtPartA[kid];
    } else {
      int t = headA;
      while (nextKeyOfThisKeyAtPartA[t] != kid)
        t = nextKeyOfThisKeyAtPartA[t];
      nextKeyOfThisKeyAtPartA[t] = nextKeyOfThisKeyAtPartA[nextKeyOfThisKeyAtPartA[t]];
    }
    uint32_t headB = keyIndicesOfThisNode[hb];
    if (headB == kid) {
      keyIndicesOfThisNode[hb] = nextKeyOfThisKeyAtPartB[kid];
    } else {
      int t = headB;
      while (nextKeyOfThisKeyAtPartB[t] != kid)
        t = nextKeyOfThisKeyAtPartB[t];
      nextKeyOfThisKeyAtPartB[t] = nextKeyOfThisKeyAtPartB[nextKeyOfThisKeyAtPartB[t]];
    }
    
    // move the last to fill the hole
    if (kid == keyCnt) return;
    kvs[kid] = kvs[keyCnt];
    uint32_t hal, hbl;
    getIndexAB(kvs[kid].first, hal, hbl);
    
    // repair the broken linked list because of key movement
    nextKeyOfThisKeyAtPartA[kid] = nextKeyOfThisKeyAtPartA[keyCnt];
    if (keyIndicesOfThisNode[hal] == keyCnt) {
      keyIndicesOfThisNode[hal] = kid;
    } else {
      int t = keyIndicesOfThisNode[hal];
      while (nextKeyOfThisKeyAtPartA[t] != keyCnt)
        t = nextKeyOfThisKeyAtPartA[t];
      nextKeyOfThisKeyAtPartA[t] = kid;
    }
    
    nextKeyOfThisKeyAtPartB[kid] = nextKeyOfThisKeyAtPartB[keyCnt];
    if (keyIndicesOfThisNode[hbl] == keyCnt) {
      keyIndicesOfThisNode[hbl] = kid;
    } else {
      int t = keyIndicesOfThisNode[hbl];
      while (nextKeyOfThisKeyAtPartB[t] != keyCnt)
        t = nextKeyOfThisKeyAtPartB[t];
      nextKeyOfThisKeyAtPartB[t] = kid;
    }
    
    // update the mapped index
    fillTreeBFS<false, false, true>(hal);
//    assert(checkIntegrity());
  }
  
  inline void updateMapping(keyType &k, valueType &val) {
    updateValueAt(queryIndex(k), val);
  }

  inline void updateMapping(keyType &&k, valueType &&val) {
    updateValueAt(queryIndex(k), val);
  }
  
  inline void updateValueAt(int index, valueType val) {
    if (index >= keyCnt) throw exception();
    
    kvs[index].second = val;
  }

  //****************************************
  //*********AS A SET
  //****************************************
public:
  inline const vector<pair<keyType, valueType>>& getKeyValuePairs() const {
    return kvs;
  }
  
  inline const vector<valueType>& getMem() const {
    return mem;
  }
  
  inline uint32_t size() {
    return keyCnt;
  }
  
  inline bool isMember(keyType& x) {
    uint32_t index = queryIndex(x);
    return (index < keyCnt && kvs[index].first == x);
  }
  
  inline void erase(keyType& x) {
    assert(isMember(x));
    
    uint32_t index = queryIndex(x);
    eraseAt(index);
  }
  
  bool checkIntegrity() {
    for (int i = 0; i < size(); ++i) {
      if (query(kvs[i].first) != kvs[i].second || queryIndex(kvs[i].first) != i) {
        throw new exception();
      }
    }
    
    return true;
  }
};

