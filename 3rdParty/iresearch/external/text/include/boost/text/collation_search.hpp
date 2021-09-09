// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_SEARCH_HPP
#define BOOST_TEXT_SEARCH_HPP

#include <boost/text/collate.hpp>
#include <boost/text/grapheme_break.hpp>
#include <boost/text/grapheme_view.hpp>
#include <boost/text/detail/algorithm.hpp>

#include <boost/algorithm/cxx14/mismatch.hpp>
#include <boost/container/small_vector.hpp>

#include <unordered_map>
#include <vector>


#ifndef BOOST_TEXT_DOXYGEN

#ifndef BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION
#define BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION 0
#endif

#endif

namespace boost { namespace text {

#if BOOST_TEXT_USE_CONCEPTS
    // concepts, etc.

    //[ collation_search_concepts

    template<typename T, typename I, typename S>
    concept searcher_break_func = std::invocable<T, I, I, S> &&
        std::convertible_to<std::invoke_result_t<T, I, I, S>, I>;

    template<typename T, typename I, typename S>
    concept searcher = std::invocable<T, I, S> &&
        std::convertible_to<std::invoke_result_t<T, I, S>, utf32_view<I>>;

    //]

#endif

    /** The result type returned from the code point overloads of
        `collation_search()`. */
#if BOOST_TEXT_USE_CONCEPTS
    template<code_point_iter I, std::sentinel_for<I> S = I>
#else
    template<typename I, typename S = I>
#endif
    struct collation_search_result
        : stl_interfaces::view_interface<collation_search_result<I, S>>
    {
        using iterator = I;
        using sentinel = S;

        constexpr collation_search_result() noexcept {}
        constexpr collation_search_result(
            iterator first, iterator last) noexcept :
            first_(first), last_(last)
        {}

        constexpr iterator begin() const noexcept { return first_; }
        constexpr sentinel end() const noexcept { return last_; }

        friend constexpr bool operator==(
            collation_search_result lhs,
            collation_search_result rhs)
        {
            return lhs.begin() == rhs.begin() && lhs.end() == rhs.end();
        }
        friend constexpr bool operator!=(
            collation_search_result lhs,
            collation_search_result rhs)
        {
            return !(lhs == rhs);
        }

        friend std::ostream &
        operator<<(std::ostream & os, collation_search_result v)
        {
            boost::text::transcode_to_utf8(
                v.begin(), v.end(), std::ostreambuf_iterator<char>(os));
            return os;
        }
#if defined(_MSC_VER)
        friend std::wostream &
        operator<<(std::wostream & os, collation_search_result v)
        {
            boost::text::transcode_to_utf16(
                v.begin(), v.end(), std::ostreambuf_iterator<wchar_t>(os));
            return os;
        }
#endif

    private:
        iterator first_;
        sentinel last_;
    };

    namespace detail {
        template<typename I, typename S, typename E>
        collation_search_result<utf_8_to_32_iterator<I, S, E>>
        make_search_result(
            utf_8_to_32_iterator<I, S, E> first,
            utf_8_to_32_iterator<I, S, E> lo,
            utf_8_to_32_iterator<I, S, E> hi,
            utf_8_to_32_iterator<I, S, E> last)
        {
            return {
                utf_8_to_32_iterator<I, S, E>{
                    first.base(), lo.base(), last.base()},
                utf_8_to_32_iterator<I, S, E>{
                    first.base(), hi.base(), last.base()}};
        }
        template<typename I, typename S, typename E>
        collation_search_result<utf_16_to_32_iterator<I, S, E>>
        make_search_result(
            utf_16_to_32_iterator<I, S, E> first,
            utf_16_to_32_iterator<I, S, E> lo,
            utf_16_to_32_iterator<I, S, E> hi,
            utf_16_to_32_iterator<I, S, E> last)
        {
            return {
                utf_16_to_32_iterator<I, S, E>{
                    first.base(), lo.base(), last.base()},
                utf_16_to_32_iterator<I, S, E>{
                    first.base(), hi.base(), last.base()}};
        }
        template<typename I, typename S, typename E>
        collation_search_result<utf_8_to_32_iterator<I, S, E>>
        make_search_result(
            utf_8_to_32_iterator<I, S, E> first,
            utf_8_to_32_iterator<I, S, E> lo,
            utf_8_to_32_iterator<I, S, E> hi,
            S last)
        {
            return {
                utf_8_to_32_iterator<I, S, E>{
                    first.base(), lo.base(), last.base()},
                utf_8_to_32_iterator<I, S, E>{
                    first.base(), hi.base(), last.base()}};
        }
        template<typename I, typename S, typename E>
        collation_search_result<utf_16_to_32_iterator<I, S, E>>
        make_search_result(
            utf_16_to_32_iterator<I, S, E> first,
            utf_16_to_32_iterator<I, S, E> lo,
            utf_16_to_32_iterator<I, S, E> hi,
            S last)
        {
            return {
                utf_16_to_32_iterator<I, S, E>{
                    first.base(), lo.base(), last.base()},
                utf_16_to_32_iterator<I, S, E>{
                    first.base(), hi.base(), last.base()}};
        }
        template<typename I, typename S>
        collation_search_result<I>
        make_search_result(I first, I lo, I hi, S last)
        {
            return {lo, hi};
        }
    }

}}

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V1 {

#ifdef BOOST_TEXT_DOXYGEN

    /** Returns the code point subrange within `[first, last)` in which
        `searcher` finds its pattern.  If the pattern is not found, the
        resulting range will be empty.

        This function only participates in overload resolution if
        `searcher(first, last)` is well formed. */
    template<typename CPIter, typename Sentinel, typename Searcher>
    collation_search_result<CPIter>
    collation_search(CPIter first, Sentinel last, Searcher const & searcher);

    /** Returns the code point subrange within `r` in which `searcher` finds
        its pattern.  If the pattern is not found, the resulting range will
        be empty.

        This function only participates in overload resolution if
        `searcher(first, last)` is well formed. */
    template<typename CPRange, typename Searcher>
    collation_search_result<CPIter>
    collation_search(CPRange && r, Searcher const & searcher);

    /** Returns the grapheme subrange within `r` in which `searcher` finds
        its pattern.  If the pattern is not found, the resulting range will
        be empty.

        This function only participates in overload resolution if
        `GraphemeRange` models the GraphemeRange concept. */
    template<typename GraphemeRange, typename Searcher>
    detail::unspecified
    collation_search(GraphemeRange && r, Searcher const & searcher);

#else

    template<typename CPIter, typename Sentinel, typename Searcher>
    auto
    collation_search(CPIter first, Sentinel last, Searcher const & searcher)
        -> decltype(detail::make_search_result(first, first, first, last))
    {
        auto const cps = searcher(first, last);
        return detail::make_search_result(first, cps.begin(), cps.end(), last);
    }

    template<typename CPRange, typename Searcher>
    auto collation_search(CPRange && r, Searcher const & searcher)
        -> detail::cp_rng_alg_ret_t<
            decltype(detail::make_search_result(
                std::begin(r), std::begin(r), std::begin(r), std::end(r))),
            CPRange>
    {
        auto const cps = searcher(std::begin(r), std::end(r));
        return detail::make_search_result(
            std::begin(r), cps.begin(), cps.end(), std::end(r));
    }

    template<typename GraphemeRange, typename Searcher>
    auto collation_search(GraphemeRange && r, Searcher const & searcher)
        -> detail::graph_rng_alg_ret_t<
            grapheme_view<decltype(r.begin().base())>,
            GraphemeRange>
    {
        using cp_iter_t = decltype(r.begin().base());
        auto const cps = searcher(r.begin().base(), r.end().base());
        auto const cp_result = detail::make_search_result(
            r.begin().base(), cps.begin(), cps.end(), r.end().base());
        return grapheme_view<cp_iter_t>{
            r.begin().base(),
            cp_result.begin(),
            cp_result.end(),
            r.end().base()};
    }

#endif

}}}


#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    template<
        code_point_iter I,
        std::sentinel_for<I> S,
        searcher<I, S> Searcher>
    auto collation_search(I first, S last, Searcher const & s)
    {
        auto const cps = s(first, last);
        return detail::make_search_result(
            first, cps.begin(), cps.end(), last);
    }

    template<
        code_point_range R,
        searcher<std::ranges::iterator_t<R>, std::ranges::sentinel_t<R>>
            Searcher>
    auto collation_search(R && r, Searcher const & s)
    {
        auto const cps = s(std::begin(r), std::end(r));
        return detail::make_search_result(
            std::begin(r), cps.begin(), cps.end(), std::end(r));
    }

    template<
        grapheme_range R,
        searcher<code_point_iterator_t<R>, code_point_sentinel_t<R>> Searcher>
    auto collation_search(R && r, Searcher const & s)
    {
        using cp_iter_t = code_point_iterator_t<R>;
        auto const cps = s(r.begin().base(), r.end().base());
        auto const cp_result = detail::make_search_result(
            r.begin().base(), cps.begin(), cps.end(), r.end().base());
        return grapheme_view<cp_iter_t>{
            r.begin().base(),
            cp_result.begin(),
            cp_result.end(),
            r.end().base()};
    }

}}}

#endif

#ifndef BOOST_TEXT_DOXYGEN

// Le sigh.
namespace std {
    template<>
    struct hash<boost::text::detail::collation_element>
    {
        using argument_type = boost::text::detail::collation_element;
        using result_type = std::size_t;
        result_type operator()(argument_type ce) const noexcept
        {
            return (result_type(ce.l1_) << 32) | (ce.l2_ << 16) | ce.l3_;
        }
    };
}

