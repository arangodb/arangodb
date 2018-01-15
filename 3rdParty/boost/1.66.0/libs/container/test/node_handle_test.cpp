//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2016-2016. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/core/lightweight_test.hpp>
#include <boost/static_assert.hpp>
#include <boost/container/node_handle.hpp>
#include <boost/container/new_allocator.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/move/adl_move_swap.hpp>
#include <boost/container/detail/pair_key_mapped_of_value.hpp>

using namespace ::boost::container;

enum EAllocState
{
   DefaultConstructed,
   MoveConstructed,
   MoveAssigned,
   CopyConstructed,
   CopyAssigned,
   Swapped,
   Destructed
};

template<class Node>
class trace_allocator
   : public new_allocator<Node>
{
   BOOST_COPYABLE_AND_MOVABLE(trace_allocator)

   typedef new_allocator<Node> base_t;

   public:

   struct propagate_on_container_move_assignment
   {
      static const bool value = true;
   };

   struct propagate_on_container_swap
   {
      static const bool value = true;
   };

   //!Obtains an new_allocator that allocates
   //!objects of type T2
   template<class T2>
   struct rebind
   {
      typedef trace_allocator<T2> other;
   };

   explicit trace_allocator(unsigned value = 999)
      : m_state(DefaultConstructed), m_value(value)
   {
      ++count;
   }

   trace_allocator(BOOST_RV_REF(trace_allocator) other)
      : base_t(boost::move(BOOST_MOVE_BASE(base_t, other))), m_state(MoveConstructed), m_value(other.m_value)
   {
      ++count;
   }

   trace_allocator(const trace_allocator &other)
      : base_t(other), m_state(CopyConstructed), m_value(other.m_value)
   {
      ++count;
   }

   trace_allocator & operator=(BOOST_RV_REF(trace_allocator) other)
   {
      m_value = other.m_value;
      m_state = MoveAssigned;
      return *this;
   }

   template<class OtherNode>
   trace_allocator(const trace_allocator<OtherNode> &other)
      : m_state(CopyConstructed), m_value(other.m_value)
   {
      ++count;
   }

   template<class OtherNode>
   trace_allocator & operator=(BOOST_COPY_ASSIGN_REF(trace_allocator<OtherNode>) other)
   {
      m_value = other.m_value;
      m_state = CopyAssigned;
      return *this;
   }

   ~trace_allocator()
   {
      m_value = 0u-1u;
      m_state = Destructed;
      --count;
   }

   void swap(trace_allocator &other)
   {
      boost::adl_move_swap(m_value, other.m_value);
      m_state = other.m_state = Swapped;
   }

   friend void swap(trace_allocator &left, trace_allocator &right)
   {
      left.swap(right);
   }

   EAllocState m_state;
   unsigned m_value;

   static unsigned int count;

   static void reset_count()
   {  count = 0;  }
};

template<class Node>
unsigned int trace_allocator<Node>::count = 0;

template<class T>
struct node
{
   typedef T value_type;
   value_type value;

         value_type &get_data()       {  return value; }
   const value_type &get_data() const {  return value; }

   node()
   {
      ++count;
   }

   ~node()
   {
      --count;
   }

   static unsigned int count;

   static void reset_count()
   {  count = 0;  }
};

template<class T1, class T2>
struct value
{
   T1 first;
   T2 second;
};

template<class T>
unsigned int node<T>::count = 0;


//Common types
typedef value<int, unsigned> test_pair;
typedef pair_key_mapped_of_value<int, unsigned> key_mapped_t;
typedef node<test_pair> node_t;
typedef trace_allocator< node_t > node_alloc_t;
typedef node_handle<node_alloc_t, void>         node_handle_set_t;
typedef node_handle<node_alloc_t, key_mapped_t> node_handle_map_t;
typedef allocator_traits<node_alloc_t>::portable_rebind_alloc<test_pair>::type value_allocator_type;

void test_types()
{
   //set
   BOOST_STATIC_ASSERT(( container_detail::is_same<node_handle_set_t::value_type, test_pair>::value ));
   BOOST_STATIC_ASSERT(( container_detail::is_same<node_handle_set_t::key_type, test_pair>::value ));
   BOOST_STATIC_ASSERT(( container_detail::is_same<node_handle_set_t::mapped_type, test_pair>::value ));
   BOOST_STATIC_ASSERT(( container_detail::is_same<node_handle_set_t::allocator_type, value_allocator_type>::value ));

   //map
   BOOST_STATIC_ASSERT(( container_detail::is_same<node_handle_map_t::value_type, test_pair>::value ));
   BOOST_STATIC_ASSERT(( container_detail::is_same<node_handle_map_t::key_type, int>::value ));
   BOOST_STATIC_ASSERT(( container_detail::is_same<node_handle_map_t::mapped_type, unsigned>::value ));
   BOOST_STATIC_ASSERT(( container_detail::is_same<node_handle_map_t::allocator_type, value_allocator_type>::value ));
}

