// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_DETAIL_NORMALIZATION_DATA_HPP
#define BOOST_TEXT_DETAIL_NORMALIZATION_DATA_HPP

#include <boost/text/normalize_fwd.hpp>
#include <boost/text/transcode_iterator.hpp>
#include <boost/text/detail/lzw.hpp>

#include <boost/container/small_vector.hpp>

#include <array>
#include <list>
#include <unordered_map>
#include <unordered_set>


namespace boost { namespace text { namespace detail {

    template<int Capacity, typename T = uint32_t>
    struct code_points
    {
        using storage_type = std::array<T, Capacity>;
        using iterator = typename storage_type::iterator;
        using const_iterator = typename storage_type::const_iterator;

        const_iterator begin() const { return storage_.begin(); }
        const_iterator end() const { return storage_.begin() + size_; }

        iterator begin() { return storage_.begin(); }
        iterator end() { return storage_.begin() + size_; }

        friend bool operator==(code_points const & lhs, code_points const & rhs)
        {
            return lhs.size_ == rhs.size_ && std::equal(
                                                 lhs.storage_.begin(),
                                                 lhs.storage_.end(),
                                                 rhs.storage_.begin());
        }
        friend bool operator!=(code_points const & lhs, code_points const & rhs)
        {
            return !(lhs == rhs);
        }

        storage_type storage_;
        int size_;
    };

    using canonical_decomposition = code_points<4>;

    /** See
        http://www.unicode.org/reports/tr44/#Character_Decomposition_Mappings for
        the source of the "18". */
    using compatible_decomposition = code_points<18>;

    /** The possible results returned by the single code point quick check
        functions.  A result of maybe indicates that a quick check is not
        possible and a full check must be performed. */
    enum class quick_check : uint8_t { yes, no, maybe };

    struct cp_range_
    {
        uint16_t first_;
        uint16_t last_;
    };

    BOOST_TEXT_DECL std::array<uint32_t, 3404>
    make_all_canonical_decompositions();
    BOOST_TEXT_DECL std::array<uint32_t, 8974>
    make_all_compatible_decompositions();

    inline uint32_t const * all_canonical_decompositions_ptr()
    {
        static auto const retval = make_all_canonical_decompositions();
        return retval.data();
    }

    inline uint32_t const * all_compatible_decompositions_ptr()
    {
        static auto const retval = make_all_compatible_decompositions();
        return retval.data();
    }

    struct cp_props
    {
        cp_range_ canonical_decomposition_;
        cp_range_ compatible_decomposition_;
        uint8_t ccc_;
        uint8_t nfd_quick_check_ : 4;
        uint8_t nfkd_quick_check_ : 4;
        uint8_t nfc_quick_check_ : 4;
        uint8_t nfkc_quick_check_ : 4;
    };

    static_assert(sizeof(cp_props) == 12, "");

    BOOST_TEXT_DECL std::unordered_map<uint32_t, cp_props> make_cp_props_map();

    inline std::unordered_map<uint32_t, cp_props> const & cp_props_map()
    {
        static auto const retval = make_cp_props_map();
        return retval;
    }

    template<
        typename T,
        int TotalBits,
        int HighBits,
        typename LookupT = uint64_t>
    struct two_stage_table
    {
        static_assert(TotalBits <= 63, "");
        static_assert(HighBits < TotalBits, "");

        static const int high_size = uint64_t(1) << HighBits;
        static const int low_bits = TotalBits - HighBits;
        static const int low_size = uint64_t(1) << low_bits;
        static const uint64_t low_mask = low_size - 1;

        two_stage_table() : high_table_(high_size), default_{0} {}

        template<typename Iter, typename KeyProj, typename ValueProj>
        two_stage_table(
            Iter first,
            Iter last,
            KeyProj && key_proj,
            ValueProj && value_proj,
            T default_value = T()) :
            high_table_(high_size),
            default_(default_value)
        {
            for (; first != last; ++first) {
                insert(*first, key_proj, value_proj);
            }
        }

        template<typename Value, typename KeyProj, typename ValueProj>
        void insert(
            Value const & value, KeyProj && key_proj, ValueProj && value_proj)
        {
            LookupT const key{key_proj(value)};
            BOOST_ASSERT(key < (uint64_t(1) << TotalBits));
            auto const hi = key >> low_bits;
            auto const lo = key & low_mask;
            if (!high_table_[hi]) {
                high_table_[hi] =
                    &*low_tables_.insert(low_tables_.end(), low_table_t{});
            }
            low_table_t & low_table = *high_table_[hi];
            low_table[lo] = value_proj(value);
        }

