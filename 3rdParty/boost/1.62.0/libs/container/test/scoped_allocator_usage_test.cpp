//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2011-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/detail/config_begin.hpp>
#include <memory>

#include <boost/move/utility_core.hpp>
#include <boost/container/vector.hpp>
#include <boost/container/deque.hpp>
#include <boost/container/list.hpp>
#include <boost/container/slist.hpp>
#include <boost/container/stable_vector.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/map.hpp>
#include <boost/container/set.hpp>
#include <boost/container/detail/mpl.hpp>

#include <boost/container/scoped_allocator.hpp>

template <typename Ty>
class SimpleAllocator
{
public:
   typedef Ty value_type;

   explicit SimpleAllocator(int value)
      : m_state(value)
   {}

   template <typename T>
   SimpleAllocator(const SimpleAllocator<T> &other)
      : m_state(other.m_state)
   {}

   Ty* allocate(std::size_t n)
   {
      return m_allocator.allocate(n);
   }

   void deallocate(Ty* p, std::size_t n)
   {
      m_allocator.deallocate(p, n);
   }

   int get_value() const
   {  return m_state;   }

   private:
   int m_state;
   std::allocator<Ty> m_allocator;

   template <typename T> friend class SimpleAllocator;

   friend bool operator == (const SimpleAllocator &, const SimpleAllocator &)
   {  return true;  }

   friend bool operator != (const SimpleAllocator &, const SimpleAllocator &)
   {  return false;  }
};

class alloc_int
{
   private: // Not copyable

   BOOST_MOVABLE_BUT_NOT_COPYABLE(alloc_int)

   public:
   typedef SimpleAllocator<int> allocator_type;

   alloc_int(BOOST_RV_REF(alloc_int)other)
      : m_value(other.m_value), m_allocator(boost::move(other.m_allocator))
   {
      other.m_value = -1;
   }

   alloc_int(BOOST_RV_REF(alloc_int)other, const allocator_type &allocator)
      : m_value(other.m_value), m_allocator(allocator)
   {
      other.m_value = -1;
   }

   alloc_int(int value, const allocator_type &allocator)
      : m_value(value), m_allocator(allocator)
   {}

   alloc_int & operator=(BOOST_RV_REF(alloc_int)other)
   {
      other.m_value = other.m_value;
      return *this;
   }

   int get_allocator_state() const
   {  return m_allocator.get_value();  }

   int get_value() const
   {  return m_value;   }

   friend bool operator < (const alloc_int &l, const alloc_int &r)
   {  return l.m_value < r.m_value;  }

   friend bool operator == (const alloc_int &l, const alloc_int &r)
   {  return l.m_value == r.m_value;  }

   private:
   int m_value;
   allocator_type m_allocator;
};

using namespace ::boost::container;

//general allocator
typedef scoped_allocator_adaptor<SimpleAllocator<alloc_int> > AllocIntAllocator;

//[multi]map/set
typedef std::pair<const alloc_int, alloc_int> MapNode;
typedef scoped_allocator_adaptor<SimpleAllocator<MapNode> > MapAllocator;
typedef map<alloc_int, alloc_int, std::less<alloc_int>, MapAllocator> Map;
typedef set<alloc_int, std::less<alloc_int>, AllocIntAllocator> Set;
typedef multimap<alloc_int, alloc_int, std::less<alloc_int>, MapAllocator> MultiMap;
typedef multiset<alloc_int, std::less<alloc_int>, AllocIntAllocator> MultiSet;

//[multi]flat_map/set
typedef std::pair<alloc_int, alloc_int> FlatMapNode;
typedef scoped_allocator_adaptor<SimpleAllocator<FlatMapNode> > FlatMapAllocator;
typedef flat_map<alloc_int, alloc_int, std::less<alloc_int>, FlatMapAllocator> FlatMap;
typedef flat_set<alloc_int, std::less<alloc_int>, AllocIntAllocator> FlatSet;
typedef flat_multimap<alloc_int, alloc_int, std::less<alloc_int>, FlatMapAllocator> FlatMultiMap;
typedef flat_multiset<alloc_int, std::less<alloc_int>, AllocIntAllocator> FlatMultiSet;

