//  (C) Copyright Andy Tompkins 2010. Permission to copy, use, modify, sell and
//  distribute this software is granted provided this copyright notice appears
//  in all copies. This software is provided "as is" without express or implied
//  warranty, and with no claim as to its suitability for any purpose.

// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

//  libs/uuid/test/test_string_generator.cpp  -------------------------------//

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/config.hpp>
#include <stdexcept>
#include <string>

int main(int, char*[])
{
    using namespace boost::uuids;
    
    uuid nil_uuid = {{0}};
    BOOST_TEST_EQ(nil_uuid.is_nil(), true);

    string_generator gen;
    uuid u = gen("00000000-0000-0000-0000-000000000000");
    BOOST_TEST_EQ(u, nil_uuid);
    BOOST_TEST_EQ(u.is_nil(), true);

    const uuid u_increasing = {{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef }};
    const uuid u_decreasing = {{ 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10 }};

    u = gen("0123456789abcdef0123456789ABCDEF");
    BOOST_TEST_EQ(memcmp(&u, &u_increasing, sizeof(uuid)), 0);
    BOOST_TEST_EQ(u, u_increasing);
    
    u = gen("{0123456789abcdef0123456789ABCDEF}");
    BOOST_TEST_EQ(u, u_increasing);

    u = gen("{01234567-89AB-CDEF-0123-456789abcdef}");
    BOOST_TEST_EQ(u, u_increasing);

    u = gen("01234567-89AB-CDEF-0123-456789abcdef");
    BOOST_TEST_EQ(u, u_increasing);
    
    u = gen(std::string("fedcba98-7654-3210-fedc-ba9876543210"));
    BOOST_TEST_EQ(u, u_decreasing);

#ifndef BOOST_NO_STD_WSTRING
    u = gen(L"fedcba98-7654-3210-fedc-ba9876543210");
    BOOST_TEST_EQ(u, u_decreasing);

    u = gen(std::wstring(L"01234567-89ab-cdef-0123-456789abcdef"));
    BOOST_TEST_EQ(u, u_increasing);

    u = gen(std::wstring(L"{01234567-89ab-cdef-0123-456789abcdef}"));
    BOOST_TEST_EQ(u, u_increasing);
#endif //BOOST_NO_STD_WSTRING

    const char raw[36] = { '0', '1', '2', '3', '4', '5', '6', '7', '-',
                           '8', '9', 'a', 'b', '-',
                           'c', 'd', 'e', 'f', '-',
                            0 , '1', '2', '3', '-',  // 0x00 character is intentional
                           '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    BOOST_TEST_THROWS(gen(std::string(raw, 36)), std::runtime_error);

    BOOST_TEST_THROWS(gen("01234567-89ab-cdef-0123456789abcdef"), std::runtime_error);
    BOOST_TEST_THROWS(gen("01234567-89ab-cdef0123-456789abcdef"), std::runtime_error);
    BOOST_TEST_THROWS(gen("01234567-89abcdef-0123-456789abcdef"), std::runtime_error);
    BOOST_TEST_THROWS(gen("0123456789ab-cdef-0123-456789abcdef"), std::runtime_error);

    BOOST_TEST_THROWS(gen("{01234567-89AB-CDEF-0123-456789abcdef)"), std::runtime_error);
    BOOST_TEST_THROWS(gen("{01234567-89AB-CDEF-0123-456789abcdef"), std::runtime_error);
    BOOST_TEST_THROWS(gen("01234567-89AB-CDEF-0123-456789abcdef}"), std::runtime_error);
#ifndef BOOST_NO_STD_WSTRING
    BOOST_TEST_THROWS(gen(std::wstring(L"{01234567-89AB-CDEF-0123-456789abcdef)")), std::runtime_error);
    BOOST_TEST_THROWS(gen(std::wstring(L"{01234567-89AB-CDEF-0123-456789abcdef")), std::runtime_error);
    BOOST_TEST_THROWS(gen(std::wstring(L"01234567-89AB-CDEF-0123-456789abcdef}")), std::runtime_error);
    BOOST_TEST_THROWS(gen(std::wstring(L"G1234567-89AB-CDEF-0123-456789abcdef}")), std::runtime_error);
#endif //BOOST_NO_STD_WSTRING

    BOOST_TEST_THROWS(gen("G1234567-89AB-CDEF-0123-456789abcdef"), std::runtime_error);
    BOOST_TEST_THROWS(gen("Have a great big roast-beef sandwich!"), std::runtime_error);

    BOOST_TEST_THROWS(gen("83f8638b-8dca-4152-zzzz-2ca8b33039b4"), std::runtime_error);

    return boost::report_errors();
}