        T const & operator[](LookupT key) const noexcept
        {
            auto const hi = key >> low_bits;
            auto const lo = key & low_mask;
            if (high_table_.size() <= (std::size_t)hi)
                return default_;
            low_table_t * low_table = high_table_[hi];
            return low_table ? (*low_table)[lo] : default_;
        }

        friend bool operator==(
            two_stage_table const & lhs, two_stage_table const & rhs) noexcept
        {
            return lhs.low_tables_ == rhs.low_tables_ &&
                   lhs.high_table_ == rhs.high_table_ &&
                   lhs.default_ == rhs.default_;
        }
        friend bool operator!=(
            two_stage_table const & lhs, two_stage_table const & rhs) noexcept
        {
            return !(lhs == rhs);
        }

        using low_table_t = std::array<T, low_size>;
        std::list<low_table_t> low_tables_;
        std::vector<low_table_t *> high_table_;
        T default_;
    };

    // Hangul decomposition constants from Unicode 11.0 Section 3.12.
    enum : int32_t {
        SBase = 0xAC00,
        LBase = 0x1100,
        VBase = 0x1161,
        TBase = 0x11A7,
        LCount = 19,
        VCount = 21,
        TCount = 28,
        NCount = VCount * TCount,
        SCount = LCount * NCount
    };

    inline constexpr bool hangul_syllable(uint32_t cp) noexcept
    {
        return SBase <= cp && cp < SBase + SCount;
    }

    inline constexpr bool hangul_lv(uint32_t cp) noexcept
    {
        return hangul_syllable(cp) && (cp - SBase) % TCount == 0;
    }

    // Hangul decomposition as described in Unicode 11.0 Section 3.12.
    template<int Capacity, typename T = uint32_t>
    inline code_points<Capacity, T>
    decompose_hangul_syllable(uint32_t cp) noexcept
    {
        BOOST_ASSERT(hangul_syllable(cp));

        auto const SIndex = cp - SBase;

        auto const LIndex = SIndex / NCount;
        auto const VIndex = (SIndex % NCount) / TCount;
        auto const TIndex = SIndex % TCount;
        auto const LPart = LBase + LIndex;
        auto const VPart = VBase + VIndex;
        if (TIndex == 0) {
            return {{{(T)LPart, (T)VPart}}, 2};
        } else {
            auto const TPart = TBase + TIndex;
            return {{{(T)LPart, (T)VPart, (T)TPart}}, 3};
        }
    }

    inline constexpr uint64_t key(uint64_t cp0, uint32_t cp1) noexcept
    {
        return (cp0 << 32) | cp1;
    }

    inline int ccc(uint32_t cp) noexcept
    {
        static const two_stage_table<int, 18, 10> table(
            detail::cp_props_map().begin(),
            detail::cp_props_map().end(),
            [](std::pair<uint32_t, cp_props> const & p) {
                auto const key = p.first;
                BOOST_ASSERT(key < (uint32_t(1) << 18));
                return key;
            },
            [](std::pair<uint32_t, cp_props> const & p) {
                return p.second.ccc_;
            },
            0);
        return table[cp];
    }

    /** Returns yes, no, or maybe if the given code point indicates that the
        sequence in which it is found in normalization form Normalization. */
    template<nf Normalization>
    quick_check quick_check_code_point(uint32_t cp) noexcept
    {
        BOOST_TEXT_STATIC_ASSERT_NORMALIZATION();
        switch (Normalization) {
        case nf::c: {
        case nf::fcc:
            static const two_stage_table<quick_check, 18, 10> table(
                detail::cp_props_map().begin(),
                detail::cp_props_map().end(),
                [](std::pair<uint32_t, cp_props> const & p) {
                    auto const key = p.first;
                    BOOST_ASSERT(key < (uint32_t(1) << 18));
                    return key;
                },
                [](std::pair<uint32_t, cp_props> const & p) {
                    return quick_check(p.second.nfc_quick_check_);
                },
                quick_check::yes);
            return table[cp];
        }
        case nf::d: {
            static const two_stage_table<quick_check, 18, 10> table(
                detail::cp_props_map().begin(),
                detail::cp_props_map().end(),
                [](std::pair<uint32_t, cp_props> const & p) {
                    auto const key = p.first;
                    BOOST_ASSERT(key < (uint32_t(1) << 18));
                    return key;
                },
                [](std::pair<uint32_t, cp_props> const & p) {
                    return quick_check(p.second.nfd_quick_check_);
                },
                quick_check::yes);
            return table[cp];
        }
        case nf::kc: {
            static const two_stage_table<quick_check, 18, 10> table(
                detail::cp_props_map().begin(),
                detail::cp_props_map().end(),
                [](std::pair<uint32_t, cp_props> const & p) {
                    auto const key = p.first;
                    BOOST_ASSERT(key < (uint32_t(1) << 18));
                    return key;
                },
                [](std::pair<uint32_t, cp_props> const & p) {
                    return quick_check(p.second.nfkc_quick_check_);
                },
                quick_check::yes);
            return table[cp];
        }
        case nf::kd: {
            static const two_stage_table<quick_check, 18, 10> table(
                detail::cp_props_map().begin(),
                detail::cp_props_map().end(),
                [](std::pair<uint32_t, cp_props> const & p) {
                    auto const key = p.first;
                    BOOST_ASSERT(key < (uint32_t(1) << 18));
                    return key;
                },
                [](std::pair<uint32_t, cp_props> const & p) {
                    return quick_check(p.second.nfkd_quick_check_);
                },
                quick_check::yes);
            return table[cp];
        }
        default: BOOST_ASSERT(!"Unreachable");
        }
        return quick_check::no;
    }