#endif

namespace boost { namespace text {

    namespace detail {
        struct coll_search_prev_grapheme_callable
        {
            template<typename CPIter, typename Sentinel>
            CPIter
            operator()(CPIter first, CPIter it, Sentinel last) const noexcept
            {
                return boost::text::prev_grapheme_break(first, it, last);
            }
        };

        collation_element inline adjust_ce_for_search(
            collation_element ce,
            collation_strength strength,
            case_level case_lvl) noexcept
        {
            auto const strength_for_copies =
                case_lvl == case_level::on
                    ? collation_strength(static_cast<int>(strength) + 1)
                    : strength;
            ce = detail::modify_for_case(
                ce, strength, case_first::off, case_lvl);
            if (strength_for_copies < collation_strength::quaternary) {
                ce.l4_ = 0;
                if (strength_for_copies < collation_strength::tertiary) {
                    ce.l3_ = 0;
                    if (strength_for_copies < collation_strength::secondary)
                        ce.l2_ = 0;
                }
            }
            return ce;
        }

        template<std::size_t N>
        typename container::small_vector<collation_element, N>::iterator
        adjust_ces_for_search(
            container::small_vector<collation_element, N> & ces,
            collation_strength strength,
            case_level case_lvl)
        {
            auto out = ces.begin();
            for (auto ce : ces) {
                ce = detail::adjust_ce_for_search(ce, strength, case_lvl);
                if (ce.l1_ || ce.l2_ || ce.l3_ || ce.l4_)
                    *out++ = ce;
            }
            return out;
        }

        template<typename Iter, typename SizeIter>
        Iter adjust_ces_for_search(
            Iter first,
            Iter last,
            SizeIter size_first,
            SizeIter size_last,
            collation_strength strength,
            case_level case_lvl)
        {
            Iter out = first;

            for (auto size_it = size_first; size_it != size_last; ++size_it) {
                auto const curr_size = *size_it;

                // Just pass over any zeros after a nonzero.
                if (!curr_size && size_it != size_first &&
                    *std::prev(size_it)) {
                    while (size_it != size_last && !*size_it) {
                        size_it++;
                    }
                    --size_it;
                    continue;
                }

                for (auto i = 0; i < curr_size; ++i, ++first) {
                    BOOST_ASSERT(first != last);
                    auto ce = detail::adjust_ce_for_search(
                        *first, strength, case_lvl);
                    if (ce.l1_ || ce.l2_ || ce.l3_ || ce.l4_) {
                        *out++ = ce;
                    } else {
                        --*size_it;
                    }
                }
            }

            return out;
        }

#if BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION
        std::ostream & operator<<(std::ostream & os, collation_element ce)
        {
            os << "[" << std::hex;
            os << "0x" << std::setfill('0') << std::setw(4) << ce.l1_ << ", ";
            os << "0x" << std::setfill('0') << std::setw(2) << ce.l2_ << ", ";
            os << "0x" << std::setfill('0') << std::setw(2) << ce.l3_ << ", ";
            os << "0x" << std::setfill('0') << std::setw(4) << ce.l4_;
            os << "]" << std::dec;
            return os;
        }
#endif

        template<typename CPIter, typename Sentinel, std::size_t N>
        void get_pattern_ces(
            CPIter first,
            Sentinel last,
            container::small_vector<collation_element, N> & ces,
            collation_table const & table,
            collation_strength strength,
            case_level case_lvl,
            variable_weighting weighting)
        {
            ces.clear();
            std::ptrdiff_t const n = boost::text::distance(first, last);
            container::small_vector<uint32_t, 1024> buf(n);
            {
                auto buf_it = buf.begin();
                for (auto it = first; it != last; ++it, ++buf_it) {
                    *buf_it = *it;
                }
            }

#if BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION
            auto const old_ces_size = ces.size();
            std::cout << "get_pattern_ces(): Gathering CEs for [" << std::hex;
            bool first_cp = true;
            for (auto cp : buf) {
                if (!first_cp)
                    std::cout << ", ";
                std::cout << "0x" << std::setw(4) << std::setfill('0') << cp;
                if (cp < 0x80)
                    std::cout << " '" << (char)cp << "'";
                first_cp = false;
            }
            std::cout << std::dec << "]\n";
#endif

            auto const ces_size = ces.size();
            ces.resize(ces_size + buf.size() * 10);
            auto ces_end = table.copy_collation_elements(
                buf.begin(),
                buf.end(),
                ces.begin() + ces_size,
                strength,
                case_first::off,
                case_lvl,
                weighting);
            ces.resize(ces_end - ces.begin());

            auto const new_ces_end =
                detail::adjust_ces_for_search(ces, strength, case_lvl);
            ces.erase(new_ces_end, ces.end());

#if BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION
            std::cout << "    modified ces: [ ";
            for (auto i = old_ces_size, end = ces.size(); i < end; ++i) {
                std::cout << ces[i] << ' ';
            }
            std::cout << "]\n";
#endif
        }

        template<typename CPIter, typename Sentinel>
        void append_search_ces_and_sizes(
            CPIter get_first,
            CPIter get_last,
            Sentinel last,
            std::vector<collation_element> & ces,
            std::vector<int> & ce_sizes,
            collation_table const & table,
            collation_strength strength,
            case_level case_lvl,
            variable_weighting weighting)
        {
            std::ptrdiff_t n = boost::text::distance(get_first, get_last);

            // Add safe stream format-sized slop to the end, to make sure,
            // as much as we can efficiently do, that we do not slice
            // constractions like Danish "aa".
            for (int i = 0; i < 32; ++i) {
                if (get_last == last)
                    break;
                ++n;
                ++get_last;
            }

            auto next_contiguous_starter_it =
                boost::text::find_if(get_last, last, [&n](uint32_t cp) {
                    auto const retval = detail::ccc(cp) == 0;
                    if (!retval)
                        ++n;
                    return retval;
                });

            container::small_vector<uint32_t, 1024> buf(n);
            std::copy(get_first, next_contiguous_starter_it, buf.begin());

            auto const old_ce_sizes_size = ce_sizes.size();

#if BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION
            std::cout << "append_search_ces_and_sizes(): Gathering CEs for ["
                      << std::hex;
            bool first_cp = true;
            for (auto cp : buf) {
                if (!first_cp)
                    std::cout << ", ";
                std::cout << "0x" << std::setw(4) << std::setfill('0') << cp;
                if (cp < 0x80)
                    std::cout << " '" << (char)cp << "'";
                first_cp = false;
            }
            std::cout << std::dec << "]\n";
#endif

            auto const ces_size = ce_sizes.size();
            auto const ce_sizes_size = ce_sizes.size();
            ces.resize(ces_size + buf.size() * 10);
            ce_sizes.resize(ce_sizes_size + buf.size() * 10);
            auto ces_size_it = ce_sizes.begin() + ces_size;
            auto ces_end = table.copy_collation_elements(
                buf.begin(),
                buf.end(),
                ces.begin() + ces_size,
                strength,
                case_first::off,
                case_lvl,
                weighting,
                &ces_size_it);
            ces.resize(ces_end - ces.begin());
            ce_sizes.erase(ces_size_it, ce_sizes.end());

            BOOST_ASSERT(buf.size() == ce_sizes.size() - old_ce_sizes_size);

#if BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION
#if 0
            std::cout << "    ces gathered: [ ";
            for (auto i = old_ces_size, end = ces.size(); i < end; ++i) {
                std::cout << ces[i] << ' ';
            }
            std::cout << "]\n";

            std::cout << "    ce_sizes gathered: [ ";
            for (auto i = old_ce_sizes_size, end = ce_sizes.size(); i < end;
                 ++i) {
                std::cout << ce_sizes[i] << ' ';
            }
            std::cout << "]\n";
#endif
#endif

            auto const new_ces_end = detail::adjust_ces_for_search(
                ces.begin() + ces_size,
                ces.end(),
                ce_sizes.begin() + old_ce_sizes_size,
                ce_sizes.end(),
                strength,
                case_lvl);
            ces.erase(new_ces_end, ces.end());

#if BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION
            std::cout << "    modified ces: [ ";
            for (auto i = old_ces_size, end = ces.size(); i < end; ++i) {
                std::cout << ces[i] << ' ';
            }
            std::cout << "]\n";

            std::cout << "    ce_sizes appended: [ ";
            for (auto i = old_ce_sizes_size, end = ce_sizes.size(); i < end;
                 ++i) {
                std::cout << ce_sizes[i] << ' ';
            }
            std::cout << "]\n";
#endif
        }

        template<
            typename Iter,
            typename Sentinel,
            typename IteratorCategory,
            typename SentinelCategory>
        Iter next_until(
            Iter it,
            std::ptrdiff_t n,
            Sentinel last,
            IteratorCategory,
            SentinelCategory)
        {
            BOOST_ASSERT(0 <= n);
            while (n && it != last) {
                --n;
                ++it;
            }
            return it;
        }

        template<typename Iter>
        Iter next_until(
            Iter it,
            std::ptrdiff_t n,
            Iter last,
            std::random_access_iterator_tag,
            non_sentinel_tag)
        {
            return std::next(it, (std::min)(n, last - it));
        }

        template<typename Iter, typename Sentinel>
        Iter next_until(Iter it, std::ptrdiff_t n, Sentinel last)
        {
            return next_until(
                it,
                n,
                last,
                typename std::iterator_traits<Iter>::iterator_category{},
                typename std::conditional<
                    std::is_same<Iter, Sentinel>::value,
                    non_sentinel_tag,
                    sentinel_tag>::type());
        }

