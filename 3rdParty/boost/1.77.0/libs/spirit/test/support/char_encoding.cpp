/*=============================================================================
    Copyright (c) 2020 Nikita Kniazev

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/spirit/home/support/char_encoding/standard_wide.hpp>

#include <boost/core/lightweight_test.hpp>

#if defined(_MSC_VER) && _MSC_VER < 1700
# pragma warning(disable: 4428) // universal-character-name encountered in source
#endif

int main()
{
    {
        using boost::spirit::char_encoding::standard_wide;
        BOOST_TEST_EQ(standard_wide::toucs4(L'\uFFFF'), 0xFFFFu);
        BOOST_TEST_EQ(standard_wide::toucs4(L'\u7FFF'), 0x7FFFu);
        BOOST_TEST_EQ(standard_wide::toucs4(L'\u0024'), 0x0024u);
    }

    return boost::report_errors();
}
