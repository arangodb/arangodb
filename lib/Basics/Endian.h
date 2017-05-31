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

inline uint16_t hostToLittle(uint16_t in){
#ifdef __APPLE__
  return OSSwapHostToLittleInt16(in);
#elif __linux__
  return htole16(in);
#elif __WIN32
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
  return htole64(in);
#elif __WIN32
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
#elif __WIN32
  if(!isLittleEndian()){
    ByteSwap(&in,8);
  }
#endif
return in;
}

inline uint16_t littleToHost(uint16_t in){
#ifdef __APPLE__
  return OSSwapLittleToHostInt16(x);
#elif __linux__
  return le16toh(in);
#elif __WIN32
  if(!isLittleEndian()){
    ByteSwap(&in,2);
  }
#endif
return in;
}

inline uint32_t littleToHost(uint32_t in){
#ifdef __APPLE__
  return OSSwapLittleToHostInt32(x);
#elif __linux__
  return le32toh(in);
#elif __WIN32
  if(!isLittleEndian()){
    ByteSwap(&in,4);
  }
#endif
return in;
}

inline uint64_t littleToHost(uint64_t in){
#ifdef __APPLE__
  return OSSwapLittleToHostInt64(x);
#elif __linux__
  return le64toh(in);
#elif __WIN32
  if(!isLittleEndian()){
    ByteSwap(&in,8);
  }
#endif
return in;
}


}}

#endif
