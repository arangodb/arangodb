/*=============================================================================
    Copyright (c) 1999-2003 Jaakko Jarvi
    Copyright (c) 2001-2011 Joel de Guzman
    Copyright (c) 2006 Dan Marsden

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/container/map/map.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/sequence/intrinsic/at.hpp>

struct key1 {};
struct key2 {};
struct key3 {};

namespace test_detail
{
    // something to prevent warnings for unused variables
    template<class T> void dummy(const T&) {}

    // no public default constructor
    class foo
    {
    public:

        explicit foo(int v) : val(v) {}

        bool operator==(const foo& other) const
        {
            return val == other.val;
        }

    private:

        foo() {}
        int val;
    };

    // another class without a public default constructor
    class no_def_constructor
    {
        no_def_constructor() {}

    public:

        no_def_constructor(std::string) {}
    };
}

inline void
test()
{
    using namespace boost::fusion;
    using namespace test_detail;

    nil empty;

    map<> empty0;

#ifndef NO_CONSTRUCT_FROM_NIL
    map<> empty1(empty);
#endif

    map<pair<key1, int> > t1;
    BOOST_TEST(at_c<0>(t1).second == int());

    map<pair<key1, float> > t2(5.5f);
    BOOST_TEST(at_c<0>(t2).second > 5.4f && at_c<0>(t2).second < 5.6f);

    map<pair<key1, foo> > t3(foo(12));
    BOOST_TEST(at_c<0>(t3).second == foo(12));

    map<pair<key1, double> > t4(t2);
    BOOST_TEST(at_c<0>(t4).second > 5.4 && at_c<0>(t4).second < 5.6);

    map<pair<key1, int>, pair<key2, float> > t5;
    BOOST_TEST(at_c<0>(t5).second == int());
    BOOST_TEST(at_c<1>(t5).second == float());

    map<pair<key1, int>, pair<key2, float> > t6(12, 5.5f);
    BOOST_TEST(at_c<0>(t6).second == 12);
    BOOST_TEST(at_c<1>(t6).second > 5.4f && at_c<1>(t6).second < 5.6f);

    map<pair<key1, int>, pair<key2, float> > t7(t6);
    BOOST_TEST(at_c<0>(t7).second == 12);
    BOOST_TEST(at_c<1>(t7).second > 5.4f && at_c<1>(t7).second < 5.6f);

    map<pair<key1, long>, pair<key2, double> > t8(t6);
    BOOST_TEST(at_c<0>(t8).second == 12);
    BOOST_TEST(at_c<1>(t8).second > 5.4f && at_c<1>(t8).second < 5.6f);

    dummy
    (
        map<
            pair<key1, no_def_constructor>,
            pair<key2, no_def_constructor>,
            pair<key3, no_def_constructor> >
        (
            pair<key1, no_def_constructor>(std::string("Jaba")), // ok, since the default
            pair<key2, no_def_constructor>(std::string("Daba")), // constructor is not used
            pair<key3, no_def_constructor>(std::string("Doo"))
        )
    );

    dummy(map<pair<key1, int>, pair<key2, double> >());
    dummy(map<pair<key1, int>, pair<key2, double> >(1,3.14));

#if defined(FUSION_TEST_FAIL)
    dummy(map<pair<key1, double&> >());         // should fail, no defaults for references
    dummy(map<pair<key1, const double&> >());   // likewise
#endif

    {
        double dd = 5;
        dummy(map<pair<key1, double&> >(pair<key1, double&>(dd)));   // ok
        dummy(map<pair<key1, const double&> >(pair<key1, const double&>(dd+3.14))); // ok, but dangerous
    }

#if defined(FUSION_TEST_FAIL)
    dummy(map<pair<key1, double&> >(dd+3.14));  // should fail,
                                                // temporary to non-const reference
#endif
}

int
main()
{
    test();
    return boost::report_errors();
}
