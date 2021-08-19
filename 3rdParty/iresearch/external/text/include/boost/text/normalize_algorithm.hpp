// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_NORMALIZE_ALGORITHM_HPP
#define BOOST_TEXT_NORMALIZE_ALGORITHM_HPP

#include <boost/text/algorithm.hpp>
#include <boost/text/concepts.hpp>
#include <boost/text/normalize_fwd.hpp>
#include <boost/text/stream_safe.hpp>
#include <boost/text/detail/algorithm.hpp>

#include <boost/stl_interfaces/view_interface.hpp>


namespace boost { namespace text {

    /** The result of inserting a sequence of code points `S` into another
        sequence of code points `D`, ensuring proper normalization.  Since the
        insertion operation may need to change some code points just before
        and/or just after the insertion due to normalization, the code points
        bounded by this range may be longer than `S`.  Values of this type
        represent the entire sequence of of code points in `D` that have
        changed.

        Note that the iterator type refers to the underlying sequence, which
        may not itself be a sequence of code points.  For example, the
        underlying sequence may be a sequence of `char` which is interpreted
        as UTF-8. */
    template<typename Iter>
    struct replace_result : stl_interfaces::view_interface<replace_result<Iter>>
    {
        using iterator = Iter;

        replace_result() : first_(), last_(first_) {}
        replace_result(iterator first, iterator last) :
            first_(first), last_(last)
        {}

        iterator begin() const noexcept { return first_; }
        iterator end() const noexcept { return last_; }

        operator iterator() const noexcept { return begin(); }

    private:
        Iter first_;
        Iter last_;
    };

    namespace detail {
        template<
            bool ForRope,
            nf Normalization,
            typename String,
            typename StringIter,
            typename Iter>
        replace_result<StringIter> replace_impl(
            String & string,
            StringIter str_first,
            StringIter str_last,
            Iter first,
            Iter last,
            insertion_normalization insertion_norm);