void test_default_constructor()
{
   node_alloc_t::reset_count();
   {
      node_handle_set_t nh;
      BOOST_TEST(node_alloc_t::count == 0);
   }
   BOOST_TEST(node_alloc_t::count == 0);
}

void test_arg_constructor()
{
   //With non-null pointer
   node_alloc_t::reset_count();
   node_t::reset_count();
   {
      const node_alloc_t al;
      BOOST_TEST(node_alloc_t::count == 1);
      {
         node_handle_set_t nh(new node_t, al);
         BOOST_TEST(node_t::count == 1);
         BOOST_TEST(node_alloc_t::count == 2);
      }
      BOOST_TEST(node_alloc_t::count == 1);
   }
   BOOST_TEST(node_t::count == 0);
   BOOST_TEST(node_alloc_t::count == 0);

   //With null pointer
   node_alloc_t::reset_count();
   node_t::reset_count();
   {
      const node_alloc_t al;
      BOOST_TEST(node_alloc_t::count == 1);
      {
         node_handle_set_t nh(0, al);
         BOOST_TEST(node_t::count == 0);
         BOOST_TEST(node_alloc_t::count == 1);
      }
      BOOST_TEST(node_alloc_t::count == 1);
      BOOST_TEST(node_t::count == 0);
   }
   BOOST_TEST(node_alloc_t::count == 0);
}

void test_move_constructor()
{
   //With non-null pointer
   node_alloc_t::reset_count();
   node_t::reset_count();
   {
      const node_alloc_t al;
      BOOST_TEST(node_alloc_t::count == 1);
      {
         node_t *const from_ptr = new node_t;
         node_handle_set_t nh(from_ptr, al);
         BOOST_TEST(node_t::count == 1);
         BOOST_TEST(node_alloc_t::count == 2);
         {
            node_handle_set_t nh2(boost::move(nh));
            BOOST_TEST(nh.empty());
            BOOST_TEST(!nh2.empty());
            BOOST_TEST(nh2.get() == from_ptr);
            BOOST_TEST(nh2.node_alloc().m_state == MoveConstructed);
            BOOST_TEST(node_t::count == 1);
            BOOST_TEST(node_alloc_t::count == 2);
         }
         BOOST_TEST(node_t::count == 0);
         BOOST_TEST(node_alloc_t::count == 1);
      }
      BOOST_TEST(node_alloc_t::count == 1);
   }
   BOOST_TEST(node_t::count == 0);
   BOOST_TEST(node_alloc_t::count == 0);

   //With null pointer
   node_alloc_t::reset_count();
   node_t::reset_count();
   {
      const node_alloc_t al;
      BOOST_TEST(node_alloc_t::count == 1);
      {
         node_handle_set_t nh;
         {
            node_handle_set_t nh2(boost::move(nh));
            BOOST_TEST(nh.empty());
            BOOST_TEST(nh2.empty());
            BOOST_TEST(node_alloc_t::count == 1);
         }
         BOOST_TEST(node_t::count == 0);
         BOOST_TEST(node_alloc_t::count == 1);
      }
      BOOST_TEST(node_alloc_t::count == 1);
   }
   BOOST_TEST(node_t::count == 0);
   BOOST_TEST(node_alloc_t::count == 0);
}

void test_related_constructor()
{
   //With non-null pointer
   node_alloc_t::reset_count();
   node_t::reset_count();
   {
      const node_alloc_t al;
      BOOST_TEST(node_alloc_t::count == 1);
      {
         node_t *const from_ptr = new node_t;
         node_handle_map_t nh(from_ptr, al);
         BOOST_TEST(node_t::count == 1);
         BOOST_TEST(node_alloc_t::count == 2);
         {
            node_handle_set_t nh2(boost::move(nh));
            BOOST_TEST(nh.empty());
            BOOST_TEST(!nh2.empty());
            BOOST_TEST(nh2.get() == from_ptr);
            BOOST_TEST(nh2.node_alloc().m_state == MoveConstructed);
            BOOST_TEST(node_t::count == 1);
            BOOST_TEST(node_alloc_t::count == 2);
         }
         BOOST_TEST(node_t::count == 0);
         BOOST_TEST(node_alloc_t::count == 1);
      }
      BOOST_TEST(node_alloc_t::count == 1);
   }
   BOOST_TEST(node_t::count == 0);
   BOOST_TEST(node_alloc_t::count == 0);

   //With null pointer
   node_alloc_t::reset_count();
   node_t::reset_count();
   {
      const node_alloc_t al;
      BOOST_TEST(node_alloc_t::count == 1);
      {
         node_handle_set_t nh;
         {
            node_handle_map_t nh2(boost::move(nh));
            BOOST_TEST(nh.empty());
            BOOST_TEST(nh2.empty());
            BOOST_TEST(node_alloc_t::count == 1);
         }
         BOOST_TEST(node_t::count == 0);
         BOOST_TEST(node_alloc_t::count == 1);
      }
      BOOST_TEST(node_alloc_t::count == 1);
   }
   BOOST_TEST(node_t::count == 0);
   BOOST_TEST(node_alloc_t::count == 0);
}

