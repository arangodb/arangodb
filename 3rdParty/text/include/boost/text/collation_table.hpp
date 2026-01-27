// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_COLLATION_TAILORING_HPP
#define BOOST_TEXT_COLLATION_TAILORING_HPP

#include <boost/text/collation_fwd.hpp>
#include <boost/text/normalize.hpp>
#include <boost/text/segmented_vector.hpp>
#include <boost/text/string_utility.hpp>
#include <boost/text/detail/collation_data.hpp>
#include <boost/text/detail/parser.hpp>

#include <boost/throw_exception.hpp>
#include <boost/stl_interfaces/view_interface.hpp>

#include <map>
#include <numeric>
#include <vector>

#ifndef BOOST_TEXT_DOXYGEN

#define BOOST_TEXT_TAILORING_INSTRUMENTATION 0
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
#include <iostream>
#endif


namespace boost { namespace filesystem {
    class path;
}}

#endif

namespace boost { namespace text {

    namespace detail {

        struct nonsimple_script_reorder
        {
            collation_element first_;
            collation_element last_;
            uint32_t lead_byte_;

            friend bool operator==(
                nonsimple_script_reorder lhs, nonsimple_script_reorder rhs)
            {
                return lhs.first_ == rhs.first_ && lhs.last_ == rhs.last_ &&
                       lhs.lead_byte_ == rhs.lead_byte_;
            }

            friend bool operator!=(
                nonsimple_script_reorder lhs, nonsimple_script_reorder rhs)
            {
                return !(lhs == rhs);
            }
        };

        using nonsimple_reorders_t =
            container::static_vector<nonsimple_script_reorder, 140>;

        inline uint32_t lead_byte(
            collation_element cce,
            nonsimple_reorders_t const & nonsimple_reorders,
            std::array<uint32_t, 256> const & simple_reorders) noexcept
        {
            auto const it = std::find_if(
                nonsimple_reorders.begin(),
                nonsimple_reorders.end(),
                [cce](detail::nonsimple_script_reorder reorder) {
                    return reorder.first_ <= cce && cce < reorder.last_;
                });
            if (it != nonsimple_reorders.end())
                return it->lead_byte_ << 24;
            auto const masked_primary = cce.l1_ & 0xff000000;
            return simple_reorders[masked_primary >> 24] << 24;
        }

        struct temp_table_element
        {
            using ces_t = container::small_vector<collation_element, 8 * 10>;

            cp_seq_t cps_;
            ces_t ces_;

            bool tailored_ = false;
        };

        inline bool less(
            temp_table_element::ces_t const & lhs,
            temp_table_element::ces_t const & rhs) noexcept;

        inline bool less_equal(
            temp_table_element::ces_t const & lhs,
            temp_table_element::ces_t const & rhs) noexcept
        {
            if (lhs == rhs)
                return true;
            return detail::less(lhs, rhs);
        }

        inline bool operator<(
            temp_table_element const & lhs, temp_table_element const & rhs)
        {
            return detail::less(lhs.ces_, rhs.ces_);
        }

        inline bool operator<(
            temp_table_element::ces_t const & lhs,
            temp_table_element const & rhs)
        {
            return detail::less(lhs, rhs.ces_);
        }

        inline bool operator<(
            temp_table_element const & lhs,
            temp_table_element::ces_t const & rhs)
        {
            return detail::less(lhs.ces_, rhs);
        }

        using temp_table_t = segmented_vector<temp_table_element>;

        struct logical_positions_t
        {
            temp_table_element::ces_t & operator[](uint32_t symbolic)
            {
                return cces_[symbolic - first_tertiary_ignorable];
            }
            std::array<temp_table_element::ces_t, 12> cces_;
        };

        struct tailoring_state_t
        {
            uint8_t first_tertiary_in_secondary_masked_ =
                first_tertiary_in_secondary_masked;
            uint8_t last_tertiary_in_secondary_masked_ =
                last_tertiary_in_secondary_masked;
            uint16_t first_secondary_in_primary_ = first_secondary_in_primary;
            uint16_t last_secondary_in_primary_ = last_secondary_in_primary;
        };

        struct collation_table_data
        {
            collation_table_data() : collation_elements_(nullptr)
            {
                std::iota(simple_reorders_.begin(), simple_reorders_.end(), 0);
            }

            std::vector<collation_element> collation_element_vec_;
            collation_element const * collation_elements_;
            collation_trie_t trie_;

            uint32_t nonstarter_first_;
            uint32_t nonstarter_last_;
            std::vector<unsigned char> nonstarter_table_;
            unsigned char const * nonstarters_;

            nonsimple_reorders_t nonsimple_reorders_;
            std::array<uint32_t, 256> simple_reorders_;

            optional<collation_strength> strength_;
            optional<variable_weighting> weighting_;
            optional<l2_weight_order> l2_order_;
            optional<case_level> case_level_;
            optional<case_first> case_first_;

            friend bool operator==(
                collation_table_data const & lhs,
                collation_table_data const & rhs)
            {
                return lhs.collation_element_vec_ ==
                           rhs.collation_element_vec_ &&
                       ((lhs.collation_elements_ == nullptr) ==
                        (rhs.collation_elements_ == nullptr)) &&
                       lhs.trie_ == rhs.trie_ &&
                       lhs.nonstarter_first_ == rhs.nonstarter_first_ &&
                       lhs.nonstarter_last_ == rhs.nonstarter_last_ &&
                       lhs.nonstarter_table_ == rhs.nonstarter_table_ &&
                       ((lhs.nonstarters_ == nullptr) ==
                        (rhs.nonstarters_ == nullptr)) &&
                       lhs.nonsimple_reorders_ == rhs.nonsimple_reorders_ &&
                       lhs.simple_reorders_ == rhs.simple_reorders_ &&
                       lhs.strength_ == rhs.strength_ &&
                       lhs.weighting_ == rhs.weighting_ &&
                       lhs.l2_order_ == rhs.l2_order_ &&
                       lhs.case_level_ == rhs.case_level_ &&
                       lhs.case_first_ == rhs.case_first_;
            }

            friend bool operator!=(
                collation_table_data const & lhs,
                collation_table_data const & rhs)
            {
                return !(lhs == rhs);
            }
        };

        inline void add_temp_tailoring(
            collation_table_data & table,
            detail::cp_seq_t const & cps,
            detail::temp_table_element::ces_t const & ces)
        {
            auto const value_first =
                (uint16_t)table.collation_element_vec_.size();
            table.collation_element_vec_.insert(
                table.collation_element_vec_.end(), ces.begin(), ces.end());
            detail::collation_elements value{
                value_first, (uint16_t)table.collation_element_vec_.size()};

            table.trie_.insert_or_assign(cps, value);

#if BOOST_TEXT_TAILORING_INSTRUMENTATION
            std::cerr << "add_temp_tailoring() ";
            bool first = true;
            for (auto cp : cps) {
                if (!first)
                    std::cerr << " ";
                std::cerr << std::hex << std::setw(4) << std::setfill('0')
                          << cp;
                first = false;
            }
            std::cerr << " ";
            for (auto ce : ces) {
                std::cerr << "[" << std::hex << std::setw(8)
                          << std::setfill('0') << ce.l1_ << " " << std::setw(4)
                          << ce.l2_ << " " << std::setw(2) << std::setfill('0')
                          << ce.l3_ << "] ";
            }
            std::cerr << "\n";
#endif
        }
    }

    struct collation_compare;

    /** A collation table.  Such a table is necessary to use the collation
        API.

        collation_table has the semantics of a `std::shared_ptr` to `const`.
        It can be copied cheaply; copies should be made freely.

        \see default_collation_table()
        \see tailored_collation_table()
    */
    struct collation_table
    {
#ifndef BOOST_TEXT_DOXYGEN

