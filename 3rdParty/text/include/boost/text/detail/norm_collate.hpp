// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_NORM_COLLATE_HPP
#define BOOST_TEXT_DETAIL_NORM_COLLATE_HPP

#include <boost/text/collate.hpp>
#include <boost/text/utf.hpp>


namespace boost { namespace text { namespace detail {

    template<nf Normalization, typename CU, typename GraphemeRange>
    text_sort_key norm_collation_sort_key(
        GraphemeRange const & str,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        constexpr format utf_format = detail::format_of<CU>();
        if (Normalization != nf::d && Normalization != nf::fcc) {
            detail::collation_norm_buffer_for<utf_format> buffer;
            boost::text::normalize_append<nf::fcc>(
                str.begin().base(), str.end().base(), buffer);
            return boost::text::collation_sort_key(
                boost::text::as_utf32(buffer), table, flags);
        } else {
            return boost::text::collation_sort_key(
                str.begin().base(), str.end().base(), table, flags);
        }
    }

    template<
        nf Normalization1,
        typename CU1,
        nf Normalization2,
        typename CU2,
        typename GraphemeRange1,
        typename GraphemeRange2>
    int norm_collate(
        GraphemeRange1 const & str1,
        GraphemeRange2 const & str2,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        constexpr format utf_format1 = detail::format_of<CU1>();
        constexpr format utf_format2 = detail::format_of<CU2>();
        if (Normalization1 != nf::d && Normalization1 != nf::fcc) {
            detail::collation_norm_buffer_for<utf_format1> buffer1;
            boost::text::normalize_append<nf::fcc>(
                str1.begin().base(), str1.end().base(), buffer1);
            auto const r1 = boost::text::as_utf32(buffer1);
            if (Normalization2 != nf::d && Normalization2 != nf::fcc) {
                detail::collation_norm_buffer_for<utf_format2> buffer2;
                boost::text::normalize_append<nf::fcc>(
                    str2.begin().base(), str2.end().base(), buffer2);
                auto const r2 = boost::text::as_utf32(buffer2);
                return boost::text::collate(r1, r2, table, flags);
            }
            return boost::text::collate(
                r1.begin(),
                r1.end(),
                str2.begin().base(),
                str2.end().base(),
                table,
                flags);
        } else if (Normalization2 != nf::d && Normalization2 != nf::fcc) {
            detail::collation_norm_buffer_for<utf_format2> buffer2;
            boost::text::normalize_append<nf::fcc>(
                str2.begin().base(), str2.end().base(), buffer2);
            auto const r2 = boost::text::as_utf32(buffer2);
            return boost::text::collate(
                str1.begin().base(),
                str1.end().base(),
                r2.begin(),
                r2.end(),
                table,
                flags);
        }
        return boost::text::collate(
            str1.begin().base(),
            str1.end().base(),
            str2.begin().base(),
            str2.end().base(),
            table,
            flags);
    }

}}}

#endif
