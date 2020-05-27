/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>
#ifdef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS
int main(){ return 0; }
#else

//[ qualified_class_of
#include <type_traits>
#include <boost/callable_traits/qualified_class_of.hpp>

namespace ct = boost::callable_traits;

struct foo;

static_assert(std::is_same<foo &,
    ct::qualified_class_of_t<int(foo::*)()>>::value, "");
    
static_assert(std::is_same<foo const &,
    ct::qualified_class_of_t<int(foo::*)() const>>::value, "");

static_assert(std::is_same<foo volatile &&,
    ct::qualified_class_of_t<int(foo::*)() volatile &&>>::value, "");

int main() {}
//]
#endif
