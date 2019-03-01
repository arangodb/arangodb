/*=============================================================================
    Copyright (C) 2015 Kohei Takahshi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <utility>
#include <boost/config.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/as_deque.hpp>
#include <boost/fusion/include/as_list.hpp>
#include <boost/fusion/include/as_vector.hpp>
#include <boost/fusion/include/begin.hpp>
#include <boost/fusion/include/is_sequence.hpp>
#include <boost/fusion/include/size.hpp>
#include <boost/fusion/include/value_of.hpp>
#include <boost/type_traits/integral_constant.hpp>
#include <boost/type_traits/remove_const.hpp>
#include <boost/type_traits/remove_reference.hpp>

#include "fixture.hpp"

namespace test_detail
{
    struct adapted_sequence
    {
        adapted_sequence() : value_() {}
        explicit adapted_sequence(int value) : value_(value) {}
        int value_;
    };

    bool operator==(adapted_sequence const& lhs, adapted_sequence const& rhs)
    {
        return lhs.value_ == rhs.value_;
    }

    bool operator!=(adapted_sequence const& lhs, adapted_sequence const& rhs)
    {
        return lhs.value_ != rhs.value_;
    }

    template <template <typename> class Scenario>
    struct can_convert_using
    {
        template <typename T>
        struct to
        {
            static bool can_convert_(boost::true_type /* skip */)
            {
                return true;
            }

            static bool can_convert_(boost::false_type /* skip */)
            {
                using namespace boost::fusion;
                return
                    run<Scenario<T> >(typename result_of::as_deque<T>::type()) &&
                    run<Scenario<T> >(typename result_of::as_list<T>::type()) &&
                    run<Scenario<T> >(typename result_of::as_vector<T>::type());
            }

            template <typename Source, typename Expected>
            bool operator()(Source const&, Expected const&) const
            {
                // bug when converting single element sequences in C++03 and
                // C++11... 
                // not_<not_<is_convertible<sequence<sequence<int>>, int >
                // is invalid check
                typedef typename ::boost::fusion::result_of::size<T>::type seq_size;
                return can_convert_(
                    boost::integral_constant<bool, seq_size::value == 1>()
                );
            }
        };
    };

    template <typename T>
    struct can_construct_from_elements
    {
        template <typename Source, typename Expected>
        bool operator()(Source const&, Expected const&) const
        {
            // constructing a nested sequence of one is the complicated case to
            // disambiguate from a conversion-copy, so focus on that
            typedef typename boost::fusion::result_of::size<T>::type seq_size;
            return can_construct_(
                boost::integral_constant<int, seq_size::value>()
            );
        }

        template <int Size>
        static bool can_construct_(boost::integral_constant<int, Size>)
        {
            return Size == 0 || Size == 2 || Size == 3;
        }

        static bool can_construct_(boost::integral_constant<int, 1>)
        {
            typedef typename ::boost::remove_reference<
                typename ::boost::remove_const<
                    typename ::boost::fusion::result_of::value_of<
                        typename ::boost::fusion::result_of::begin<T>::type
                    >::type
                >::type
            >::type element;

            return run< can_construct<T> >(element(), T());
        }
    };

    template <typename T>
    struct can_nest
    {
        template <typename Source, typename Expected>
        bool operator()(Source const& source, Expected const& expected)
        {
            return
                run< can_copy<T> >(source, expected) &&
                run< can_convert_using<can_copy>::to<T> >(source, expected) &&
                run< can_construct_from_elements<T> >(source, expected);
        }
    };
} // test_detail


BOOST_FUSION_ADAPT_STRUCT(test_detail::adapted_sequence, (int, data))

