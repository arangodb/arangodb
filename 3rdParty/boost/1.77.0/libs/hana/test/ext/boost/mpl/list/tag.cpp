// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/boost/mpl/list.hpp>

#include <boost/hana/core/tag_of.hpp>

#include <boost/mpl/identity.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/quote.hpp>
#include <boost/mpl/transform.hpp>

#include <type_traits>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


struct t1; struct t2; struct t3; struct t4;

using mpl_id = mpl::quote1<mpl::identity>;

static_assert(std::is_same<
    hana::tag_of_t<mpl::list<>>,
    hana::ext::boost::mpl::list_tag
>{}, "");
static_assert(std::is_same<
    hana::tag_of_t<mpl::list<>::type>,
    hana::ext::boost::mpl::list_tag
>{}, "");
static_assert(std::is_same<
    hana::tag_of_t<mpl::transform<mpl::list<>, mpl_id>::type>,
    hana::ext::boost::mpl::list_tag
>{}, "");

static_assert(std::is_same<
    hana::tag_of_t<mpl::list<t1>>,
    hana::ext::boost::mpl::list_tag
>{}, "");
static_assert(std::is_same<
    hana::tag_of_t<mpl::list<t1>::type>,
    hana::ext::boost::mpl::list_tag
>{}, "");
static_assert(std::is_same<
    hana::tag_of_t<mpl::transform<mpl::list<t1>, mpl_id>::type>,
    hana::ext::boost::mpl::list_tag
>{}, "");

static_assert(std::is_same<
    hana::tag_of_t<mpl::list<t1, t2>>,
    hana::ext::boost::mpl::list_tag
>{}, "");
static_assert(std::is_same<
    hana::tag_of_t<mpl::list<t1, t2>::type>,
    hana::ext::boost::mpl::list_tag
>{}, "");
static_assert(std::is_same<
    hana::tag_of_t<mpl::transform<mpl::list<t1, t2>, mpl_id>::type>,
    hana::ext::boost::mpl::list_tag
>{}, "");

int main() { }