        /** Returns a comparison object. */
        collation_compare compare(
            collation_strength strength = collation_strength::tertiary,
            case_first case_1st = case_first::off,
            case_level case_lvl = case_level::off,
            variable_weighting weighting = variable_weighting::non_ignorable,
            l2_weight_order l2_order = l2_weight_order::forward) const;

        /** Returns a comparison object. */
        collation_compare compare(collation_flags flags) const;

        template<
            typename CPIter,
            typename CEOutIter,
            typename SizeOutIter = std::ptrdiff_t *>
        CEOutIter copy_collation_elements(
            CPIter first,
            CPIter last,
            CEOutIter out,
            collation_strength strength = collation_strength::tertiary,
            case_first case_1st = case_first::off,
            case_level case_lvl = case_level::off,
            variable_weighting weighting = variable_weighting::non_ignorable,
            SizeOutIter * size_out = nullptr) const;

        optional<l2_weight_order> l2_order() const noexcept
        {
            return data_->l2_order_;
        }

        optional<case_first> case_1st() const noexcept
        {
            return data_->case_first_;
        }

        optional<case_level> case_lvl() const noexcept
        {
            return data_->case_level_;
        }

        optional<variable_weighting> weighting() const noexcept
        {
            return data_->weighting_;
        }

        detail::collation_trie_t const & trie() const noexcept
        {
            return data_->trie_;
        }

        bool nonstarter(uint32_t cp) const noexcept
        {
            if (cp < data_->nonstarter_first_ || data_->nonstarter_last_ <= cp)
                return false;
            uint32_t const table_element = cp - data_->nonstarter_first_;
            unsigned char const c = data_->nonstarters_[table_element >> 3];
            return c & (1 << (table_element & 0b111));
        }

        friend bool
        operator==(collation_table const & lhs, collation_table const & rhs)
        {
            return *lhs.data_ == *rhs.data_;
        }

        friend bool
        operator!=(collation_table const & lhs, collation_table const & rhs)
        {
            return !(lhs == rhs);
        }

    private:
        collation_table() :
            data_(std::make_shared<detail::collation_table_data>())
        {}

        detail::collation_element const *
        collation_elements_begin() const noexcept
        {
            return data_->collation_elements_
                       ? data_->collation_elements_
                       : &data_->collation_element_vec_[0];
        }

        std::shared_ptr<detail::collation_table_data> data_;

        friend collation_table default_collation_table();

        friend collation_table tailored_collation_table(
            string_view tailoring,
            string_view tailoring_filename,
            parser_diagnostic_callback report_errors,
            parser_diagnostic_callback report_warnings);

        template<typename CharIter>
        friend CharIter
        write_table(collation_table const & table, CharIter out) noexcept;
        template<typename CharIter>
        friend read_table_result<CharIter> read_table(CharIter it);

        friend void save_table(
            collation_table const & table, filesystem::path const & path);
        friend collation_table load_table(filesystem::path const & path);

        template<
            typename CPIter1,
            typename Sentinel1,
            typename CPIter2,
            typename Sentinel2>
        friend int detail::collate(
            CPIter1 lhs_first,
            Sentinel1 lhs_last,
            CPIter2 rhs_first,
            Sentinel2 rhs_last,
            collation_strength strength,
            case_first case_1st,
            case_level case_lvl,
            variable_weighting weighting,
            l2_weight_order l2_order,
            collation_table const & table);
#endif
    };

#ifdef BOOST_TEXT_DOXYGEN

    bool operator==(collation_table const & lhs, collation_table const & rhs);
    bool operator!=(collation_table const & lhs, collation_table const & rhs);

#endif

    /** A function object suitable for use with standard algorithms that
        accept a comparison object. */
    struct collation_compare
    {
        collation_compare(
            collation_table table,
            collation_strength strength,
            case_first case_1st = case_first::off,
            case_level case_lvl = case_level::off,
            variable_weighting weighting = variable_weighting::non_ignorable,
            l2_weight_order l2_order = l2_weight_order::forward) :
            table_(std::move(table)),
            strength_(strength),
            case_first_(case_1st),
            case_level_(case_lvl),
            weighting_(weighting),
            l2_order_(l2_order)
        {}

        collation_compare(collation_table table, collation_flags flags) :
            table_(std::move(table)),
            strength_(detail::to_strength(flags)),
            case_first_(detail::to_case_first(flags)),
            case_level_(detail::to_case_level(flags)),
            weighting_(detail::to_weighting(flags)),
            l2_order_(detail::to_l2_order(flags))
        {}

#if BOOST_TEXT_USE_CONCEPTS
        template<code_point_range R1, code_point_range R2>
#else
        /** Compares code point ranges `R1` and `R2`. */
        template<typename R1, typename R2>
#endif
        bool operator()(R1 const & r1, R2 const & r2) const;

    private:
        collation_table table_;
        collation_strength strength_;
        case_first case_first_;
        case_level case_level_;
        variable_weighting weighting_;
        l2_weight_order l2_order_;
    };

    namespace detail {
        inline temp_table_t make_temp_table()
        {
            temp_table_t retval;
            for (int i = 0, end = trie_keys().size(); i != end; ++i) {
                temp_table_element element;
                element.cps_.assign(
                    trie_keys()[i].begin(), trie_keys()[i].end());
                element.ces_.assign(
                    trie_values(collation_elements_ptr())[i].begin(
                        collation_elements_ptr()),
                    trie_values(collation_elements_ptr())[i].end(
                        collation_elements_ptr()));
                retval.push_back(element);
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                if (trie_keys()[i].size_ == 1 &&
                    *trie_keys()[i].begin() == 0xe5b) {
                    std::cerr << "========== 0xe5b\n";
                    for (auto ce : element.ces_) {
                        std::cerr << "[" << std::hex << std::setw(8)
                                  << std::setfill('0') << ce.l1_ << " "
                                  << std::setw(4) << ce.l2_ << " "
                                  << std::setw(2) << std::setfill('0') << ce.l3_
                                  << "] ";
                    }
                    std::cerr << "\n========== \n";
                }
#endif
            }
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
            // Dump the temp table to allow comparisom with table data from
            // FractionalUCA.txt.
            for (auto const & element : retval) {
                bool first = true;
                for (auto cp : element.cps_) {
                    if (!first)
                        std::cerr << " ";
                    std::cerr << std::hex << std::setw(4) << std::setfill('0')
                              << cp;
                    first = false;
                }
                std::cerr << " ";
                for (auto ce : element.ces_) {
                    std::cerr << "[" << std::hex << std::setw(8)
                              << std::setfill('0') << ce.l1_ << " "
                              << std::setw(4) << ce.l2_ << " " << std::setw(2)
                              << std::setfill('0') << ce.l3_ << "] ";
                }
                std::cerr << "\n";
            }
#endif
            return retval;
        }

        inline temp_table_element::ces_t
        get_ces(cp_seq_t cps, collation_table_data const & table);

        template<typename Iter>
        Iter last_ce_at_least_strength(
            Iter first, Iter last, collation_strength strength) noexcept
        {
            for (auto it = last; it != first;) {
                if (detail::ce_strength(*--it) <= strength)
                    return it;
            }
            return last;
        }