        template<
            bool ForRope,
            nf Normalization,
            typename String,
            typename StringIter>
        replace_result<StringIter>
        erase_impl(String & string, StringIter first, StringIter last);
    }
}}

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V1 {

    /** Inserts `[first, last)` into string `string`, replacing the sequence
        `[str_first, str_last)` within `string`, and returning a view
        indicating the changed portion of `string`.  Note that the replacement
        operation may mutate some code points just before or just after the
        inserted sequence.  The output is UTF-8 if `sizeof(*s.begin()) == 1`,
        and UTF-16 otherwise.  The inserted string is normalized and put into
        stream-safe format if `insertion_norm` is `insertion_not_normalized`.
        The code points at either end of the insertion may need to be
        normalized, regardless of whether the inserted string does.

        This function only participates in overload resolution if `CPIter`
        models the CPIter concept.

        \pre `string` is in normalization form `Normalization`.
        \pre `string` is stream-safe format.
        \pre `str_first` and `str_last` are at code point boundaries.

        \see https://unicode.org/reports/tr15/#Stream_Safe_Text_Format */
    template<
        nf Normalization,
        typename String,
        typename CPIter,
        typename StringIter = typename String::iterator>
    auto normalize_replace(
        String & string,
        StringIter str_first,
        StringIter str_last,
        CPIter first,
        CPIter last,
        insertion_normalization insertion_norm = insertion_not_normalized)
        ->detail::cp_iter_ret_t<replace_result<StringIter>, CPIter>
    {
        return detail::replace_impl<false, Normalization>(
            string, str_first, str_last, first, last, insertion_norm);
    }

    /** Inserts `[first, last)` into string `string` at location `at`,
        returning a view indicating the changed portion of `string`.  Note
        that the insertion operation may mutate some code points just before
        or just after the inserted sequence.  The output is UTF-8 if
        `sizeof(*s.begin()) == 1`, and UTF-16 otherwise.  The inserted string
        is normalized and put into stream-safe format if `insertion_norm` is
        `insertion_not_normalized`.  The code points at either end of the
        insertion may need to be normalized, regardless of whether the
        inserted string does.

        This function only participates in overload resolution if `CPIter`
        models the CPIter concept.

        \pre `string` is in normalization form `Normalization`.
        \pre `string` is stream-safe format.
        \pre `at` is at a code point boundary.

        \see https://unicode.org/reports/tr15/#Stream_Safe_Text_Format */
    template<
        nf Normalization,
        typename String,
        typename CPIter,
        typename StringIter = typename String::iterator>
    auto normalize_insert(
        String & string,
        StringIter at,
        CPIter first,
        CPIter last,
        insertion_normalization insertion_norm = insertion_not_normalized)
        ->detail::cp_iter_ret_t<replace_result<StringIter>, CPIter>
    {
        if (first == last)
            return {at, at};
        return detail::replace_impl<false, Normalization>(
            string, at, at, first, last, insertion_norm);
    }

    /** Inserts `r` into string `string` at location `at`, returning a view
        indicating the changed portion of `string`.  Note that the insertion
        operation may mutate some code points just before or just after the
        inserted sequence.  The output is UTF-8 if `sizeof(*s.begin()) == 1`,
        and UTF-16 otherwise.  The inserted string is normalized and put into
        stream-safe format if `insertion_norm` is `insertion_not_normalized`.
        The code points at either end of the insertion may need to be
        normalized, regardless of whether the inserted string does.

        \pre `string` is in normalization form `Normalization`.
        \pre `string` is stream-safe format.
        \pre `at` is at a code point boundary.

        \see https://unicode.org/reports/tr15/#Stream_Safe_Text_Format */
    template<
        nf Normalization,
        typename String,
        typename CPRange,
        typename StringIter = typename String::iterator>
    replace_result<StringIter> normalize_insert(
        String & string,
        StringIter at,
        CPRange && r,
        insertion_normalization insertion_norm = insertion_not_normalized)
    {
        if (std::begin(r) == std::end(r))
            return {at, at};
        return detail::replace_impl<false, Normalization>(
            string, at, at, std::begin(r), std::end(r), insertion_norm);
    }

    /** Erases the subsequence `[str_first, str_last)` within `string`,
        maintaining stream-safe format.  Returns a view indicating the changed
        portion of `string`.  Note that the insertion operation may mutate
        some code points just before or just after the erased sequence.

        \pre `string` is in normalization form `Normalization`.
        \pre `string` is stream-safe format.
        \pre `str_first` and `str_last` are at code point boundaries.

        \see https://unicode.org/reports/tr15/#Stream_Safe_Text_Format */
    template<
        nf Normalization,
        typename String,
        typename StringIter = typename String::iterator>
    replace_result<StringIter> normalize_erase(
        String & string, StringIter str_first, StringIter str_last)
    {
        return detail::erase_impl<false, Normalization>(
            string, str_first, str_last);
    }

}}}

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    /** Inserts `[first, last)` into string `string`, replacing the sequence
        `[str_first, str_last)` within `string`, and returning a view
        indicating the changed portion of `string`.  Note that the replacement
        operation may mutate some code points just before or just after the
        inserted sequence.  The output is UTF-8 if `sizeof(*s.begin()) == 1`,
        and UTF-16 otherwise.  The inserted string is normalized and put into
        stream-safe format if `insertion_norm` is `insertion_not_normalized`.
        The code points at either end of the insertion may need to be
        normalized, regardless of whether the inserted string does.

        \pre `string` is in normalization form `Normalization`.
        \pre `string` is stream-safe format.
        \pre `str_first` and `str_last` are at code point boundaries.

        \see https://unicode.org/reports/tr15/#Stream_Safe_Text_Format */
    template<
        nf Normalization,
        utf_string String,
        code_point_iter I,
        typename StringIter = std::ranges::iterator_t<String>>
    replace_result<StringIter> normalize_replace(
        String & string,
        StringIter str_first,
        StringIter str_last,
        I first,
        I last,
        insertion_normalization insertion_norm = insertion_not_normalized)
    {
        return detail::replace_impl<false, Normalization>(
            string, str_first, str_last, first, last, insertion_norm);
    }

    /** Inserts `[first, last)` into string `string` at location `at`,
        returning a view indicating the changed portion of `string`.  Note
        that the insertion operation may mutate some code points just before
        or just after the inserted sequence.  The output is UTF-8 if
        `sizeof(*s.begin()) == 1`, and UTF-16 otherwise.  The inserted string
        is normalized and put into stream-safe format if `insertion_norm` is
        `insertion_not_normalized`.  The code points at either end of the
        insertion may need to be normalized, regardless of whether the
        inserted string does.

        \pre `string` is in normalization form `Normalization`.
        \pre `string` is stream-safe format.
        \pre `at` is at a code point boundary.

        \see https://unicode.org/reports/tr15/#Stream_Safe_Text_Format */
    template<
        nf Normalization,
        utf_string String,
        code_point_iter I,
        typename StringIter = std::ranges::iterator_t<String>>
    replace_result<StringIter> normalize_insert(
        String & string,
        StringIter at,
        I first,
        I last,
        insertion_normalization insertion_norm = insertion_not_normalized)
    {
        if (first == last)
            return {at, at};
        return detail::replace_impl<false, Normalization>(
            string, at, at, first, last, insertion_norm);
    }

    /** Inserts `r` into string `string` at location `at`, returning a view
        indicating the changed portion of `string`.  Note that the insertion
        operation may mutate some code points just before or just after the
        inserted sequence.  The output is UTF-8 if `sizeof(*s.begin()) == 1`,
        and UTF-16 otherwise.  The inserted string is normalized and put into
        stream-safe format if `insertion_norm` is `insertion_not_normalized`.
        The code points at either end of the insertion may need to be
        normalized, regardless of whether the inserted string does.

        \pre `string` is in normalization form `Normalization`.
        \pre `string` is stream-safe format.
        \pre `at` is at a code point boundary.

        \see https://unicode.org/reports/tr15/#Stream_Safe_Text_Format */
    template<
        nf Normalization,
        utf_string String,
        code_point_range R,
        typename StringIter = std::ranges::iterator_t<String>>
    replace_result<StringIter> normalize_insert(
        String & string,
        StringIter at,
        R && r,
        insertion_normalization insertion_norm = insertion_not_normalized)
    {
        if (std::ranges::begin(r) == std::ranges::end(r))
            return {at, at};
        return detail::replace_impl<false, Normalization>(
            string,
            at,
            at,
            std::ranges::begin(r),
            std::ranges::end(r),
            insertion_norm);
    }

    /** Erases the subsequence `[str_first, str_last)` within `string`,
        maintaining stream-safe format.  Returns a view indicating the changed
        portion of `string`.  Note that the insertion operation may mutate
        some code points just before or just after the erased sequence.

        \pre `string` is in normalization form `Normalization`.
        \pre `string` is stream-safe format.
        \pre `str_first` and `str_last` are at code point boundaries.

        \see https://unicode.org/reports/tr15/#Stream_Safe_Text_Format */
    template<
        nf Normalization,
        utf_string String,
        typename StringIter = std::ranges::iterator_t<String>>
    replace_result<StringIter> normalize_erase(
        String & string, StringIter str_first, StringIter str_last)
    {
        return detail::erase_impl<false, Normalization>(
            string, str_first, str_last);
    }

}}}

