/*=============================================================================
    Copyright (c) 2001-2009 Joel de Guzman

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
#include <boost/type_traits/is_same.hpp>
#include <iostream>
#include <string>

namespace ns
{
    class point
    {
    public:
    
        point() : x(0), y(0), z(0) {}
        point(int in_x, int in_y, int in_z) : x(in_x), y(in_y), z(in_z) {}
            
        int get_x() const { return x; }
        int get_y() const { return y; }
        int get_z() const { return z; }
        void set_x(int x_) { x = x_; }
        void set_y(int y_) { y = y_; }
        void set_z(int z_) { z = z_; }
        
    private:
        
        int x;
        int y;
        int z;
    };

#if !BOOST_WORKAROUND(__GNUC__,<4)
    class point_with_private_members
    {
        friend struct boost::fusion::extension::access;

    public:
        point_with_private_members() : x(0), y(0), z(0) {}
        point_with_private_members(int in_x, int in_y, int in_z) 
          : x(in_x), y(in_y), z(in_z) {}
            
        int get_x() const { return x; }
        int get_y() const { return y; }
        int get_z() const { return z; }
        void set_x(int x_) { x = x_; }
        void set_y(int y_) { y = y_; }
        void set_z(int z_) { z = z_; }
        
    private:
        
        int x;
        int y;
        int z;    
    };
#endif
    
    // A sequence that has data members defined in an unrelated namespace
    // (std, in this case). This allows testing ADL issues.
    class name
    {
    public:
        name() {}
        name(const std::string& last, const std::string& first)
           : last(last), first(first) {}
        
        const std::string& get_last() const { return last; }
        const std::string& get_first() const { return first; }
        void set_last(const std::string& last_) { last = last_; }
        void set_first(const std::string& first_) { first = first_; }
    private:
        std::string last;
        std::string first;
    };
}

#if BOOST_PP_VARIADICS
  BOOST_FUSION_ADAPT_ADT(
      ns::point,
      (int, int, obj.get_x(), obj.set_x(val))
      // Mixing auto & BOOST_FUSION_ADAPT_AUTO to test backward compatibility
      (auto, BOOST_FUSION_ADAPT_AUTO, obj.get_y(), obj.set_y(val))
      (obj.get_z(), obj.set_z(val))
  )

#   if !BOOST_WORKAROUND(__GNUC__,<4)
    BOOST_FUSION_ADAPT_ADT(
        ns::point_with_private_members,
        (obj.get_x(), obj.set_x(val))
        (obj.get_y(), obj.set_y(val))
        (obj.get_z(), obj.set_z(val))
    )
#   endif


  BOOST_FUSION_ADAPT_ADT(
      ns::name,
      (obj.get_last(), obj.set_last(val))
      (obj.get_first(), obj.set_first(val))
  )


#else // BOOST_PP_VARIADICS
  BOOST_FUSION_ADAPT_ADT(
      ns::point,
      (int, int, obj.get_x(), obj.set_x(val))
      (auto, auto, obj.get_y(), obj.set_y(val))
      (auto, auto, obj.get_z(), obj.set_z(val))
  )

#   if !BOOST_WORKAROUND(__GNUC__,<4)
    BOOST_FUSION_ADAPT_ADT(
        ns::point_with_private_members,
        (auto, auto, obj.get_x(), obj.set_x(val))
        (auto, auto, obj.get_y(), obj.set_y(val))
        (auto, auto, obj.get_z(), obj.set_z(val))
    )
#   endif

  BOOST_FUSION_ADAPT_ADT(
      ns::name,
      (const std::string&, const std::string&, obj.get_last(), obj.set_last(val))
      (BOOST_FUSION_ADAPT_AUTO, auto, obj.get_first(), obj.set_first(val))
  )

#endif

class empty_adt{};
BOOST_FUSION_ADAPT_ADT(empty_adt,)

int
main()
{
    using namespace boost::fusion;
    using namespace boost;

    std::cout << tuple_open('[');
    std::cout << tuple_close(']');
    std::cout << tuple_delimiter(", ");

    {
        BOOST_MPL_ASSERT_NOT((traits::is_view<ns::point>));
        BOOST_STATIC_ASSERT(!traits::is_view<ns::point>::value);
        ns::point p(123, 456, 789);

        std::cout << at_c<0>(p) << std::endl;
        std::cout << at_c<1>(p) << std::endl;
        std::cout << at_c<2>(p) << std::endl;
        std::cout << p << std::endl;
        BOOST_TEST(p == make_vector(123, 456, 789));

        at_c<0>(p) = 6;
        at_c<1>(p) = 9;
        at_c<2>(p) = 12;
        BOOST_TEST(p == make_vector(6, 9, 12));

        BOOST_STATIC_ASSERT(boost::fusion::result_of::size<ns::point>::value == 3);
        BOOST_STATIC_ASSERT(!boost::fusion::result_of::empty<ns::point>::value);

        BOOST_TEST(front(p) == 6);
        BOOST_TEST(back(p) == 12);
    }

    {
        fusion::vector<int, float, int> v1(4, 2.f, 2);
        ns::point v2(5, 3, 3);
        fusion::vector<long, double, int> v3(5, 4., 4);
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
        fusion::vector<std::string, std::string> v1("Lincoln", "Abraham");
        ns::name v2("Roosevelt", "Franklin");
        ns::name v3("Roosevelt", "Theodore");
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
        // conversion from ns::point to vector
        ns::point p(5, 3, 3);
        fusion::vector<int, long, int> v(p);
        v = p;
    }

    {
        // conversion from ns::point to list
        ns::point p(5, 3, 3);
        fusion::list<int, long, int> l(p);
        l = p;
    }

    {
        BOOST_MPL_ASSERT((mpl::is_sequence<ns::point>));
        BOOST_MPL_ASSERT((boost::is_same<
            boost::fusion::result_of::value_at_c<ns::point,0>::type
          , mpl::front<ns::point>::type>));
    }

#if !BOOST_WORKAROUND(__GNUC__,<4)
    {
        BOOST_MPL_ASSERT_NOT((traits::is_view<ns::point_with_private_members>));
        BOOST_STATIC_ASSERT(!traits::is_view<ns::point_with_private_members>::value);
        ns::point_with_private_members p(123, 456, 789);

        std::cout << at_c<0>(p) << std::endl;
        std::cout << at_c<1>(p) << std::endl;
        std::cout << at_c<2>(p) << std::endl;
        std::cout << p << std::endl;
        BOOST_TEST(p == make_vector(123, 456, 789));

        at_c<0>(p) = 6;
        at_c<1>(p) = 9;
        at_c<2>(p) = 12;
        BOOST_TEST(p == make_vector(6, 9, 12));

        BOOST_STATIC_ASSERT(boost::fusion::result_of::size<ns::point_with_private_members>::value == 3);
        BOOST_STATIC_ASSERT(!boost::fusion::result_of::empty<ns::point_with_private_members>::value);

        BOOST_TEST(front(p) == 6);
        BOOST_TEST(back(p) == 12);
    }
#endif

    {
        // Check types provided in case it's provided
        BOOST_MPL_ASSERT((
            boost::is_same<
                boost::fusion::result_of::front<ns::point>::type,
                boost::fusion::extension::adt_attribute_proxy<ns::point,0,false>
            >));
        BOOST_MPL_ASSERT((
            boost::is_same<
                boost::fusion::result_of::front<ns::point>::type::type,
                int
            >));
        BOOST_MPL_ASSERT((
            boost::is_same<
                boost::fusion::result_of::front<ns::point const>::type,
                boost::fusion::extension::adt_attribute_proxy<ns::point,0,true>
            >));
        BOOST_MPL_ASSERT((
            boost::is_same<
                boost::fusion::result_of::front<ns::point const>::type::type,
                int
            >));

        // Check types provided in case it's deduced
        BOOST_MPL_ASSERT((
            boost::is_same<
                boost::fusion::result_of::back<ns::point>::type,
                boost::fusion::extension::adt_attribute_proxy<ns::point,2,false>
            >));
        BOOST_MPL_ASSERT((
            boost::is_same<
                boost::fusion::result_of::back<ns::point>::type::type,
                int
            >));
        BOOST_MPL_ASSERT((
            boost::is_same<
                boost::fusion::result_of::back<ns::point const>::type,
                boost::fusion::extension::adt_attribute_proxy<ns::point,2,true>
            >));
        BOOST_MPL_ASSERT((
            boost::is_same<
                boost::fusion::result_of::back<ns::point const>::type::type,
                int
            >));
    }

    return boost::report_errors();
}