        struct search_skip_table
        {
            search_skip_table() : default_() {}

            search_skip_table(
                std::ptrdiff_t pattern_ces, std::ptrdiff_t default_value) :
                default_(default_value), map_(pattern_ces)
            {}

            void insert(collation_element key, std::ptrdiff_t value)
            {
                map_[key] = value;
            }

            bool empty() const { return map_.empty(); }

            std::ptrdiff_t operator[](collation_element key) const
            {
                auto const it = map_.find(key);
                return it == map_.end() ? default_ : it->second;
            }

        private:
            using map_t = std::unordered_map<collation_element, std::ptrdiff_t>;

            std::ptrdiff_t default_;
            map_t map_;
        };

#if BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION
        struct collation_element_dumper
        {
            template<typename Range>
            collation_element_dumper(Range const & r) :
                first_(&*r.begin()), last_(&*r.end())
            {}

            friend std::ostream &
            operator<<(std::ostream & os, collation_element_dumper d)
            {
                for (auto it = d.first_; it != d.last_; ++it) {
                    if (it != d.first_)
                        os << ", ";
                    os << *it;
                }
                return os;
            }

        private:
            collation_element const * first_;
            collation_element const * last_;
        };
#endif

        // O, to have constexpr if!
        template<
            typename CPIter,
            std::size_t N,
            typename StrCEsIter,
            typename PatternCEsIter,
            typename AtBreakFunc,
            typename PopFrontFunc,
            typename PopsFunc>
        utf32_view<CPIter> search_mismatch_impl(
            CPIter it,
            container::small_vector<collation_element, N> const & pattern_ces,
            std::vector<collation_element> const & str_ces,
            std::vector<int> const & str_ce_sizes,
            StrCEsIter str_ces_first,
            StrCEsIter str_ces_last,
            PatternCEsIter pattern_ces_first,
            AtBreakFunc at_break,
            PopFrontFunc pop_front,
            PopsFunc pops_on_mismatch)
        {
            auto const pattern_length = pattern_ces.size();

#if BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION
            std::cout << "Comparing str="
                      << collation_element_dumper(
                             std::vector<collation_element>(
                                 str_ces_first, str_ces_last))
                      << "\n";
            std::cout << "To     substr="
                      << collation_element_dumper(
                             std::vector<collation_element>(
                                 pattern_ces_first,
                                 std::next(pattern_ces_first, pattern_length)))
                      << "\n";
#endif

            auto const mismatch =
                std::mismatch(str_ces_first, str_ces_last, pattern_ces_first);
            if (mismatch.first == str_ces_last) {
#if BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION
                std::cout << "*** == ***\n";
#endif
                std::ptrdiff_t remainder = pattern_length;
                auto match_end = it;
                for (auto size : str_ce_sizes) {
                    if (size && !remainder)
                        break;
                    remainder -= size;
                    ++match_end;
                    if (remainder < 0) {
                        match_end = it;
                        break;
                    }
                }
                if (match_end != it && at_break(match_end))
                    return utf32_view<CPIter>(it, match_end);
                else
                    pop_front(str_ce_sizes.front());
            } else {
#if BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION
                std::cout << "*** != ***\n";
#endif
                auto const ces_to_pop = pops_on_mismatch(mismatch, str_ces);
                pop_front(ces_to_pop);
            }

            return utf32_view<CPIter>(it, it);
        }

        enum class mismatch_dir { fwd, rev };

        template<mismatch_dir MismatchDir>
        struct search_mismatch_t
        {
            template<
                typename CPIter,
                std::size_t N,
                typename AtBreakFunc,
                typename PopFrontFunc,
                typename PopsFunc>
            utf32_view<CPIter> operator()(
                CPIter it,
                container::small_vector<collation_element, N> const &
                    pattern_ces,
                std::vector<collation_element> const & str_ces,
                std::vector<int> const & str_ce_sizes,
                AtBreakFunc at_break,
                PopFrontFunc pop_front,
                PopsFunc pops_on_mismatch)
            {
                return detail::search_mismatch_impl(
                    it,
                    pattern_ces,
                    str_ces,
                    str_ce_sizes,
                    str_ces.begin(),
                    str_ces.begin() + pattern_ces.size(),
                    pattern_ces.begin(),
                    at_break,
                    pop_front,
                    pops_on_mismatch);
            }
        };

        template<>
        struct search_mismatch_t<mismatch_dir::rev>
        {
            template<
                typename CPIter,
                std::size_t N,
                typename AtBreakFunc,
                typename PopFrontFunc,
                typename PopsFunc>
            utf32_view<CPIter> operator()(
                CPIter it,
                container::small_vector<collation_element, N> const &
                    pattern_ces,
                std::vector<collation_element> const & str_ces,
                std::vector<int> const & str_ce_sizes,
                AtBreakFunc at_break,
                PopFrontFunc pop_front,
                PopsFunc pops_on_mismatch)
            {
                return detail::search_mismatch_impl(
                    it,
                    pattern_ces,
                    str_ces,
                    str_ce_sizes,
                    str_ces.rend() - pattern_ces.size(),
                    str_ces.rend(),
                    pattern_ces.rbegin(),
                    at_break,
                    pop_front,
                    pops_on_mismatch);
            }
        };

        template<
            mismatch_dir MismatchDir,
            typename CPIter,
            typename Sentinel,
            typename BreakFunc,
            std::size_t N,
            typename PopsFunc>
        utf32_view<CPIter> search_impl(
            CPIter first,
            Sentinel last,
            container::small_vector<collation_element, N> const & pattern_ces,
            BreakFunc break_fn,
            collation_table const & table,
            collation_strength strength,
            case_level case_lvl,
            variable_weighting weighting,
            PopsFunc pops_on_mismatch)
        {
            if (first == last || pattern_ces.empty())
                return utf32_view<CPIter>(first, first);

            std::ptrdiff_t const pattern_length = pattern_ces.size();

            std::vector<collation_element> str_ces;
            std::vector<int> str_ce_sizes;

            auto it = first;

            auto pop_front = [&str_ces, &str_ce_sizes, &it](int ces) {
#if BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION
                auto const old_str_ces_size = str_ces.size();
                auto const old_str_ce_sizes_size = str_ce_sizes.size();
#endif
                str_ces.erase(str_ces.begin(), str_ces.begin() + ces);
                auto sizes_it = str_ce_sizes.begin();
                for (; 0 < ces; ++sizes_it, ++it) {
                    ces -= *sizes_it;
                }
                sizes_it = std::find_if(
                    sizes_it, str_ce_sizes.end(), [](int s) { return s != 0; });
                str_ce_sizes.erase(str_ce_sizes.begin(), sizes_it);
                str_ces.erase(str_ces.begin(), str_ces.begin() - ces);
#if BOOST_TEXT_COLLATION_SEARCH_INSTRUMENTATION
                std::cout << " === Popped "
                          << (old_str_ces_size - str_ces.size()) << " CEs, "
                          << (old_str_ce_sizes_size - str_ce_sizes.size())
                          << " sizes\n";
#endif
            };

            auto at_break = [first, last, break_fn](CPIter it) {
                return it == last || break_fn(first, it, last) == it;
            };

            while (it != last) {
                if (at_break(it)) {
                    std::ptrdiff_t const str_ces_needed_for_search =
                        pattern_length - (std::ptrdiff_t)str_ces.size();
                    // We need to have sufficient lookahead (at least
                    // pattern_length CEs) for the search below to work.
                    if (0 < str_ces_needed_for_search) {
                        auto const append_it =
                            std::next(it, str_ce_sizes.size());
                        detail::append_search_ces_and_sizes(
                            append_it,
                            detail::next_until(
                                append_it, str_ces_needed_for_search, last),
                            last,
                            str_ces,
                            str_ce_sizes,
                            table,
                            strength,
                            case_lvl,
                            weighting);
                    }
                    if ((std::ptrdiff_t)str_ces.size() < pattern_length) {
                        while (it != last) {
                            ++it;
                        }
                        return utf32_view<CPIter>(it, it);
                    }
                    search_mismatch_t<MismatchDir> search_mismatch;
                    auto const result = search_mismatch(
                        it,
                        pattern_ces,
                        str_ces,
                        str_ce_sizes,
                        at_break,
                        pop_front,
                        pops_on_mismatch);
                    if (!result.empty())
                        return result;
                } else {
                    if (!str_ces.empty())
                        pop_front(str_ce_sizes.front());
                    else
                        ++it;
                }
            }

            return utf32_view<CPIter>(it, it);
        }
    }


    /** A callable type that detects a text boundary at every code point (a
        no-op).  This is suitable for use as the break function used in the
        collation search API. */
    struct cp_break
    {
#if BOOST_TEXT_USE_CONCEPTS
        template<code_point_iter I, std::sentinel_for<I> S>
#else
        template<typename I, typename S>
#endif
        I operator()(I first, I it, S last) const noexcept
        {
            return it;
        }
    };


    // Searchers

