//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision: 62023 $
//
//  Description : unit test for new assertion construction based on input expression
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MODULE Boost.Test assertion consruction test
#include <boost/test/unit_test.hpp>
#include <boost/test/tools/assertion.hpp>
#include <boost/test/utils/is_forward_iterable.hpp>

#include <boost/noncopyable.hpp>

#include <map>
#include <set>

namespace utf = boost::unit_test;

//____________________________________________________________________________//

#define EXPR_TYPE( expr ) ( assertion::seed() ->* expr )


#if !defined(BOOST_TEST_FWD_ITERABLE_CXX03)
// some broken compilers do not implement properly decltype on expressions
// partial implementation of is_forward_iterable when decltype not available
struct not_fwd_iterable_1 {
  typedef int const_iterator;
  typedef int value_type;

  bool size();
};

struct not_fwd_iterable_2 {
  typedef int const_iterator;
  typedef int value_type;

  bool begin();
};

struct not_fwd_iterable_3 {
  typedef int value_type;
  bool begin();
  bool size();
};

// this one does not have const_iterator, but should be forward iterable
struct fwd_iterable_4 {
  typedef int value_type;
  struct iterator {
    typedef unsigned int value_type;
  };
  iterator begin();
  iterator end();
  bool size();
};

struct fwd_iterable_custom {
  typedef std::vector<int>::const_iterator custom_iterator; // named "exotic" on purpose

  custom_iterator begin() const { return values.begin(); }
  custom_iterator end() const { return values.end(); }

#if !defined(BOOST_MSVC) || (BOOST_MSVC_FULL_VER > 180040629)
#define MY_TEST_HAS_INIT_LIST
  fwd_iterable_custom(std::initializer_list<int> ilist) : values{ilist}
  {}
#else
  fwd_iterable_custom(int v1, int v2, int v3) {
    values.push_back(v1);
    values.push_back(v2);
    values.push_back(v3);
  }
#endif
private:
  std::vector<int> values;
};

