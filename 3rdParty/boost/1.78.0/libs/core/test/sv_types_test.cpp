// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/core/is_same.hpp>
#include <iterator>

struct Ch
{
};

int main()
{
    typedef boost::core::basic_string_view<Ch> ch_string_view;

    BOOST_TEST_TRAIT_SAME(ch_string_view::traits_type, std::char_traits<Ch>);
    BOOST_TEST_TRAIT_SAME(ch_string_view::value_type, Ch);
    BOOST_TEST_TRAIT_SAME(ch_string_view::pointer, Ch*);
    BOOST_TEST_TRAIT_SAME(ch_string_view::const_pointer, Ch const*);
    BOOST_TEST_TRAIT_SAME(ch_string_view::reference, Ch&);
    BOOST_TEST_TRAIT_SAME(ch_string_view::const_reference, Ch const&);
    BOOST_TEST_TRAIT_SAME(ch_string_view::iterator, ch_string_view::const_iterator);
    BOOST_TEST_TRAIT_SAME(std::iterator_traits<ch_string_view::iterator>::iterator_category, std::random_access_iterator_tag);
    BOOST_TEST_TRAIT_SAME(ch_string_view::reverse_iterator, ch_string_view::const_reverse_iterator);
    BOOST_TEST_TRAIT_SAME(ch_string_view::reverse_iterator, std::reverse_iterator<ch_string_view::iterator>);
    BOOST_TEST_TRAIT_SAME(ch_string_view::size_type, std::size_t);
    BOOST_TEST_TRAIT_SAME(ch_string_view::difference_type, std::ptrdiff_t);

    BOOST_TEST_EQ(ch_string_view::npos, static_cast<std::size_t>(-1));

    BOOST_TEST_TRAIT_SAME(boost::core::string_view, boost::core::basic_string_view<char>);
    BOOST_TEST_TRAIT_SAME(boost::core::wstring_view, boost::core::basic_string_view<wchar_t>);
#if !defined(BOOST_NO_CXX11_CHAR16_T)
    BOOST_TEST_TRAIT_SAME(boost::core::u16string_view, boost::core::basic_string_view<char16_t>);
#endif
#if !defined(BOOST_NO_CXX11_CHAR32_T)
    BOOST_TEST_TRAIT_SAME(boost::core::u32string_view, boost::core::basic_string_view<char32_t>);
#endif
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
    BOOST_TEST_TRAIT_SAME(boost::core::u8string_view, boost::core::basic_string_view<char8_t>);
#endif

    return boost::report_errors();
}
