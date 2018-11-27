#pragma once
/*!
 \file othello.h
 Describes the data structure *l-Othello*...
 */

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

#include "control_plane_othello.h"
using namespace std;

/*!
 * \brief Describes the data structure *l-Othello*. It classifies keys of *keyType* into *2^L* classes.
 * The array are all stored in an array of uint64_t. There are actually m_a+m_b cells in this array, each of length L.
 * \note Be VERY careful!!!! valueType must be some kind of int with no more than 8 bytes' length
 */
template<class keyType, class valueType, uint8_t valueLength = 0>
class DataPlaneOthello {
  static_assert(sizeof(valueType)*8>=valueLength, "sizeof(valueType)*8 < valueLength");
private:
  //*******builtin values
  const static int MAX_REHASH = 50; //!< Maximum number of rehash tries before report an error. If this limit is reached, Othello build fails. 
  const static uint32_t L = valueLength ? valueLength : sizeof(valueType) * 8; //!< the bit length of return value.
  const static uint64_t LMASK = ((L == 64) ? (~0ULL) : ((1ULL << L) - 1));
public:
  DataPlaneOthello() {
    int hl1 = 7; //start from ma=128
    int hl2 = 8; //start from mb=256
    
    ma = max(ma, (1U << hl1));
    mb = max(mb, (1U << hl2));
    
    if (ma + mb > hashSizeReserve) {
      hashSizeReserve = (ma + mb) * 2;
      ma *= 2;
      mb *= 2;
      mem.resize(getMemSize());
    }
  }
  
  inline int getMemSize() const {
    if (valueLength) return (hashSizeReserve + 1) * 3 / 4;
    else return hashSizeReserve;
  }
  
  DataPlaneOthello(ControlPlaneOthello<keyType, valueType, valueLength>& control)
      : ma(control.ma), mb(control.mb), hashSizeReserve(control.ma + control.mb), Ha(control.Ha), Hb(control.hashSizeReserve), mem(control.mem) {
  }
    
  void updateFromControlPlane(ControlPlaneOthello<keyType, valueType, valueLength>& control) {
    this->ma = control.ma;
    this->mb = control.mb;
    this->hashSizeReserve = control.ma + control.mb;
    this->Ha = control.Ha;
    this->Hb = control.Hb;
    this->mem = control.mem;
  }
  
  //****************************************
  //*************DATA Plane
  //****************************************
private:
  vector<valueType> mem; //!< actual memory space for arrayA and arrayB.
  uint32_t ma = 0; //!< length of arrayA.
  uint32_t mb = 0; //!< length of arrayB
  uint32_t hashSizeReserve = 0;
  Hasher32<keyType> Ha; //<! hash function Ha
  Hasher32<keyType> Hb; //<! hash function Hb
  
  void inline get_hash_1(const keyType &k, uint32_t &ret1) const {
    ret1 = (Ha)(k) & (ma - 1);
  }
  
  void inline get_hash_2(const keyType &k, uint32_t &ret1) const {
    ret1 = (Hb)(k) & (mb - 1);
    ret1 += ma;
  }
  
  void inline get_hash(const keyType &v, uint32_t &ret1, uint32_t &ret2) const {
    get_hash_1(v, ret1);
    get_hash_2(v, ret2);
  }
  
public:
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
  inline valueType query(const keyType &k) const {
    uint32_t ha, hb;
    return query(k, ha, hb);
  }
  
  /*!
   \brief compute the hash value of key and return query value.
   \param [in] keyType &k key
   \param [out] uint32_t &ha  computed hash function ha
   \param [out] uint32_t &hb  computed hash function hb
   \retval valueType
   */
  inline valueType query(const keyType &k, uint32_t &ha, uint32_t &hb) const {
    get_hash(k, ha, hb);
    valueType aa = memGet(ha);
    valueType bb = memGet(hb);
    //cout << hex << ha << ", " << hb << ", " << aa << ", " << bb;
    ////printf("%llx   [%x] %x ^ [%x] %x = %x\n", k,ha,aa&LMASK,hb,bb&LMASK,(aa^bb)&LMASK);
    return LMASK & (aa ^ bb);
  }
  
  valueType inline memGet(int index) const {
    static_assert(valueLength == 0 || (valueLength == 12 && sizeof(valueType)==sizeof(uint16_t)), "");
    
    if (valueLength) {
      uint16_t res = *(uint16_t*) ((uint8_t*) &mem[0] + index * 3 / 2);
      if (index & 1) res >>= 4;
      
      return res & 0x0FFF;
    } else {
      return mem[index];
    }
  }
  
  inline const vector<valueType>& getMem() const {
    return mem;
  }
  
  //****************************************
  //*********ADJUSTMENTS
  //****************************************
public:
  uint64_t reportDataPlaneMemUsage() const {
    uint64_t size = mem.size() * sizeof(valueType);
    
    cout << "Ma: " << ma * sizeof(valueType) << ", Mb: " << mb * sizeof(valueType) << endl;
    
    return size;
  }
  
  // return the mapped count of a value
  vector<uint32_t> getCnt() const {
    vector<uint32_t> cnt(1ULL << L);
    
    for (int i = 0; i < ma; i++) {
      for (int j = ma; j < ma + mb; j++) {
        cnt[memGet(i) ^ memGet(j)]++;
      }
    }
    return cnt;
  }
  
  void outputMappedValue(ofstream& fout) const {
    bool partial = (uint64_t) ma * (uint64_t) mb > (1UL << 22);
    
    if (partial) {
      for (int i = 0; i < (1 << 22); i++) {
        fout << uint32_t(memGet(rand() % (ma - 1)) ^ memGet(ma + rand() % (mb - 1))) << endl;
      }
    } else {
      for (int i = 0; i < ma; i++) {
        for (int j = ma; j < ma + mb; ++j) {
          fout << uint32_t(memGet(ma) ^ memGet(j)) << endl;
        }
      }
    }
  }
  
  int getStaticCnt() {
    return ma * mb;
  }
};

