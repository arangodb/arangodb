
#include <boost/callable_traits.hpp>
#include "test.hpp"

struct foo;

#define CALLABLE_TRAIT_UNDER_TEST add_member_const
#include "qualifier_metafunction_constraints.hpp"
#undef CALLABLE_TRAIT_UNDER_TEST

#define CALLABLE_TRAIT_UNDER_TEST remove_member_const
#include "qualifier_metafunction_constraints.hpp"
#undef CALLABLE_TRAIT_UNDER_TEST

#define CALLABLE_TRAIT_UNDER_TEST add_member_volatile
#include "qualifier_metafunction_constraints.hpp"
#undef CALLABLE_TRAIT_UNDER_TEST

#define CALLABLE_TRAIT_UNDER_TEST remove_member_volatile
#include "qualifier_metafunction_constraints.hpp"
#undef CALLABLE_TRAIT_UNDER_TEST

#define CALLABLE_TRAIT_UNDER_TEST add_member_cv
#include "qualifier_metafunction_constraints.hpp"
#undef CALLABLE_TRAIT_UNDER_TEST

#define CALLABLE_TRAIT_UNDER_TEST remove_member_cv
#include "qualifier_metafunction_constraints.hpp"
#undef CALLABLE_TRAIT_UNDER_TEST

#ifndef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS

#define CALLABLE_TRAIT_UNDER_TEST add_member_lvalue_reference
#include "qualifier_metafunction_constraints.hpp"
#undef CALLABLE_TRAIT_UNDER_TEST

#define CALLABLE_TRAIT_UNDER_TEST add_member_rvalue_reference
#include "qualifier_metafunction_constraints.hpp"
#undef CALLABLE_TRAIT_UNDER_TEST

#define CALLABLE_TRAIT_UNDER_TEST remove_member_reference
#include "qualifier_metafunction_constraints.hpp"
#undef CALLABLE_TRAIT_UNDER_TEST

#endif // #ifndef BOOST_CLBL_TRTS_DISABLE_REFERENCE_QUALIFIERS

int main(){}

