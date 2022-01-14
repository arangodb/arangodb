/*=============================================================================
    Copyright (c) 2016 Lee Clagett

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/container/vector.hpp>
#include <boost/fusion/tuple/tuple.hpp>
#include <boost/config.hpp>
#include <boost/type_traits/is_constructible.hpp>
#include <boost/type_traits/is_convertible.hpp>

#define FUSION_SEQUENCE boost::fusion::tuple
#define FUSION_ALT_SEQUENCE boost::fusion::vector
#include "traits.hpp"

struct not_convertible {};

/*  Some construction differences in fusion::tuple from std::tuple:
      - Construction from elements cannot call an explicit constructor.
      - There is no implicit construction from elements.
      - Construction from std::pair is _enabled_ when tuple is not of size 2.
      - Construction from tuple is _enabled_ when destination tuple is of
        different size.
      - Implicit construction from std::pair can call explicit constructors on
        elements.
      - Implicit construction from tuple can call explicit constructors on
        elements.

    These differences are historical. Matching the behavior of std::tuple
    could break existing code, however, switching to fusion::vector would
    restore the historical behavior. */
int
main()
{
    using namespace boost::fusion;

    test_convertible(false /* no conversion construction */ );

    BOOST_TEST((is_convertible<std::pair<int, int>, tuple<int, int> >(true)));
    BOOST_TEST((
        is_convertible<std::pair<int, int>, tuple<convertible, int> >(true)
    ));
    BOOST_TEST((
        is_convertible<std::pair<int, int>, tuple<int, convertible> >(true)
    ));
    BOOST_TEST((
        is_convertible<
            std::pair<int, int>, tuple<convertible, convertible>
        >(true)
    ));

#if defined(FUSION_TEST_HAS_CONSTRUCTIBLE)
    test_constructible();

    BOOST_TEST((is_constructible< tuple<> >(true)));
    BOOST_TEST((is_constructible<tuple<>, int>(false)));

    BOOST_TEST((is_constructible< tuple<int> >(true)));
    BOOST_TEST((is_constructible<tuple<int>, int>(true)));
    BOOST_TEST((is_constructible<tuple<convertible>, int>(true)));
    BOOST_TEST((is_constructible<tuple<not_convertible>, int>(false)));
    BOOST_TEST((is_constructible< tuple<int>, vector<int> >(false)));
    BOOST_TEST((is_constructible<tuple<int>, int, int>(false)));

    BOOST_TEST((is_constructible< tuple<int, int> >(true)));
    // boost::is_constructible always fail to test ctor which takes 2 or more arguments on GCC 4.7.
#if !BOOST_WORKAROUND(BOOST_GCC, < 40700)
    BOOST_TEST((is_constructible<tuple<int, int>, int, int>(true)));
    BOOST_TEST((
        is_constructible<tuple<convertible, convertible>, int, int>(true)
    ));
#endif // !(gcc < 4.7)
    BOOST_TEST((is_constructible<tuple<int, not_convertible>, int, int>(false)));
    BOOST_TEST((is_constructible<tuple<not_convertible, int>, int, int>(false)));
    BOOST_TEST((
        is_constructible<tuple<not_convertible, not_convertible>, int, int>(false)
    ));
#if defined(BOOST_FUSION_HAS_VARIADIC_VECTOR)
    // C++03 fusion::tuple has constructors that can never be used
    BOOST_TEST((is_constructible<tuple<int, int>, int>(false)));
#endif
    BOOST_TEST((is_constructible<tuple<int, int>, int, int, int>(false)));

#endif // FUSION_TEST_HAS_CONSTRUCTIBLE

    return boost::report_errors();
}