#endif

#include <boost/text/normalize_string.hpp>

namespace boost { namespace text { namespace detail {

    template<nf Normalization, typename CPIter>
    CPIter last_stable_cp(CPIter first, CPIter last) noexcept
    {
        auto const it = find_if_backward(
            first, last, detail::stable_code_point<Normalization>);
        if (it == last)
            return first;
        return it;
    }

    template<nf Normalization, typename CPIter>
    CPIter first_stable_cp(CPIter first, CPIter last) noexcept
    {
        auto const it =
            find_if(first, last, detail::stable_code_point<Normalization>);
        return it;
    }

    template<typename CPIter>
    struct stable_cps_result_t
    {
        bool empty() const noexcept { return first_ == last_; }
        CPIter begin() const noexcept { return first_; }
        CPIter end() const noexcept { return last_; }
        CPIter first_;
        CPIter last_;
    };

    template<typename T, typename R1, typename R2>
    struct cons_iter : stl_interfaces::proxy_iterator_interface<
                           cons_iter<T, R1, R2>,
                           std::bidirectional_iterator_tag,
                           T>
    {
        using first_iterator = decltype(std::declval<R1 &>().begin());
        using second_iterator = decltype(std::declval<R2 &>().begin());

        cons_iter() : in_r1_(false) {}
        cons_iter(
            first_iterator it,
            first_iterator r1_last,
            second_iterator r2_first) :
            r1_last_(r1_last),
            it1_(it),
            r2_first_(r2_first),
            it2_(r2_first),
            in_r1_(true)
        {
            if (it1_ == r1_last_)
                in_r1_ = false;
        }
        cons_iter(
            second_iterator it,
            first_iterator r1_last,
            second_iterator r2_first,
            int) :
            r1_last_(r1_last),
            it1_(r1_last),
            r2_first_(r2_first),
            it2_(it),
            in_r1_(false)
        {}

        cons_iter & operator++() noexcept
        {
            if (in_r1_) {
                BOOST_ASSERT(it1_ != r1_last_);
                ++it1_;
                if (it1_ == r1_last_)
                    in_r1_ = false;
            } else {
                ++it2_;
            }
            return *this;
        }

        cons_iter & operator--() noexcept
        {
            if (!in_r1_) {
                if (it2_ == r2_first_) {
                    in_r1_ = true;
                    --it1_;
                } else {
                    --it2_;
                }
            } else {
                --it1_;
            }
            return *this;
        }

        T operator*() const noexcept { return in_r1_ ? *it1_ : *it2_; }

        friend bool operator==(cons_iter lhs, cons_iter rhs)
        {
            return lhs.in_r1_ == rhs.in_r1_ &&
                   (lhs.in_r1_ ? lhs.it1_ == rhs.it1_ : lhs.it2_ == rhs.it2_);
        }

        using base_type = stl_interfaces::proxy_iterator_interface<
            cons_iter<T, R1, R2>,
            std::bidirectional_iterator_tag,
            T>;
        using base_type::operator++;
        using base_type::operator--;

    private:
        first_iterator r1_last_;
        first_iterator it1_;
        second_iterator r2_first_;
        second_iterator it2_;
        bool in_r1_;
    };