BOOST_AUTO_TEST_CASE( test_forward_iterable_concept )
{
  {
    typedef std::vector<int> type;
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_size<type>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_begin<type>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_end<type>::value, "has_member_end failed");
    BOOST_CHECK_MESSAGE(utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");
  }

  {
    // should also work for references, but from is_forward_iterable
    typedef std::vector<int>& type;
    BOOST_CHECK_MESSAGE(utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");
  }


  {
    typedef std::list<int> type;
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_size<type>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_begin<type>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_end<type>::value, "has_member_end failed");
    BOOST_CHECK_MESSAGE(utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");
  }

  {
    typedef std::map<int, int> type;
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_size<type>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_begin<type>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_end<type>::value, "has_member_end failed");
    BOOST_CHECK_MESSAGE(utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");
  }

  {
    typedef std::set<int> type;
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_size<type>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_begin<type>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_end<type>::value, "has_member_end failed");
    BOOST_CHECK_MESSAGE(utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");
  }


  {
    typedef float type;
    BOOST_CHECK_MESSAGE(!utf::ut_detail::has_member_size<type>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(!utf::ut_detail::has_member_begin<type>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(!utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(!utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");
  }

  {
    typedef not_fwd_iterable_1 type;
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_size<type>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(!utf::ut_detail::has_member_begin<type>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(!utf::ut_detail::has_member_end<type>::value, "has_member_end failed");
    BOOST_CHECK_MESSAGE(!utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(!utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");
  }



  {
    typedef not_fwd_iterable_2 type;
    BOOST_CHECK_MESSAGE(!utf::ut_detail::has_member_size<type>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_begin<type>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(!utf::ut_detail::has_member_end<type>::value, "has_member_end failed");
    BOOST_CHECK_MESSAGE(!utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(!utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");
  }

  {
    typedef not_fwd_iterable_3 type;
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_size<type>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_begin<type>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(!utf::ut_detail::has_member_end<type>::value, "has_member_end failed");
    BOOST_CHECK_MESSAGE(!utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(!utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");
  }

  {
    typedef fwd_iterable_4 type;
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_size<type>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_begin<type>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_end<type>::value, "has_member_end failed");
    BOOST_CHECK_MESSAGE(utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(!utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");
  }

  {
    // for this one we should be able to get the size
    typedef fwd_iterable_custom type;
    BOOST_CHECK_MESSAGE(!utf::ut_detail::has_member_size<type>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_begin<type>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_end<type>::value, "has_member_end failed");
    BOOST_CHECK_MESSAGE(utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(!utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");

#ifdef MY_TEST_HAS_INIT_LIST
    fwd_iterable_custom a{3,4,5};
#else
    fwd_iterable_custom a(3,4,5);
#endif
    BOOST_TEST( utf::bt_iterator_traits<fwd_iterable_custom>::size(a) == 3 );
  }
  {
    typedef char type;
    BOOST_CHECK_MESSAGE(!utf::ut_detail::has_member_size<type>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(!utf::ut_detail::has_member_begin<type>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(!utf::ut_detail::has_member_end<type>::value, "has_member_end failed");
    BOOST_CHECK_MESSAGE(!utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(!utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");
  }

  {
    // C-tables are in the forward_iterable concept, but are not containers
    typedef int type[10];
    BOOST_CHECK_MESSAGE(!utf::ut_detail::has_member_size<type>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_begin<type>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_end<type>::value, "has_member_end failed");
    BOOST_CHECK_MESSAGE(utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(!utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");
  }

  {
    // basic_cstring should be forward iterable and container
    typedef boost::unit_test::basic_cstring<char> type;
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_size<type>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_begin<type>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_end<type>::value, "has_member_end failed");
    BOOST_CHECK_MESSAGE(utf::is_forward_iterable< type >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(utf::is_container_forward_iterable< type >::value, "is_container_forward_iterable failed");

    typedef boost::unit_test::basic_cstring<const char> type2;
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_size<type2>::value, "has_member_size failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_begin<type2>::value, "has_member_begin failed");
    BOOST_CHECK_MESSAGE(utf::ut_detail::has_member_end<type2>::value, "has_member_end failed");
    BOOST_CHECK_MESSAGE(utf::is_forward_iterable< type2 >::value, "is_forward_iterable failed");
    BOOST_CHECK_MESSAGE(utf::is_container_forward_iterable< type2 >::value, "is_container_forward_iterable failed");
  }
}


//is_container_forward_iterable_impl
#endif

BOOST_AUTO_TEST_CASE( test_basic_value_expression_construction )
{
    using namespace boost::test_tools;

    {
        predicate_result const& res = EXPR_TYPE( 1 ).evaluate();
        BOOST_TEST( res );
        BOOST_TEST( res.message().is_empty() );
    }

    {
        predicate_result const& res = EXPR_TYPE( 0 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " ['0' evaluates to false]" );
    }

    {
        predicate_result const& res = EXPR_TYPE( true ).evaluate();
        BOOST_TEST( res );
        BOOST_TEST( res.message().is_empty() );
    }

    {
        predicate_result const& res = EXPR_TYPE( 1.5 ).evaluate();
        BOOST_TEST( res );
    }

    {
        predicate_result const& res = EXPR_TYPE( "abc" ).evaluate();
        BOOST_TEST( res );
    }

    {
        predicate_result const& res = EXPR_TYPE( 1>2 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [1 <= 2]" );
    }

}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_comparison_expression )
{
    using namespace boost::test_tools;

    {
        predicate_result const& res = EXPR_TYPE( 1>2 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [1 <= 2]" );
    }

    {
        predicate_result const& res = EXPR_TYPE( 100 < 50 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [100 >= 50]" );
    }

    {
        predicate_result const& res = EXPR_TYPE( 5 <= 4 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [5 > 4]" );
    }

    {
        predicate_result const& res = EXPR_TYPE( 10>=20 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [10 < 20]" );
    }

    {
        int i = 10;
        predicate_result const& res = EXPR_TYPE( i != 10 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [10 == 10]" );
    }

    {
        int i = 5;
        predicate_result const& res = EXPR_TYPE( i == 3 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [5 != 3]" );
    }
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_arithmetic_ops )
{
    using namespace boost::test_tools;

    {
        int i = 3;
        int j = 5;
        predicate_result const& res = EXPR_TYPE( i+j !=8 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [3 + 5 == 8]" );
    }

    {
        int i = 3;
        int j = 5;
        predicate_result const& res = EXPR_TYPE( 2*i-j > 1 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [2 * 3 - 5 <= 1]" );
    }

    {
        int j = 5;
        predicate_result const& res = EXPR_TYPE( 2<<j < 30 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [2 << 5 >= 30]" );
    }

    {
        int i = 2;
        int j = 5;
        predicate_result const& res = EXPR_TYPE( i&j ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [2 & 5]" );
    }

    {
        int i = 3;
        int j = 5;
        predicate_result const& res = EXPR_TYPE( i^j^6 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [3 ^ 5 ^ 6]" );
    }

    // do not support
    // EXPR_TYPE( 99/2 == 48 || 101/2 > 50 );
    // EXPR_TYPE( a ? 100 < 50 : 25*2 == 50 );
    // EXPR_TYPE( true,false );
}

//____________________________________________________________________________//

struct Testee {
    static int s_copy_counter;

    Testee() : m_value( false ) {}
    Testee( Testee const& ) : m_value(false) { s_copy_counter++; }
    Testee( Testee&& ) : m_value(false) {}
    Testee( Testee const&& ) : m_value(false) {}

    bool foo() { return m_value; }
    operator bool() const { return m_value; }

    friend std::ostream& operator<<( std::ostream& ostr, Testee const& ) { return ostr << "Testee"; }

    bool m_value;
};

int Testee::s_copy_counter = 0;

Testee          get_obj() { return Testee(); }
Testee const    get_const_obj() { return Testee(); }

class NC : boost::noncopyable {
public:
    NC() {}

    bool operator==(NC const&)  const { return false; }
    friend std::ostream& operator<<(std::ostream& ostr, NC const&)
    {
        return ostr << "NC";
    }
};

BOOST_AUTO_TEST_CASE( test_objects )
{
    using namespace boost::test_tools;

    int expected_copy_count = 0;

    {
        Testee obj;
        Testee::s_copy_counter = 0;

        predicate_result const& res = EXPR_TYPE( obj ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " ['Testee' evaluates to false]" );
        BOOST_TEST( Testee::s_copy_counter == expected_copy_count );
    }

    {
        Testee const obj;
        Testee::s_copy_counter = 0;

        predicate_result const& res = EXPR_TYPE( obj ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " ['Testee' evaluates to false]" );
        BOOST_TEST( Testee::s_copy_counter == expected_copy_count );
    }

    {
        Testee::s_copy_counter = 0;

        predicate_result const& res = EXPR_TYPE( get_obj() ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " ['Testee' evaluates to false]" );
        BOOST_TEST( Testee::s_copy_counter == expected_copy_count );
    }

    {
        Testee::s_copy_counter = 0;

        predicate_result const& res = EXPR_TYPE( get_const_obj() ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " ['Testee' evaluates to false]" );
        BOOST_TEST( Testee::s_copy_counter == expected_copy_count );
    }

    {
        Testee::s_copy_counter = 0;

        Testee t1;
        Testee t2;

        predicate_result const& res = EXPR_TYPE( t1 != t2 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [Testee == Testee]" );
        BOOST_TEST( Testee::s_copy_counter == 0 );
    }

    {
        NC nc1;
        NC nc2;

        predicate_result const& res = EXPR_TYPE( nc1 == nc2 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [NC != NC]" );
    }
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_pointers )
{
    using namespace boost::test_tools;

    {
        Testee* ptr = 0;

        predicate_result const& res = EXPR_TYPE( ptr ).evaluate();
        BOOST_TEST( !res );
    }

    {
        Testee obj1;
        Testee obj2;

        predicate_result const& res = EXPR_TYPE( &obj1 == &obj2 ).evaluate();
        BOOST_TEST( !res );
    }

    {
        Testee obj;
        Testee* ptr =&obj;

        predicate_result const& res = EXPR_TYPE( *ptr ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " ['Testee' evaluates to false]" );
    }

    {
        Testee obj;
        Testee* ptr =&obj;
        bool Testee::* mem_ptr =&Testee::m_value;

        predicate_result const& res = EXPR_TYPE( ptr->*mem_ptr ).evaluate();
        BOOST_TEST( !res );
    }

    // do not support
    // Testee obj;
    // bool Testee::* mem_ptr =&Testee::m_value;
    // EXPR_TYPE( obj.*mem_ptr );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_mutating_ops )
{
    using namespace boost::test_tools;

    {
        int j = 5;

        predicate_result const& res = EXPR_TYPE( j = 0 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " ['0' evaluates to false]" );
        BOOST_TEST( j == 0 );
    }

    {
        int j = 5;

        predicate_result const& res = EXPR_TYPE( j -= 5 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " ['0' evaluates to false]" );
        BOOST_TEST( j == 0 );
    }

    {
        int j = 5;

        predicate_result const& res = EXPR_TYPE( j *= 0 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " ['0' evaluates to false]" );
        BOOST_TEST( j == 0 );
    }

    {
        int j = 5;

        predicate_result const& res = EXPR_TYPE( j /= 10 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " ['0' evaluates to false]" );
        BOOST_TEST( j == 0 );
    }

    {
        int j = 4;

        predicate_result const& res = EXPR_TYPE( j %= 2 ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " ['0' evaluates to false]" );
        BOOST_TEST( j == 0 );
    }

    {
        int j = 5;

        predicate_result const& res = EXPR_TYPE( j ^= j ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " ['0' evaluates to false]" );
        BOOST_TEST( j == 0 );
   }
}

BOOST_AUTO_TEST_CASE( test_specialized_comparator_string )
{
    using namespace boost::test_tools;

    {
        std::string s("abc");

        predicate_result const& res = EXPR_TYPE( s == "a" ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [abc != a]" );
        BOOST_TEST( s == "abc" );
    }

    {
        predicate_result const& res = EXPR_TYPE( std::string("abc") == "a" ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [abc != a]" );
    }

    {
        predicate_result const& res = EXPR_TYPE( "abc" == std::string("a") ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [abc != a]" );
    }

    {
        predicate_result const& res = EXPR_TYPE( std::string("abc") == std::string("a") ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [abc != a]" );
    }
}

BOOST_AUTO_TEST_CASE( test_comparison_with_arrays )
{
    using namespace boost::test_tools;

    {
        char c_string_array[] = "abc";

        predicate_result const& res = EXPR_TYPE( c_string_array == "a" ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [abc != a]" );
        BOOST_TEST( c_string_array == "abc" );
    }

    {
        char c_string_array[] = "abc";

        predicate_result const& res = EXPR_TYPE( "a" == c_string_array ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [a != abc]" );
        BOOST_TEST( "abc" == c_string_array );
    }

    {
        char const c_string_array[] = "abc";

        predicate_result const& res = EXPR_TYPE( c_string_array == "a" ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [abc != a]" );
        BOOST_TEST( c_string_array == "abc" );
    }

    {
        char const c_string_array[] = "abc";

        predicate_result const& res = EXPR_TYPE( "a" == c_string_array ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == " [a != abc]" );
        BOOST_TEST( "abc" == c_string_array );
    }


    {
        long int c_long_array[] = {3,4,7};
        std::vector<long int> v_long_array(c_long_array, c_long_array + sizeof(c_long_array)/sizeof(c_long_array[0]));
        std::swap(v_long_array[1], v_long_array[2]);

        predicate_result const& res = EXPR_TYPE( c_long_array == v_long_array ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == ": \n  - mismatch at position 1: [4 == 7] is false\n  - mismatch at position 2: [7 == 4] is false" );

        std::swap(v_long_array[1], v_long_array[2]);
        BOOST_TEST( c_long_array == v_long_array );
    }

    {
        long int c_long_array[] = {3,4,7};

        std::vector<long int> v_long_array(c_long_array, c_long_array + sizeof(c_long_array)/sizeof(c_long_array[0]));
        std::swap(v_long_array[1], v_long_array[2]);

        predicate_result const& res = EXPR_TYPE( v_long_array == c_long_array ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == ": \n  - mismatch at position 1: [7 == 4] is false\n  - mismatch at position 2: [4 == 7] is false" );
        std::swap(v_long_array[1], v_long_array[2]);
        BOOST_TEST( c_long_array == v_long_array );
    }

    {
        long int const c_long_array[] = {3,4,7};
        std::vector<long int> v_long_array(c_long_array, c_long_array + sizeof(c_long_array)/sizeof(c_long_array[0]));
        std::swap(v_long_array[1], v_long_array[2]);

        predicate_result const& res = EXPR_TYPE( c_long_array == v_long_array ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == ": \n  - mismatch at position 1: [4 == 7] is false\n  - mismatch at position 2: [7 == 4] is false" );

        std::swap(v_long_array[1], v_long_array[2]);
        BOOST_TEST( c_long_array == v_long_array );
    }

    {
        long int const c_long_array[] = {3,4,7};

        std::vector<long int> v_long_array(c_long_array, c_long_array + sizeof(c_long_array)/sizeof(c_long_array[0]));
        std::swap(v_long_array[1], v_long_array[2]);

        predicate_result const& res = EXPR_TYPE( v_long_array == c_long_array ).evaluate();
        BOOST_TEST( !res );
        BOOST_TEST( res.message() == ": \n  - mismatch at position 1: [7 == 4] is false\n  - mismatch at position 2: [4 == 7] is false" );
        std::swap(v_long_array[1], v_long_array[2]);
        BOOST_TEST( c_long_array == v_long_array );
    }

}

// EOF
