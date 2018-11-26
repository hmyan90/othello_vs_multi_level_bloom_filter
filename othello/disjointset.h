#pragma once
#include <vector>
using namespace std;
/*! \file disjointset.h
 *  Disjoint Set data structure.  
 */
/*!
 * \brief Disjoint Set data structure. Helps to test the acyclicity of the graph during construction. 
 * */
class DisjointSet {
  vector<int32_t> mem;
public:
  uint32_t representative(int i) {
    if (mem[i] < 0) {
      return mem[i] = i;
    } else if (mem[i] != i) {
      return mem[i] = representative(mem[i]);
    } else {
      return i;
    }
  }
  void merge(int a, int b) {
    mem[representative(b)] = representative(a);
  }
  bool sameSet(int a, int b) {
    return representative(a) == representative(b);
  }
  bool isRoot(int a) {
    return (mem[a] == a);
  }

  //! re-initilize the disjoint sets.
  void reset() {
    for (int32_t &a : mem)
      a = -1;
  }
  
  //! add new keys, so that the total number of elements equal to n.
  void resize(int n) {
    mem.resize(n, -1);
  }
};
