//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/move/detail/type_traits.hpp>
#include <boost/move/core.hpp>
#include <boost/static_assert.hpp>
#include <boost/core/lightweight_test.hpp>

//
//       pod_struct
//
#if defined(BOOST_MOVE_IS_POD)
struct pod_struct
{
   int i;
   float f;
};
#endif

//
//       deleted_copy_and_assign_type
//
#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)

struct deleted_copy_and_assign_type
{
   deleted_copy_and_assign_type(const deleted_copy_and_assign_type&) = delete;
   deleted_copy_and_assign_type & operator=(const deleted_copy_and_assign_type&) = delete;
};

#endif   //defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)

//
//       boost_move_type
//
class boost_move_type
{
   BOOST_MOVABLE_BUT_NOT_COPYABLE(boost_move_type)
   public:
   boost_move_type(BOOST_RV_REF(boost_move_type)){}
   boost_move_type & operator=(BOOST_RV_REF(boost_move_type)){ return *this; }
};

namespace is_pod_test
{

void test()
{
   BOOST_STATIC_ASSERT((boost::move_detail::is_pod<int>::value));
   #if defined(BOOST_MOVE_IS_POD)
   BOOST_STATIC_ASSERT((boost::move_detail::is_pod<pod_struct>::value));
   #endif
}

}  //namespace is_pod_test

namespace trivially_memcopyable_test {

void test()
{
   #if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
   BOOST_STATIC_ASSERT(!(boost::move_detail::is_trivially_copy_constructible<deleted_copy_and_assign_type>::value));
   BOOST_STATIC_ASSERT(!(boost::move_detail::is_trivially_copy_assignable<deleted_copy_and_assign_type>::value));
   #endif   //#if !defined(BOOST_NO_CXX11_DELETED_FUNCTIONS)
   //boost_move_type
   BOOST_STATIC_ASSERT(!(boost::move_detail::is_trivially_copy_constructible<boost_move_type>::value));
   BOOST_STATIC_ASSERT(!(boost::move_detail::is_trivially_copy_assignable<boost_move_type>::value));
   BOOST_STATIC_ASSERT(!(boost::move_detail::is_copy_constructible<boost_move_type>::value));
   BOOST_STATIC_ASSERT(!(boost::move_detail::is_copy_assignable<boost_move_type>::value));
   //POD
   BOOST_STATIC_ASSERT((boost::move_detail::is_trivially_copy_constructible<int>::value));
   BOOST_STATIC_ASSERT((boost::move_detail::is_trivially_copy_assignable<int>::value));
   BOOST_STATIC_ASSERT((boost::move_detail::is_copy_constructible<int>::value));
   BOOST_STATIC_ASSERT((boost::move_detail::is_copy_assignable<int>::value));
   #if defined(BOOST_MOVE_IS_POD)
   BOOST_STATIC_ASSERT((boost::move_detail::is_trivially_copy_constructible<pod_struct>::value));
   BOOST_STATIC_ASSERT((boost::move_detail::is_trivially_copy_assignable<pod_struct>::value));
   BOOST_STATIC_ASSERT((boost::move_detail::is_copy_constructible<pod_struct>::value));
   BOOST_STATIC_ASSERT((boost::move_detail::is_copy_assignable<pod_struct>::value));
   #endif
}

}  //namespace trivially_memcopyable_test {

int main()
{
   trivially_memcopyable_test::test();
   is_pod_test::test();
   boost::report_errors();
}