        inline uint32_t increment_32_bit(uint32_t w, bool is_primary)
        {
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
            std::cerr << "0x" << std::hex << std::setfill('0') << std::setw(8)
                      << w;
#endif

            // First, try to find the first zero byte and increment that.
            // This keeps sort keys as short as possible.  Don't increment a
            // primary's lead byte though.
            if (!is_primary && !(w & 0xff000000)) {
                w += 0x01000000;
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                std::cerr << " -> 0x" << std::hex << std::setfill('0')
                          << std::setw(8) << w << "\n";
#endif
                return w;
            } else if (!(w & 0xff0000)) {
                w += 0x010000;
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                std::cerr << " -> 0x" << std::hex << std::setfill('0')
                          << std::setw(8) << w << "\n";
#endif
                return w;
            } else if (!(w & 0xff00)) {
                w += 0x0100;
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                std::cerr << " -> 0x" << std::hex << std::setfill('0')
                          << std::setw(8) << w << "\n";
#endif
                return w;
            } else if (!(w & 0xff)) {
                w += 1;
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                std::cerr << " -> 0x" << std::hex << std::setfill('0')
                          << std::setw(8) << w << "\n";
#endif
                return w;
            }

            // Otherwise, just add 1 and check that this does not increment
            // the lead byte.
            uint32_t const initial_lead_byte = w & 0xff000000;
            w += 1;
            uint32_t const lead_byte = w & 0xff000000;
            if (lead_byte != initial_lead_byte && is_primary) {
                boost::throw_exception(tailoring_error(
                    "Unable to increment collation element value "
                    "without changing its lead bytes"));
            }

#if BOOST_TEXT_TAILORING_INSTRUMENTATION
            std::cerr << " -> 0x" << std::hex << std::setfill('0')
                      << std::setw(8) << w << "\n";
#endif

            return w;
        }

        inline temp_table_t::iterator bump_region_end(
            temp_table_element::ces_t const & ces, temp_table_t & temp_table)
        {
            temp_table_element::ces_t group_first_ces;
            group_first_ces.push_back(reorder_groups()[0].first_);
            if (detail::less(ces, group_first_ces)) {
                return std::lower_bound(
                    temp_table.begin(), temp_table.end(), group_first_ces);
            }

            temp_table_element::ces_t group_last_ces;
            for (auto const & group : reorder_groups()) {
                group_first_ces.clear();
                group_first_ces.push_back(group.first_);
                group_last_ces.clear();
                group_last_ces.push_back(group.last_);

                if (detail::less_equal(group_first_ces, ces) &&
                    detail::less_equal(ces, group_last_ces)) {
                    return std::lower_bound(
                        temp_table.begin(), temp_table.end(), group_last_ces);
                }
            }

            return temp_table.end();
        }

        inline uint8_t increment_l2_byte(uint8_t byte)
        {
            if (byte < min_secondary_byte) {
                return min_secondary_byte;
            } else {
                ++byte;
                if (max_secondary_byte < byte)
                    return 0;
                return byte;
            }
        }

        inline uint16_t increment_l2(uint16_t l2)
        {
            uint8_t bytes[] = {uint8_t(l2 >> 8), uint8_t(l2 & 0xff)};

            bytes[1] = detail::increment_l2_byte(bytes[1]);
            if (!bytes[1])
                bytes[0] = detail::increment_l2_byte(bytes[0]);

            return (uint16_t(bytes[0]) << 8) | bytes[1];
        }

        inline uint8_t increment_l3_byte(uint8_t byte)
        {
            if (byte < min_tertiary_byte) {
                return min_tertiary_byte;
            } else {
                ++byte;
                if (max_tertiary_byte < byte)
                    return 0;
                return byte;
            }
        }

        inline uint16_t increment_l3(uint16_t l3)
        {
            uint8_t const byte_mask = case_level_bits_mask >> 8;
            uint8_t const byte_disable_mask = disable_case_level_mask >> 8;

            uint8_t bytes[] = {uint8_t(l3 >> 8), uint8_t(l3 & 0xff)};
            uint8_t const case_masks[] = {
                uint8_t(bytes[0] & byte_mask), uint8_t(bytes[1] & byte_mask)};
            bytes[0] &= byte_disable_mask;
            bytes[1] &= byte_disable_mask;

            bytes[1] = detail::increment_l3_byte(bytes[1]);
            if (!bytes[1])
                bytes[0] = detail::increment_l3_byte(bytes[0]);

            bytes[0] |= case_masks[0];
            bytes[1] |= case_masks[1];

            return (uint16_t(bytes[0]) << 8) | bytes[1];
        }

        inline void increment_ce(
            collation_element & ce,
            collation_strength strength,
            bool initial_bump)
        {
            switch (strength) {
            case collation_strength::primary:
                ce.l1_ = detail::increment_32_bit(ce.l1_, true);
                if (initial_bump) {
                    ce.l2_ = common_l2_weight_compressed;
                    auto const case_bits = ce.l3_ & case_level_bits_mask;
                    ce.l3_ = common_l3_weight_compressed | case_bits;
                }
                break;
            case collation_strength::secondary:
                ce.l2_ = detail::increment_l2(ce.l2_);
                if (initial_bump) {
                    auto const case_bits = ce.l3_ & case_level_bits_mask;
                    ce.l3_ = common_l3_weight_compressed | case_bits;
                }
                break;
            case collation_strength::tertiary:
                ce.l3_ = detail::increment_l3(ce.l3_);
                break;
            case collation_strength::quaternary:
                ce.l4_ = detail::increment_32_bit(ce.l4_, false);
                break;
            default: break;
            }
        }

        inline bool well_formed_1(collation_element ce)
        {
            bool higher_level_zero = (ce.l3_ & disable_case_level_mask) == 0;
            if (ce.l2_) {
                if (higher_level_zero) {
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                    std::cerr << "0x" << std::hex << std::setfill('0')
                              << std::setw(8) << ce.l1_ << " " << std::setw(4)
                              << ce.l2_ << " " << std::setw(2) << ce.l3_ << " "
                              << std::setw(8) << ce.l4_
                              << " WF1 violation for L2\n";
#endif
                    return false;
                }
            } else {
                higher_level_zero = true;
            }
            if (ce.l1_) {
                if (higher_level_zero) {
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                    std::cerr << "0x" << std::hex << std::setfill('0')
                              << std::setw(8) << ce.l1_ << " " << std::setw(4)
                              << ce.l2_ << " " << std::setw(2) << ce.l3_ << " "
                              << std::setw(8) << ce.l4_
                              << " WF1 violation for L1\n";
#endif
                    return false;
                }
            }
            return true;
        }

        inline bool well_formed_2(
            collation_element ce, tailoring_state_t const & tailoring_state)
        {
            switch (detail::ce_strength(ce)) {
            case collation_strength::secondary:
                if (ce.l2_ <= tailoring_state.last_secondary_in_primary_)
                    return false;
                break;
            case collation_strength::tertiary:
                if ((ce.l3_ & disable_case_level_mask) <=
                    (tailoring_state.last_tertiary_in_secondary_masked_ &
                     disable_case_level_mask)) {
                    return false;
                }
                break;
            default: break;
            }
            return true;
        }

