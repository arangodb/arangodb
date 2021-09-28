// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_CASE_MAPPING_DATA_HPP
#define BOOST_TEXT_DETAIL_CASE_MAPPING_DATA_HPP

#include <boost/text/config.hpp>
#include <boost/text/detail/case_constants.hpp>
#include <boost/text/detail/case_mapping_tables.hpp>

#include <absl/container/flat_hash_set.h>
#include <absl/container/flat_hash_map.h>

namespace boost { namespace text { namespace detail {
    struct case_mapping_to
    {
        uint16_t first_;      // cp indices
        uint16_t last_;
        uint16_t conditions_;
    };

    struct case_mapping
    {
        uint32_t from_;
        uint16_t first_;      // case_mapping_to indices
        uint16_t last_;
    };

    struct case_elements
    {
        using iterator = case_mapping_to const *;

        iterator begin(case_mapping_to const * elements) const noexcept
        {
            return elements + first_;
        }
        iterator end(case_mapping_to const * elements) const noexcept
        {
            return elements + last_;
        }

        int size() const noexcept { return last_ - first_; }
        explicit operator bool() const noexcept { return first_ != last_; }

        uint16_t first_;
        uint16_t last_;

        friend bool operator==(case_elements lhs, case_elements rhs) noexcept
        {
            return lhs.first_ == rhs.first_ && lhs.last_ == rhs.last_;
        }
        friend bool operator!=(case_elements lhs, case_elements rhs) noexcept
        {
            return !(lhs == rhs);
        }
    };

    using case_map_t = iresearch_absl::flat_hash_map<uint32_t, case_elements>;

    BOOST_TEXT_DECL case_map_t make_to_lower_map();
    BOOST_TEXT_DECL case_map_t make_to_title_map();
    BOOST_TEXT_DECL case_map_t make_to_upper_map();
    BOOST_TEXT_DECL iresearch_absl::flat_hash_set<uint32_t> make_cased_cps();
    BOOST_TEXT_DECL iresearch_absl::flat_hash_set<uint32_t> make_case_ignorable_cps();
    BOOST_TEXT_DECL iresearch_absl::flat_hash_set<uint32_t> make_changes_when_uppered_cps();
    BOOST_TEXT_DECL iresearch_absl::flat_hash_set<uint32_t> make_changes_when_lowered_cps();
    BOOST_TEXT_DECL iresearch_absl::flat_hash_set<uint32_t> make_changes_when_titled_cps();
    BOOST_TEXT_DECL std::array<uint32_t, 3007> make_case_cps();
    BOOST_TEXT_DECL std::array<case_mapping_to, 2926> make_case_mapping_to();

    inline case_map_t const & to_lower_map()
    {
        static auto const retval = make_to_lower_map();
        return retval;
    }

    inline case_map_t const & to_title_map()
    {
        static auto const retval = make_to_title_map();
        return retval;
    }

    inline case_map_t const & to_upper_map()
    {
        static auto const retval = make_to_upper_map();
        return retval;
    }

    inline uint32_t const * case_cps_ptr()
    {
        static auto const retval = make_case_cps();
        return retval.data();
    }

    inline case_mapping_to const * case_mapping_to_ptr()
    {
        static auto const retval = make_case_mapping_to();
        return retval.data();
    }

    inline bool cased(uint32_t cp) noexcept
    {
        static auto const cps = make_cased_cps();
        return 0 < cps.count(cp);
    }

    inline bool case_ignorable(uint32_t cp) noexcept
    {
        static auto const cps = make_case_ignorable_cps();
        return 0 < cps.count(cp);
    }

    constexpr bool soft_dotted(uint32_t cp) noexcept
    {
        return 0 < detail::SOFT_DOTTED_CPS.count(cp);
    }

    inline bool changes_when_uppered(uint32_t cp) noexcept
    {
        static auto const cps = make_changes_when_uppered_cps();
        return 0 < cps.count(cp);
    }

    inline bool changes_when_lowered(uint32_t cp) noexcept
    {
        static auto const cps = make_changes_when_lowered_cps();
        return 0 < cps.count(cp);
    }

    inline bool changes_when_titled(uint32_t cp) noexcept
    {
        static auto const cps = make_changes_when_titled_cps();
        return 0 < cps.count(cp);
    }

}}}

#endif
