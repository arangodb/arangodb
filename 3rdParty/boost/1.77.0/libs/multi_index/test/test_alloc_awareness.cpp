/* Boost.MultiIndex test for allocator awareness.
 *
 * Copyright 2003-2020 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#include "test_alloc_awareness.hpp"

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/detail/lightweight_test.hpp>
#include <boost/move/core.hpp>
#include <boost/move/utility_core.hpp>
#include "pre_multi_index.hpp"
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/ranked_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include "rooted_allocator.hpp"

struct move_tracker
{
  move_tracker(int n):n(n),move_cted(false){}
  move_tracker(const move_tracker& x):n(x.n),move_cted(false){}
  move_tracker(BOOST_RV_REF(move_tracker) x):n(x.n),move_cted(true){}
  move_tracker& operator=(BOOST_COPY_ASSIGN_REF(move_tracker) x)
    {n=x.n;return *this;}
  move_tracker& operator=(BOOST_RV_REF(move_tracker) x){n=x.n;return *this;}

  int  n;
  bool move_cted;

private:
  BOOST_COPYABLE_AND_MOVABLE(move_tracker)
};

inline bool operator==(const move_tracker& x,const move_tracker& y)
{
  return x.n==y.n;
}

inline bool operator<(const move_tracker& x,const move_tracker& y)
{
  return x.n<y.n;
}

#if defined(BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP)
namespace boost{
#endif

inline std::size_t hash_value(const move_tracker& x)
{
  boost::hash<int> h;
  return h(x.n);
}

#if defined(BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP)
} /* namespace boost */
#endif

#if defined(BOOST_NO_CXX17_IF_CONSTEXPR)&&defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4127) /* conditional expression is constant */
#endif

template<bool Propagate,bool AlwaysEqual>
void test_allocator_awareness_for()
{
  using namespace boost::multi_index;

  typedef rooted_allocator<move_tracker,Propagate,AlwaysEqual> allocator;
  typedef multi_index_container<
    move_tracker,
    indexed_by<
      hashed_unique<identity<move_tracker> >,
      ordered_unique<identity<move_tracker> >,
      random_access<>,
      ranked_unique<identity<move_tracker> >,
      sequenced<>
    >,
    allocator
  >                                                            container;

  allocator root1(0),root2(0);
  container c(root1);
  for(int i=0;i<10;++i)c.emplace(i);

  BOOST_TEST(c.get_allocator().comes_from(root1));

  {
    container c2(c,root2);
    BOOST_TEST(c2.get_allocator().comes_from(root2));
    BOOST_TEST(c2==c);
  }
  {
    container           c2(c);
    const move_tracker* pfirst=&*c2.begin();
    container           c3(boost::move(c2),root2);
    BOOST_TEST(c3.get_allocator().comes_from(root2));
    BOOST_TEST(c3==c);
    BOOST_TEST(c2.empty());
    BOOST_TEST(AlwaysEqual==(&*c3.begin()==pfirst));
    BOOST_TEST(!AlwaysEqual==(c3.begin()->move_cted));
  }
  {
    container c2(root2);
    c2=c;
    BOOST_TEST(c2.get_allocator().comes_from(Propagate?root1:root2));
    BOOST_TEST(c2==c);
  }
  {
    const bool          element_transfer=Propagate||AlwaysEqual;

    container           c2(c);
    const move_tracker* pfirst=&*c2.begin();
    container           c3(root2);
    c3=boost::move(c2);
    BOOST_TEST(c3.get_allocator().comes_from(Propagate?root1:root2));
    BOOST_TEST(c3==c);
    BOOST_TEST(c2.empty());
    BOOST_TEST(element_transfer==(&*c3.begin()==pfirst));
    BOOST_TEST(!element_transfer==(c3.begin()->move_cted));
  }
  if(Propagate||AlwaysEqual){
    container           c2(c);
    const move_tracker* pfirst=&*c2.begin();
    container           c3(root2);
    c3.swap(c2);
    BOOST_TEST(c2.get_allocator().comes_from(Propagate?root2:root1));
    BOOST_TEST(c3.get_allocator().comes_from(Propagate?root1:root2));
    BOOST_TEST(c3==c);
    BOOST_TEST(c2.empty());
    BOOST_TEST(&*c3.begin()==pfirst);
    BOOST_TEST(!c3.begin()->move_cted);
  }
}

#if defined(BOOST_NO_CXX17_IF_CONSTEXPR)&&defined(BOOST_MSVC)
#pragma warning(pop) /* C4127 */
#endif

void test_allocator_awareness()
{
  test_allocator_awareness_for<false,false>();
  test_allocator_awareness_for<false,true>();

#if !defined(BOOST_NO_CXX11_ALLOCATOR)
  /* only in C+11 onwards are allocators potentially expected to propagate */

  test_allocator_awareness_for<true,false>();
  test_allocator_awareness_for<true,true>();

#endif
}