    template<typename T, typename R1, typename R2>
    struct cons_view_t : stl_interfaces::view_interface<cons_view_t<T, R1, R2>>
    {
        using iterator = cons_iter<T, R1, R2>;

        cons_view_t(iterator first, iterator last) : first_(first), last_(last)
        {}

        iterator begin() const noexcept { return first_; }
        iterator end() const noexcept { return last_; }

    private:
        iterator first_;
        iterator last_;
    };

    template<
        typename T,
        typename R1,
        typename R2,
        typename Iter1,
        typename Iter2>
    auto cons_view(R1 & r1, R2 & r2, Iter1 first, Iter2 last)
    {
        using iterator = cons_iter<T, R1, R2>;
        return cons_view_t<T, R1, R2>(
            iterator(first, r1.end(), r2.begin()),
            iterator(last, r1.end(), r2.begin(), 0));
    }

    template<bool ForRope>
    struct string_buffer_replace_impl
    {
        template<typename String, typename CPIter, typename Buffer>
        static replace_result<typename String::iterator> call(
            String & string, CPIter first_, CPIter last_, Buffer const & buffer)
        {
            auto const unpacked =
                boost::text::detail::unpack_iterator_and_sentinel(
                    first_, last_);
            BOOST_ASSERT((std::is_same<
                          decltype(unpacked.tag_),
                          decltype(detail::unpack_iterator_and_sentinel(
                                       string.begin(), string.end())
                                       .tag_)>::value));
            auto const first = unpacked.f_;
            auto const last = unpacked.l_;
            auto const replaceable_size = std::distance(first, last);
            if ((std::ptrdiff_t)buffer.size() <= replaceable_size) {
                auto it = std::copy(buffer.begin(), buffer.end(), first);
                it = string.erase(it, last);
                return {std::prev(it, buffer.size()), it};
            } else {
                auto const copy_last = buffer.begin() + replaceable_size;
                std::copy(buffer.begin(), copy_last, first);
                // This dance is here because somehow libstdc++ in the Travis
                // build environment thinks it's being built as C++98 code, in
                // which std::string::insert() returns void.  This works around
                // that.
                auto const insertion_offset = last - string.begin();
                string.insert(last, copy_last, buffer.end());
                auto const it = string.begin() + insertion_offset;
                return {
                    std::prev(it, replaceable_size),
                    std::next(it, buffer.end() - copy_last)};
            }
        }
    };

    template<>
    struct string_buffer_replace_impl<true>
    {
        template<typename String, typename CPIter, typename Buffer>
        static replace_result<typename String::iterator> call(
            String & string, CPIter first_, CPIter last_, Buffer const & buffer)
        {
            auto const unpacked =
                boost::text::detail::unpack_iterator_and_sentinel(
                    first_, last_);
            BOOST_ASSERT((std::is_same<
                          decltype(unpacked.tag_),
                          decltype(detail::unpack_iterator_and_sentinel(
                                       string.begin(), string.end())
                                       .tag_)>::value));
            auto const first = unpacked.f_;
            auto const last = unpacked.l_;
            auto const first_offset = first - string.begin();
            string.replace(first, last, buffer.begin(), buffer.end());
            return {
                std::next(string.begin(), first_offset),
                std::next(string.begin(), first_offset + buffer.size())};
        }
    };