//vector, deque, list, slist, stable_vector.
typedef vector<alloc_int, AllocIntAllocator>          Vector;
typedef deque<alloc_int, AllocIntAllocator>           Deque;
typedef list<alloc_int, AllocIntAllocator>            List;
typedef slist<alloc_int, AllocIntAllocator>           Slist;
typedef stable_vector<alloc_int, AllocIntAllocator>   StableVector;
typedef small_vector<alloc_int, 9, AllocIntAllocator> SmallVector;

/////////
//is_unique_assoc
/////////

template<class T>
struct is_unique_assoc
{
   static const bool value = false;
};

template<class Key, class T, class Compare, class Allocator>
struct is_unique_assoc< map<Key, T, Compare, Allocator> >
{
   static const bool value = true;
};

template<class Key, class T, class Compare, class Allocator>
struct is_unique_assoc< flat_map<Key, T, Compare, Allocator> >
{
   static const bool value = true;
};

template<class Key, class Compare, class Allocator>
struct is_unique_assoc< set<Key, Compare, Allocator> >
{
   static const bool value = true;
};

template<class Key, class Compare, class Allocator>
struct is_unique_assoc< flat_set<Key, Compare, Allocator> >
{
   static const bool value = true;
};


/////////
//is_map
/////////

template<class T>
struct is_map
{
   static const bool value = false;
};

template<class Key, class T, class Compare, class Allocator>
struct is_map< map<Key, T, Compare, Allocator> >
{
   static const bool value = true;
};

template<class Key, class T, class Compare, class Allocator>
struct is_map< flat_map<Key, T, Compare, Allocator> >
{
   static const bool value = true;
};

template<class Key, class T, class Compare, class Allocator>
struct is_map< multimap<Key, T, Compare, Allocator> >
{
   static const bool value = true;
};

template<class Key, class T, class Compare, class Allocator>
struct is_map< flat_multimap<Key, T, Compare, Allocator> >
{
   static const bool value = true;
};

template<class T>
struct is_set
{
   static const bool value = false;
};

template<class Key, class Compare, class Allocator>
struct is_set< set<Key, Compare, Allocator> >
{
   static const bool value = true;
};

template<class Key, class Compare, class Allocator>
struct is_set< flat_set<Key, Compare, Allocator> >
{
   static const bool value = true;
};

template<class Key, class Compare, class Allocator>
struct is_set< multiset<Key, Compare, Allocator> >
{
   static const bool value = true;
};

template<class Key, class Compare, class Allocator>
struct is_set< flat_multiset<Key, Compare, Allocator> >
{
   static const bool value = true;
};

/////////
//container_wrapper
/////////

//Try to define-allocator_aware requirements
template< class Container
        , bool Assoc = is_set<Container>::value || is_map<Container>::value
        , bool UniqueAssoc = is_unique_assoc<Container>::value
        , bool Map  = is_map<Container>::value
        >
struct container_wrapper_inserter
{
   typedef typename Container::const_iterator   const_iterator;
   typedef typename Container::iterator         iterator;

   template<class Arg>
   static iterator emplace(Container &c, const_iterator p, const Arg &arg)
   {  return c.emplace(p, arg);   }
};

template<class Container>  //map
struct container_wrapper_inserter<Container, true, true, true>
{
   typedef typename Container::const_iterator   const_iterator;
   typedef typename Container::iterator         iterator;

   template<class Arg>
   static iterator emplace(Container &c, const_iterator, const Arg &arg)
   {  return c.emplace(arg, arg).first;   }
};

template<class Container>  //set
struct container_wrapper_inserter<Container, true, true, false>
{
   typedef typename Container::const_iterator   const_iterator;
   typedef typename Container::iterator         iterator;

   template<class Arg>
   static iterator emplace(Container &c, const_iterator, const Arg &arg)
   {  return c.emplace(arg).first;  }
};

template<class Container>  //multimap
struct container_wrapper_inserter<Container, true, false, true>
{
   typedef typename Container::const_iterator   const_iterator;
   typedef typename Container::iterator         iterator;

   template<class Arg>
   static iterator emplace(Container &c, const_iterator, const Arg &arg)
   {  return c.emplace(arg, arg);   }
};

//multiset
template<class Container>  //multimap
struct container_wrapper_inserter<Container, true, false, false>
{
   typedef typename Container::const_iterator   const_iterator;
   typedef typename Container::iterator         iterator;