    struct decomposition
    {
        bool empty() const noexcept { return first_ == last_; }

        uint32_t const * first_;
        uint32_t const * last_;
    };

    /** Returns a range of CPs that is the compatible decomposistion of `cp`.
        The result will be an empty range if `cp` has no such
        decomposition. */
    inline decomposition compatible_decompose(uint32_t cp) noexcept
    {
        static const two_stage_table<cp_range_, 18, 10> table(
            detail::cp_props_map().begin(),
            detail::cp_props_map().end(),
            [](std::pair<uint32_t, cp_props> const & p) {
                auto const key = p.first;
                BOOST_ASSERT(key < (uint32_t(1) << 18));
                return key;
            },
            [](std::pair<uint32_t, cp_props> const & p) {
                return p.second.compatible_decomposition_;
            },
            cp_range_{0, 0});
        cp_range_ const indices = table[cp];
        auto const base = all_compatible_decompositions_ptr();
        return decomposition{base + indices.first_, base + indices.last_};
    }

    /** Returns true iff `cp` is a stable code point under normalization form
        `Normalization` (meaning that it is ccc=0 and
        Quick_Check_<<Normalization>>=Yes).

        \see https://www.unicode.org/reports/tr15/#Stable_Code_Points */
    template<nf Normalization>
    bool stable_code_point(uint32_t cp) noexcept
    {
        BOOST_TEXT_STATIC_ASSERT_NORMALIZATION();
        constexpr nf form = Normalization == nf::fcc ? nf::c : Normalization;
        return detail::ccc(cp) == 0 &&
               quick_check_code_point<form>(cp) == quick_check::yes;
    }

    struct lzw_to_cp_props_iter
    {
        using value_type = std::pair<uint32_t, cp_props>;
        using difference_type = int;
        using pointer = value_type *;
        using reference = value_type &;
        using iterator_category = std::output_iterator_tag;
        using buffer_t = container::small_vector<unsigned char, 256>;

        lzw_to_cp_props_iter(
            std::unordered_map<uint32_t, cp_props> & map, buffer_t & buf) :
            map_(&map),
            buf_(&buf)
        {}

        template<typename BidiRange>
        lzw_to_cp_props_iter & operator=(BidiRange const & r)
        {
            buf_->insert(buf_->end(), r.rbegin(), r.rend());
            auto const element_bytes = 3 + 2 + 2 + 2 + 2 + 1 + 1 + 1;
            auto it = buf_->begin();
            for (auto end = buf_->end() - buf_->size() % element_bytes;
                 it != end;
                 it += element_bytes) {
                unsigned char * ptr = &*it;

                uint32_t const cp = bytes_to_cp(ptr);
                ptr += 3;

                cp_props props;
                props.canonical_decomposition_.first_ = bytes_to_uint16_t(ptr);
                ptr += 2;
                props.canonical_decomposition_.last_ = bytes_to_uint16_t(ptr);
                ptr += 2;
                props.compatible_decomposition_.first_ = bytes_to_uint16_t(ptr);
                ptr += 2;
                props.compatible_decomposition_.last_ = bytes_to_uint16_t(ptr);
                ptr += 2;
                props.ccc_ = *ptr++;
                unsigned char c = *ptr++;
                props.nfd_quick_check_ = (c >> 4) & 0xf;
                props.nfkd_quick_check_ = (c >> 0) & 0xf;
                c = *ptr++;
                props.nfc_quick_check_ = (c >> 4) & 0xf;
                props.nfkc_quick_check_ = (c >> 0) & 0xf;

                (*map_)[cp] = props;
            }
            buf_->erase(buf_->begin(), it);
            return *this;
        }
        lzw_to_cp_props_iter & operator*() { return *this; }
        lzw_to_cp_props_iter & operator++() { return *this; }
        lzw_to_cp_props_iter & operator++(int) { return *this; }

    private:
        std::unordered_map<uint32_t, cp_props> * map_;
        buffer_t * buf_;
    };

}}}

#endif