    template<typename String>
    using normalized_insert_buffer_t = std::conditional_t<
        sizeof(*std::declval<String>().begin()) == 1,
        container::small_vector<char, 1024>,
        container::small_vector<uint16_t, 512>>;

    template<
        typename Buffer,
        bool UTF8 = sizeof(*std::declval<Buffer>().begin()) == 1>
    struct append_normalized_to_buffer
    {
        template<typename Iter>
        static void call(Buffer & buffer, Iter first, Iter last)
        {
            auto r = boost::text::as_utf16(first, last);
            buffer.insert(buffer.end(), r.begin(), r.end());
        }
    };

    template<typename Buffer>
    struct append_normalized_to_buffer<Buffer, true>
    {
        template<typename Iter>
        static void call(Buffer & buffer, Iter first, Iter last)
        {
            auto r = boost::text::as_utf8(first, last);
            buffer.insert(buffer.end(), r.begin(), r.end());
        }
    };

    template<
        bool ForRope,
        typename StringIter,
        typename Buffer,
        typename String,
        typename Iter>
    replace_result<StringIter> replace_erase_impl_suffix(
        String & string,
        Buffer const & buffer,
        stable_cps_result_t<Iter> string_prefix_range,
        stable_cps_result_t<Iter> string_suffix_range)
    {
        auto const buffer_cps = boost::text::as_utf32(buffer);

        auto const first_buffer_mismatch_offset =
            std::mismatch(
                string_prefix_range.begin(),
                string_prefix_range.end(),
                buffer_cps.begin(),
                buffer_cps.end())
                .second.base() -
            buffer.begin();

        auto const last_buffer_mismatch_offset =
            std::mismatch(
                std::make_reverse_iterator(string_suffix_range.end()),
                std::make_reverse_iterator(string_suffix_range.begin()),
                std::make_reverse_iterator(buffer_cps.end()),
                std::make_reverse_iterator(buffer_cps.begin()))
                .second.base()
                .base() -
            buffer.end();

        detail::string_buffer_replace_impl<ForRope> string_buffer_replace;
        auto const replaced_range = string_buffer_replace.call(
            string,
            string_prefix_range.begin(),
            string_suffix_range.end(),
            buffer);

        return {
            std::next(replaced_range.begin(), first_buffer_mismatch_offset),
            std::next(replaced_range.end(), last_buffer_mismatch_offset)};
    }

