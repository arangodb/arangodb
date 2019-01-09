//  endian/example/third_party_format.hpp

//  Copyright Beman Dawes 2014

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

#include <cstdint>

namespace third_party
{
    struct record
    {
      uint32_t   id;       // big endian
      int32_t    balance;  // big endian

      // ...  data members whose endianness is not a concern
    };
}
