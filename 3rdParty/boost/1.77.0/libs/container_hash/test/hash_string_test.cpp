
// Copyright 2005-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "./config.hpp"

#ifdef BOOST_HASH_TEST_STD_INCLUDES
#  include <functional>
#else
#  include <boost/container_hash/hash.hpp>
#endif

#include <boost/core/lightweight_test.hpp>
#include <string>
#include "./compile_time.hpp"

void string_tests()
{
    compile_time_tests((std::string*) 0);

    BOOST_HASH_TEST_NAMESPACE::hash<std::string> x1;
    BOOST_HASH_TEST_NAMESPACE::hash<std::string> x2;

    BOOST_TEST(x1("Hello") == x2(std::string("Hel") + "lo"));
    BOOST_TEST(x1("") == x2(std::string()));

#if defined(BOOST_HASH_TEST_EXTENSIONS)
    std::string value1;
    std::string value2("Hello");

    BOOST_TEST(x1(value1) == BOOST_HASH_TEST_NAMESPACE::hash_value(value1));
    BOOST_TEST(x1(value2) == BOOST_HASH_TEST_NAMESPACE::hash_value(value2));
    BOOST_TEST(BOOST_HASH_TEST_NAMESPACE::hash_value(value1) ==
            BOOST_HASH_TEST_NAMESPACE::hash_range(value1.begin(), value1.end()));
    BOOST_TEST(BOOST_HASH_TEST_NAMESPACE::hash_value(value2) ==
            BOOST_HASH_TEST_NAMESPACE::hash_range(value2.begin(), value2.end()));
#endif
}

void string0_tests()
{
    std::string x1(1, '\0');
    std::string x2(2, '\0');
    std::string x3(3, '\0');
    std::string x4(10, '\0');

    BOOST_HASH_TEST_NAMESPACE::hash<std::string> hasher;

    BOOST_TEST(hasher(x1) != hasher(x2));
    BOOST_TEST(hasher(x1) != hasher(x3));
    BOOST_TEST(hasher(x1) != hasher(x4));
    BOOST_TEST(hasher(x2) != hasher(x3));
    BOOST_TEST(hasher(x2) != hasher(x4));
    BOOST_TEST(hasher(x3) != hasher(x4));
}

#if !defined(BOOST_NO_STD_WSTRING) && !defined(BOOST_NO_INTRINSIC_WCHAR_T)
void wstring_tests()
{
    compile_time_tests((std::wstring*) 0);

    BOOST_HASH_TEST_NAMESPACE::hash<std::wstring> x1;
    BOOST_HASH_TEST_NAMESPACE::hash<std::wstring> x2;

    BOOST_TEST(x1(L"Hello") == x2(std::wstring(L"Hel") + L"lo"));
    BOOST_TEST(x1(L"") == x2(std::wstring()));

#if defined(BOOST_HASH_TEST_EXTENSIONS)
    std::wstring value1;
    std::wstring value2(L"Hello");

    BOOST_TEST(x1(value1) == BOOST_HASH_TEST_NAMESPACE::hash_value(value1));
    BOOST_TEST(x1(value2) == BOOST_HASH_TEST_NAMESPACE::hash_value(value2));
    BOOST_TEST(BOOST_HASH_TEST_NAMESPACE::hash_value(value1) ==
            BOOST_HASH_TEST_NAMESPACE::hash_range(value1.begin(), value1.end()));
    BOOST_TEST(BOOST_HASH_TEST_NAMESPACE::hash_value(value2) ==
            BOOST_HASH_TEST_NAMESPACE::hash_range(value2.begin(), value2.end()));
#endif
}
#endif

#if !defined(BOOST_NO_CXX11_CHAR16_T)
void u16string_tests()
{
    compile_time_tests((std::u16string*) 0);

    BOOST_HASH_TEST_NAMESPACE::hash<std::u16string> x1;
    BOOST_HASH_TEST_NAMESPACE::hash<std::u16string> x2;

    BOOST_TEST(x1(u"Hello") == x2(std::u16string(u"Hel") + u"lo"));
    BOOST_TEST(x1(u"") == x2(std::u16string()));

#if defined(BOOST_HASH_TEST_EXTENSIONS)
    std::u16string value1;
    std::u16string value2(u"Hello");

    BOOST_TEST(x1(value1) == BOOST_HASH_TEST_NAMESPACE::hash_value(value1));
    BOOST_TEST(x1(value2) == BOOST_HASH_TEST_NAMESPACE::hash_value(value2));
    BOOST_TEST(BOOST_HASH_TEST_NAMESPACE::hash_value(value1) ==
            BOOST_HASH_TEST_NAMESPACE::hash_range(value1.begin(), value1.end()));
    BOOST_TEST(BOOST_HASH_TEST_NAMESPACE::hash_value(value2) ==
            BOOST_HASH_TEST_NAMESPACE::hash_range(value2.begin(), value2.end()));
#endif
}
#endif

#if !defined(BOOST_NO_CXX11_CHAR32_T)
void u32string_tests()
{
    compile_time_tests((std::u32string*) 0);

    BOOST_HASH_TEST_NAMESPACE::hash<std::u32string> x1;
    BOOST_HASH_TEST_NAMESPACE::hash<std::u32string> x2;

    BOOST_TEST(x1(U"Hello") == x2(std::u32string(U"Hel") + U"lo"));
    BOOST_TEST(x1(U"") == x2(std::u32string()));

#if defined(BOOST_HASH_TEST_EXTENSIONS)
    std::u32string value1;
    std::u32string value2(U"Hello");

    BOOST_TEST(x1(value1) == BOOST_HASH_TEST_NAMESPACE::hash_value(value1));
    BOOST_TEST(x1(value2) == BOOST_HASH_TEST_NAMESPACE::hash_value(value2));
    BOOST_TEST(BOOST_HASH_TEST_NAMESPACE::hash_value(value1) ==
            BOOST_HASH_TEST_NAMESPACE::hash_range(value1.begin(), value1.end()));
    BOOST_TEST(BOOST_HASH_TEST_NAMESPACE::hash_value(value2) ==
            BOOST_HASH_TEST_NAMESPACE::hash_range(value2.begin(), value2.end()));
#endif
}
#endif

template <typename StringType>
void generic_string_tests(StringType*)
{
    std::string x1(1, '\0');
    std::string x2(2, '\0');
    std::string x3(3, '\0');
    std::string x4(10, '\0');
    std::string x5 = x2 + "hello" + x2;

    StringType strings[] = {
        "",
        "hello",
        x1,
        x2,
        x3,
        x4,
        x5
    };

    std::size_t const strings_length = sizeof(strings) / sizeof(StringType);
    boost::hash<StringType> hash;

    for (std::size_t i = 0; i < strings_length; ++i) {
        std::size_t hash_i = hash(strings[i]);
        for (std::size_t j = 0; j < strings_length; ++j) {
            std::size_t hash_j = hash(strings[j]);
            BOOST_TEST((hash_i == hash_j) == (i == j));
        }
    }
}

int main()
{
    string_tests();
    string0_tests();
#if !defined(BOOST_NO_STD_WSTRING) && !defined(BOOST_NO_INTRINSIC_WCHAR_T)
    wstring_tests();
#endif
#if !defined(BOOST_NO_CXX11_CHAR16_T)
    u16string_tests();
#endif
#if !defined(BOOST_NO_CXX11_CHAR32_T)
    u32string_tests();
#endif

    generic_string_tests((std::string*) 0);
#if BOOST_HASH_HAS_STRING_VIEW
    generic_string_tests((std::string_view*) 0);
#endif

    return boost::report_errors();
}