    template<
        bool ForRope,
        nf Normalization,
        typename String,
        typename StringIter,
        typename Iter>
    replace_result<StringIter> replace_impl(
        String & string,
        StringIter str_first,
        StringIter str_last,
        Iter first,
        Iter last,
        insertion_normalization insertion_norm)
    {
        BOOST_TEXT_STATIC_ASSERT_NORMALIZATION();

        if (first == last) {
            return detail::erase_impl<ForRope, Normalization>(
                string, str_first, str_last);
        }

        auto const string_cps =
            boost::text::as_utf32(string.begin(), string.end());
        auto const string_cp_first = boost::text::utf32_iterator(
            string.begin(), str_first, string.end());
        auto const string_cp_last =
            boost::text::utf32_iterator(string.begin(), str_last, string.end());

        using stable_cps_type =
            stable_cps_result_t<decltype(string_cps.begin())>;

        // Find the unstable CPs on either side of the insertion, and make a
        // range for each, using str_first and str_last as the other bounds
        // respectively.
        auto const string_prefix_range = stable_cps_type{
            detail::last_stable_cp<Normalization>(
                string_cps.begin(), string_cp_first),
            string_cp_first};
        auto const string_suffix_range = stable_cps_type{
            string_cp_last,
            detail::first_stable_cp<Normalization>(
                string_cp_last, string_cps.end())};
        auto const insertion_range =
            detail::stable_cps_result_t<Iter>{first, last};

        using buffer_type = detail::normalized_insert_buffer_t<String>;
        buffer_type buffer;

        auto const insertion_first_stable =
            detail::first_stable_cp<Normalization>(first, last);
        // If the inserted text contains no stable code points, we need to
        // normalize the prefix, the entire insertion, and the suffix all in
        // one go.
        bool const all_at_once = insertion_first_stable == last;

        // [first-stable-cp-before-str_first, first-stable-cp-after-first)
        auto const lo_cons_view = detail::cons_view<uint32_t>(
            string_prefix_range,
            insertion_range,
            string_prefix_range.begin(),
            insertion_first_stable);

        if (all_at_once) {
            auto const cons_view = detail::cons_view<uint32_t>(
                lo_cons_view,
                string_suffix_range,
                lo_cons_view.begin(),
                string_suffix_range.end());
            boost::text::normalize_append<Normalization>(
                boost::text::as_stream_safe(cons_view), buffer);
        } else {
            boost::text::normalize_append<Normalization>(
                boost::text::as_stream_safe(lo_cons_view), buffer);
        }

        auto const insertion_last_stable =
            detail::last_stable_cp<Normalization>(insertion_first_stable, last);

        if (!all_at_once) {
            // Middle of insertion is [insertion_first_stable,
            // insertion_last_stable)
            if (insertion_norm == insertion_normalized) {
                append_normalized_to_buffer<buffer_type>::call(
                    buffer, insertion_first_stable, insertion_last_stable);
            } else {
                boost::text::normalize_append<Normalization>(
                    boost::text::as_stream_safe(
                        insertion_first_stable, insertion_last_stable),
                    buffer);
            }
        }

        // [first-stable-cp-before-last, first-stable-cp-after-str_last)
        auto const hi_cons_view = detail::cons_view<uint32_t>(
            insertion_range,
            string_suffix_range,
            insertion_last_stable,
            string_suffix_range.end());

        if (!all_at_once) {
            boost::text::normalize_append<Normalization>(
                boost::text::as_stream_safe(hi_cons_view), buffer);
        }

        return detail::replace_erase_impl_suffix<ForRope, StringIter>(
            string, buffer, string_prefix_range, string_suffix_range);
    }

    template<
        bool ForRope,
        nf Normalization,
        typename String,
        typename StringIter>
    replace_result<StringIter>
    erase_impl(String & string, StringIter first, StringIter last)
    {
        BOOST_TEXT_STATIC_ASSERT_NORMALIZATION();

        if (first == last)
            return {first, first};

        auto const string_cps =
            boost::text::as_utf32(string.begin(), string.end());
        auto const string_cp_first =
            boost::text::utf32_iterator(string.begin(), first, string.end());
        auto const string_cp_last =
            boost::text::utf32_iterator(string.begin(), last, string.end());

        using stable_cps_type =
            stable_cps_result_t<decltype(string_cps.begin())>;

        // Find the unstable CPs on either side of the deletion, and make a
        // range for each, using first and last as the other bounds
        // respectively.
        auto const string_prefix_range = stable_cps_type{
            detail::last_stable_cp<Normalization>(
                string_cps.begin(), string_cp_first),
            string_cp_first};
        auto const string_suffix_range = stable_cps_type{
            string_cp_last,
            detail::first_stable_cp<Normalization>(
                string_cp_last, string_cps.end())};

        normalized_insert_buffer_t<String> buffer;

        // [first-stable-cp-before-at, first-stable-cp-after-first)
        auto const cons_view = detail::cons_view<uint32_t>(
            string_prefix_range,
            string_suffix_range,
            string_prefix_range.begin(),
            string_suffix_range.end());
        boost::text::normalize_append<Normalization>(
            boost::text::as_stream_safe(cons_view), buffer);

        return detail::replace_erase_impl_suffix<ForRope, StringIter>(
            string, buffer, string_prefix_range, string_suffix_range);
    }

}}}

#if BOOST_TEXT_USE_CONCEPTS

namespace std::ranges {
    template<typename Iter>
    inline constexpr bool
        enable_borrowed_range<boost::text::replace_result<Iter>> = true;

    template<typename CPIter>
    inline constexpr bool enable_borrowed_range<
        boost::text::detail::stable_cps_result_t<CPIter>> = true;

    template<typename T, typename R1, typename R2>
    inline constexpr bool
        enable_borrowed_range<boost::text::detail::cons_view_t<T, R1, R2>> =
            true;
}

#endif

#endif
