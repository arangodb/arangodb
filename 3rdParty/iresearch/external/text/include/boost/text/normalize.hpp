// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_TEXT_NORMALIZE_HPP
#define BOOST_TEXT_NORMALIZE_HPP

#include <boost/text/algorithm.hpp>
#include <boost/text/concepts.hpp>
#include <boost/text/transcode_algorithm.hpp>
#include <boost/text/transcode_iterator.hpp>
#include <boost/text/transcode_view.hpp>
#include <boost/text/detail/normalization_data.hpp>
#include <boost/text/detail/normalize.hpp>

#include <boost/container/static_vector.hpp>

#include <algorithm>


namespace boost { namespace text {

    namespace detail {

        template<typename CPIter, typename Sentinel>
        using utf8_range_expr = is_char_iter<decltype(
            detail::unpack_iterator_and_sentinel(
                std::declval<CPIter>(), std::declval<Sentinel>())
                .f_)>;

        template<typename CPIter, typename Sentinel>
        using utf8_fast_path =
            detected_or_t<std::false_type, utf8_range_expr, CPIter, Sentinel>;

        template<typename OutIter>
        struct norm_result
        {
            OutIter out_;
            bool normalized_;
        };

        template<
            nf Normalization,
            typename OutIter, // will be bool for the check-only case
            typename CPIter,
            typename Sentinel,
            bool UTF8 = utf8_fast_path<CPIter, Sentinel>::value,
            bool Composition =
                Normalization != nf::d && Normalization != nf::kd>
        struct norm_impl
        {
            // Primary template does decomposition.
            template<typename Appender>
            static norm_result<OutIter>
            call(CPIter first_, Sentinel last_, Appender appender)
            {
                constexpr bool do_writes = !std::is_same<OutIter, bool>::value;

                auto const r = boost::text::as_utf16(first_, last_);
                auto first = r.begin();
                auto const last = r.end();

                int const chunk_size = 512;
                std::array<uint16_t, chunk_size> input;
                auto input_first = input.data();

                while (first != last) {
                    int n = 0;
                    auto input_last = input_first;
                    for (; first != last && n < chunk_size - 1;
                         ++first, ++input_last, ++n) {
                        *input_last = *first;
                    }
                    if (high_surrogate(*std::prev(input_last)) &&
                        first != last) {
                        *input_last++ = *first;
                        ++first;
                    }
                    auto const & table = Normalization == nf::kd
                                             ? detail::nfkc_table()
                                             : detail::nfc_table();
                    detail::reordering_appender<Appender> buffer(
                        table, appender);
                    auto const input_new_first = detail::decompose<do_writes>(
                        table, input.data(), input_last, buffer);
                    if (!do_writes && input_new_first != input_last)
                        return norm_result<OutIter>{appender.out(), false};
                    input_first =
                        std::copy(input_new_first, input_last, input.data());
                }

                return norm_result<OutIter>{appender.out(), true};
            }
        };


        template<
            nf Normalization,
            typename OutIter,
            typename CPIter,
            typename Sentinel,
            bool UTF8>
        struct norm_impl<Normalization, OutIter, CPIter, Sentinel, UTF8, true>
        {
            template<typename Appender>
            static norm_result<OutIter>
            call(CPIter first, Sentinel last, Appender appender)
            {
                constexpr bool do_writes = !std::is_same<OutIter, bool>::value;

                auto const r = boost::text::as_utf16(first, last);
                auto const & table = Normalization == nf::kc
                                         ? detail::nfkc_table()
                                         : detail::nfc_table();
                detail::reordering_appender<Appender> reorder_buffer(
                    table, appender);
                auto const normalized =
                    detail::compose<Normalization == nf::fcc, do_writes>(
                        table, r.begin(), r.end(), reorder_buffer);
                return norm_result<OutIter>{appender.out(), (bool)normalized};
            }
        };

        template<
            nf Normalization,
            typename OutIter,
            typename CPIter,
            typename Sentinel>
        struct norm_impl<Normalization, OutIter, CPIter, Sentinel, true, true>
        {
            template<typename Appender>
            static norm_result<OutIter>
            call(CPIter first, Sentinel last, Appender appender)
            {
                constexpr bool do_writes = !std::is_same<OutIter, bool>::value;

                auto const r = boost::text::as_utf8(first, last);
                auto const & table = Normalization == nf::kc
                                         ? detail::nfkc_table()
                                         : detail::nfc_table();
                auto const normalized =
                    detail::compose_utf8<Normalization == nf::fcc, do_writes>(
                        table, r.begin(), r.end(), appender);
                return norm_result<OutIter>{appender.out(), (bool)normalized};
            }
        };

        template<
            nf Normalization,
            typename CPIter,
            typename Sentinel,
            typename OutIter,
            bool UTF8 = utf8_fast_path<CPIter, Sentinel>::value &&
                            Normalization != nf::d && Normalization != nf::kd>
        struct normalization_appender
        {
            using type = utf16_to_utf32_appender<OutIter>;
        };