        inline bool bump_ces(
            temp_table_element::ces_t & ces,
            collation_strength strength,
            tailoring_state_t const & tailoring_state)
        {
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
            std::cerr << "bump_ces()\n";
            for (auto ce : ces) {
                std::cerr << "[" << std::setw(8) << std::setfill('0') << ce.l1_
                          << " " << std::setw(4) << ce.l2_ << " "
                          << std::setw(2) << std::setfill('0') << ce.l3_ << "] "
                    /*<< std::setw(8) << std::setfill('0') << ce.l4_ << " "*/;
            }
            std::cerr << "\n";
            switch (strength) {
            case collation_strength::primary: std::cerr << "primary\n"; break;
            case collation_strength::secondary:
                std::cerr << "secondary\n";
                break;
            case collation_strength::tertiary: std::cerr << "tertiary\n"; break;
            case collation_strength::quaternary:
                std::cerr << "quaternary\n";
                break;
            case collation_strength::identical:
                std::cerr << "identical\n";
                break;
            }
#endif
            // "Find the last collation element whose strength is at least
            // as great as the strength of the operator. For example, for <<
            // find the last primary or secondary CE. This CE will be
            // modified; all following CEs should be removed. If there is no
            // such CE, then reset the collation elements to a single
            // completely-ignorable CE."
            auto ces_it = detail::last_ce_at_least_strength(
                ces.begin(), ces.end(), strength);
            if (ces_it != ces.end())
                ces.erase(std::next(ces_it), ces.end());
            if (ces_it == ces.end()) {
                ces.clear();
                ces.push_back(collation_element{0, 0, 0, 0});
                ces_it = ces.begin();
            }
            auto & ce = *ces_it;

            // "Increment the collation element weight corresponding to the
            // strength of the operator. For example, for << increment the
            // secondary weight."
            detail::increment_ce(ce, strength, true);

            bool retval = false;
            if (!detail::well_formed_2(ce, tailoring_state)) {
                auto const s = detail::ce_strength(ce);
                if (s == collation_strength::secondary) {
                    ce.l2_ = tailoring_state.last_secondary_in_primary_;
                } else if (s == collation_strength::tertiary) {
                    ce.l3_ = tailoring_state.last_tertiary_in_secondary_masked_;
                }
                detail::increment_ce(ce, strength, true);
                retval = true;
            }

            if (!detail::well_formed_1(ce)) {
                if (ce.l1_) {
                    if (!ce.l2_)
                        ce.l2_ = common_l2_weight_compressed;
                    if (!ce.l3_)
                        ce.l3_ = common_l3_weight_compressed;
                }
                if (ce.l2_) {
                    if (!ce.l3_)
                        ce.l3_ = common_l3_weight_compressed;
                }
                retval = true;
            }

#if BOOST_TEXT_TAILORING_INSTRUMENTATION
            std::cerr << "final ces: ";
            for (auto ce : ces) {
                std::cerr << "[" << std::setw(8) << std::setfill('0') << ce.l1_
                          << " " << std::setw(4) << ce.l2_ << " "
                          << std::setw(2) << std::setfill('0') << ce.l3_ << "] "
                    /*<< std::setw(8) << std::setfill('0') << ce.l4_ << " "*/;
            }
            std::cerr << "\n";
#endif

            return retval;
        }

        inline bool well_formed_1(temp_table_element::ces_t const & ces)
        {
            return std::all_of(
                ces.begin(), ces.end(), [](collation_element ce) {
                    return detail::well_formed_1(ce);
                });
        }

        inline bool well_formed_2(
            temp_table_element::ces_t const & ces,
            tailoring_state_t const & tailoring_state)
        {
            return std::all_of(
                ces.begin(), ces.end(), [&](collation_element ce) {
                    return detail::well_formed_2(ce, tailoring_state);
                });
        }

        // Variable naming follows
        // http://www.unicode.org/reports/tr35/tr35-collation.html#Case_Tailored
        inline void adjust_case_bits(
            temp_table_element::ces_t const & initial_relation_ces,
            temp_table_element::ces_t & reset_ces)
        {
            container::small_vector<uint16_t, 64> initial_case_bits;
            for (auto ce : initial_relation_ces) {
                if (ce.l1_)
                    initial_case_bits.push_back(ce.l3_ & case_level_bits_mask);
            }

            auto const N = std::ptrdiff_t(initial_case_bits.size());
            auto const M = std::count_if(
                reset_ces.begin(), reset_ces.end(), [](collation_element ce) {
                    return detail::ce_strength(ce) ==
                           collation_strength::primary;
                });

            if (N <= M) {
                auto it = initial_case_bits.begin();
                for (std::ptrdiff_t i = 0; i < M; ++i) {
                    auto & ce = reset_ces[i];
                    if (ce.l1_) {
                        ce.l3_ &= disable_case_level_mask;
                        if (it != initial_case_bits.end())
                            ce.l3_ |= *it++;
                    }
                }
            } else {
                auto it = initial_case_bits.begin();
                for (std::ptrdiff_t i = 0; i < M; ++i) {
                    auto & ce = reset_ces[i];
                    if (ce.l1_) {
                        ce.l3_ &= disable_case_level_mask;
                        if (i < M - 1) {
                            ce.l3_ |= *it++;
                        } else {
                            if (std::all_of(
                                    it,
                                    initial_case_bits.end(),
                                    [](uint16_t bits) {
                                        return bits == upper_case_bits;
                                    })) {
                                ce.l3_ |= upper_case_bits;
                            } else if (std::all_of(
                                           it,
                                           initial_case_bits.end(),
                                           [](uint16_t bits) {
                                               return bits == lower_case_bits;
                                           })) {
                                ce.l3_ |= lower_case_bits;
                            } else {
                                ce.l3_ |= mixed_case_bits;
                            }
                        }
                    }
                }
            }

            for (auto & ce : reset_ces) {
                auto const strength = detail::ce_strength(ce);
                if (strength == collation_strength::secondary) {
                    ce.l3_ &= disable_case_level_mask;
                } else if (strength == collation_strength::tertiary) {
                    ce.l3_ &= disable_case_level_mask;
                    ce.l3_ |= upper_case_bits;
                } else if (strength == collation_strength::quaternary) {
                    ce.l3_ &= disable_case_level_mask;
                    ce.l3_ |= lower_case_bits;
                }
            }
        }

        inline void update_key_ces(
            temp_table_element::ces_t const & ces,
            logical_positions_t & logical_positions,
            tailoring_state_t & tailoring_state)
        {
            // Update logical_positions.
            {
                auto const strength = detail::ce_strength(ces[0]);
                if (strength == collation_strength::primary) {
                    if (ces < logical_positions[first_variable]) {
                        BOOST_ASSERT(
                            (ces[0].l1_ & 0xff000000) ==
                            (logical_positions[first_variable][0].l1_ &
                             0xff000000));
                        logical_positions[first_variable] = ces;
                    } else if (logical_positions[first_regular] < ces) {
                        if ((ces[0].l1_ & 0xff000000) ==
                            (logical_positions[last_variable][0].l1_ &
                             0xff000000)) {
                            logical_positions[last_variable] = ces;
                        } else {
                            logical_positions[first_regular] = ces;
                        }
                    } else if (logical_positions[last_regular] < ces) {
                        logical_positions[last_regular] = ces;
                    }
                } else if (strength == collation_strength::secondary) {
                    if (ces < logical_positions[first_primary_ignorable])
                        logical_positions[first_primary_ignorable] = ces;
                    else if (logical_positions[last_primary_ignorable] < ces)
                        logical_positions[last_primary_ignorable] = ces;
                } else if (strength == collation_strength::tertiary) {
                    if (ces < logical_positions[first_secondary_ignorable])
                        logical_positions[first_secondary_ignorable] = ces;
                    else if (logical_positions[last_secondary_ignorable] < ces)
                        logical_positions[last_secondary_ignorable] = ces;
                } else if (strength == collation_strength::quaternary) {
                    if (ces < logical_positions[first_tertiary_ignorable])
                        logical_positions[first_tertiary_ignorable] = ces;
                    else if (logical_positions[last_tertiary_ignorable] < ces)
                        logical_positions[last_tertiary_ignorable] = ces;
                }
            }

            // Update tailoring_state.
            for (auto ce : ces) {
                auto const strength = detail::ce_strength(ce);
                if (strength == collation_strength::primary) {
                    if (tailoring_state.last_secondary_in_primary_ < ce.l2_)
                        tailoring_state.last_secondary_in_primary_ = ce.l2_;
                } else if (strength == collation_strength::secondary) {
                    if ((tailoring_state.last_tertiary_in_secondary_masked_ &
                         disable_case_level_mask) <
                        (ce.l3_ & disable_case_level_mask)) {
                        tailoring_state.last_tertiary_in_secondary_masked_ =
                            ce.l3_;
                    }
                }
            }
        }

