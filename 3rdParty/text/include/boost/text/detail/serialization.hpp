// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_SERIALIZATION_HPP
#define BOOST_TEXT_DETAIL_SERIALIZATION_HPP

#include <boost/text/collation_table.hpp>

#include <boost/endian/buffers.hpp>


namespace boost { namespace text { namespace detail {

    enum current_version : int { serialization_version = 0 };

    struct serialized_collation_element_t
    {
        endian::little_uint32_buf_t l1_;
        endian::little_uint16_buf_t l2_;
        endian::little_uint16_buf_t l3_;
        endian::little_uint32_buf_t l4_;
    };

    inline serialized_collation_element_t convert(collation_element ce)
    {
        serialized_collation_element_t retval;
        retval.l1_ = ce.l1_;
        retval.l2_ = ce.l2_;
        retval.l3_ = ce.l3_;
        retval.l4_ = ce.l4_;
        return retval;
    }

    inline collation_element convert(serialized_collation_element_t sce)
    {
        collation_element retval;
        retval.l1_ = sce.l1_.value();
        retval.l2_ = sce.l2_.value();
        retval.l3_ = sce.l3_.value();
        retval.l4_ = sce.l4_.value();
        return retval;
    }

    struct serialized_nonsimple_script_reorder_t
    {
        serialized_collation_element_t first_;
        serialized_collation_element_t last_;
        endian::little_uint32_buf_t lead_byte_;
    };

    inline serialized_nonsimple_script_reorder_t
    convert(nonsimple_script_reorder nsr)
    {
        serialized_nonsimple_script_reorder_t retval;
        retval.first_ = convert(nsr.first_);
        retval.last_ = convert(nsr.last_);
        retval.lead_byte_ = nsr.lead_byte_;
        return retval;
    }

    inline nonsimple_script_reorder
    convert(serialized_nonsimple_script_reorder_t snsr)
    {
        nonsimple_script_reorder retval;
        retval.first_ = convert(snsr.first_);
        retval.last_ = convert(snsr.last_);
        retval.lead_byte_ = snsr.lead_byte_.value();
        return retval;
    }

    struct header_t
    {
        header_t() {}
        header_t(
            collation_table_data const & table,
            collation_trie_t::trie_map_type const & trie)
        {
            version_ = detail::serialization_version;
            collation_elements_ = int(table.collation_element_vec_.size());
            trie_ = int(trie.size());
            nonstarters_ = int(table.nonstarter_table_.size());
            nonsimple_reorders_ = int(table.nonsimple_reorders_.size());
            simple_reorders_ = int(table.simple_reorders_.size());
            have_strength_ = !!table.strength_;
            if (table.strength_)
                strength_ = uint8_t(*table.strength_);
            have_weighting_ = !!table.weighting_;
            if (table.weighting_)
                weighting_ = uint8_t(*table.weighting_);
            have_l2_order_ = !!table.l2_order_;
            if (table.l2_order_)
                l2_order_ = uint8_t(*table.l2_order_);
            have_case_level_ = !!table.case_level_;
            if (table.case_level_)
                case_level_ = uint8_t(*table.case_level_);
            have_case_first_ = !!table.case_first_;
            if (table.case_first_)
                case_first_ = uint8_t(*table.case_first_);
        }

        endian::little_int32_buf_t version_;

        endian::little_int32_buf_t collation_elements_;
        endian::little_int32_buf_t trie_;
        endian::little_int32_buf_t nonstarters_;
        endian::little_int32_buf_t nonsimple_reorders_;
        endian::little_int32_buf_t simple_reorders_;

        endian::little_uint8_buf_t have_strength_;
        endian::little_uint8_buf_t strength_;
        endian::little_uint8_buf_t have_weighting_;
        endian::little_uint8_buf_t weighting_;
        endian::little_uint8_buf_t have_l2_order_;
        endian::little_uint8_buf_t l2_order_;
        endian::little_uint8_buf_t have_case_level_;
        endian::little_uint8_buf_t case_level_;
        endian::little_uint8_buf_t have_case_first_;
        endian::little_uint8_buf_t case_first_;
    };

    void header_to_table(header_t const & header, collation_table_data & table)
    {
        if (header.have_strength_.value())
            table.strength_ = collation_strength(header.strength_.value());
        if (header.have_weighting_.value())
            table.weighting_ = variable_weighting(header.weighting_.value());
        if (header.have_l2_order_.value())
            table.l2_order_ = l2_weight_order(header.l2_order_.value());
        if (header.have_case_level_.value())
            table.case_level_ = case_level(header.case_level_.value());
        if (header.have_case_first_.value())
            table.case_first_ = case_first(header.case_first_.value());
    }

    struct iterator_tag_t
    {};

    template<typename Source>
    struct tag
    {
        using type = iterator_tag_t;
    };

    template<typename Source>
    using tag_t = typename tag<Source>::type;

    template<typename Source, typename T>
    void read_bytes(Source & in, T & x)
    {
        read_bytes(in, x, tag_t<Source>{});
    }

    template<typename T, typename Sink>
    void write_bytes(T const & x, Sink & out)
    {
        write_bytes(x, out, tag_t<Sink>{});
    }

