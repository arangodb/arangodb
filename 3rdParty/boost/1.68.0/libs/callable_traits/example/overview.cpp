/*<-
Copyright Barrett Adair 2016-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>
#ifdef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
int main(){}
#else

//[ overview
#include <boost/callable_traits.hpp>

#include <type_traits>
#include <tuple>

using std::is_same;
using std::tuple;

using namespace boost::callable_traits;

struct number {
    int value;
    int add(int n) const { return value + n; }
};

using pmf = decltype(&number::add);

//` Manipulate member functions pointers with ease:
static_assert(is_same<
    remove_member_const_t<pmf>,
    int(number::*)(int)
>{}, "");

static_assert(is_same<
    add_member_volatile_t<pmf>,
    int(number::*)(int) const volatile
>{}, "");

static_assert(is_same<
    class_of_t<pmf>,
    number
>{}, "");

//` INVOKE-aware metafunctions:

static_assert(is_same<
    args_t<pmf>,
    tuple<const number&, int>
>{}, "");

static_assert(is_same<
    return_type_t<pmf>,
    int
>{}, "");

static_assert(is_same<
    function_type_t<pmf>,
    int(const number&, int)
>{}, "");

static_assert(is_same<
    qualified_class_of_t<pmf>,
    const number&
>{}, "");

//` Here are a few other trait examples:
static_assert(is_const_member<pmf>{}, "");
static_assert(!is_volatile_member<pmf>{}, "");
static_assert(!has_void_return<pmf>{}, "");
static_assert(!has_varargs<pmf>{}, "");

//]

int main() {}

#endif