void test_move_assignment()
{
   //empty = full
   {
      node_alloc_t::reset_count();
      node_t::reset_count();
      node_t *const from_ptr = new node_t;
      node_handle_set_t nh_from(from_ptr, node_alloc_t());
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);

      node_handle_set_t nh_to;
      BOOST_TEST(nh_to.empty());
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);

      nh_to = boost::move(nh_from);

      BOOST_TEST(nh_from.empty());
      BOOST_TEST(!nh_to.empty());
      BOOST_TEST(nh_to.get() == from_ptr);
      BOOST_TEST(nh_to.node_alloc().m_state == MoveConstructed);
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);
   }

   //empty = empty
   {
      node_alloc_t::reset_count();
      node_t::reset_count();

      node_handle_set_t nh_from;
      BOOST_TEST(nh_from.empty());
      BOOST_TEST(node_t::count == 0);
      BOOST_TEST(node_alloc_t::count == 0);

      node_handle_set_t nh_to;
      BOOST_TEST(nh_to.empty());
      BOOST_TEST(node_t::count == 0);
      BOOST_TEST(node_alloc_t::count == 0);

      nh_to = boost::move(nh_from);

      BOOST_TEST(nh_from.empty());
      BOOST_TEST(nh_to.empty());
      BOOST_TEST(node_t::count == 0);
      BOOST_TEST(node_alloc_t::count == 0);
   }

   //full = empty
   {
      node_alloc_t::reset_count();
      node_t::reset_count();

      node_handle_set_t nh_from;
      BOOST_TEST(nh_from.empty());
      BOOST_TEST(node_t::count == 0);
      BOOST_TEST(node_alloc_t::count == 0);

      node_handle_set_t nh_to(new node_t, node_alloc_t());
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);

      nh_to = boost::move(nh_from);

      BOOST_TEST(nh_from.empty());
      BOOST_TEST(nh_to.empty());
      BOOST_TEST(node_t::count == 0);
      BOOST_TEST(node_alloc_t::count == 0);
   }

   //full = full
   {
      node_alloc_t::reset_count();
      node_t::reset_count();

      node_t *const from_ptr = new node_t;
      node_handle_set_t nh_from(from_ptr, node_alloc_t());
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);

      node_handle_set_t nh_to(new node_t, node_alloc_t());
      BOOST_TEST(node_t::count == 2);
      BOOST_TEST(node_alloc_t::count == 2);

      nh_to = boost::move(nh_from);

      BOOST_TEST(nh_from.empty());
      BOOST_TEST(!nh_to.empty());
      BOOST_TEST(nh_to.get() == from_ptr);
      BOOST_TEST(nh_to.node_alloc().m_state == MoveAssigned);
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);
   }
}

void test_value_key_mapped()
{
   //value()
   {
      node_t *from_ptr = new node_t;
      const node_handle_set_t nh_from(from_ptr, node_alloc_t());
      from_ptr->value.first  = -99;
      from_ptr->value.second =  99;
      BOOST_TEST(nh_from.value().first  == -99);
      BOOST_TEST(nh_from.value().second ==  99);
   }
   //key()/mapped()
   {
      node_t *from_ptr = new node_t;
      const node_handle_map_t nh_from(from_ptr, node_alloc_t());
      from_ptr->value.first  = -98;
      from_ptr->value.second =  98;
      BOOST_TEST(nh_from.key()    == -98);
      BOOST_TEST(nh_from.mapped() ==  98);
   }
}

void test_get_allocator()
{
   const node_handle_set_t nh(new node_t, node_alloc_t(888));
   allocator_traits<node_alloc_t>::portable_rebind_alloc<test_pair>::type a = nh.get_allocator();
   BOOST_TEST(a.m_value == 888);
}

