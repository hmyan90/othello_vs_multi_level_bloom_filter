/*
 * config.h
 *
 *  Created on: Nov 5, 2017
 *      Author: ssqstone
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/time.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <functional> 
#include <algorithm> 
#include <iterator>

#include <inttypes.h>
#include <queue>
#include <unordered_map>
#include <map>
#include <tuple>
#include <vector>
#include <cassert>
#include "lfsr64.h"

using namespace std;

#pragma pack(push, 1)
struct Addr_Port {  // 6B
  uint32_t addr = 0;
  uint16_t port = 0;

  inline bool operator ==(const Addr_Port& another) const {
    return std::tie(addr, port) == std::tie(another.addr, another.port);
  }
  
  inline bool operator <(const Addr_Port& another) const {
    return std::tie(addr, port) < std::tie(another.addr, another.port);
  }
  
};

inline ostream& operator <<(ostream & os, const Addr_Port& addr) {
  os << std::hex << addr.addr << ":" << addr.port;
  return os;
}

struct DIP {
  Addr_Port addr;
  int weight;
};

struct Tuple5 {  // 13B
  Addr_Port dst, src;
  uint16_t protocol = 6;

  inline bool operator ==(const Tuple5& another) const {
    return std::tie(dst, src, protocol) == std::tie(another.dst, another.src, another.protocol);
  }
  
  inline bool operator <(const Tuple5& another) const {
    return std::tie(dst, src, protocol) < std::tie(another.dst, another.src, another.protocol);
  }
};

struct Tuple3 {  // 8B
  Addr_Port src;
  uint16_t protocol = 6;  // intentionally padded. will be much faster
  
  inline bool operator ==(const Tuple3& another) const {
    return std::tie(src, protocol) == std::tie(another.src, another.protocol);
  }
  
  inline bool operator <(const Tuple3& another) const {
    return std::tie(src, protocol) < std::tie(another.src, another.protocol);
  }
  
};
inline ostream& operator <<(ostream & os, Tuple3 const & tuple) {
  os << std::hex << tuple.src << ", " << tuple.protocol;
  return os;
}
#pragma pack(pop)
//
//template<int coreId = 0>
//inline void getVip(Addr_Port *vip) {
//  static int addr = 0x0a800000 + coreId * 10;
//  vip->addr = addr++;
//  vip->port = 0;
//  if (addr >= 0x0a800000 + VIP_NUM) addr = 0x0a800000;
//}
//
//template<int coreId = 0>
//inline void getTuple3(Tuple3* tuple3) {
//  static LFSRGen<Tuple3> tuple3Gen(0xe2211, CONN_NUM, coreId * 10);
//  tuple3Gen.gen(tuple3);
//}
//
//template<int coreId = 0>
//inline void get(Tuple3* tuple, Addr_Port* vip) {
//  getTuple3<coreId>(tuple); // this is why we assume CONN_NUM is multiple of VIP_NUM
//  getVip<coreId>(vip);
//  if(tuple->protocol&1) tuple->protocol = 17;
//  else tuple->protocol = 6;
//}

inline int diff_ms(timeval t1, timeval t2) {
  return (((t1.tv_sec - t2.tv_sec) * 1000000) + (t1.tv_usec - t2.tv_usec)) / 1000;
}

inline int diff_us(timeval t1, timeval t2) {
  return ((t1.tv_sec - t2.tv_sec) * 1000000 + (t1.tv_usec - t2.tv_usec));
}

std::string human(uint64_t word);

#include <sched.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <unistd.h>
int stick_this_thread_to_core(int core_id);

void sync_printf(const char *format, ...);
void commonInit();

template<typename InType, 
        template<typename U, typename alloc = allocator<U>> class InContainer,
        typename OutType = InType,
        template<typename V, typename alloc = allocator<V>> class OutContainer = InContainer>
OutContainer<OutType> mapf(const InContainer<InType>& input, function<OutType(const InType&)> func) {
  OutContainer<OutType> output;
  output.resize(input.size());
  transform(input.begin(), input.end(), output.begin(), func);
  return output;
}

#ifndef NAME
#define NAME "concury"
#endif

extern ofstream queryLog, addLog, memoryLog, dynamicLog;
#endif /* CONFIG_H_ */
