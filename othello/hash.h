/*!
 \file hash.h
 Describes hash functions used in this project.
 */

#pragma once
#include <functional>
#include <type_traits>
#include <inttypes.h>

//! \brief A hash function that hashes keyType to uint32_t. When SSE4.2 support is found, use sse4.2 instructions, otherwise use default hash function  std::hash.
template<class keyType>
class Hasher32 {
public:
  uint32_t s;    //!< hash s.
  
public:
  Hasher32()
      : s(0xe221193) {
  }
  
  Hasher32(uint32_t _s)
      : s(_s) {
  }
  
  //! set bitmask and s
  void setSeed(uint32_t _s) {
    s = _s;
  }
  
  uint32_t operator()(const keyType &k0) const {
    uint32_t crc1 = ~0;
    uint64_t *base = typeid(keyType) == typeid(string) ? (uint64_t*) &k0[0]: (uint64_t*) &k0;
    uint64_t *k = base;
    uint32_t s1 = s;
    const uint16_t keyByteLength = typeid(keyType) == typeid(string) ? k0.size() :sizeof(keyType);
        
    if (keyByteLength >= 8) {
      asm(".byte 0xf2, 0x48, 0xf, 0x38, 0xf1, 0xf1;" :"=S"(crc1) :"0"(crc1), "c" ((*k)+s1) );
      s1 = ((((uint64_t) s1) * s1 >> 16) ^ (s1 << 2));
      k++;
    }
    if (keyByteLength >= 16) {
      asm(".byte 0xf2, 0x48, 0xf, 0x38, 0xf1, 0xf1;" :"=S"(crc1) :"0"(crc1), "c" ((*k)+s1) );
      s1 = ((((uint64_t) s1) * s1 >> 16) ^ (s1 << 2));
      k++;
    }
    if (keyByteLength >= 24) {
      asm(".byte 0xf2, 0x48, 0xf, 0x38, 0xf1, 0xf1;" :"=S"(crc1) :"0"(crc1), "c" ((*k)+s1) );
      s1 = ((((uint64_t) s1) * s1 >> 16) ^ (s1 << 2));
      k++;
    }
    if (keyByteLength >= 32) {
      asm(".byte 0xf2, 0x48, 0xf, 0x38, 0xf1, 0xf1;" :"=S"(crc1) :"0"(crc1), "c" ((*k)+s1) );
      s1 = ((((uint64_t) s1) * s1 >> 16) ^ (s1 << 2));
      k++;
    }
    
    if (keyByteLength == 4) {  // for faster process
      uint32_t *k32;
      k32 = (uint32_t*) k;
      asm( ".byte 0xf2, 0xf, 0x38, 0xf1, 0xf1;" :"=S"(crc1) :"0" (crc1),"c" ((*k32)+s1) );
    } else if (keyByteLength & 7) {
      uint64_t padded = *k;  // higher bits to zero
      padded = padded & (((unsigned long long) -1) >> (64 - (keyByteLength & 7) * 8));
      
      asm(".byte 0xf2, 0x48, 0xf, 0x38, 0xf1, 0xf1;" :"=S"(crc1) :"0"(crc1), "c" (padded+s1));
    }
    
//    asm( ".byte 0xf2, 0xf, 0x38, 0xf1, 0xf1;" :"=S"(crc1) :"0" (crc1),"c" (s1) );
    crc1 ^= (crc1 >> (32 ^ (7 & s1))); // * crc1;
    return crc1 ^ (*(uint32_t*) base);
  }
};
