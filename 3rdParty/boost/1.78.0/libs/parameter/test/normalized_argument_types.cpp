// Copyright Daniel Wallin 2006.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/config.hpp>
#include <cstddef>

#if (BOOST_PARAMETER_MAX_ARITY < 2)
#error Define BOOST_PARAMETER_MAX_ARITY as 2 or greater.
#endif
#if !defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING) && \
    (BOOST_PARAMETER_EXPONENTIAL_OVERLOAD_THRESHOLD_ARITY < 3)
#error Define BOOST_PARAMETER_EXPONENTIAL_OVERLOAD_THRESHOLD_ARITY \
as 3 or greater.
#endif

namespace test {

    struct count_instances
    {
        count_instances()
        {
            ++count_instances::count;
        }

        count_instances(count_instances const&)
        {
            ++count_instances::count;
        }

        template <typename T>
        count_instances(T const&)
        {
            ++count_instances::count;
        }

        ~count_instances()
        {
            --count_instances::count;
        }

        static std::size_t count;

        void noop() const
        {
        }
    };

    std::size_t count_instances::count = 0;
} // namespace test

#include <boost/parameter/name.hpp>

namespace test {

    BOOST_PARAMETER_NAME(x)
    BOOST_PARAMETER_NAME(y)
} // namespace test

#include <boost/parameter/preprocessor.hpp>

#if defined(BOOST_PARAMETER_CAN_USE_MP11)
#include <type_traits>
#else
#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/type_traits/is_convertible.hpp>
#endif

namespace test {

    BOOST_PARAMETER_FUNCTION((int), f, tag,
        (required
            (x, (long))
        )
        (optional
            (y, (long), 2L)
        )
    )
    {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        static_assert(
            std::is_convertible<x_type,long>::value
          , "is_convertible<x_type,long>"
        );
        static_assert(
            std::is_convertible<y_type,long>::value
          , "is_convertible<y_type,long>"
        );
#else
        BOOST_MPL_ASSERT((
            typename boost::mpl::if_<
                boost::is_convertible<x_type,long>
              , boost::mpl::true_
              , boost::mpl::false_
            >::type
        ));
        BOOST_MPL_ASSERT((
            typename boost::mpl::if_<
                boost::is_convertible<y_type,long>
              , boost::mpl::true_
              , boost::mpl::false_
            >::type
        ));
#endif  // BOOST_PARAMETER_CAN_USE_MP11
        return 0;
    }
} // namespace test

#include <boost/core/lightweight_test.hpp>

namespace test {

    BOOST_PARAMETER_FUNCTION((int), g, tag,
        (required
            (x, (test::count_instances))
        )
    )
    {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        static_assert(
            std::is_convertible<x_type,test::count_instances>::value
          , "is_convertible<x_type,test::count_instances>"
        );
#else
        BOOST_MPL_ASSERT((
            typename boost::mpl::if_<
                boost::is_convertible<x_type,test::count_instances>
              , boost::mpl::true_
              , boost::mpl::false_
            >::type
        ));
#endif
        x.noop();
#if !BOOST_WORKAROUND(BOOST_GCC, < 40000)
        BOOST_TEST_LT(0, test::count_instances::count);
#endif
        return 0;
    }

    BOOST_PARAMETER_FUNCTION((int), h, tag,
        (required
            (x, (test::count_instances const&))
        )
    )
    {
#if defined(BOOST_PARAMETER_CAN_USE_MP11)
        static_assert(
            std::is_convertible<x_type,test::count_instances const>::value
          , "is_convertible<x_type,test::count_instances const>"
        );
#else
        BOOST_MPL_ASSERT((
            typename boost::mpl::if_<
                boost::is_convertible<x_type,test::count_instances const>
              , boost::mpl::true_
              , boost::mpl::false_
            >::type
        ));
#endif
        x.noop();
#if !BOOST_WORKAROUND(BOOST_GCC, < 40000)
        BOOST_TEST_EQ(1, test::count_instances::count);
#endif
        return 0;
    }
} // namespace test

int main()
{
    test::f(1, 2);
    test::f(1., 2.f);
    test::f(1U);
    test::g(0);
    test::h(0);
    return boost::report_errors();
}