        template<
            nf Normalization,
            typename CPIter,
            typename Sentinel,
            typename OutIter>
        struct normalization_appender<
            Normalization,
            CPIter,
            Sentinel,
            OutIter,
            true>
        {
            using type = utf8_to_utf32_appender<OutIter>;
        };

        template<
            nf Normalization,
            typename CPIter,
            typename Sentinel,
            typename OutIter>
        using normalization_appender_t = typename normalization_appender<
            Normalization,
            CPIter,
            Sentinel,
            OutIter>::type;
    }

}}

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V1 {

    /** Writes sequence `[first, last)` in Unicode normalization form
        `Normalization` to `out`.

        This function only participates in overload resolution if `CPIter`
        models the CPIter concept.

        \see https://unicode.org/notes/tn5 */
    template<
        nf Normalization,
        typename CPIter,
        typename Sentinel,
        typename OutIter>
    auto normalize(CPIter first, Sentinel last, OutIter out)
        ->detail::cp_iter_ret_t<OutIter, CPIter>
    {
        BOOST_TEXT_STATIC_ASSERT_NORMALIZATION();
        detail::
            normalization_appender_t<Normalization, CPIter, Sentinel, OutIter>
                appender(out);
        return detail::norm_impl<Normalization, OutIter, CPIter, Sentinel>::
            call(first, last, appender)
                .out_;
    }

    /** Writes sequence `r` in Unicode normalization form `Normalization` to
        `out`.

        \see https://unicode.org/notes/tn5 */
    template<nf Normalization, typename CPRange, typename OutIter>
    OutIter normalize(CPRange && r, OutIter out)
    {
        return v1::normalize<Normalization>(std::begin(r), std::end(r), out);
    }

    /** Returns true iff the given sequence of code points is in Unicode
        normalization form `Normalization`.

        This function only participates in overload resolution if `CPIter`
        models the CPIter concept.

        \see https://unicode.org/notes/tn5 */
    template<nf Normalization, typename CPIter, typename Sentinel>
    auto normalized(
        CPIter first,
        Sentinel last) noexcept->detail::cp_iter_ret_t<bool, CPIter>
    {
        BOOST_TEXT_STATIC_ASSERT_NORMALIZATION();
        detail::null_appender appender;
        return detail::norm_impl<Normalization, bool, CPIter, Sentinel>::call(
                   first, last, appender)
            .normalized_;
    }

    /** Returns true iff the given sequence of code points is in Unicode
        normalization form `Normalization`. */
    template<nf Normalization, typename CPRange>
    bool normalized(CPRange && r) noexcept
    {
        return v1::normalized<Normalization>(std::begin(r), std::end(r));
    }

}}}

#if defined(BOOST_TEXT_DOXYGEN) || BOOST_TEXT_USE_CONCEPTS

namespace boost { namespace text { BOOST_TEXT_NAMESPACE_V2 {

    /** Writes sequence `[first, last)` in Unicode normalization form
        `Normalization` to `out`.

        \see https://unicode.org/notes/tn5 */
    template<
        nf Normalization,
        code_point_iter I,
        std::sentinel_for<I> S,
        std::output_iterator<uint32_t> O>
    O normalize(I first, S last, O out)
    {
        BOOST_TEXT_STATIC_ASSERT_NORMALIZATION();
        detail::normalization_appender_t<Normalization, I, S, O> appender(out);
        return detail::norm_impl<Normalization, O, I, S>::call(
                   first, last, appender)
            .out_;
    }

    /** Writes sequence `r` in Unicode normalization form `Normalization` to
        `out`.

        \see https://unicode.org/notes/tn5 */
    template<
        nf Normalization,
        code_point_range R,
        std::output_iterator<uint32_t> O>
    O normalize(R && r, O out)
    {
        return boost::text::normalize<Normalization>(
            std::ranges::begin(r), std::ranges::end(r), out);
    }

    /** Returns true iff the given sequence of code points is in Unicode
        normalization form `Normalization`.

        \see https://unicode.org/notes/tn5 */
    template<nf Normalization, code_point_iter I, std::sentinel_for<I> S>
    bool normalized(I first, S last) noexcept
    {
        BOOST_TEXT_STATIC_ASSERT_NORMALIZATION();
        detail::null_appender appender;
        return detail::norm_impl<Normalization, bool, I, S>::call(
                   first, last, appender)
            .normalized_;
    }

    /** Returns true iff the given sequence of code points is in Unicode
        normalization form `Normalization`. */
    template<nf Normalization, code_point_range R>
    bool normalized(R && r) noexcept
    {
        return boost::text::normalized<Normalization>(
            std::ranges::begin(r), std::ranges::end(r));
    }

}}}

#endif

#endif