    /** A searcher for use with the collation_search() algorithm.  This
        searcher uses a simple brute-force matching algorithm, like that
        found in `std::search()`.

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`. */
#if BOOST_TEXT_USE_CONCEPTS
    template<
        code_point_iter I,
        std::sentinel_for<I> S,
        searcher_break_func<I, S> BreakFunc>
#else
    template<typename I, typename S, typename BreakFunc>
#endif
    struct simple_collation_searcher
    {
        /** Constructs a simple_collation_searcher from: a sequence of code
            points to find; a break function to use to determine which code
            points are acceptable match boundaries; and a collation table with
            collation configuration settings.  Consider using
            make_simple_collation_searcher() instead of using this constructor
            directly. */
        simple_collation_searcher(
            I pattern_first,
            S pattern_last,
            BreakFunc break_fn,
            collation_table const & table,
            collation_strength strength = collation_strength::tertiary,
            case_level case_lvl = case_level::off,
            variable_weighting weighting = variable_weighting::non_ignorable) :
            table_(table),
            strength_(strength),
            case_level_(case_lvl),
            weighting_(weighting),
            break_fn_(break_fn)
        {
            detail::get_pattern_ces(
                pattern_first,
                pattern_last,
                pattern_ces_,
                table_,
                strength_,
                case_level_,
                weighting_);
        }

        /** Returns a code point range indicating the matching subsequence
            within `[first, last)` in which this searcher's pattern was
            found. The range will be empty if no match exists. */
#if BOOST_TEXT_USE_CONCEPTS
        template<code_point_iter I2, std::sentinel_for<I2> S2>
#else
        template<typename I2, typename S2>
#endif
        utf32_view<I2> operator()(I2 first, S2 last) const
        {
            using mismatch_t = std::pair<
                std::vector<detail::collation_element>::const_iterator,
                container::small_vector<detail::collation_element, 256>::
                    const_iterator>;
            return detail::search_impl<detail::mismatch_dir::fwd>(
                first,
                last,
                pattern_ces_,
                break_fn_,
                table_,
                strength_,
                case_level_,
                weighting_,
                [](mismatch_t, std::vector<detail::collation_element> const &) {
                    return 1;
                });
        }

    private:
        collation_table table_;
        collation_strength strength_;
        case_level case_level_;
        variable_weighting weighting_;
        container::small_vector<detail::collation_element, 256> pattern_ces_;
        BreakFunc break_fn_;
    };

    /** A searcher for use with the collation_search() algorithm.  This
        searcher uses the Boyer-Moore-Horspool matching algorithm.

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`. */
#if BOOST_TEXT_USE_CONCEPTS
    template<
        code_point_iter I,
        std::sentinel_for<I> S,
        searcher_break_func<I, S> BreakFunc>
#else
    template<typename I, typename S, typename BreakFunc>
#endif
    struct boyer_moore_horspool_collation_searcher
    {
        /** Constructs a boyer_moore_horspool_collation_searcher from a
            sequence of code points to find, a break function to use to
            determine which code points are acceptable match boundaries, and a
            collation table with collation configuration settings.  Consider
            using make_boyer_moore_horspool_collation_searcher() instead of
            using this constructor directly. */
        boyer_moore_horspool_collation_searcher(
            I pattern_first,
            S pattern_last,
            BreakFunc break_fn,
            collation_table const & table,
            collation_strength strength = collation_strength::tertiary,
            case_level case_lvl = case_level::off,
            variable_weighting weighting = variable_weighting::non_ignorable) :
            table_(table),
            strength_(strength),
            case_level_(case_lvl),
            weighting_(weighting),
            break_fn_(break_fn)
        {
            if (pattern_first == pattern_last)
                return;

            detail::get_pattern_ces(
                pattern_first,
                pattern_last,
                pattern_ces_,
                table_,
                strength_,
                case_level_,
                weighting_);

            skips_ = detail::search_skip_table(
                pattern_ces_.size(), pattern_ces_.size());

            std::ptrdiff_t i = 0;
            for (auto it = pattern_ces_.begin(),
                      end = std::prev(pattern_ces_.end());
                 it != end;
                 ++it, ++i) {
                skips_.insert(*it, pattern_ces_.size() - 1 - i);
            }
        }

        /** Returns a code point range indicating the matching subsequence
            within `[first, last)` in which this searcher's pattern was
            found. The range will be empty if no match exists.*/
#if BOOST_TEXT_USE_CONCEPTS
        template<code_point_iter I2, std::sentinel_for<I2> S2>
#else
        template<typename I2, typename S2>
#endif
        utf32_view<I2> operator()(I2 first, S2 last) const
        {
            using mismatch_t = std::pair<
                std::vector<detail::collation_element>::const_reverse_iterator,
                ces_t::const_reverse_iterator>;
            return detail::search_impl<detail::mismatch_dir::rev>(
                first,
                last,
                pattern_ces_,
                break_fn_,
                table_,
                strength_,
                case_level_,
                weighting_,
                [this](
                    mismatch_t,
                    std::vector<detail::collation_element> const & str_ces) {
                    return skips_[str_ces[pattern_ces_.size() - 1]];
                });
        }

    private:
        using ces_t = container::small_vector<detail::collation_element, 256>;

        collation_table table_;
        collation_strength strength_;
        case_level case_level_;
        variable_weighting weighting_;
        detail::search_skip_table skips_;
        ces_t pattern_ces_;
        BreakFunc break_fn_;
    };

    /** A searcher for use with the collation_search() algorithm.  This
        searcher uses the Boyer-Moore matching algorithm.

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`. */
#if BOOST_TEXT_USE_CONCEPTS
    template<
        code_point_iter I,
        std::sentinel_for<I> S,
        searcher_break_func<I, S> BreakFunc>
#else
    template<typename I, typename S, typename BreakFunc>
#endif
    struct boyer_moore_collation_searcher
    {
        /** Constructs a boyer_moore_collation_searcher from: a sequence of
            code points to find; a break function to use to determine which
            code points are acceptable match boundaries; and a collation table
            with collation configuration settings.  Consider using
            make_boyer_moore_collation_searcher() instead of using this
            constructor directly. */
        boyer_moore_collation_searcher(
            I pattern_first,
            S pattern_last,
            BreakFunc break_fn,
            collation_table const & table,
            collation_strength strength = collation_strength::tertiary,
            case_level case_lvl = case_level::off,
            variable_weighting weighting = variable_weighting::non_ignorable) :
            table_(table),
            strength_(strength),
            case_level_(case_lvl),
            weighting_(weighting),
            break_fn_(break_fn)
        {
            detail::get_pattern_ces(
                pattern_first,
                pattern_last,
                pattern_ces_,
                table_,
                strength_,
                case_level_,
                weighting_);

            skips_ = detail::search_skip_table(pattern_ces_.size(), -1);

            std::ptrdiff_t i = 0;
            for (auto ce : pattern_ces_) {
                skips_.insert(ce, i++);
            }

            build_suffix_table();
        }

        /** Returns a code point range indicating the matching subsequence
            within `[first, last)` in which this searcher's pattern was
            found. The range will be empty if no match exists.*/
#if BOOST_TEXT_USE_CONCEPTS
        template<code_point_iter I2, std::sentinel_for<I2> S2>
#else
        template<typename I2, typename S2>
#endif
        utf32_view<I2> operator()(I2 first, S2 last) const
        {
            using mismatch_t = std::pair<
                std::vector<detail::collation_element>::const_reverse_iterator,
                ces_t::const_reverse_iterator>;
            return detail::search_impl<detail::mismatch_dir::rev>(
                first,
                last,
                pattern_ces_,
                break_fn_,
                table_,
                strength_,
                case_level_,
                weighting_,
                [this](
                    mismatch_t mismatch,
                    std::vector<detail::collation_element> const & str_ces) {
                    auto const skip_lookup = skips_[*mismatch.first];
                    auto const mismatch_index = str_ces.rend() - mismatch.first;
                    auto const m = mismatch_index - skip_lookup - 1;
                    auto const mismatch_suffix = suffixes_[mismatch_index];
                    if (skip_lookup < mismatch_index && mismatch_suffix < m) {
                        return m;
                    } else {
                        return mismatch_suffix;
                    }
                });
        }

#ifndef BOOST_TEXT_DOXYGEN
    private:
        using ces_t = container::small_vector<detail::collation_element, 256>;
        using ces_iter = ces_t::iterator;

        template<typename CEIter>
        std::vector<std::ptrdiff_t> compute_prefixes(CEIter first)
        {
            std::vector<std::ptrdiff_t> retval(pattern_ces_.size());

            retval[0] = 0;
            std::size_t k = 0;
            for (std::size_t i = 1, end = retval.size(); i < end; ++i) {
                BOOST_ASSERT(k < end);
                while (0 < k && first[k] != first[i]) {
                    BOOST_ASSERT(k < end);
                    k = retval[k - 1];
                }
                if (first[k] == first[i])
                    ++k;
                retval[i] = k;
            }

            return retval;
        }

        void build_suffix_table()
        {
            if (pattern_ces_.empty())
                return;

            std::ptrdiff_t const pattern_size = pattern_ces_.size();
            suffixes_.resize(pattern_size + 1);

            std::vector<std::ptrdiff_t> const prefixes =
                compute_prefixes(pattern_ces_.begin());
            std::vector<std::ptrdiff_t> const prefixes_reversed =
                compute_prefixes(pattern_ces_.rbegin());

            std::fill(
                suffixes_.begin(),
                suffixes_.end(),
                pattern_size - prefixes[pattern_size - 1]);

            for (std::ptrdiff_t i = 0; i < pattern_size; ++i) {
                auto const reversed_i = prefixes_reversed[i];
                auto const j = pattern_size - reversed_i;
                auto const k = i - reversed_i + 1;

                if (k < suffixes_[j])
                    suffixes_[j] = k;
            }
        }

