/*=============================================================================
    Copyright (c) 2010 Christopher Schmidt

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/adapted/adt/adapt_adt.hpp>
#include <boost/fusion/sequence/intrinsic/at.hpp>
#include <boost/fusion/sequence/intrinsic/size.hpp>
#include <boost/fusion/sequence/intrinsic/empty.hpp>
#include <boost/fusion/sequence/intrinsic/front.hpp>
#include <boost/fusion/sequence/intrinsic/back.hpp>
#include <boost/fusion/sequence/intrinsic/value_at.hpp>
#include <boost/fusion/sequence/io/out.hpp>
#include <boost/fusion/container/vector/vector.hpp>
#include <boost/fusion/container/list/list.hpp>
#include <boost/fusion/container/generation/make_vector.hpp>
#include <boost/fusion/container/vector/convert.hpp>
#include <boost/fusion/sequence/comparison/equal_to.hpp>
#include <boost/fusion/sequence/comparison/not_equal_to.hpp>
#include <boost/fusion/sequence/comparison/less.hpp>
#include <boost/fusion/sequence/comparison/less_equal.hpp>
#include <boost/fusion/sequence/comparison/greater.hpp>
#include <boost/fusion/sequence/comparison/greater_equal.hpp>
#include <boost/fusion/mpl.hpp>
#include <boost/fusion/support/is_view.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/static_assert.hpp>
#include <iostream>
#include <string>

namespace ns
{
    template<typename X, typename Y>
    class point
    {
    public:
    
        point() : x(0), y(0), z(0) {}
        point(X x_, Y y_, int z_) : x(x_), y(y_), z(z_) {}
            
        X get_x() const { return x; }
        Y get_y() const { return y; }
        int get_z() const { return z; }
        void set_x(X x_) { x = x_; }
        void set_y(Y y_) { y = y_; }
        void set_z(int z_) { z = z_; }
        
    private:
        
        X x;
        Y y;
        int z;
    };
}


#if BOOST_PP_VARIADICS

  BOOST_FUSION_ADAPT_TPL_ADT(
      (X)(Y),
      (ns::point)(X)(Y),
      (X, X, obj.get_x(), obj.set_x(val))
      (Y, Y, obj.get_y(), obj.set_y(val))
      (obj.get_z(), obj.set_z(val))
  )

#else // BOOST_PP_VARIADICS

  BOOST_FUSION_ADAPT_TPL_ADT(
      (X)(Y),
      (ns::point)(X)(Y),
      (X, X, obj.get_x(), obj.set_x(val))
      (Y, Y, obj.get_y(), obj.set_y(val))
      (auto, auto, obj.get_z(), obj.set_z(val))
  )
#endif

template <typename TypeToConstruct>
class empty_adt_templated_factory {

  TypeToConstruct operator()() {
    return TypeToConstruct();
  }

};

BOOST_FUSION_ADAPT_TPL_ADT(
    (TypeToConstruct),
    (empty_adt_templated_factory)(TypeToConstruct),
) 

int
main()
{
    using namespace boost::fusion;

    typedef ns::point<int, int> point;
    typedef ns::point<std::string, std::string> name;

    std::cout << tuple_open('[');
    std::cout << tuple_close(']');
    std::cout << tuple_delimiter(", ");

    {
        BOOST_MPL_ASSERT_NOT((traits::is_view<point>));
        BOOST_STATIC_ASSERT(!traits::is_view<point>::value);
        point p(123, 456, 789);

        std::cout << at_c<0>(p) << std::endl;
        std::cout << at_c<1>(p) << std::endl;
        std::cout << at_c<2>(p) << std::endl;
        std::cout << p << std::endl;
        BOOST_TEST(p == make_vector(123, 456, 789));

        at_c<0>(p) = 6;
        at_c<1>(p) = 9;
        at_c<2>(p) = 12;
        BOOST_TEST(p == make_vector(6, 9, 12));

        BOOST_STATIC_ASSERT(boost::fusion::result_of::size<point>::value == 3);
        BOOST_STATIC_ASSERT(!boost::fusion::result_of::empty<point>::value);

        BOOST_TEST(front(p) == 6);
        BOOST_TEST(back(p) == 12);
    }

    {
        boost::fusion::vector<int, float, int> v1(4, 2.f, 2);
        point v2(5, 3, 3);
        boost::fusion::vector<long, double, int> v3(5, 4., 4);
        BOOST_TEST(v1 < v2);
        BOOST_TEST(v1 <= v2);
        BOOST_TEST(v2 > v1);
        BOOST_TEST(v2 >= v1);
        BOOST_TEST(v2 < v3);
        BOOST_TEST(v2 <= v3);
        BOOST_TEST(v3 > v2);
        BOOST_TEST(v3 >= v2);
    }

    {
        boost::fusion::vector<std::string, std::string, int> v1("Lincoln", "Abraham", 3);
        name v2("Roosevelt", "Franklin", 3);
        name v3("Roosevelt", "Theodore", 3);
        BOOST_TEST(v1 < v2);
        BOOST_TEST(v1 <= v2);
        BOOST_TEST(v2 > v1);
        BOOST_TEST(v2 >= v1);
        BOOST_TEST(v2 < v3);
        BOOST_TEST(v2 <= v3);
        BOOST_TEST(v3 > v2);
        BOOST_TEST(v3 >= v2);
    }
    
    {
        // conversion from point to vector
        point p(5, 3, 3);
        boost::fusion::vector<int, long, int> v(p);
        v = p;
    }

    {
        // conversion from point to list
        point p(5, 3, 3);
        boost::fusion::list<int, long, int> l(p);
        l = p;
    }

    {
        BOOST_MPL_ASSERT((boost::mpl::is_sequence<point>));
        BOOST_MPL_ASSERT((boost::is_same<
            boost::fusion::result_of::value_at_c<point,0>::type
          , boost::mpl::front<point>::type>));
    }

    return boost::report_errors();
}