        // http://www.unicode.org/reports/tr35/tr35-collation.html#Orderings
        inline void modify_table(
            collation_table_data & table,
            temp_table_t & temp_table,
            logical_positions_t & logical_positions,
            tailoring_state_t & tailoring_state,
            cp_seq_t reset,
            bool before,
            collation_strength strength,
            cp_seq_t const & initial_relation,
            optional_cp_seq_t const & prefix,
            optional_cp_seq_t const & extension)
        {
            temp_table_element::ces_t reset_ces;
            if (reset.size() == 1u &&
                detail::first_tertiary_ignorable <= reset[0] &&
                reset[0] <= detail::first_implicit) {
                reset_ces = logical_positions[reset[0]];
            } else {
                reset_ces = detail::get_ces(reset, table);
            }

#if BOOST_TEXT_TAILORING_INSTRUMENTATION
            std::cerr << "========== reset= "
                      << text::to_string(reset.begin(), reset.end()) << " ";
            for (auto cp : reset) {
                std::cerr << std::hex << std::setw(8) << std::setfill('0') << cp
                          << " ";
            }
            std::cerr << " ";
            for (auto ce : reset_ces) {
                std::cerr << "[" << std::hex << std::setw(8)
                          << std::setfill('0') << ce.l1_ << " " << std::setw(4)
                          << ce.l2_ << " " << std::setw(2) << std::setfill('0')
                          << ce.l3_ << "] ";
            }
            std::cerr << " ";
            switch (strength) {
            case collation_strength::primary: std::cerr << "<"; break;
            case collation_strength::secondary: std::cerr << "<<"; break;
            case collation_strength::tertiary: std::cerr << "<<<"; break;
            case collation_strength::quaternary: std::cerr << "<<<<"; break;
            case collation_strength::identical: std::cerr << "="; break;
            }
            std::cerr << " relation= "
                      << text::to_string(
                             initial_relation.begin(), initial_relation.end())
                      << " ";
            for (auto cp : initial_relation) {
                std::cerr << std::hex << std::setw(8) << std::setfill('0') << cp
                          << " ";
            }
            std::cerr << "\n";
#endif
            temp_table_element::ces_t const initial_relation_ces =
                detail::get_ces(initial_relation, table);

            cp_seq_t relation = initial_relation;

            // "Prefix" is an odd name.  When we see a prefix, it means that
            // the "prefix", which actually comes second, is the relation, and
            // the relation, which comes first, is actually the prefix. Sigh.
            if (prefix) {
                reset_ces.insert(
                    reset_ces.begin(),
                    initial_relation_ces.begin(),
                    initial_relation_ces.end());
                relation.insert(relation.end(), prefix->begin(), prefix->end());
            }

            if (before) {
                auto ces_it = detail::last_ce_at_least_strength(
                    reset_ces.begin(), reset_ces.end(), strength);
                if (ces_it == reset_ces.end()) {
                    reset_ces.clear();
                    reset_ces.push_back(collation_element{0, 0, 0, 0});
                    ces_it = reset_ces.begin();
                }
                auto const ce = *ces_it;

                reset_ces.clear();
                reset_ces.push_back(ce);
                auto it = std::lower_bound(
                    temp_table.begin(), temp_table.end(), reset_ces);
                BOOST_ASSERT(it != temp_table.begin());
                auto prev_it = temp_table.end();
                while (it != temp_table.begin()) {
                    --it;
                    auto const curr_ce = it->ces_[0];
                    if (curr_ce.l1_ != ce.l1_) {
                        prev_it = it;
                        break;
                    } else if (
                        collation_strength::secondary <= strength &&
                        curr_ce.l2_ != ce.l2_) {
                        prev_it = it;
                        break;
                    } else if (
                        collation_strength::tertiary <= strength &&
                        (curr_ce.l3_ & disable_case_level_mask) !=
                            (ce.l3_ & disable_case_level_mask)) {
                        prev_it = it;
                        break;
                    }
                }
                if (prev_it == temp_table.end()) {
                    boost::throw_exception(tailoring_error(
                        "Could not find the collation table element before the "
                        "one requested here"));
                }
                reset_ces.assign(prev_it->ces_.begin(), prev_it->ces_.end());

                if (reset.size() == 1u && reset[0] == first_variable) {
                    // Note: Special case: If the found CEs are < first
                    // variable, we need to set the lead byte to be the same
                    // as the first variable.
                    auto const lead_byte =
                        logical_positions[first_variable][0].l1_ & 0xff000000;
                    reset_ces[0].l1_ =
                        replace_lead_byte(reset_ces[0].l1_, lead_byte);
                }
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                std::cerr << "========== before -> "
                          << text::to_string(
                                 prev_it->cps_.begin(), prev_it->cps_.end());
                std::cerr << " ";
                for (auto cp : prev_it->cps_) {
                    std::cerr << std::hex << std::setw(8) << std::setfill('0')
                              << cp << " ";
                }
                std::cerr << " ";
                for (auto ce : reset_ces) {
                    std::cerr << "[" << std::hex << std::setw(8)
                              << std::setfill('0') << ce.l1_ << " "
                              << std::setw(4) << ce.l2_ << " " << std::setw(2)
                              << std::setfill('0') << ce.l3_ << "] ";
                }
                std::cerr << "\n";
#endif
            }

            detail::adjust_case_bits(initial_relation_ces, reset_ces);

            if (extension) {
                auto const extension_ces = detail::get_ces(*extension, table);
                reset_ces.insert(
                    reset_ces.end(),
                    extension_ces.begin(),
                    extension_ces.end());
            }

            // The insert should happen at/before this point.  We may need to
            // adjust CEs at/after this to make that work.
            auto table_target_it = std::upper_bound(
                temp_table.begin(), temp_table.end(), reset_ces);

            if (strength != collation_strength::identical) {
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                std::cerr << "before bump:\n";
                bool first = true;
                for (auto cp : reset) {
                    if (!first)
                        std::cerr << " ";
                    std::cerr << std::hex << std::setw(8) << std::setfill('0')
                              << cp;
                    first = false;
                }
                std::cerr << " ";
                for (auto ce : reset_ces) {
                    std::cerr << "[" << std::hex << std::setw(8)
                              << std::setfill('0') << ce.l1_ << " "
                              << std::setw(4) << ce.l2_ << " " << std::setw(2)
                              << std::setfill('0') << ce.l3_ << "] ";
                }
                std::cerr << "\n";
#endif
                if (detail::bump_ces(reset_ces, strength, tailoring_state)) {
                    table_target_it = std::upper_bound(
                        temp_table.begin(), temp_table.end(), reset_ces);
                }

                // "Weights must be allocated in accordance with the UCA
                // well-formedness conditions."
                if (!detail::well_formed_1(reset_ces)) {
                    boost::throw_exception(tailoring_error(
                        "Unable to implement this tailoring rule, because it "
                        "was not possible to meet UCA well-formedness "
                        "condition 1; see "
                        "http://www.unicode.org/reports/tr10/#WF1"));
                }
                if (!detail::well_formed_2(reset_ces, tailoring_state)) {
                    boost::throw_exception(tailoring_error(
                        "Unable to implement this tailoring rule, because it "
                        "was not possible to meet UCA well-formedness "
                        "condition 2; see "
                        "http://www.unicode.org/reports/tr10/#WF2"));
                }

                detail::update_key_ces(
                    reset_ces, logical_positions, tailoring_state);

                BOOST_ASSERT(table_target_it != temp_table.end());

                // The checks in here only need to be performed if the increment
                // above did not slot cleanly between two existing CEs.
                if (!detail::less(reset_ces, table_target_it->ces_)) {
                    // "The new weight must be less than the next weight for the
                    // same combination of higher-level weights of any collation
                    // element according to the current state." -- this will be
                    // true as long as we can bump one or more subsequent CEs up
                    // so that this condition is maintained.

                    // For reorderings to work, we can't keep bumping
                    // indefinitely; stop before we leave the current script, if
                    // applicable.
                    auto const end =
                        detail::bump_region_end(reset_ces, temp_table) -
                        temp_table.begin();
                    auto i = table_target_it - temp_table.begin();
                    auto prev_ces = reset_ces;
                    do {
                        auto element = temp_table[i];
                        while (!detail::less(prev_ces, element.ces_)) {
                            detail::increment_ce(
                                element.ces_.front(), strength, false);
                        }
                        element.tailored_ = true;
                        detail::add_temp_tailoring(
                            table, element.cps_, element.ces_);
                        BOOST_ASSERT(detail::well_formed_1(element.ces_));
                        BOOST_ASSERT(detail::well_formed_2(
                            element.ces_, tailoring_state));
                        detail::update_key_ces(
                            element.ces_, logical_positions, tailoring_state);
                        temp_table.replace(temp_table.begin() + i, element);
                        prev_ces = element.ces_;
                        ++i;
                    } while (i != end &&
                             !detail::less(prev_ces, temp_table[i].ces_));
                }
            }

            // Remove the previous instance of relation from the table, if
            // there was one.
            if (table.trie_.contains(relation)) {
                temp_table_element::ces_t const relation_ces =
                    detail::get_ces(relation, table);
                auto remove_it = std::lower_bound(
                    temp_table.begin(), temp_table.end(), relation_ces);
                if (remove_it == temp_table.end() ||
                    remove_it->cps_ != relation) {
                    remove_it = std::find_if(
                        temp_table.begin(),
                        temp_table.end(),
                        [&](temp_table_element const & element) {
                            return element.cps_ == relation;
                        });
                }
                if (remove_it != temp_table.end() &&
                    remove_it->cps_ == relation) {
                    if (remove_it < table_target_it)
                        --table_target_it;
                    temp_table.erase(remove_it);
                }
            }

            detail::add_temp_tailoring(table, relation, reset_ces);
            temp_table_element element;
            element.cps_ = std::move(relation);
            element.ces_ = std::move(reset_ces);
            element.tailored_ = true;

            temp_table.insert(table_target_it, std::move(element));
        }

