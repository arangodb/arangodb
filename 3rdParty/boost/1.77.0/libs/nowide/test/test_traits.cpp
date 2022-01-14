//
//  Copyright (c) 2020 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/nowide/detail/is_path.hpp>
#include <boost/nowide/detail/is_string_container.hpp>
#include "test.hpp"
#include <iostream>
#include <string>

#ifdef __has_include
#if __has_include(<version>)
#include <version>
#endif
#endif

#ifdef __cpp_lib_string_view
#include <string_view>
#define BOOST_NOWIDE_TEST_STD_STRINGVIEW
#endif

// Exclude apple as support there is target level specific -.-
#if defined(__cpp_lib_filesystem) && !defined(__APPLE__)
#include <filesystem>
#define BOOST_NOWIDE_TEST_SFS_PATH
#endif

#ifdef BOOST_NOWIDE_TEST_BFS_PATH
#include <boost/filesystem/path.hpp>
#endif

using boost::nowide::detail::is_string_container;
static_assert(is_string_container<std::string, true>::value, "!");
static_assert(is_string_container<std::wstring, false>::value, "!");
static_assert(is_string_container<std::u16string, false>::value, "!");
static_assert(is_string_container<std::u32string, false>::value, "!");
static_assert(!is_string_container<int, true>::value, "!");
static_assert(!is_string_container<int, false>::value, "!");

using boost::nowide::detail::get_data_width;
static_assert(get_data_width<std::string>::value == sizeof(char), "!");
static_assert(get_data_width<std::wstring>::value == sizeof(wchar_t), "!");
static_assert(get_data_width<std::u16string>::value == sizeof(char16_t), "!");
static_assert(get_data_width<std::u32string>::value == sizeof(char32_t), "!");

// coverity [root_function]
void test_main(int, char**, char**)
{
#ifdef BOOST_NOWIDE_TEST_STD_STRINGVIEW
    std::cout << "Testing string_view" << std::endl;
    static_assert(is_string_container<std::string_view, true>::value, "!");
    static_assert(is_string_container<std::wstring_view, false>::value, "!");
    static_assert(is_string_container<std::u16string_view, false>::value, "!");
    static_assert(is_string_container<std::u32string_view, false>::value, "!");
#endif
#ifdef BOOST_NOWIDE_TEST_SFS_PATH
    std::cout << "Testing std::filesystem::path" << std::endl;
    static_assert(boost::nowide::detail::is_path<std::filesystem::path>::value, "!");
#endif
#ifdef BOOST_NOWIDE_TEST_BFS_PATH
    std::cout << "Testing boost::filesystem::path" << std::endl;
    static_assert(boost::nowide::detail::is_path<boost::filesystem::path>::value, "!");
#endif
}
