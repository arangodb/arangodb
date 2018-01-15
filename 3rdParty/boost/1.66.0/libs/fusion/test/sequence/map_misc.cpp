/*=============================================================================
    Copyright (c) 1999-2003 Jaakko Jarvi
    Copyright (c) 2001-2013 Joel de Guzman
    Copyright (c) 2006 Dan Marsden

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/container/map/map.hpp>
#include <boost/fusion/container/map/convert.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/sequence/intrinsic.hpp>
#include <boost/fusion/support/is_sequence.hpp>
#include <boost/fusion/mpl.hpp>
#include <boost/mpl/find.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/type_traits/is_same.hpp>
#include <string>

struct k1 {};
struct k2 {};
struct k3 {};
struct k4 {};

template <typename S1, typename S2>
struct is_same
{
};

namespace fn = boost::fusion;

struct test_intrinsics1
{
    // test at, begin, end, next, prior, advance, size, deref, etc.

    typedef fn::map<
        fn::pair<k1, int>, fn::pair<k2, float>,
        fn::pair<k3, bool>, fn::pair<k3, char> >
    sequence;

    typedef boost::mpl::begin<sequence>::type first;
    typedef boost::mpl::next<first>::type second;
    typedef boost::mpl::next<second>::type third;
    typedef boost::mpl::next<third>::type fourth;
    typedef boost::mpl::end<sequence>::type last;

    BOOST_STATIC_ASSERT((boost::is_same<
        boost::mpl::deref<first>::type, fn::pair<k1, int> >::value));

    BOOST_STATIC_ASSERT((boost::is_same<
        boost::mpl::deref<second>::type, fn::pair<k2, float> >::value));

    BOOST_STATIC_ASSERT((boost::is_same<
        boost::mpl::deref<third>::type, fn::pair<k3, bool> >::value));

    BOOST_STATIC_ASSERT((boost::is_same<
        boost::mpl::deref<fourth>::type, fn::pair<k3, char> >::value));

    BOOST_STATIC_ASSERT((boost::is_same<
        boost::mpl::at_c<sequence, 2>::type, fn::pair<k3, bool> >::value));

    BOOST_STATIC_ASSERT((boost::is_same<
        boost::mpl::front<sequence>::type, fn::pair<k1, int> >::value));

    BOOST_STATIC_ASSERT((boost::is_same<
        boost::mpl::deref<
            boost::mpl::advance_c<second, 2>::type>::type, fn::pair<k3, char> >::value));

    BOOST_STATIC_ASSERT((boost::mpl::size<sequence>::value == 4));
    BOOST_STATIC_ASSERT(!(boost::mpl::empty<sequence>::value));
    BOOST_STATIC_ASSERT((boost::mpl::distance<second, fourth>::value == 2));

    typedef boost::mpl::prior<last>::type fourth_;
    typedef boost::mpl::prior<fourth_>::type third_;
    typedef boost::mpl::prior<third_>::type second_;
    typedef boost::mpl::prior<second_>::type first_;

    BOOST_STATIC_ASSERT((boost::is_same<
        boost::mpl::deref<first_>::type, fn::pair<k1, int> >::value));

    BOOST_STATIC_ASSERT((boost::is_same<
        boost::mpl::deref<second_>::type, fn::pair<k2, float> >::value));

    BOOST_STATIC_ASSERT((boost::is_same<
        boost::mpl::deref<third_>::type, fn::pair<k3, bool> >::value));

    BOOST_STATIC_ASSERT((boost::is_same<
        boost::mpl::deref<fourth_>::type, fn::pair<k3, char> >::value));

    BOOST_STATIC_ASSERT((boost::is_same<
        boost::mpl::back<sequence>::type, fn::pair<k3, char> >::value));

};

void
test()
{
    using namespace boost::fusion;

    {   // testing const sequences

        const map<pair<k1, int>, pair<k2, float> > t1(5, 3.3f);
        BOOST_TEST(at_c<0>(t1).second == 5);
        BOOST_TEST(at_c<1>(t1).second == 3.3f);
    }

    {   // testing at<N> works with MPL integral constants
        const map<pair<k1, int>, pair<k2, char> > t1(101, 'z');
        BOOST_TEST(boost::fusion::at<boost::mpl::int_<0> >(t1).second == 101);
        BOOST_TEST(boost::fusion::at<boost::mpl::int_<1> >(t1).second == 'z');
        // explicitly try something other than mpl::int_
        BOOST_TEST((boost::fusion::at<boost::mpl::integral_c<long, 0> >(t1).second == 101));
        BOOST_TEST((boost::fusion::at<boost::mpl::integral_c<long, 1> >(t1).second == 'z'));
    }

    {   // testing size & empty

        typedef map<pair<k1, int>, pair<k2, float>, pair<k3, double> > t1;
        typedef map<> t2;

        BOOST_STATIC_ASSERT(boost::fusion::result_of::size<t1>::value == 3);
        BOOST_STATIC_ASSERT(boost::fusion::result_of::size<t2>::value == 0);
        BOOST_STATIC_ASSERT(!boost::fusion::result_of::empty<t1>::value);
        BOOST_STATIC_ASSERT(boost::fusion::result_of::empty<t2>::value);
    }

    {   // testing front & back

        typedef map<pair<k1, int>, pair<k2, float>, pair<k3, std::string> > tup;
        tup t(1, 2.2f, std::string("Kimpo"));

        BOOST_TEST(front(t).second == 1);
        BOOST_TEST(back(t).second == "Kimpo");
    }

    {   // testing is_sequence

        typedef map<pair<k1, int>, pair<k2, float>, pair<k3, double> > t1;
        typedef map<> t2;
        typedef map<pair<k1, char> > t3;

        BOOST_STATIC_ASSERT(traits::is_sequence<t1>::value);
        BOOST_STATIC_ASSERT(traits::is_sequence<t2>::value);
        BOOST_STATIC_ASSERT(traits::is_sequence<t3>::value);
        BOOST_STATIC_ASSERT(!traits::is_sequence<int>::value);
        BOOST_STATIC_ASSERT(!traits::is_sequence<char>::value);
    }

    {   // testing mpl::is_sequence

        typedef map<pair<k1, int>, pair<k2, float>, pair<k3, double> > t1;
        typedef map<> t2;
        typedef map<pair<k1, char> > t3;

        BOOST_STATIC_ASSERT(boost::mpl::is_sequence<t1>::value);
        BOOST_STATIC_ASSERT(boost::mpl::is_sequence<t2>::value);
        BOOST_STATIC_ASSERT(boost::mpl::is_sequence<t3>::value);
    }

    {   // testing mpl compatibility

        // test an algorithm
        typedef map<pair<k1, int>, pair<k2, float>, pair<k3, double> > t1;
        typedef boost::mpl::find<t1, pair<k2, float> >::type iter;
        typedef boost::mpl::deref<iter>::type type;
        BOOST_STATIC_ASSERT((boost::is_same<type, pair<k2, float> >::value));
    }
}

int
main()
{
    test();
    return boost::report_errors();
}