    template<typename Source>
    void read_trie(Source & in, collation_trie_t & trie, int size)
    {
        for (int i = 0; i < size; ++i) {
            endian::little_int32_buf_t key_size;
            read_bytes(in, key_size);
            collation_trie_key<32> key;
            key.size_ = key_size.value();
            for (int j = 0, end = key.size_; j < end; ++j) {
                endian::little_uint32_buf_t cp;
                read_bytes(in, cp);
                key.cps_.values_[j] = cp.value();
            }

            endian::little_uint16_buf_t first;
            read_bytes(in, first);
            endian::little_uint16_buf_t last;
            read_bytes(in, last);

            endian::little_uint8_buf_t lead_primary;
            read_bytes(in, lead_primary);
            endian::little_uint8_buf_t lead_primary_shifted;
            read_bytes(in, lead_primary_shifted);

            collation_elements const value{first.value(),
                                           last.value(),
                                           lead_primary.value(),
                                           lead_primary_shifted.value()};

            trie.insert(key, value);
        }
    }

    template<typename Sink>
    void write_trie(collation_trie_t::trie_map_type const & trie, Sink & out)
    {
        for (auto const & element : trie) {
            endian::little_int32_buf_t key_size;
            key_size = element.key.size_;
            write_bytes(key_size, out);
            for (int j = 0, end = element.key.size_; j < end; ++j) {
                endian::little_uint32_buf_t cp;
                cp = element.key.cps_.values_[j];
                write_bytes(cp, out);
            }

            endian::little_uint16_buf_t first;
            first = element.value.first();
            write_bytes(first, out);
            endian::little_uint16_buf_t last;
            last = element.value.last();
            write_bytes(last, out);

            endian::little_uint8_buf_t lead_primary;
            lead_primary =
                element.value.lead_primary(variable_weighting::non_ignorable);
            write_bytes(lead_primary, out);
            endian::little_uint8_buf_t lead_primary_shifted;
            lead_primary_shifted =
                element.value.lead_primary(variable_weighting::shifted);
            write_bytes(lead_primary_shifted, out);
        }
    }

    template<typename Source>
    void read_collation_elements(
        Source & in,
        std::vector<collation_element> & collation_element_vec,
        collation_element const *& collation_elements,
        int size)
    {
        if (size) {
            collation_element_vec.resize(size);
            for (int i = 0; i < size; ++i) {
                serialized_collation_element_t sce;
                read_bytes(in, sce);
                collation_element_vec[i] = convert(sce);
            }
        } else {
            collation_elements = collation_elements_ptr();
        }
    }

    template<typename Sink>
    void write_collation_elements(
        std::vector<collation_element> const & collation_element_vec,
        collation_element const * collation_elements,
        Sink & out)
    {
        for (auto ce : collation_element_vec) {
            serialized_collation_element_t const sce = convert(ce);
            write_bytes(sce, out);
        }
    }

    template<typename Source>
    void read_nonstarters(
        Source & in,
        std::vector<unsigned char> & nonstarter_table,
        unsigned char const *& nonstarters,
        int size)
    {
        if (size) {
            nonstarter_table.resize(size);
            for (int i = 0; i < size; ++i) {
                endian::little_uint8_buf_t c;
                read_bytes(in, c);
                nonstarter_table[i] = c.value();
            }
        } else {
            nonstarters = default_table_nonstarters_ptr();
        }
    }

    template<typename Sink>
    void write_nonstarters(
        std::vector<unsigned char> const & nonstarter_table,
        unsigned char const * nonstarters,
        Sink & out)
    {
        endian::little_uint8_buf_t c;
        for (unsigned char c_ : nonstarter_table) {
            c = c_;
            write_bytes(c, out);
        }
    }

    template<typename Source>
    void read_nonsimple_reorders(
        Source & in, nonsimple_reorders_t & nonsimple_reorders, int size)
    {
        nonsimple_reorders.resize(size);
        for (int i = 0; i < size; ++i) {
            serialized_nonsimple_script_reorder_t snsr;
            read_bytes(in, snsr);
            nonsimple_reorders[i] = convert(snsr);
        }
    }

    template<typename Sink>
    void write_nonsimple_reorders(
        nonsimple_reorders_t const & nonsimple_reorders, Sink & out)
    {
        for (auto reorder : nonsimple_reorders) {
            serialized_nonsimple_script_reorder_t const snsr = convert(reorder);
            write_bytes(snsr, out);
        }
    }

    template<typename Source>
    void read_simple_reorders(
        Source & in, std::array<uint32_t, 256> & simple_reorders, int size)
    {
        BOOST_ASSERT(size == 256);
        for (int i = 0; i < size; ++i) {
            endian::little_uint32_buf_t r;
            read_bytes(in, r);
            simple_reorders[i] = r.value();
        }
    }

    template<typename Sink>
    void write_simple_reorders(
        std::array<uint32_t, 256> const & simple_reorders, Sink & out)
    {
        for (auto reorder : simple_reorders) {
            endian::little_uint32_buf_t const r{reorder};
            write_bytes(r, out);
        }
    }

}}}

#endif
