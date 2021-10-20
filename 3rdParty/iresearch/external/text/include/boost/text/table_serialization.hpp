// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_TABLE_SERIALIZATION_HPP
#define BOOST_TEXT_TABLE_SERIALIZATION_HPP

#include <boost/text/collation_table.hpp>
#include <boost/text/detail/serialization.hpp>


namespace boost { namespace text {

    template<typename CharIter>
    CharIter write_table(collation_table const & table, CharIter out) noexcept;

    /** The type returned by read_table().  Contains the resulting collation
        table and the final position of the iterator from which the table was
        read. */
    template<typename CharIter>
    struct read_table_result
    {
        operator collation_table() && { return table; }

        collation_table table;
        CharIter it;
    };

    template<typename CharIter>
    read_table_result<CharIter> read_table(CharIter it);

    namespace detail {

        template<typename CharIter, typename T>
        void read_bytes(CharIter & it, T & x, iterator_tag_t)
        {
            char * ptr = reinterpret_cast<char *>(&x);
            for (std::ptrdiff_t i = 0, end = sizeof(T); i < end;
                 ++i, ++ptr, ++it) {
                *ptr = *it;
            }
        }

        template<typename T, typename CharIter>
        void write_bytes(T const & x, CharIter & out, iterator_tag_t)
        {
            char const * const ptr = reinterpret_cast<char const *>(&x);
            out = std::copy_n(ptr, sizeof(T), out);
        }
    }

    /** Serializes the given collation table, writing the results to `out`.
        Returns the final value of `out` after the write. */
    template<typename CharIter>
    CharIter
    write_table(collation_table const & table_proper, CharIter out) noexcept
    {
        auto const & table = *table_proper.data_;

        detail::header_t header(table);

        detail::write_bytes(header, out);

        detail::write_collation_elements(
            table.collation_element_vec_, table.collation_elements_, out);

        detail::write_nonstarters(
            table.nonstarter_table_, table.nonstarters_, out);

        detail::write_nonsimple_reorders(table.nonsimple_reorders_, out);

        detail::write_simple_reorders(table.simple_reorders_, out);

        detail::write_trie(table.trie_, out);

        return out;
    }

    /** Deserializes a collation table by reading from `it`.  Returns the
        final value of `it` after the read. */
    template<typename CharIter>
    read_table_result<CharIter> read_table(CharIter it)
    {
        collation_table retval;

        auto & table = *retval.data_;

        detail::header_t header;
        detail::read_bytes(it, header);

        detail::read_collation_elements(
            it,
            table.collation_element_vec_,
            table.collation_elements_,
            header.collation_elements_.value());

        detail::read_nonstarters(
            it,
            table.nonstarter_table_,
            table.nonstarters_,
            header.nonstarters_.value());

        detail::read_nonsimple_reorders(
            it, table.nonsimple_reorders_, header.nonsimple_reorders_.value());

        detail::read_simple_reorders(
            it, table.simple_reorders_, header.simple_reorders_.value());

        detail::header_to_table(header, table);

        detail::read_trie(it, table.trie_, header.trie_.value());

        return read_table_result<CharIter>{std::move(retval), it};
    }

}}

#endif
