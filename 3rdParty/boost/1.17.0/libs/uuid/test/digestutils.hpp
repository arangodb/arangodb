//
// Copyright (C) 2019 James E. King III
//
// Permission to copy, use, modify, sell and
// distribute this software is granted provided this copyright notice appears
// in all copies. This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_UUID_TEST_ENDIAN_2019
#define BOOST_UUID_TEST_ENDIAN_2019

#include <boost/predef/other/endian.h>

namespace boost {
namespace uuids {
namespace test {

//
// A utility to copy a raw digest out from one of the detail hash functions
// to a byte string for comparisons in testing to known values.
//
static void copy_raw_digest(unsigned char* out, const unsigned int *in, size_t inlen)
{
    for (size_t chunk = 0; chunk < inlen; ++chunk)
    {
        const unsigned char * cin = reinterpret_cast<const unsigned char *>(&in[chunk]);
        for (size_t byte = 0; byte < 4; ++byte)
        {
#if BOOST_ENDIAN_LITTLE_BYTE
            *out++ = *(cin + (3-byte));
#else
            *out++ = *(cin + byte);
#endif
        }
    }
}

//
// A utility to compare and report two raw hashes
//
static void test_digest_equal_array(char const * file, int line, char const * function,
                             const unsigned char *lhs,
                             const unsigned char *rhs,
                             size_t len)
{
    for (size_t i=0; i<len; i++) {
        if ( lhs[i] != rhs[i]) {
            std::cerr << file << "(" << line << "): digest [";
            for (size_t l=0; l<len; l++) {
                std::cerr << std::hex << (int)lhs[l];
            }

            std::cerr << "] not equal [";
            for (size_t r=0; r<len; r++) {
                std::cerr << std::hex << (int)rhs[r];
            }
            std::cerr << "] in function '" << function << "'" << std::endl;
            ++boost::detail::test_errors();
            return;
        }
    }
}

}
}
}

#endif
