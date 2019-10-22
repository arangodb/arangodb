//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2014. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/bs_set.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/avl_set.hpp>
#include <boost/intrusive/sg_set.hpp>
#include <boost/intrusive/treap_set.hpp>
#include <boost/intrusive/splay_set.hpp>
#include <boost/intrusive/detail/mpl.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/aligned_storage.hpp>
#include <boost/static_assert.hpp>
#include <cstring>
#include <new>

using namespace boost::intrusive;

struct Type
   : list_base_hook<>
   , slist_base_hook<>
   , set_base_hook<>
   , avl_set_base_hook<>
   , bs_set_base_hook<>
{};

typedef boost::aligned_storage<sizeof(void*)*4>::type buffer_t;

static buffer_t buffer_0x00;
static buffer_t buffer_0xFF;

template<class Iterator>
const Iterator &on_0x00_buffer()
{
   BOOST_STATIC_ASSERT(sizeof(buffer_t) >= sizeof(Iterator));
   return * ::new(std::memset(&buffer_0x00, 0x00, sizeof(buffer_0x00))) Iterator();
}

template<class Iterator>
const Iterator &on_0xFF_buffer()
{
   BOOST_STATIC_ASSERT(sizeof(buffer_t) >= sizeof(Iterator));
   return * ::new(std::memset(&buffer_0xFF, 0xFF, sizeof(buffer_0xFF))) Iterator();
}

BOOST_INTRUSIVE_INSTANTIATE_DEFAULT_TYPE_TMPLT(reverse_iterator)
BOOST_INTRUSIVE_INSTANTIATE_DEFAULT_TYPE_TMPLT(const_reverse_iterator)


template<class Container>
void check_null_iterators()
{
   typedef typename Container::iterator               iterator;
   typedef typename Container::const_iterator         const_iterator;
   typedef BOOST_INTRUSIVE_OBTAIN_TYPE_WITH_DEFAULT
      (::, Container
      ,reverse_iterator, iterator)                    reverse_iterator;
   typedef BOOST_INTRUSIVE_OBTAIN_TYPE_WITH_DEFAULT
      (::, Container
      ,const_reverse_iterator, const_iterator)        const_reverse_iterator;

   BOOST_TEST(on_0xFF_buffer<iterator>()               == on_0x00_buffer<iterator>());
   BOOST_TEST(on_0xFF_buffer<const_iterator>()         == on_0x00_buffer<const_iterator>());
   BOOST_TEST(on_0xFF_buffer<reverse_iterator>()       == on_0x00_buffer<reverse_iterator>());
   BOOST_TEST(on_0xFF_buffer<const_reverse_iterator>() == on_0x00_buffer<const_reverse_iterator>());
}

int main()
{
   check_null_iterators< list<Type> >();
   check_null_iterators< slist<Type> >();
   check_null_iterators< bs_set<Type> >();
   check_null_iterators< set<Type> >();
   check_null_iterators< multiset<Type> >();
   check_null_iterators< avl_set<Type> >();
   check_null_iterators< avl_multiset<Type> >();
   check_null_iterators< sg_set<Type> >();
   check_null_iterators< sg_multiset<Type> >();
   check_null_iterators< treap_set<Type> >();
   check_null_iterators< treap_multiset<Type> >();
   check_null_iterators< splay_set<Type> >();
   check_null_iterators< splay_multiset<Type> >();

   return boost::report_errors();
}