        inline void suppress_impl(
            collation_table_data & table,
            collation_trie_t::match_result subseq,
            bool first)
        {
            if (subseq.match && !first)
                table.trie_.erase(subseq);
            if (!subseq.leaf) {
                container::small_vector<uint32_t, 256> next_cps;
                table.trie_.copy_next_key_elements(
                    subseq, std::back_inserter(next_cps));
                for (auto next_cp : next_cps) {
                    detail::suppress_impl(
                        table,
                        table.trie_.extend_subsequence(subseq, next_cp),
                        false);
                }
            }
        }

        inline void suppress(collation_table_data & table, uint32_t cp)
        {
            auto const first_cp_subseq =
                table.trie_.longest_subsequence(&cp, &cp + 1);
            detail::suppress_impl(table, first_cp_subseq, true);
        }
    }
}}

#ifndef BOOST_TEXT_DOXYGEN

namespace std {
    template<>
    struct hash<boost::text::detail::temp_table_element::ces_t>
    {
        using argument_type = boost::text::detail::temp_table_element::ces_t;
        using result_type = std::size_t;
        result_type operator()(argument_type const & ces) const noexcept
        {
            result_type retval = ces.size();
            for (auto ce : ces) {
                retval = boost::text::detail::hash_combine_(
                    retval,
                    (result_type(ce.l1_) << 32) | (ce.l2_ << 16) | ce.l3_);
            }
            return retval;
        }
    };
}

#endif

namespace boost { namespace text {

    namespace detail {

        struct cp_rng
        {
            uint32_t const * begin() const noexcept { return &cp_; }
            uint32_t const * end() const noexcept { return &cp_ + 1; }

            uint32_t cp_;
        };

        struct key_and_index_t
        {
            std::array<uint32_t, 3> cps_ = {{0, 0, 0}};
            int index_;

            friend bool operator<(key_and_index_t lhs, key_and_index_t rhs)
            {
                return lhs.cps_ < rhs.cps_;
            }
        };

        inline collation_trie_t
        make_default_trie(bool reset_non_ignorables = false)
        {
            collation_trie_t retval;
            std::vector<key_and_index_t> key_and_indices;
            {
                key_and_indices.resize(trie_keys().size());
                for (int i = 0, end = (int)key_and_indices.size(); i < end;
                     ++i) {
                    auto & kai = key_and_indices[i];
                    auto key = trie_keys()[i];
                    std::copy(key.begin(), key.end(), kai.cps_.begin());
                    kai.index_ = i;
                }
                std::sort(key_and_indices.begin(), key_and_indices.end());
            }
            for (auto kai : key_and_indices) {
                auto value = trie_values(collation_elements_ptr())[kai.index_];
                if (reset_non_ignorables)
                    value.reset_non_ignorables();
                retval.insert(trie_keys()[kai.index_], value);
            }
            return retval;
        }
    }

    /** Returns a collation table created from the default, untailored
        collation data. */
    inline collation_table default_collation_table()
    {
        collation_table retval;
        retval.data_->collation_elements_ = detail::collation_elements_ptr();
        retval.data_->trie_ = detail::make_default_trie();
        retval.data_->nonstarter_first_ =
            detail::default_table_min_nonstarter();
        retval.data_->nonstarter_last_ =
            detail::default_table_max_nonstarter() + 1;
        retval.data_->nonstarters_ = detail::default_table_nonstarters_ptr();

        return retval;
    }

    /** Returns a collation table tailored using the tailoring specified in
        `tailoring`.

        If `report_errors` and/or `report_warnings` are provided, they will be
        used to report parse warnings and errors, respectively.

        \note The `suppressContractions` element only supports code points and
        code point ranges of the form "cp0-cp1".

        \throws parse_error when a parse error is encountered, or
        tailoring_error when some aspect of the requested tailoring cannot be
        satisfied. */
    inline collation_table tailored_collation_table(
        string_view tailoring,
        string_view tailoring_filename = "",
        parser_diagnostic_callback report_errors = parser_diagnostic_callback(),
        parser_diagnostic_callback report_warnings =
            parser_diagnostic_callback());

}}

#include <boost/text/collate.hpp>

namespace boost { namespace text {

    inline collation_compare collation_table::compare(
        collation_strength strength,
        case_first case_1st,
        case_level case_lvl,
        variable_weighting weighting,
        l2_weight_order l2_order) const
    {
        return collation_compare(
            *this, strength, case_1st, case_lvl, weighting, l2_order);
    }

    inline collation_compare
    collation_table::compare(collation_flags flags) const
    {
        return collation_compare(*this, flags);
    }

