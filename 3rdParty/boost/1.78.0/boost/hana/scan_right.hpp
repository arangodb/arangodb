/*!
@file
Defines `boost::hana::scan_right`.

@copyright Louis Dionne 2013-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
 */

#ifndef BOOST_HANA_SCAN_RIGHT_HPP
#define BOOST_HANA_SCAN_RIGHT_HPP

#include <boost/hana/fwd/scan_right.hpp>

#include <boost/hana/at.hpp>
#include <boost/hana/concept/sequence.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/core/dispatch.hpp>
#include <boost/hana/core/make.hpp>
#include <boost/hana/empty.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/length.hpp>
#include <boost/hana/prepend.hpp>

#include <cstddef>
#include <utility>


namespace boost { namespace hana {
    //! @cond
    template <typename Xs, typename F>
    constexpr auto scan_right_t::operator()(Xs&& xs, F const& f) const {
        using S = typename hana::tag_of<Xs>::type;
        using ScanRight = BOOST_HANA_DISPATCH_IF(scan_right_impl<S>,
            hana::Sequence<S>::value
        );

#ifndef BOOST_HANA_CONFIG_DISABLE_CONCEPT_CHECKS
        static_assert(hana::Sequence<S>::value,
        "hana::scan_right(xs, f) requires 'xs' to be a Sequence");
#endif

        return ScanRight::apply(static_cast<Xs&&>(xs), f);
    }

    template <typename Xs, typename State, typename F>
    constexpr auto scan_right_t::operator()(Xs&& xs, State&& state, F const& f) const {
        using S = typename hana::tag_of<Xs>::type;
        using ScanRight = BOOST_HANA_DISPATCH_IF(scan_right_impl<S>,
            hana::Sequence<S>::value
        );

#ifndef BOOST_HANA_CONFIG_DISABLE_CONCEPT_CHECKS
        static_assert(hana::Sequence<S>::value,
        "hana::scan_right(xs, state, f) requires 'xs' to be a Sequence");
#endif

        return ScanRight::apply(static_cast<Xs&&>(xs),
                                static_cast<State&&>(state), f);
    }
    //! @endcond

    template <typename S, bool condition>
    struct scan_right_impl<S, when<condition>> : default_ {
        // Without initial state
        template <typename Xs, typename F, std::size_t n1, std::size_t n2, std::size_t ...ns>
        static constexpr auto
        apply1_impl(Xs&& xs, F const& f, std::index_sequence<n1, n2, ns...>) {
            auto rest = scan_right_impl::apply1_impl(static_cast<Xs&&>(xs),
                                                     f, std::index_sequence<n2, ns...>{});
            auto element = f(hana::at_c<n1>(static_cast<Xs&&>(xs)), hana::front(rest));
            return hana::prepend(std::move(rest), std::move(element));
        }

        template <typename Xs, typename F, std::size_t n>
        static constexpr auto apply1_impl(Xs&& xs, F const&, std::index_sequence<n>) {
            return hana::make<S>(hana::at_c<n>(static_cast<Xs&&>(xs)));
        }

        template <typename Xs, typename F>
        static constexpr auto apply1_impl(Xs&&, F const&, std::index_sequence<>) {
            return hana::empty<S>();
        }

        template <typename Xs, typename F>
        static constexpr auto apply(Xs&& xs, F const& f) {
            constexpr std::size_t Len = decltype(hana::length(xs))::value;
            return scan_right_impl::apply1_impl(static_cast<Xs&&>(xs),
                                                f, std::make_index_sequence<Len>{});
        }


        // With initial state
        template <typename Xs, typename State, typename F,
                  std::size_t n1, std::size_t n2, std::size_t ...ns>
        static constexpr auto
        apply_impl(Xs&& xs, State&& state, F const& f,
                   std::index_sequence<n1, n2, ns...>)
        {
            auto rest = scan_right_impl::apply_impl(static_cast<Xs&&>(xs),
                                                    static_cast<State&&>(state),
                                                    f, std::index_sequence<n2, ns...>{});
            auto element = f(hana::at_c<n1>(static_cast<Xs&&>(xs)), hana::front(rest));
            return hana::prepend(std::move(rest), std::move(element));
        }

        template <typename Xs, typename State, typename F, std::size_t n>
        static constexpr auto
        apply_impl(Xs&& xs, State&& state, F const& f, std::index_sequence<n>) {
            auto element = f(hana::at_c<n>(static_cast<Xs&&>(xs)), state);
            return hana::make<S>(std::move(element), static_cast<State&&>(state));
        }

        template <typename Xs, typename State, typename F>
        static constexpr auto
        apply_impl(Xs&&, State&& state, F const&, std::index_sequence<>) {
            return hana::make<S>(static_cast<State&&>(state));
        }

        template <typename Xs, typename State, typename F>
        static constexpr auto apply(Xs&& xs, State&& state, F const& f) {
            constexpr std::size_t Len = decltype(hana::length(xs))::value;
            return scan_right_impl::apply_impl(static_cast<Xs&&>(xs),
                                               static_cast<State&&>(state),
                                               f, std::make_index_sequence<Len>{});
        }
    };
}} // end namespace boost::hana

#endif // !BOOST_HANA_SCAN_RIGHT_HPP
