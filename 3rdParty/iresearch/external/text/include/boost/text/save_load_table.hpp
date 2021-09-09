// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_SAVE_LOAD_TABLE_HPP
#define BOOST_TEXT_SAVE_LOAD_TABLE_HPP

#include <boost/text/collation_table.hpp>
#include <boost/text/detail/serialization.hpp>

#include <boost/filesystem.hpp>


namespace boost { namespace text {

    struct collation_table;

    void
    save_table(collation_table const & table, filesystem::path const & path);
    collation_table load_table(filesystem::path const & path);

    namespace detail {

        struct filesystem_fstream_tag_t
        {};

        template<>
        struct tag<filesystem::ifstream>
        {
            using type = filesystem_fstream_tag_t;
        };

        template<>
        struct tag<filesystem::ofstream>
        {
            using type = filesystem_fstream_tag_t;
        };

        template<typename T>
        void
        read_bytes(filesystem::ifstream & ifs, T & x, filesystem_fstream_tag_t)
        {
            ifs.read(reinterpret_cast<char *>(&x), sizeof(T));
        }

        template<typename T>
        void write_bytes(
            T const & x, filesystem::ofstream & ofs, filesystem_fstream_tag_t)
        {
            ofs.write(reinterpret_cast<char const *>(&x), sizeof(T));
        }
    }

    /** Writes the given collation table to `path`. */
    void save_table(
        collation_table const & table_proper, filesystem::path const & path)
    {
        auto const & table = *table_proper.data_;

        detail::collation_trie_t::trie_map_type trie_map(table.trie_.impl_);

        detail::header_t header(table, trie_map);

        filesystem::ofstream ofs(path, std::ios_base::binary);

        detail::write_bytes(header, ofs);

        detail::write_collation_elements(
            table.collation_element_vec_, table.collation_elements_, ofs);

        detail::write_nonstarters(
            table.nonstarter_table_, table.nonstarters_, ofs);

        detail::write_nonsimple_reorders(table.nonsimple_reorders_, ofs);

        detail::write_simple_reorders(table.simple_reorders_, ofs);

        detail::write_trie(trie_map, ofs);
    }

    /** Reads a collation table from `path`. */
    collation_table load_table(filesystem::path const & path)
    {
        collation_table retval;

        auto & table = *retval.data_;

        detail::header_t header;

        filesystem::ifstream ifs(path, std::ios_base::binary);

        detail::read_bytes(ifs, header);

        detail::read_collation_elements(
            ifs,
            table.collation_element_vec_,
            table.collation_elements_,
            header.collation_elements_.value());

        detail::read_nonstarters(
            ifs,
            table.nonstarter_table_,
            table.nonstarters_,
            header.nonstarters_.value());

        detail::read_nonsimple_reorders(
            ifs, table.nonsimple_reorders_, header.nonsimple_reorders_.value());

        detail::read_simple_reorders(
            ifs, table.simple_reorders_, header.simple_reorders_.value());

        detail::header_to_table(header, table);

        detail::read_trie(ifs, table.trie_, header.trie_.value());

        return retval;
    }

}}

#endif