    template<typename CPIter, typename CEOutIter, typename SizeOutIter>
    CEOutIter collation_table::copy_collation_elements(
        CPIter first,
        CPIter last,
        CEOutIter out,
        collation_strength strength,
        case_first case_1st,
        case_level case_lvl,
        variable_weighting weighting,
        SizeOutIter * size_out) const
    {
        if (data_->weighting_)
            weighting = *data_->weighting_;
        if (data_->case_first_)
            case_1st = *data_->case_first_;
        if (data_->case_level_)
            case_lvl = *data_->case_level_;
        auto const retain_case_bits =
            case_1st != case_first::off || case_lvl != case_level::off
                ? detail::retain_case_bits_t::yes
                : detail::retain_case_bits_t::no;
        return detail::s2(
            first,
            last,
            out,
            data_->trie_,
            collation_elements_begin(),
            [&](detail::collation_element ce) {
                return detail::lead_byte(
                    ce, data_->nonsimple_reorders_, data_->simple_reorders_);
            },
            strength,
            weighting,
            retain_case_bits,
            size_out);
    }

#if BOOST_TEXT_USE_CONCEPTS
    template<code_point_range R1, code_point_range R2>
#else
    template<typename R1, typename R2>
#endif
    bool collation_compare::operator()(R1 const & r1, R2 const & r2) const
    {
        return collate(
                   r1.begin(),
                   r1.end(),
                   r2.begin(),
                   r2.end(),
                   table_,
                   strength_,
                   case_first_,
                   case_level_,
                   weighting_,
                   l2_order_) < 0;
    }

    namespace detail {
        inline void fill_nonstarters(
            collation_trie_t const & trie, std::vector<uint32_t> & nonstarters)
        {
            collation_trie_t::trie_map_type trie_map(trie.impl_);
            std::vector<uint16_t> buf;
            for (auto pair : trie_map) {
                if (pair.key.size_ &&
                    boost::text::high_surrogate(
                        pair.key.cps_.values_[pair.key.size_ - 1])) {
                    continue;
                }
                buf.assign(pair.key.begin(), pair.key.end());
                auto const cps = boost::text::as_utf32(buf);
                std::copy(
                    std::next(cps.begin()),
                    cps.end(),
                    std::back_inserter(nonstarters));
            }

            std::sort(nonstarters.begin(), nonstarters.end());
            nonstarters.erase(
                std::unique(nonstarters.begin(), nonstarters.end()),
                nonstarters.end());
        }

        inline void fill_nonstarter_table(
            collation_trie_t const & trie,
            uint32_t & nonstarter_first,
            uint32_t & nonstarter_last,
            std::vector<unsigned char> & nonstarter_table)
        {
            std::vector<uint32_t> nonstarters;
            detail::fill_nonstarters(trie, nonstarters);

            if (nonstarters.empty()) {
                nonstarter_first = 0;
                nonstarter_last = 0;
                return;
            }

            nonstarter_first = nonstarters.front();
            nonstarter_last = nonstarters.back() + 1;

            auto extent = nonstarter_last - nonstarter_first;

            nonstarter_table.resize(extent / 8 + 1);
            for (auto cp : nonstarters) {
                uint32_t const table_element = cp - nonstarter_first;
                unsigned char & c = nonstarter_table[table_element >> 3];
                c |= (1 << (table_element & 0b111));
            }
        }

        inline void fill_non_ignorables_impl(
            collation_trie_t::impl_type & trie,
            trie_match_t coll,
            collation_element const * p)
        {
            auto node_ref = trie[coll];
            if (node_ref)
                node_ref->fill_non_ignorables(p);

            std::vector<uint16_t> next_elements;
            trie.copy_next_key_elements(
                coll, std::back_inserter(next_elements));
            for (uint16_t cu : next_elements) {
                fill_non_ignorables_impl(
                    trie, trie.extend_subsequence(coll, cu), p);
            }
        }

        inline void fill_non_ignorables(
            collation_trie_t & trie, collation_element const * p)
        {
            uint16_t const * null = nullptr;
            detail::fill_non_ignorables_impl(
                trie.impl_, trie.impl_.longest_subsequence(null, null), p);
        }
    }