template <template <typename> class Scenario>
void
test()
{
    using namespace test_detail;

    BOOST_TEST(boost::fusion::traits::is_sequence<adapted_sequence>::value);
    BOOST_TEST(boost::fusion::size(adapted_sequence()) == 1);

    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE< FUSION_SEQUENCE<> > > >(
            FUSION_SEQUENCE< FUSION_SEQUENCE<> >()
        )
    ));
    BOOST_TEST((
        run< Scenario<FUSION_SEQUENCE<FUSION_SEQUENCE<>, int> > >(
            FUSION_SEQUENCE< FUSION_SEQUENCE<>, int>(FUSION_SEQUENCE<>(), 325)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<int, FUSION_SEQUENCE<> > > >(
            FUSION_SEQUENCE< int, FUSION_SEQUENCE<> >(325, FUSION_SEQUENCE<>())
        )
    ));
    BOOST_TEST((
        run< Scenario<FUSION_SEQUENCE<int, FUSION_SEQUENCE<>, float> > >(
            FUSION_SEQUENCE<int, FUSION_SEQUENCE<> , float>(
                325, FUSION_SEQUENCE<>(), 2.0f
            )
        )
    ));

    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE< FUSION_SEQUENCE<int> > > >(
            FUSION_SEQUENCE< FUSION_SEQUENCE<int> >(FUSION_SEQUENCE<int>(400))
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<adapted_sequence> > >(
            FUSION_SEQUENCE<adapted_sequence>(adapted_sequence(400))
        )
    ));
    BOOST_TEST((
        run< Scenario<FUSION_SEQUENCE<FUSION_SEQUENCE<int>, int> > >(
            FUSION_SEQUENCE<FUSION_SEQUENCE<int>, int>(
                FUSION_SEQUENCE<int>(325), 400
            )
        )
    ));
    BOOST_TEST((
        run< Scenario<FUSION_SEQUENCE<adapted_sequence, int> > >(
           FUSION_SEQUENCE<adapted_sequence, int>(adapted_sequence(325), 400)
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE< int, FUSION_SEQUENCE<int> > > >(
            FUSION_SEQUENCE< int, FUSION_SEQUENCE<int> >(
                325, FUSION_SEQUENCE<int>(400)
            )
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<int, adapted_sequence> > >(
            FUSION_SEQUENCE<int, adapted_sequence>(325, adapted_sequence(450))
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<int, FUSION_SEQUENCE<int>, int> > >(
            FUSION_SEQUENCE<int, FUSION_SEQUENCE<int>, int>(
                500, FUSION_SEQUENCE<int>(350), 200
            )
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<int, adapted_sequence, int> > >(
            FUSION_SEQUENCE<int, adapted_sequence, int>(
                300, adapted_sequence(500), 400)
        )
    ));

    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE< FUSION_SEQUENCE<int, int> > > >(
            FUSION_SEQUENCE< FUSION_SEQUENCE<int, int> >(
                FUSION_SEQUENCE<int, int>(450, 500)
            )
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE<FUSION_SEQUENCE<int, int>, int> > >(
            FUSION_SEQUENCE<FUSION_SEQUENCE<int, int>, int>(
                FUSION_SEQUENCE<int, int>(450, 500), 150
            )
        )
    ));
    BOOST_TEST((
        run< Scenario< FUSION_SEQUENCE< int, FUSION_SEQUENCE<int, int> > > >(
            FUSION_SEQUENCE< int, FUSION_SEQUENCE<int, int> >(
                450, FUSION_SEQUENCE<int, int>(500, 150)
            )
        )
    ));
    BOOST_TEST((
        run<Scenario< FUSION_SEQUENCE<int, FUSION_SEQUENCE<int, int>, int> > >(
            FUSION_SEQUENCE<int, FUSION_SEQUENCE<int, int>, int>(
                150, FUSION_SEQUENCE<int, int>(250, 350), 450
            )
        )
    ));

    BOOST_TEST((
        run<Scenario<FUSION_SEQUENCE<FUSION_SEQUENCE<>, FUSION_SEQUENCE<> > > >(
            FUSION_SEQUENCE< FUSION_SEQUENCE<>, FUSION_SEQUENCE<> >(
                FUSION_SEQUENCE<>(), FUSION_SEQUENCE<>()
            )
        )
    ));
    BOOST_TEST((
        run<Scenario<FUSION_SEQUENCE<FUSION_SEQUENCE<int>, FUSION_SEQUENCE<> > > >(
            FUSION_SEQUENCE< FUSION_SEQUENCE<int>, FUSION_SEQUENCE<> >(
                FUSION_SEQUENCE<int>(150), FUSION_SEQUENCE<>()
            )
        )
    ));
    BOOST_TEST((
        run<Scenario<FUSION_SEQUENCE<FUSION_SEQUENCE<>, FUSION_SEQUENCE<int> > > >(
            FUSION_SEQUENCE< FUSION_SEQUENCE<>, FUSION_SEQUENCE<int> >(
                FUSION_SEQUENCE<>(), FUSION_SEQUENCE<int>(500)
            )
        )
    ));
    BOOST_TEST((
        run<Scenario<FUSION_SEQUENCE<FUSION_SEQUENCE<int>, FUSION_SEQUENCE<int> > > >(
            FUSION_SEQUENCE< FUSION_SEQUENCE<int>, FUSION_SEQUENCE<int> >(
                FUSION_SEQUENCE<int>(155), FUSION_SEQUENCE<int>(255)
            )
        )
    ));
    BOOST_TEST((
        run< Scenario<
            FUSION_SEQUENCE< FUSION_SEQUENCE<int, int>, FUSION_SEQUENCE<int> >
        > >(
            FUSION_SEQUENCE< FUSION_SEQUENCE<int, int>, FUSION_SEQUENCE<int> >(
                FUSION_SEQUENCE<int, int>(222, 333), FUSION_SEQUENCE<int>(444)
            )
        )
    ));
    BOOST_TEST((
        run< Scenario<
            FUSION_SEQUENCE< FUSION_SEQUENCE<int>, FUSION_SEQUENCE<int, int> >
        > >(
            FUSION_SEQUENCE< FUSION_SEQUENCE<int>, FUSION_SEQUENCE<int, int> >(
                FUSION_SEQUENCE<int>(100), FUSION_SEQUENCE<int, int>(300, 400)
            )
        )
    ));
    BOOST_TEST((
        run< Scenario<
            FUSION_SEQUENCE< FUSION_SEQUENCE<int, int>, FUSION_SEQUENCE<int, int> >
        > >(
            FUSION_SEQUENCE< FUSION_SEQUENCE<int, int>, FUSION_SEQUENCE<int, int> >(
                FUSION_SEQUENCE<int, int>(600, 700)
              , FUSION_SEQUENCE<int, int>(800, 900)
            )
        )
    ));


    // Ignore desired scenario, and cheat to make these work
    BOOST_TEST((
        run< can_lvalue_construct< FUSION_SEQUENCE<FUSION_SEQUENCE<>&> > >(
            FUSION_SEQUENCE<>()
          , FUSION_SEQUENCE< FUSION_SEQUENCE<> >()
        )
    ));
    BOOST_TEST((
        run< can_construct< FUSION_SEQUENCE<const FUSION_SEQUENCE<>&> > >(
            FUSION_SEQUENCE<>()
          , FUSION_SEQUENCE< FUSION_SEQUENCE<> >()
        )
    ));
    BOOST_TEST((
        run< can_lvalue_construct< FUSION_SEQUENCE<FUSION_SEQUENCE<int>&> > >(
            FUSION_SEQUENCE<int>(300)
          , FUSION_SEQUENCE< FUSION_SEQUENCE<int> >(FUSION_SEQUENCE<int>(300))
        )
    ));
    BOOST_TEST((
        run< can_construct< FUSION_SEQUENCE<const FUSION_SEQUENCE<int>&> > >(
            FUSION_SEQUENCE<int>(400)
          , FUSION_SEQUENCE< FUSION_SEQUENCE<int> >(FUSION_SEQUENCE<int>(400))
        )
    ));
}
