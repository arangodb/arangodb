////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_ENDIAN_H
#define ARANGODB_BASICS_ENDIAN_H 1

#include <cstdint>
#include <cstring>

#ifdef __APPLE__
  #include <machine/endian.h>
  #include <libkern/OSByteOrder.h>
#elif _WIN32
#elif __linux__
  #include <endian.h>
#else
  #pragma messsage("unsupported os or compiler")
#endif

namespace arangodb {
namespace basics {

inline bool isLittleEndian(){
  int num = 42;
  static bool rv = ( *(char *)&num == 42 );
  return rv;
}

inline void ByteSwap( void *ptr, int bytes )
{
  for( int i = 0; i < bytes/2; i ++ ) {
    uint8_t swap = ((uint8_t*)( ptr ))[ i ];
    ((uint8_t*)( ptr ))[ i ] = ((uint8_t*)( ptr ))[ bytes - i - 1 ];
    ((uint8_t*)( ptr ))[ bytes - i - 1 ] = swap;
  }
}

// hostToLittle
inline uint16_t hostToLittle(uint16_t in){
#ifdef __APPLE__
  return OSSwapHostToLittleInt16(in);
#elif __linux__
  return htole16(in);
#elif _WIN32
  if(!isLittleEndian()){
    ByteSwap(&in,2);
  }
#endif
return in;
}

inline uint32_t hostToLittle(uint32_t in){
#ifdef __APPLE__
  return OSSwapHostToLittleInt32(in);
#elif __linux__
  return htole32(in);
#elif _WIN32
  if(!isLittleEndian()){
    ByteSwap(&in,4);
  }
#endif
return in;
}

inline uint64_t hostToLittle(uint64_t in){
#ifdef __APPLE__
  return OSSwapHostToLittleInt64(in);
#elif __linux__
  return htole64(in);
#elif _WIN32
  if(!isLittleEndian()){
    ByteSwap(&in,8);
  }
#endif
return in;
}

inline int16_t hostToLittle(int16_t in){
uint16_t tmp;
std::memcpy(&tmp,&in,2);

#ifdef __APPLE__
  tmp = OSSwapHostToLittleInt16(tmp);
#elif __linux__
  tmp =  htole16(tmp);
#elif _WIN32
  if(!isLittleEndian()){
    ByteSwap(&tmp,2);
  }
#endif

std::memcpy(&in,&tmp,2);
return in;
}

inline int32_t hostToLittle(int32_t in){
uint32_t tmp;
std::memcpy(&tmp,&in,4);

#ifdef __APPLE__
  tmp = OSSwapHostToLittleInt32(tmp);
#elif __linux__
  tmp =  htole32(tmp);
#elif _WIN32
  if(!isLittleEndian()){
    ByteSwap(&tmp,4);
  }
#endif

std::memcpy(&in,&tmp,4);
return in;
}

inline int64_t hostToLittle(int64_t in){
uint64_t tmp;
std::memcpy(&tmp,&in,8);

#ifdef __APPLE__
  tmp = OSSwapHostToLittleInt64(tmp);
#elif __linux__
  tmp =  htole64(tmp);
#elif _WIN32
  if(!isLittleEndian()){
    ByteSwap(&tmp,8);
  }
#endif

std::memcpy(&in,&tmp,8);
return in;
}

// littleToHost
inline uint16_t littleToHost(uint16_t in){
#ifdef __APPLE__
  return OSSwapLittleToHostInt16(in);
#elif __linux__
  return le16toh(in);
#elif _WIN32
  if(!isLittleEndian()){
    ByteSwap(&in,2);
  }
#endif
return in;
}

inline uint32_t littleToHost(uint32_t in){
#ifdef __APPLE__
  return OSSwapLittleToHostInt32(in);
#elif __linux__
  return le32toh(in);
#elif _WIN32
  if(!isLittleEndian()){
    ByteSwap(&in,4);
  }
#endif
return in;
}

inline uint64_t littleToHost(uint64_t in){
#ifdef __APPLE__
  return OSSwapLittleToHostInt64(in);
#elif __linux__
  return le64toh(in);
#elif _WIN32
  if(!isLittleEndian()){
    ByteSwap(&in,8);
  }
#endif
return in;
}

inline int16_t littleToHost(int16_t in){
uint16_t tmp;
std::memcpy(&tmp,&in,2);
#ifdef __APPLE__
  tmp = OSSwapLittleToHostInt16(tmp);
#elif __linux__
  tmp = le16toh(tmp);
#elif _WIN32
  if(!isLittleEndian()){
    ByteSwap(&tmp,2);
  }
#endif
std::memcpy(&in,&tmp,2);
return in;
}

inline int32_t littleToHost(int32_t in){
uint32_t tmp;
std::memcpy(&tmp,&in,4);
#ifdef __APPLE__
  tmp = OSSwapLittleToHostInt32(tmp);
#elif __linux__
  tmp = le32toh(tmp);
#elif _WIN32
  if(!isLittleEndian()){
    ByteSwap(&tmp,4);
  }
#endif
std::memcpy(&in,&tmp,4);
return in;
}

inline int64_t littleToHost(int64_t in){
uint64_t tmp;
std::memcpy(&tmp,&in,8);
#ifdef __APPLE__
  tmp = OSSwapLittleToHostInt64(tmp);
#elif __linux__
  tmp = le64toh(tmp);
#elif _WIN32
  if(!isLittleEndian()){
    ByteSwap(&tmp,8);
  }
#endif
std::memcpy(&in,&tmp,8);
return in;
}


}}

#endif