    inline collation_table tailored_collation_table(
        string_view tailoring,
        string_view tailoring_filename,
        parser_diagnostic_callback report_errors,
        parser_diagnostic_callback report_warnings)
    {
        detail::temp_table_t temp_table = detail::make_temp_table();

        collation_table table;
        table.data_->trie_ = detail::make_default_trie(true);
        table.data_->collation_element_vec_.assign(
            detail::collation_elements_ptr(),
            detail::collation_elements_ptr() +
                detail::collation_elements_().size());

        uint32_t const symbol_lookup[] = {
            detail::initial_first_tertiary_ignorable,
            detail::initial_last_tertiary_ignorable,
            detail::initial_first_secondary_ignorable,
            detail::initial_last_secondary_ignorable,
            detail::initial_first_primary_ignorable,
            detail::initial_last_primary_ignorable,
            detail::initial_first_variable,
            detail::initial_last_variable,
            detail::initial_first_regular,
            detail::initial_last_regular,
            detail::initial_first_implicit,
            detail::initial_first_trailing};

        detail::logical_positions_t logical_positions;
        {
            auto lookup_and_assign = [&](uint32_t symbol) {
                auto const cp =
                    symbol_lookup[symbol - detail::first_tertiary_ignorable];
                auto const elems = table.data_->trie_[detail::cp_rng{cp}];
                logical_positions[symbol].assign(
                    elems->begin(detail::collation_elements_ptr()),
                    elems->end(detail::collation_elements_ptr()));
            };
            lookup_and_assign(detail::first_tertiary_ignorable);
            lookup_and_assign(detail::last_tertiary_ignorable);
            // These magic numbers come from "{first,last} secondary ignorable"
            // in FractionalUCA.txt.
            logical_positions[detail::first_secondary_ignorable].push_back(
                detail::collation_element{0, 0, 0x3d02, 0});
            logical_positions[detail::last_secondary_ignorable].push_back(
                detail::collation_element{0, 0, 0x3d02, 0});
            lookup_and_assign(detail::first_primary_ignorable);
            lookup_and_assign(detail::last_primary_ignorable);
            lookup_and_assign(detail::first_variable);
            lookup_and_assign(detail::last_variable);
            lookup_and_assign(detail::first_regular);
            lookup_and_assign(detail::last_regular);

            detail::add_derived_elements(
                symbol_lookup
                    [detail::first_implicit - detail::first_tertiary_ignorable],
                std::back_inserter(logical_positions[detail::first_implicit]),
                table.data_->trie_,
                table.collation_elements_begin(),
                [&table](detail::collation_element ce) {
                    return detail::lead_byte(
                        ce,
                        table.data_->nonsimple_reorders_,
                        table.data_->simple_reorders_);
                });

            lookup_and_assign(detail::first_trailing);
        }

        detail::tailoring_state_t tailoring_state;

        detail::cp_seq_t curr_reset;
        bool reset_is_before = false;

        detail::collation_tailoring_interface callbacks = {
            [&](detail::cp_seq_t const & reset, bool before) {
                curr_reset = reset;
                reset_is_before = before;
            },
            [&](detail::relation_t const & rel) {
                detail::modify_table(
                    *table.data_,
                    temp_table,
                    logical_positions,
                    tailoring_state,
                    curr_reset,
                    reset_is_before,
                    static_cast<collation_strength>(rel.op_),
                    rel.cps_,
                    rel.prefix_and_extension_.prefix_,
                    rel.prefix_and_extension_.extension_);
                curr_reset = rel.prefix_and_extension_.prefix_
                                 ? *rel.prefix_and_extension_.prefix_
                                 : rel.cps_;
                reset_is_before = false;
            },
            [&](collation_strength strength) {
                table.data_->strength_ = strength;
            },
            [&](variable_weighting weighting) {
                table.data_->weighting_ = weighting;
            },
            [&](l2_weight_order l2_order) {
                table.data_->l2_order_ = l2_order;
            },
            [&](case_level cl) { table.data_->case_level_ = cl; },
            [&](case_first cf) { table.data_->case_first_ = cf; },
            [&](detail::cp_seq_t const & suppressions_) {
                for (auto cp : suppressions_) {
                    detail::suppress(*table.data_, cp);
                }
            },
            [&](std::vector<detail::reorder_group> const & reorder_groups) {
                uint32_t curr_reorder_lead_byte =
                    (detail::reorder_groups()[0].first_.l1_ & 0xff000000) -
                    0x01000000;
                bool prev_group_compressible = false;
                detail::collation_element prev_group_first = {
                    0xffffffff, 0, 0, 0};
                detail::collation_element prev_group_last = {
                    0xffffffff, 0, 0, 0};
                bool first = true;
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                std::cerr << std::hex;
#endif
                auto compressible = [](detail::collation_element prev_first,
                                       detail::collation_element prev_last,
                                       detail::collation_element curr_first) {
                    // The end of the previous group must stay in the same
                    // lead byte as the beginning of that group.
                    if ((prev_first.l1_ & 0xff000000) !=
                        (prev_last.l1_ & 0xff000000)) {
                        return false;
                    }
                    prev_last.l1_ &= 0x00ffffff;
                    curr_first.l1_ &= 0x00ffffff;
                    return prev_last <= curr_first;
                };
                for (auto const & group : reorder_groups) {
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                    std::cerr << "processing group " << group.name_ << "\n";
#endif
                    bool const compress =
                        group.compressible_ && prev_group_compressible &&
                        compressible(
                            prev_group_first, prev_group_last, group.first_);
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                    std::cerr << "  compress=" << compress << "\n";
#endif
                    if (!compress || first) {
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                        std::cerr << "  new lead byte\n";
#endif
                        curr_reorder_lead_byte += 0x01000000;
                    }

                    if ((detail::implicit_weights_final_lead_byte << 24) <
                        curr_reorder_lead_byte) {
                        boost::throw_exception(tailoring_error(
                            "It was not possible to tailor the "
                            "collation in the way you requested.  "
                            "Try using fewer groups in '[reorder "
                            "...]'."));
                    }

#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                    std::cerr << " simple=" << group.simple_ << "\n";
#endif
                    if (!compress && group.simple_) {
                        uint32_t const group_first =
                            group.first_.l1_ & 0xff000000;
                        for (uint32_t byte = group_first,
                                      end = group.last_.l1_ & 0xff000000;
                             byte < end &&
                             byte < (detail::implicit_weights_final_lead_byte
                                     << 24);
                             byte += 0x01000000) {
                            table.data_->simple_reorders_[byte >> 24] =
                                curr_reorder_lead_byte >> 24;
                            curr_reorder_lead_byte += 0x01000000;
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                            std::cerr
                                << " simple reorder " << (byte >> 24) << " -> "
                                << table.data_->simple_reorders_[byte >> 24]
                                << "\n";
#endif
                        }
                        curr_reorder_lead_byte -= 0x01000000;
                    } else {
                        table.data_->nonsimple_reorders_.push_back(
                            detail::nonsimple_script_reorder{
                                group.first_,
                                group.last_,
                                curr_reorder_lead_byte >> 24});
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                        std::cerr << " nonsimple reorder ";
                        std::cerr << "[" << group.first_.l1_ << " "
                                  << group.first_.l2_ << " " << group.first_.l3_
                                  << " " << group.first_.l4_ << "] - ";
                        std::cerr << "[" << group.last_.l1_ << " "
                                  << group.last_.l2_ << " " << group.last_.l3_
                                  << " " << group.last_.l4_ << "] -> ";
                        std::cerr << (curr_reorder_lead_byte >> 24) << "\n";
#endif
                    }
                    prev_group_compressible = group.compressible_;
                    prev_group_first = group.first_;
                    prev_group_last = group.last_;
                    first = false;
                }
#if BOOST_TEXT_TAILORING_INSTRUMENTATION
                std::cerr << std::dec;
#endif
            },
            report_errors,
            report_warnings};

        detail::parse(
            tailoring.begin(), tailoring.end(), callbacks, tailoring_filename);

        for (auto & ce : table.data_->collation_element_vec_) {
            ce.l1_ = detail::replace_lead_byte(
                ce.l1_,
                detail::lead_byte(
                    ce,
                    table.data_->nonsimple_reorders_,
                    table.data_->simple_reorders_));
        }

        detail::fill_nonstarter_table(
            table.data_->trie_,
            table.data_->nonstarter_first_,
            table.data_->nonstarter_last_,
            table.data_->nonstarter_table_);
        table.data_->nonstarters_ =
            table.data_->nonstarter_first_ == table.data_->nonstarter_last_
                ? nullptr
                : table.data_->nonstarter_table_.data();

        detail::fill_non_ignorables(
            table.data_->trie_, table.data_->collation_element_vec_.data());

        return table;
    }

    namespace detail {

        inline bool less(
            temp_table_element::ces_t const & lhs,
            temp_table_element::ces_t const & rhs) noexcept
        {
            container::static_vector<uint32_t, 256> lhs_bytes;
            container::static_vector<uint32_t, 256> rhs_bytes;

            uint32_t const * cps = nullptr;
            s3(lhs.begin(),
               lhs.end(),
               lhs.size(),
               collation_strength::quaternary,
               case_first::off,
               case_level::off,
               l2_weight_order::forward,
               cps,
               cps,
               0,
               lhs_bytes);
            s3(rhs.begin(),
               rhs.end(),
               rhs.size(),
               collation_strength::quaternary,
               case_first::off,
               case_level::off,
               l2_weight_order::forward,
               cps,
               cps,
               0,
               rhs_bytes);

            auto const pair = algorithm::mismatch(
                lhs_bytes.begin(),
                lhs_bytes.end(),
                rhs_bytes.begin(),
                rhs_bytes.end());
            if (pair.first == lhs_bytes.end()) {
                if (pair.second == rhs_bytes.end())
                    return false;
                return true;
            } else {
                if (pair.second == rhs_bytes.end())
                    return false;
                return *pair.first < *pair.second;
            }
        }

        inline temp_table_element::ces_t
        get_ces(cp_seq_t cps, collation_table_data const & table)
        {
            temp_table_element::ces_t retval(cps.size() * 10);

            auto retval_end = detail::s2(
                cps.begin(),
                cps.end(),
                retval.begin(),
                table.trie_,
                table.collation_elements_ ? table.collation_elements_
                                          : &table.collation_element_vec_[0],
                [&table](detail::collation_element ce) {
                    return detail::lead_byte(
                        ce, table.nonsimple_reorders_, table.simple_reorders_);
                },
                collation_strength::tertiary,
                variable_weighting::non_ignorable,
                detail::retain_case_bits_t::yes);
            retval.resize(retval_end - retval.begin());

#if BOOST_TEXT_TAILORING_INSTRUMENTATION
            {
                bool first = true;
                for (auto cp : cps) {
                    if (!first)
                        std::cerr << " ";
                    std::cerr << std::hex << std::setw(8) << std::setfill('0')
                              << cp;
                    first = false;
                }
                std::cerr << "\n";
            }
            std::cerr << "++++++++++\n";
            for (auto ce : retval) {
                std::cerr << "[" << std::hex << std::setw(8)
                          << std::setfill('0') << ce.l1_ << " " << std::setw(4)
                          << ce.l2_ << " " << std::setw(2) << std::setfill('0')
                          << ce.l3_ << "] ";
            }
            std::cerr << "\n++++++++++\n";
#endif

            return retval;
        }
    }

}}

#endif