void test_bool_conversion_empty()
{
   const node_handle_set_t nh(new node_t, node_alloc_t(777));
   const node_handle_set_t nh_null;
   BOOST_TEST(nh && !nh_null);
   BOOST_TEST(!(!nh || nh_null));
   BOOST_TEST(!nh.empty() && nh_null.empty());
   BOOST_TEST(!(nh.empty() || !nh_null.empty()));
}

void test_swap()
{
   //empty.swap(full)
   {
      node_alloc_t::reset_count();
      node_t::reset_count();
      node_t *const from_ptr = new node_t;
      node_handle_set_t nh_from(from_ptr, node_alloc_t());
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);

      node_handle_set_t nh_to;
      BOOST_TEST(nh_to.empty());
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);

      nh_to.swap(nh_from);

      BOOST_TEST(nh_from.empty());
      BOOST_TEST(!nh_to.empty());
      BOOST_TEST(nh_to.get() == from_ptr);
      BOOST_TEST(nh_to.node_alloc().m_state == MoveConstructed);
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);
   }

   //empty.swap(empty)
   {
      node_alloc_t::reset_count();
      node_t::reset_count();

      node_handle_set_t nh_from;
      BOOST_TEST(nh_from.empty());
      BOOST_TEST(node_t::count == 0);
      BOOST_TEST(node_alloc_t::count == 0);

      node_handle_set_t nh_to;
      BOOST_TEST(nh_to.empty());
      BOOST_TEST(node_t::count == 0);
      BOOST_TEST(node_alloc_t::count == 0);

      nh_to.swap(nh_from);

      BOOST_TEST(nh_from.empty());
      BOOST_TEST(nh_to.empty());
      BOOST_TEST(node_t::count == 0);
      BOOST_TEST(node_alloc_t::count == 0);
   }

   //full.swap(empty)
   {
      node_alloc_t::reset_count();
      node_t::reset_count();

      node_handle_set_t nh_from;
      BOOST_TEST(nh_from.empty());
      BOOST_TEST(node_t::count == 0);
      BOOST_TEST(node_alloc_t::count == 0);

      node_t *const to_ptr = new node_t;
      node_handle_set_t nh_to(to_ptr, node_alloc_t());
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);

      nh_to.swap(nh_from);

      BOOST_TEST(!nh_from.empty());
      BOOST_TEST(nh_from.node_alloc().m_state == MoveConstructed);
      BOOST_TEST(nh_from.get() == to_ptr);
      BOOST_TEST(nh_to.empty());

      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);
   }

   //full.swap(full)
   {
      node_alloc_t::reset_count();
      node_t::reset_count();

      node_t *const from_ptr = new node_t;
      node_handle_set_t nh_from(from_ptr, node_alloc_t());
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);

      node_t *const to_ptr = new node_t;
      node_handle_set_t nh_to(to_ptr, node_alloc_t());
      BOOST_TEST(node_t::count == 2);
      BOOST_TEST(node_alloc_t::count == 2);

      nh_to.swap(nh_from);

      BOOST_TEST(!nh_from.empty());
      BOOST_TEST(nh_from.get() == to_ptr);
      BOOST_TEST(nh_from.node_alloc().m_state == Swapped);

      BOOST_TEST(!nh_to.empty());
      BOOST_TEST(nh_to.get() == from_ptr);
      BOOST_TEST(nh_to.node_alloc().m_state == Swapped);

      BOOST_TEST(node_t::count == 2);
      BOOST_TEST(node_alloc_t::count == 2);
   }
}

void test_get_release()
{
   //get()
   {
      node_alloc_t::reset_count();
      node_t::reset_count();

      node_t *const ptr = new node_t;
      const node_handle_set_t nh(ptr, node_alloc_t());
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);

      BOOST_TEST(nh.get() == ptr);
      BOOST_TEST(!nh.empty());
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);
   }
   BOOST_TEST(node_t::count == 0);
   BOOST_TEST(node_alloc_t::count == 0);

   //release()
   {
      node_alloc_t::reset_count();
      node_t::reset_count();

      node_t *const ptr = new node_t;
      node_handle_set_t nh(ptr, node_alloc_t());
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 1);

      BOOST_TEST(nh.release() == ptr);
      BOOST_TEST(nh.empty());
      BOOST_TEST(node_t::count == 1);
      BOOST_TEST(node_alloc_t::count == 0);
      delete ptr;
   }
   BOOST_TEST(node_t::count == 0);
}

int main()
{
   test_types();
   test_default_constructor();
   test_arg_constructor();
   test_move_constructor();
   test_related_constructor();
   test_move_assignment();
   test_value_key_mapped();
   test_get_allocator();
   test_bool_conversion_empty();
   test_swap();
   test_get_release();
   return ::boost::report_errors();
}
