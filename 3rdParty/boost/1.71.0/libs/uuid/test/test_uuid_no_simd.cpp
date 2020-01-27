//  (C) Copyright Andy Tompkins 2007. Permission to copy, use, modify, sell and
//  distribute this software is granted provided this copyright notice appears
//  in all copies. This software is provided "as is" without express or implied
//  warranty, and with no claim as to its suitability for any purpose.

// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

//  libs/uuid/test/test_uuid_no_simd.cpp  -------------------------------//

// This test is a subset of libs/uuid/test/test_uuid.cpp, compiled without any
// SIMD optimizations. The test specifically verifies generic implementations
// of the routines.

#define BOOST_UUID_NO_SIMD

#include <iostream>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/current_function.hpp>

void test_uuid_equal_array(char const * file, int line, char const * function,
                           boost::uuids::uuid const& lhs, const unsigned char (&rhs)[16])
{
    for (size_t i=0; i<16; i++) {
        if ( *(lhs.begin()+i) != rhs[i]) {
            std::cerr << file << "(" << line << "): uuid " << lhs << " not equal " << "{";
            for (size_t j=0; j<16; j++) {
                if (j != 0) {
                    std::cerr << " ";
                }
                std::cerr << std::hex << (int)rhs[j];
            }
            std::cerr << "} in function '" << function << "'" << std::endl;
            ++boost::detail::test_errors();
            return;
        }
    }
}


#define BOOST_TEST_UUID(lhs, rhs) ( test_uuid_equal_array(__FILE__, __LINE__, BOOST_CURRENT_FUNCTION, lhs, rhs) )


int main(int, char*[])
{
    using namespace boost::uuids;

    { // uuid::operator=()
        uuid u1 = {{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15}};
        uuid u2 = u1;
        BOOST_TEST_EQ(u2, u1);
    }

    { // uuid::is_nil()
        uuid u1 = {{0}};
        BOOST_TEST_EQ(u1.is_nil(), true);

        uuid u2 = {{1,0}};
        BOOST_TEST_EQ(u2.is_nil(), false);
    }

    { // uuid::swap(), swap()
        uuid u1 = {{0}};
        uuid u2 = {{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15}};
        u1.swap(u2);

        unsigned char values1[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        unsigned char values2[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
        BOOST_TEST_UUID(u1, values2);
        BOOST_TEST_UUID(u2, values1);

        swap(u1, u2);
        BOOST_TEST_UUID(u1, values1);
        BOOST_TEST_UUID(u2, values2);
    }

    { // test comparsion
        uuid u1 = {{0}};
        uuid u2 = {{1,0}};
        uuid u3 = {{255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255}};
        uuid u4 = {{0,1,0}};
        uuid u5 = {{0,255,0}};

        BOOST_TEST_EQ(u1, u1);

        BOOST_TEST_NE(u1, u2);

        BOOST_TEST(u1 < u2);
        BOOST_TEST(u2 < u3);
        BOOST_TEST(u1 < u4);
        BOOST_TEST(u1 < u5);
        BOOST_TEST(u4 < u5);
        BOOST_TEST(u4 < u2);
        BOOST_TEST(u5 < u2);

        BOOST_TEST(u1 <= u1);
        BOOST_TEST(u1 <= u2);
        BOOST_TEST(u2 <= u3);

        BOOST_TEST(u2 >= u1);
        BOOST_TEST(u3 >= u1);

        BOOST_TEST(u3 >= u3);
        BOOST_TEST(u2 >= u1);
        BOOST_TEST(u3 >= u1);
    }

    { // ticket 10510
        // the uuids in the report
        uuid u6 = {{0x14,0x5c,0xfc,0x95,0x80,0x50,0x45,0x5a,0x83,0x82,0x44,0xca,0x57,0xc1,0x48,0x3b}};
        uuid u7 = {{0x14,0x5c,0xfc,0x95,0x80,0x50,0x45,0x5a,0x83,0x82,0x44,0xca,0x57,0xc1,0x48,0x3c}};

        // simple uuids to reproduce problem
        uuid u8 = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}};
        uuid u9 = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2}};
        uuid u10 = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,254}};
        uuid u11 = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255}};

        // some additional uuids for testing boundary cases
        uuid u12 = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1}};
        uuid u13 = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,2}};
        uuid u14 = {{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}};
        uuid u15 = {{255,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255}};

        BOOST_TEST(u6 < u7);
        BOOST_TEST(!(u7 < u6));

        BOOST_TEST(u8 < u9);
        BOOST_TEST(!(u9 < u8));
        BOOST_TEST(u8 < u10);
        BOOST_TEST(!(u10 < u8));
        BOOST_TEST(u8 < u11);
        BOOST_TEST(!(u11 < u8));

        BOOST_TEST(u9 < u10);
        BOOST_TEST(!(u10 < u9));
        BOOST_TEST(u9 < u11);
        BOOST_TEST(!(u11 < u9));

        BOOST_TEST(u10 < u11);
        BOOST_TEST(!(u11 < u10));

        BOOST_TEST(u12 < u13);
        BOOST_TEST(!(u13 < u12));
        BOOST_TEST(u14 < u15);
        BOOST_TEST(!(u15 < u14));
    }

    return boost::report_errors();
}