        collation_table table_;
        collation_strength strength_;
        case_level case_level_;
        variable_weighting weighting_;
        detail::search_skip_table skips_;
        std::vector<std::ptrdiff_t> suffixes_;
        ces_t pattern_ces_;
        BreakFunc break_fn_;
#endif
    };

}}

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V1 {

#ifdef BOOST_TEXT_DOXYGEN

    // make simple

    /** Returns a simple_collation_searcher that will find the pattern
        `[first, last)`.  A match must begin and end at a grapheme boundary.

        This function only participates in overload resolution if `CPIter`
        models the CPIter concept. */
    template<typename CPIter, typename Sentinel>
    detail::unspecified make_simple_collation_searcher(
        CPIter first,
        Sentinel last,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a simple_collation_searcher that will find the pattern
        `[first, last)`.  Any occurence of the pattern must be found starting
        at and ending at a boundary found by `break_fn` (e.g. a grapheme or
        word boundary).

        This function only participates in overload resolution if `CPIter`
        models the CPIter concept.

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`. */
    template<typename CPIter, typename Sentinel, typename BreakFunc>
    detail::unspecified make_simple_collation_searcher(
        CPIter first,
        Sentinel last,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a simple_collation_searcher that will find the pattern `r`.  A
        match must begin and end at a grapheme boundary.

        This function only participates in overload resolution if `CPRange`
        models the CPRange concept. */
    template<typename CPRange>
    detail::unspecified make_simple_collation_searcher(
        CPRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a simple_collation_searcher that will find the pattern `r`.
        A match must begin and end at a grapheme boundary.

        This function only participates in overload resolution if
        `GraphemeRange` models the GraphemeRange concept. */
    template<typename GraphemeRange>
    detail::unspecified make_simple_collation_searcher(
        GraphemeRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a simple_collation_searcher that will find the pattern `r`.
        Any occurence of the pattern must be found starting at and ending at a
        boundary found by `break_fn` (e.g. a grapheme or word boundary).

        This function only participates in overload resolution if `CPRange`
        models the CPRange concept.

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`, where `CPIter` is
        `decltype(r.begin())` and `Sentinel` is `decltype(r.end())`. */
    template<typename CPRange, typename BreakFunc>
    detail::unspecified make_simple_collation_searcher(
        CPRange && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a simple_collation_searcher that will find the pattern `r`.
        Any occurence of the pattern must be found starting at and ending at
        a boundary found by `break_fn` (e.g. a grapheme or word boundary).

        This function only participates in overload resolution if
        `GraphemeRange` models the GraphemeRange concept.

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`, where `CPIter` is
        `decltype(r.begin().base())` and `Sentinel` is
        `decltype(r.end().base())`. */
    template<typename GraphemeRange, typename BreakFunc>
    detail::unspecified make_simple_collation_searcher(
        GraphemeRange && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none);


    // make Boyer-Moore-Horspool

    /** Returns a boyer_moore_horspool_collation_searcher that will find the
        pattern `[first, last)`.  A match must begin and end at a grapheme
        boundary.

        This function only participates in overload resolution if `CPIter`
        models the CPIter concept. */
    template<typename CPIter, typename Sentinel>
    detail::unspecified make_boyer_moore_horspool_collation_searcher(
        CPIter first,
        Sentinel last,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a boyer_moore_horspool_collation_searcher that will find the
        pattern `[first, last)`.  Any occurence of the pattern must be found
        starting at and ending at a boundary found by `break_fn` (e.g. a
        grapheme or word boundary).

        This function only participates in overload resolution if `CPIter`
        models the CPIter concept.

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`. */
    template<typename CPIter, typename Sentinel, typename BreakFunc>
    detail::unspecified make_boyer_moore_horspool_collation_searcher(
        CPIter first,
        Sentinel last,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a boyer_moore_horspool_collation_searcher that will find the
        pattern `r`.  A match must begin and end at a grapheme boundary.

        This function only participates in overload resolution if `CPRange`
        models the CPRange concept. */
    template<typename CPRange>
    detail::unspecified make_boyer_moore_horspool_collation_searcher(
        CPRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a boyer_moore_horspool_collation_searcher that will find the
        pattern `r`.  A match must begin and end at a grapheme boundary.

        This function only participates in overload resolution if
        `GraphemeRange` models the GraphemeRange concept. */
    template<typename GraphemeRange>
    detail::unspecified make_boyer_moore_horspool_collation_searcher(
        GraphemeRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a boyer_moore_horspool_collation_searcher that will find the
        pattern `r`.  Any occurence of the pattern must be found starting at
        and ending at a boundary found by `break_fn` (e.g. a grapheme or
        word boundary.

        This function only participates in overload resolution if `CPRange`
        models the CPRange concept).

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`, where `CPIter` is
        `decltype(r.begin())` and `Sentinel` is `decltype(r.end())`. */
    template<typename CPRange, typename BreakFunc>
    detail::unspecified make_boyer_moore_horspool_collation_searcher(
        CPRange && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a boyer_moore_horspool_collation_searcher that will find the
        pattern `r`.  Any occurence of the pattern must be found starting at
        and ending at a boundary found by `break_fn` (e.g. a grapheme or
        word boundary).

        This function only participates in overload resolution if
        `GraphemeRange` models the GraphemeRange concept.

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`, where `CPIter` is
        `decltype(r.begin().base())` and `Sentinel` is
        `decltype(r.end().base())`. */
    template<typename GraphemeRange, typename BreakFunc>
    detail::unspecified make_boyer_moore_horspool_collation_searcher(
        GraphemeRange && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none);


    // make Boyer-Moore

    /** Returns a boyer_moore_collation_searcher that will find the pattern
        `[first, last)`.  A match must begin and end at a grapheme boundary.

        This function only participates in overload resolution if
        `CPIter` models the CPIter concept. */
    template<typename CPIter, typename Sentinel>
    detail::unspecified make_boyer_moore_collation_searcher(
        CPIter first,
        Sentinel last,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a boyer_moore_collation_searcher that will find the pattern
        `[first, last)`.  Any occurence of the pattern must be found
        starting at and ending at a boundary found by `break_fn` (e.g. a
        grapheme or word boundary).

        This function only participates in overload resolution if `CPIter`
        models the CPIter concept.

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`. */
    template<typename CPIter, typename Sentinel, typename BreakFunc>
    detail::unspecified make_boyer_moore_collation_searcher(
        CPIter first,
        Sentinel last,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a boyer_moore_collation_searcher that will find the pattern
        `r`.  A match must begin and end at a grapheme boundary.

        This function only participates in overload resolution if `CPRange`
        models the CPRange concept. */
    template<typename CPRange>
    detail::unspecified make_boyer_moore_collation_searcher(
        CPRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a boyer_moore_collation_searcher that will find the pattern
        `r`.  A match must begin and end at a grapheme boundaryy.

        This function only participates in overload resolution if
        `GraphemeRange` models the GraphemeRange concept. */
    template<typename GraphemeRange>
    detail::unspecified make_boyer_moore_collation_searcher(
        GraphemeRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a boyer_moore_collation_searcher that will find the pattern
        `r`.  Any occurence of the pattern must be found starting at and
        ending at a boundary found by `break_fn` (e.g. a grapheme or word
        boundary).

        This function only participates in overload resolution if `CPRange`
        models the CPRange concept.

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`, where `CPIter` is
        `decltype(r.begin())` and `Sentinel` is `decltype(r.end())`. */
    template<typename CPRange, typename BreakFunc>
    detail::unspecified make_boyer_moore_collation_searcher(
        CPRange && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a boyer_moore_collation_searcher that will find the pattern
        `r`.  Any occurence of the pattern must be found starting at and
        ending at a boundary found by `break_fn` (e.g. a grapheme or word
        boundary).

        This function only participates in overload resolution if
        `GraphemeRange` models the GraphemeRange concept.

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`, where `CPIter` is
        `decltype(r.begin().base())` and `Sentinel` is
        `decltype(r.end().base())`. */
    template<typename GraphemeRange, typename BreakFunc>
    detail::unspecified make_boyer_moore_collation_searcher(
        GraphemeRange && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

#else

    // make simple

    template<typename CPIter, typename Sentinel>
    auto make_simple_collation_searcher(
        CPIter first,
        Sentinel last,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_iter_ret_t<
            simple_collation_searcher<
                CPIter,
                Sentinel,
                detail::coll_search_prev_grapheme_callable>,
            CPIter>
    {
        return simple_collation_searcher<
            CPIter,
            Sentinel,
            detail::coll_search_prev_grapheme_callable>(
            first,
            last,
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename CPIter, typename Sentinel, typename BreakFunc>
    auto make_simple_collation_searcher(
        CPIter first,
        Sentinel last,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_iter_ret_t<
            simple_collation_searcher<CPIter, Sentinel, BreakFunc>,
            CPIter>
    {
        return simple_collation_searcher<CPIter, Sentinel, BreakFunc>(
            first,
            last,
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename CPRange>
    auto make_simple_collation_searcher(
        CPRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_rng_alg_ret_t<
            simple_collation_searcher<
                decltype(std::begin(r)),
                decltype(std::end(r)),
                detail::coll_search_prev_grapheme_callable>,
            CPRange>
    {
        using r_iter = decltype(std::begin(r));
        using r_sntl = decltype(std::end(r));
        return simple_collation_searcher<
            r_iter,
            r_sntl,
            detail::coll_search_prev_grapheme_callable>(
            std::begin(r),
            std::end(r),
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename GraphemeRange>
    auto make_simple_collation_searcher(
        GraphemeRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::graph_rng_alg_ret_t<
            simple_collation_searcher<
                decltype(r.begin().base()),
                decltype(r.end().base()),
                detail::coll_search_prev_grapheme_callable>,
            GraphemeRange>
    {
        using cp_iter_t = decltype(r.begin().base());
        return simple_collation_searcher<
            cp_iter_t,
            cp_iter_t,
            detail::coll_search_prev_grapheme_callable>(
            r.begin().base(),
            r.end().base(),
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename CPRange, typename BreakFunc>
    auto make_simple_collation_searcher(
        CPRange && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_rng_alg_ret_t<
            simple_collation_searcher<
                decltype(std::begin(r)),
                decltype(std::end(r)),
                BreakFunc>,
            CPRange>
    {
        using r_iter = decltype(std::begin(r));
        using r_sntl = decltype(std::end(r));
        return simple_collation_searcher<r_iter, r_sntl, BreakFunc>(
            std::begin(r),
            std::end(r),
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename GraphemeRange, typename BreakFunc>
    auto make_simple_collation_searcher(
        GraphemeRange && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::graph_rng_alg_ret_t<
            simple_collation_searcher<
                decltype(r.begin().base()),
                decltype(r.end().base()),
                BreakFunc>,
            GraphemeRange>
    {
        using cp_iter_t = decltype(r.begin().base());
        return simple_collation_searcher<cp_iter_t, cp_iter_t, BreakFunc>(
            r.begin().base(),
            r.end().base(),
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }


    // make Boyer-Moore-Horspool

    template<typename CPIter, typename Sentinel>
    auto make_boyer_moore_horspool_collation_searcher(
        CPIter first,
        Sentinel last,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_iter_ret_t<
            boyer_moore_horspool_collation_searcher<
                CPIter,
                Sentinel,
                detail::coll_search_prev_grapheme_callable>,
            CPIter>
    {
        return boyer_moore_horspool_collation_searcher<
            CPIter,
            Sentinel,
            detail::coll_search_prev_grapheme_callable>(
            first,
            last,
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename CPIter, typename Sentinel, typename BreakFunc>
    auto make_boyer_moore_horspool_collation_searcher(
        CPIter first,
        Sentinel last,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_iter_ret_t<
            boyer_moore_horspool_collation_searcher<
                CPIter,
                Sentinel,
                BreakFunc>,
            CPIter>
    {
        return boyer_moore_horspool_collation_searcher<
            CPIter,
            Sentinel,
            BreakFunc>(
            first,
            last,
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename CPRange>
    auto make_boyer_moore_horspool_collation_searcher(
        CPRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_rng_alg_ret_t<
            boyer_moore_horspool_collation_searcher<
                decltype(std::begin(r)),
                decltype(std::end(r)),
                detail::coll_search_prev_grapheme_callable>,
            CPRange>
    {
        using r_iter = decltype(std::begin(r));
        using r_sntl = decltype(std::end(r));
        return boyer_moore_horspool_collation_searcher<
            r_iter,
            r_sntl,
            detail::coll_search_prev_grapheme_callable>(
            std::begin(r),
            std::end(r),
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename GraphemeRange>
    auto make_boyer_moore_horspool_collation_searcher(
        GraphemeRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::graph_rng_alg_ret_t<
            boyer_moore_horspool_collation_searcher<
                decltype(r.begin().base()),
                decltype(r.end().base()),
                detail::coll_search_prev_grapheme_callable>,
            GraphemeRange>
    {
        using cp_iter_t = decltype(r.begin().base());
        return boyer_moore_horspool_collation_searcher<
            cp_iter_t,
            cp_iter_t,
            detail::coll_search_prev_grapheme_callable>(
            r.begin().base(),
            r.end().base(),
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename CPRange, typename BreakFunc>
    auto make_boyer_moore_horspool_collation_searcher(
        CPRange && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_rng_alg_ret_t<
            boyer_moore_horspool_collation_searcher<
                decltype(std::begin(r)),
                decltype(std::end(r)),
                BreakFunc>,
            CPRange>
    {
        using r_iter = decltype(std::begin(r));
        using r_sntl = decltype(std::end(r));
        return boyer_moore_horspool_collation_searcher<
            r_iter,
            r_sntl,
            BreakFunc>(
            std::begin(r),
            std::end(r),
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename GraphemeRange, typename BreakFunc>
    auto make_boyer_moore_horspool_collation_searcher(
        GraphemeRange && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::graph_rng_alg_ret_t<
            boyer_moore_horspool_collation_searcher<
                decltype(r.begin().base()),
                decltype(r.end().base()),
                BreakFunc>,
            GraphemeRange>
    {
        using cp_iter_t = decltype(r.begin().base());
        return boyer_moore_horspool_collation_searcher<
            cp_iter_t,
            cp_iter_t,
            BreakFunc>(
            r.begin().base(),
            r.end().base(),
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }


    // make Boyer-Moore

    template<typename CPIter, typename Sentinel>
    auto make_boyer_moore_collation_searcher(
        CPIter first,
        Sentinel last,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_iter_ret_t<
            boyer_moore_collation_searcher<
                CPIter,
                Sentinel,
                detail::coll_search_prev_grapheme_callable>,
            CPIter>
    {
        return boyer_moore_collation_searcher<
            CPIter,
            Sentinel,
            detail::coll_search_prev_grapheme_callable>(
            first,
            last,
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename CPIter, typename Sentinel, typename BreakFunc>
    auto make_boyer_moore_collation_searcher(
        CPIter first,
        Sentinel last,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_iter_ret_t<
            boyer_moore_collation_searcher<CPIter, Sentinel, BreakFunc>,
            CPIter>
    {
        return boyer_moore_collation_searcher<CPIter, Sentinel, BreakFunc>(
            first,
            last,
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename CPRange>
    auto make_boyer_moore_collation_searcher(
        CPRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_rng_alg_ret_t<
            boyer_moore_collation_searcher<
                decltype(std::begin(r)),
                decltype(std::end(r)),
                detail::coll_search_prev_grapheme_callable>,
            CPRange>
    {
        using r_iter = decltype(std::begin(r));
        using r_sntl = decltype(std::end(r));
        return boyer_moore_collation_searcher<
            r_iter,
            r_sntl,
            detail::coll_search_prev_grapheme_callable>(
            std::begin(r),
            std::end(r),
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename GraphemeRange>
    auto make_boyer_moore_collation_searcher(
        GraphemeRange && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::graph_rng_alg_ret_t<
            boyer_moore_collation_searcher<
                decltype(r.begin().base()),
                decltype(r.end().base()),
                detail::coll_search_prev_grapheme_callable>,
            GraphemeRange>
    {
        using cp_iter_t = decltype(r.begin().base());
        return boyer_moore_collation_searcher<
            cp_iter_t,
            cp_iter_t,
            detail::coll_search_prev_grapheme_callable>(
            r.begin().base(),
            r.end().base(),
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename CPRange, typename BreakFunc>
    auto make_boyer_moore_collation_searcher(
        CPRange && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_rng_alg_ret_t<
            boyer_moore_collation_searcher<
                decltype(std::begin(r)),
                decltype(std::end(r)),
                BreakFunc>,
            CPRange>
    {
        using r_iter = decltype(std::begin(r));
        using r_sntl = decltype(std::end(r));
        return boyer_moore_collation_searcher<r_iter, r_sntl, BreakFunc>(
            std::begin(r),
            std::end(r),
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<typename GraphemeRange, typename BreakFunc>
    auto make_boyer_moore_collation_searcher(
        GraphemeRange && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::graph_rng_alg_ret_t<
            boyer_moore_collation_searcher<
                decltype(r.begin().base()),
                decltype(r.end().base()),
                BreakFunc>,
            GraphemeRange>
    {
        using cp_iter_t = decltype(r.begin().base());
        return boyer_moore_collation_searcher<cp_iter_t, cp_iter_t, BreakFunc>(
            r.begin().base(),
            r.end().base(),
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

#endif

}}}


#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    // make simple

    template<code_point_iter I, std::sentinel_for<I> S>
    simple_collation_searcher<I, S, detail::coll_search_prev_grapheme_callable>
    make_simple_collation_searcher(
        I first,
        S last,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return simple_collation_searcher<
            I,
            S,
            detail::coll_search_prev_grapheme_callable>(
            first,
            last,
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<
        code_point_iter I,
        std::sentinel_for<I> S,
        searcher_break_func<I, S> BreakFunc>
    simple_collation_searcher<I, S, BreakFunc> make_simple_collation_searcher(
        I first,
        S last,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return simple_collation_searcher<I, S, BreakFunc>(
            first,
            last,
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<code_point_range R>
    simple_collation_searcher<
        std::ranges::iterator_t<R>,
        std::ranges::sentinel_t<R>,
        detail::coll_search_prev_grapheme_callable>
    make_simple_collation_searcher(
        R && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using r_iter = std::ranges::iterator_t<R>;
        using r_sntl = std::ranges::sentinel_t<R>;
        return simple_collation_searcher<
            r_iter,
            r_sntl,
            detail::coll_search_prev_grapheme_callable>(
            std::begin(r),
            std::end(r),
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<grapheme_range R>
    simple_collation_searcher<
        code_point_iterator_t<R>,
        code_point_sentinel_t<R>,
        detail::coll_search_prev_grapheme_callable>
    make_simple_collation_searcher(
        R && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using cp_iter_t = code_point_iterator_t<R>;
        return simple_collation_searcher<
            cp_iter_t,
            cp_iter_t,
            detail::coll_search_prev_grapheme_callable>(
            r.begin().base(),
            r.end().base(),
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<
        code_point_range R,
        searcher_break_func<
            std::ranges::iterator_t<R>,
            std::ranges::sentinel_t<R>> BreakFunc>
    simple_collation_searcher<
        std::ranges::iterator_t<R>,
        std::ranges::sentinel_t<R>,
        BreakFunc>
    make_simple_collation_searcher(
        R && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using r_iter = std::ranges::iterator_t<R>;
        using r_sntl = std::ranges::sentinel_t<R>;
        return simple_collation_searcher<r_iter, r_sntl, BreakFunc>(
            std::begin(r),
            std::end(r),
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<
        grapheme_range R,
        searcher_break_func<code_point_iterator_t<R>, code_point_sentinel_t<R>>
            BreakFunc>
    simple_collation_searcher<
        code_point_iterator_t<R>,
        code_point_sentinel_t<R>,
        BreakFunc>
    make_simple_collation_searcher(
        R && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using cp_iter_t = code_point_iterator_t<R>;
        return simple_collation_searcher<cp_iter_t, cp_iter_t, BreakFunc>(
            r.begin().base(),
            r.end().base(),
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }


    // make Boyer-Moore-Horspool

    template<code_point_iter I, std::sentinel_for<I> S>
    boyer_moore_horspool_collation_searcher<
        I,
        S,
        detail::coll_search_prev_grapheme_callable>
    make_boyer_moore_horspool_collation_searcher(
        I first,
        S last,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return boyer_moore_horspool_collation_searcher<
            I,
            S,
            detail::coll_search_prev_grapheme_callable>(
            first,
            last,
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<
        code_point_iter I,
        std::sentinel_for<I> S,
        searcher_break_func<I, S> BreakFunc>
    boyer_moore_horspool_collation_searcher<I, S, BreakFunc>
    make_boyer_moore_horspool_collation_searcher(
        I first,
        S last,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return boyer_moore_horspool_collation_searcher<I, S, BreakFunc>(
            first,
            last,
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<code_point_range R>
    boyer_moore_horspool_collation_searcher<
        std::ranges::iterator_t<R>,
        std::ranges::sentinel_t<R>,
        detail::coll_search_prev_grapheme_callable>
    make_boyer_moore_horspool_collation_searcher(
        R && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using r_iter = std::ranges::iterator_t<R>;
        using r_sntl = std::ranges::sentinel_t<R>;
        return boyer_moore_horspool_collation_searcher<
            r_iter,
            r_sntl,
            detail::coll_search_prev_grapheme_callable>(
            std::begin(r),
            std::end(r),
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<grapheme_range R>
    boyer_moore_horspool_collation_searcher<
        code_point_iterator_t<R>,
        code_point_sentinel_t<R>,
        detail::coll_search_prev_grapheme_callable>
    make_boyer_moore_horspool_collation_searcher(
        R && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using cp_iter_t = code_point_iterator_t<R>;
        return boyer_moore_horspool_collation_searcher<
            cp_iter_t,
            cp_iter_t,
            detail::coll_search_prev_grapheme_callable>(
            r.begin().base(),
            r.end().base(),
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<
        code_point_range R,
        searcher_break_func<
            std::ranges::iterator_t<R>,
            std::ranges::sentinel_t<R>> BreakFunc>
    boyer_moore_horspool_collation_searcher<
        std::ranges::iterator_t<R>,
        std::ranges::sentinel_t<R>,
        BreakFunc>
    make_boyer_moore_horspool_collation_searcher(
        R && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using r_iter = std::ranges::iterator_t<R>;
        using r_sntl = std::ranges::sentinel_t<R>;
        return boyer_moore_horspool_collation_searcher<
            r_iter,
            r_sntl,
            BreakFunc>(
            std::begin(r),
            std::end(r),
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<
        grapheme_range R,
        searcher_break_func<code_point_iterator_t<R>, code_point_sentinel_t<R>>
            BreakFunc>
    boyer_moore_horspool_collation_searcher<
        code_point_iterator_t<R>,
        code_point_sentinel_t<R>,
        BreakFunc>
    make_boyer_moore_horspool_collation_searcher(
        R && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using cp_iter_t = code_point_iterator_t<R>;
        return boyer_moore_horspool_collation_searcher<
            cp_iter_t,
            cp_iter_t,
            BreakFunc>(
            r.begin().base(),
            r.end().base(),
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }


    // make Boyer-Moore

    template<code_point_iter I, std::sentinel_for<I> S>
    boyer_moore_collation_searcher<
        I,
        S,
        detail::coll_search_prev_grapheme_callable>
    make_boyer_moore_collation_searcher(
        I first,
        S last,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return boyer_moore_collation_searcher<
            I,
            S,
            detail::coll_search_prev_grapheme_callable>(
            first,
            last,
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<
        code_point_iter I,
        std::sentinel_for<I> S,
        searcher_break_func<I, S> BreakFunc>
    boyer_moore_collation_searcher<I, S, BreakFunc>
    make_boyer_moore_collation_searcher(
        I first,
        S last,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        return boyer_moore_collation_searcher<I, S, BreakFunc>(
            first,
            last,
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<code_point_range R>
    boyer_moore_collation_searcher<
        std::ranges::iterator_t<R>,
        std::ranges::sentinel_t<R>,
        detail::coll_search_prev_grapheme_callable>
    make_boyer_moore_collation_searcher(
        R && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using r_iter = std::ranges::iterator_t<R>;
        using r_sntl = std::ranges::sentinel_t<R>;
        return boyer_moore_collation_searcher<
            r_iter,
            r_sntl,
            detail::coll_search_prev_grapheme_callable>(
            std::begin(r),
            std::end(r),
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<grapheme_range R>
    boyer_moore_collation_searcher<
        code_point_iterator_t<R>,
        code_point_sentinel_t<R>,
        detail::coll_search_prev_grapheme_callable>
    make_boyer_moore_collation_searcher(
        R && r,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using cp_iter_t = code_point_iterator_t<R>;
        return boyer_moore_collation_searcher<
            cp_iter_t,
            cp_iter_t,
            detail::coll_search_prev_grapheme_callable>(
            r.begin().base(),
            r.end().base(),
            detail::coll_search_prev_grapheme_callable{},
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<
        code_point_range R,
        searcher_break_func<
            std::ranges::iterator_t<R>,
            std::ranges::sentinel_t<R>> BreakFunc>
    boyer_moore_collation_searcher<
        std::ranges::iterator_t<R>,
        std::ranges::sentinel_t<R>,
        BreakFunc>
    make_boyer_moore_collation_searcher(
        R && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using r_iter = std::ranges::iterator_t<R>;
        using r_sntl = std::ranges::sentinel_t<R>;
        return boyer_moore_collation_searcher<r_iter, r_sntl, BreakFunc>(
            std::begin(r),
            std::end(r),
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

    template<
        grapheme_range R,
        searcher_break_func<code_point_iterator_t<R>, code_point_sentinel_t<R>>
            BreakFunc>
    boyer_moore_collation_searcher<
        code_point_iterator_t<R>,
        code_point_sentinel_t<R>,
        BreakFunc>
    make_boyer_moore_collation_searcher(
        R && r,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using cp_iter_t = code_point_iterator_t<R>;
        return boyer_moore_collation_searcher<cp_iter_t, cp_iter_t, BreakFunc>(
            r.begin().base(),
            r.end().base(),
            break_fn,
            table,
            detail::to_strength(flags),
            detail::to_case_level(flags),
            detail::to_weighting(flags));
    }

}}}

#endif

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V1 {

    // Convenience overloads

    /** Returns a code point range indicating the first occurrence of the
        subsequence `[pattern_first, pattern_last)` in the range `[first,
        last)`, or an empty range if no such occurrence is found.  Any
        occurence of the pattern must be found starting at and ending at a
        boundary found by `break_fn` (e.g. a grapheme or word boundary).
        This function uses the same simple brute-force matching approach as
        `std::search()`.

        BreakFunc must be an invocable type whose signature is `CPIter1
        (CPIter1 first, CPIter1 it, Sentinel1 last)`. */
    template<
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2,
        typename BreakFunc>
    collation_search_result<CPIter1> collation_search(
        CPIter1 first,
        Sentinel1 last,
        CPIter2 pattern_first,
        Sentinel2 pattern_last,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        auto const s = boost::text::make_simple_collation_searcher(
            pattern_first, pattern_last, break_fn, table, flags);
        return boost::text::collation_search(first, last, s);
    }

#ifdef BOOST_TEXT_DOXYGEN

    /** Returns a code point range indicating the first occurrence of
        `pattern` in `str`, or an empty range if no such occurrence is
        found. Any occurence of the pattern must be found starting at and
        ending at a boundary found by `break_fn` (e.g. a grapheme or word
        boundary).  This function uses the same simple brute-force matching
        approach as `std::search()`.

        This function only participates in overload resolution if `CPRange1`
        models the CPRange concept.

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`, where `CPIter` is
        `decltype(str.begin())` and `Sentinel` is `decltype(str.end())`. */
    template<typename CPRange1, typename CPRange2, typename BreakFunc>
    collation_search_result<detail::unspecified> collation_search(
        CPRange1 && str,
        CPRange2 && pattern,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a grapheme range indicating the first occurrence of
       `pattern` in `str`, or an empty range if no such occurrence is found.
        Any occurence of the pattern must be found starting at and ending at
        a boundary found by `break_fn` (e.g. a grapheme or word boundary).
        This function uses the same simple brute-force matching approach as
        `std::search()`.

        This function only participates in overload resolution if
        `GraphemeRange1` models the GraphemeRange concept.

        BreakFunc must be an invocable type whose signature is `CPIter (CPIter
        first, CPIter it, Sentinel last)`, where `CPIter` is
        `decltype(str.begin().base())` and `Sentinel` is
        `decltype(str.end().base())`. */
    template<
        typename GraphemeRange1,
        typename GraphemeRange2,
        typename BreakFunc>
    detail::unspecified collation_search(
        GraphemeRange1 && str,
        GraphemeRange2 && pattern,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

#else

    template<typename CPRange1, typename CPRange2, typename BreakFunc>
    auto collation_search(
        CPRange1 && str,
        CPRange2 && pattern,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_rng_alg_ret_t<
            collation_search_result<
                detail::iterator_t<decltype(str)>>,
            CPRange1>
    {
        auto const s = boost::text::make_simple_collation_searcher(
            pattern, break_fn, table, flags);
        return boost::text::collation_search(std::begin(str), std::end(str), s);
    }

    template<
        typename GraphemeRange1,
        typename GraphemeRange2,
        typename BreakFunc>
    auto collation_search(
        GraphemeRange1 && str,
        GraphemeRange2 && pattern,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::graph_rng_alg_ret_t<
            grapheme_view<decltype(str.begin().base())>,
            GraphemeRange1>
    {
        using cp_iter_t = decltype(str.begin().base());
        auto const s = boost::text::make_simple_collation_searcher(
            pattern.begin().base(),
            pattern.end().base(),
            break_fn,
            table,
            flags);
        auto const cps = boost::text::collation_search(
            str.begin().base(), str.end().base(), s);
        auto const cp_result = detail::make_search_result(
            str.begin().base(), cps.begin(), cps.end(), str.end().base());
        return grapheme_view<cp_iter_t>{
            str.begin().base(),
            cp_result.begin(),
            cp_result.end(),
            str.end().base()};
    }

#endif

    /** Returns a code point range indicating the first occurrence of the
        subsequence `[pattern_first, pattern_last)` in the range `[first,
        last)`, or an empty range if no such occurrence is found.  A match
        must begin and end at a grapheme boundary.  This function uses the
        same simple brute-force matching approach as `std::search()`. */
    template<
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2>
    collation_search_result<CPIter1> collation_search(
        CPIter1 first,
        Sentinel1 last,
        CPIter2 pattern_first,
        Sentinel2 pattern_last,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        auto const s = boost::text::make_simple_collation_searcher(
            pattern_first,
            pattern_last,
            detail::coll_search_prev_grapheme_callable{},
            table,
            flags);
        return boost::text::collation_search(first, last, s);
    }

#ifdef BOOST_TEXT_DOXYGEN

    /** Returns a code point range indicating the first occurrence of
        `pattern` in `str`, or an empty range if no such occurrence is
        found. A match must begin and end at a grapheme boundary.  This
        function uses the same simple brute-force matching approach as
        `std::search()`.

        This function only participates in overload resolution if `CPRange1`
        models the CPRange concept. */
    template<typename CPRange1, typename CPRange2>
    collation_search_result<detail::unspecified> collation_search(
        CPRange1 && str,
        CPRange2 && pattern,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

    /** Returns a code point range indicating the first occurrence of
        `pattern` in `str`, or an empty range if no such occurrence is
        found. A match must begin and end at a grapheme boundary.  This
        function uses the same simple brute-force matching approach as
        `std::search()`.

        This function only participates in overload resolution if
        `GraphemeRange1` models the GraphemeRange concept. */
    template<typename GraphemeRange1, typename GraphemeRange2>
    detail::unspecified collation_search(
        GraphemeRange1 && str,
        GraphemeRange2 && pattern,
        collation_table const & table,
        collation_flags flags = collation_flags::none);

#else

    template<typename CPRange1, typename CPRange2>
    auto collation_search(
        CPRange1 && str,
        CPRange2 && pattern,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::cp_rng_alg_ret_t<
            collation_search_result<
                detail::iterator_t<decltype(str)>>,
            CPRange1>
    {
        auto const s = boost::text::make_simple_collation_searcher(
            pattern,
            detail::coll_search_prev_grapheme_callable{},
            table,
            flags);
        return boost::text::collation_search(std::begin(str), std::end(str), s);
    }

    template<typename GraphemeRange1, typename GraphemeRange2>
    auto collation_search(
        GraphemeRange1 && str,
        GraphemeRange2 && pattern,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
        -> detail::graph_rng_alg_ret_t<
            grapheme_view<decltype(str.begin().base())>,
            GraphemeRange1>
    {
        using cp_iter_t = decltype(str.begin().base());
        auto const s = boost::text::make_simple_collation_searcher(
            pattern.begin().base(),
            pattern.end().base(),
            detail::coll_search_prev_grapheme_callable{},
            table,
            flags);
        auto const cps = boost::text::collation_search(
            str.begin().base(), str.end().base(), s);
        auto const cp_result = detail::make_search_result(
            str.begin().base(), cps.begin(), cps.end(), str.end().base());
        return grapheme_view<cp_iter_t>{
            str.begin().base(),
            cp_result.begin(),
            cp_result.end(),
            str.end().base()};
    }

#endif

}}}


#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    // Convenience overloads

    template<
        code_point_iter I1,
        std::sentinel_for<I1> S1,
        code_point_iter I2,
        std::sentinel_for<I2> S2,
        searcher_break_func<I1, S1> BreakFunc>
    collation_search_result<I1> collation_search(
        I1 first,
        S1 last,
        I2 pattern_first,
        S2 pattern_last,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        auto const s = boost::text::make_simple_collation_searcher(
            pattern_first, pattern_last, break_fn, table, flags);
        return boost::text::collation_search(first, last, s);
    }

    template<
        code_point_range R1,
        code_point_range R2,
        searcher_break_func<
            std::ranges::iterator_t<R1>,
            std::ranges::sentinel_t<R1>> BreakFunc>
    collation_search_result<std::ranges::iterator_t<R1>> collation_search(
        R1 && str,
        R2 && pattern,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        auto const s = boost::text::make_simple_collation_searcher(
            pattern, break_fn, table, flags);
        return boost::text::collation_search(std::begin(str), std::end(str), s);
    }

    template<
        grapheme_range R1,
        grapheme_range R2,
        searcher_break_func<
            code_point_iterator_t<R1>,
            code_point_sentinel_t<R1>> BreakFunc>
    grapheme_view<code_point_iterator_t<R1>> collation_search(
        R1 && str,
        R2 && pattern,
        BreakFunc break_fn,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using cp_iter_t = code_point_iterator_t<R1>;
        auto const s = boost::text::make_simple_collation_searcher(
            pattern.begin().base(),
            pattern.end().base(),
            break_fn,
            table,
            flags);
        auto const cps = boost::text::collation_search(
            str.begin().base(), str.end().base(), s);
        auto const cp_result = detail::make_search_result(
            str.begin().base(), cps.begin(), cps.end(), str.end().base());
        return grapheme_view<cp_iter_t>{
            str.begin().base(),
            cp_result.begin(),
            cp_result.end(),
            str.end().base()};
    }

    template<
        code_point_iter I1,
        std::sentinel_for<I1> S1,
        code_point_iter I2,
        std::sentinel_for<I2> S2>
    collation_search_result<I1> collation_search(
        I1 first,
        S1 last,
        I2 pattern_first,
        S2 pattern_last,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        auto const s = boost::text::make_simple_collation_searcher(
            pattern_first,
            pattern_last,
            detail::coll_search_prev_grapheme_callable{},
            table,
            flags);
        return boost::text::collation_search(first, last, s);
    }

    template<code_point_range R1, code_point_range R2>
    collation_search_result<std::ranges::iterator_t<R1>> collation_search(
        R1 && str,
        R2 && pattern,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        auto const s = boost::text::make_simple_collation_searcher(
            pattern,
            detail::coll_search_prev_grapheme_callable{},
            table,
            flags);
        return boost::text::collation_search(std::begin(str), std::end(str), s);
    }

    template<grapheme_range R1, grapheme_range R2>
    grapheme_view<code_point_iterator_t<R1>> collation_search(
        R1 && str,
        R2 && pattern,
        collation_table const & table,
        collation_flags flags = collation_flags::none)
    {
        using cp_iter_t = code_point_iterator_t<R1>;
        auto const s = boost::text::make_simple_collation_searcher(
            pattern.begin().base(),
            pattern.end().base(),
            detail::coll_search_prev_grapheme_callable{},
            table,
            flags);
        auto const cps = boost::text::collation_search(
            str.begin().base(), str.end().base(), s);
        auto const cp_result = detail::make_search_result(
            str.begin().base(), cps.begin(), cps.end(), str.end().base());
        return grapheme_view<cp_iter_t>{
            str.begin().base(),
            cp_result.begin(),
            cp_result.end(),
            str.end().base()};
    }

}}}

#endif

#if BOOST_TEXT_USE_CONCEPTS

namespace std::ranges {
    template<typename Iter, typename Sentinel>
    inline constexpr bool enable_borrowed_range<
        boost::text::collation_search_result<Iter, Sentinel>> = true;
}

#endif

#endif