   template<class Arg>
   static iterator emplace(Container &c, const_iterator, const Arg &arg)
   {  return c.emplace(arg);  }
};

template< class Container>
struct container_wrapper
   : public Container
{
   private:
   BOOST_COPYABLE_AND_MOVABLE(container_wrapper)

   public:
   typedef typename Container::allocator_type   allocator_type;
   typedef typename Container::const_iterator   const_iterator;
   typedef typename Container::iterator         iterator;

   container_wrapper(const allocator_type &a)
      : Container(a)
   {}

   container_wrapper(BOOST_RV_REF(container_wrapper) o, const allocator_type &a)
      : Container(BOOST_MOVE_BASE(Container, o), a)
   {}

   container_wrapper(const container_wrapper &o, const allocator_type &a)
      : Container(o, a)
   {}

   template<class Arg>
   iterator emplace(const_iterator p, const Arg &arg)
   {  return container_wrapper_inserter<Container>::emplace(*this, p, arg);  }
};


bool test_value_and_state_equals(const alloc_int &r, int value, int state)
{  return r.get_value() == value && r.get_allocator_state() == state;  }

template<class F, class S>
bool test_value_and_state_equals(const container_detail::pair<F, S> &p, int value, int state)
{  return test_value_and_state_equals(p.first, value, state) && test_alloc_state_equals(p.second, value, state);  }

template<class F, class S>
bool test_value_and_state_equals(const std::pair<F, S> &p, int value, int state)
{  return test_value_and_state_equals(p.first, value, state) && test_value_and_state_equals(p.second, value, state);  }

template<class Container>
bool one_level_allocator_propagation_test()
{
   typedef container_wrapper<Container> ContainerWrapper;
   typedef typename ContainerWrapper::iterator iterator;
   typedef typename ContainerWrapper::allocator_type allocator_type;
   typedef typename ContainerWrapper::value_type value_type;
   {
      allocator_type al(SimpleAllocator<value_type>(5));
      ContainerWrapper c(al);

      c.clear();
      iterator it = c.emplace(c.cbegin(), 42);

      if(!test_value_and_state_equals(*it, 42, 5))
         return false;
   }
   {
      allocator_type al(SimpleAllocator<value_type>(4));
      ContainerWrapper c2(al);
      ContainerWrapper c(::boost::move(c2), allocator_type(SimpleAllocator<value_type>(5)));

      c.clear();
      iterator it = c.emplace(c.cbegin(), 42);

      if(!test_value_and_state_equals(*it, 42, 5))
         return false;
   }/*
   {
      ContainerWrapper c2(allocator_type(SimpleAllocator<value_type>(3)));
      ContainerWrapper c(c2, allocator_type(SimpleAllocator<value_type>(5)));

      c.clear();
      iterator it = c.emplace(c.cbegin(), 42);

      if(!test_value_and_state_equals(*it, 42, 5))
         return false;
   }*/
   return true;
}

int main()
{
   //unique assoc
   if(!one_level_allocator_propagation_test<FlatMap>())
      return 1;
   if(!one_level_allocator_propagation_test<Map>())
      return 1;
   if(!one_level_allocator_propagation_test<FlatSet>())
      return 1;
   if(!one_level_allocator_propagation_test<Set>())
      return 1;
   //multi assoc
   if(!one_level_allocator_propagation_test<FlatMultiMap>())
      return 1;
   if(!one_level_allocator_propagation_test<MultiMap>())
      return 1;
   if(!one_level_allocator_propagation_test<FlatMultiSet>())
      return 1;
   if(!one_level_allocator_propagation_test<MultiSet>())
      return 1;
   //sequence containers
   if(!one_level_allocator_propagation_test<Vector>())
      return 1;
   if(!one_level_allocator_propagation_test<Deque>())
      return 1;
   if(!one_level_allocator_propagation_test<List>())
      return 1;
   if(!one_level_allocator_propagation_test<Slist>())
      return 1;
   if(!one_level_allocator_propagation_test<StableVector>())
      return 1;
   if(!one_level_allocator_propagation_test<SmallVector>())
      return 1;
   return 0;
}

#include <boost/container/detail/config_end.hpp>
