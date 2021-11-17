/* Copyright 2016-2018 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#include "test_iterators.hpp"

#include <boost/core/lightweight_test.hpp>
#include <iterator>
#include <type_traits>
#include "any_types.hpp"
#include "base_types.hpp"
#include "function_types.hpp"
#include "test_utilities.hpp"

using namespace test_utilities;

template<typename Iterator>
using is_input=std::is_base_of<
  std::input_iterator_tag,
  typename std::iterator_traits<Iterator>::iterator_category
>;
template<typename Iterator>
using is_forward=std::is_base_of<
  std::forward_iterator_tag,
  typename std::iterator_traits<Iterator>::iterator_category
>;
template<typename Iterator>
using is_random_access=std::is_base_of<
  std::random_access_iterator_tag,
  typename std::iterator_traits<Iterator>::iterator_category
>;

template<typename Type,typename PolyCollection>
void test_iterators(PolyCollection& p)
{
  using local_base_iterator=typename PolyCollection::local_base_iterator;
  using const_local_base_iterator=
    typename PolyCollection::const_local_base_iterator;
  using local_iterator=typename PolyCollection::template local_iterator<Type>;
  using const_local_iterator=
    typename PolyCollection::template const_local_iterator<Type>;
  using base_segment_info=typename PolyCollection::base_segment_info;
  using const_base_segment_info=
    typename PolyCollection::const_base_segment_info;
  using const_segment_info=
    typename PolyCollection::template const_segment_info<Type>;
  using segment_info=typename PolyCollection::template segment_info<Type>;

  static_assert(is_random_access<local_iterator>::value,
                "local_iterator must be random access");
  static_assert(is_random_access<const_local_iterator>::value,
                "const_local_iterator must be random access");
  static_assert(std::is_base_of<const_segment_info,segment_info>::value,
                "segment_info must derive from const_segment_info");

  {
    local_iterator       lit,lit2;
    const_local_iterator clit,clit2(lit); /* sorry about the names */

    lit=lit2;
    clit=clit2;
    clit=lit;
  }

  const PolyCollection&     cp=p;
  std::size_t               n=0;
  local_base_iterator       lbfirst=p.begin(typeid(Type)),
                            lblast=p.end(typeid(Type));
  const_local_base_iterator clbfirst=cp.begin(typeid(Type)),
                            clblast=cp.end(typeid(Type));
  local_iterator            lfirst=p.template begin<Type>(),
                            llast=p.template end<Type>();
  const_local_iterator      clfirst=cp.template begin<Type>(),
                            cllast=cp.template end<Type>();
  base_segment_info         bi=p.segment(typeid(Type));
  const_base_segment_info   cbi=cp.segment(typeid(Type));
  segment_info              i=p.template segment<Type>();
  const_segment_info        ci=cp.template segment<Type>();

  BOOST_TEST(clbfirst==cp.cbegin(typeid(Type)));
  BOOST_TEST(clblast==cp.cend(typeid(Type)));
  BOOST_TEST(clfirst==cp.template cbegin<Type>());
  BOOST_TEST(cllast==cp.template cend<Type>());

  BOOST_TEST(lbfirst==bi.begin());
  BOOST_TEST(lblast==bi.end());
  BOOST_TEST(clbfirst==bi.cbegin());
  BOOST_TEST(clbfirst==cbi.begin());
  BOOST_TEST(clblast==bi.cend());
  BOOST_TEST(clblast==cbi.end());

  BOOST_TEST(lfirst==i.begin());
  BOOST_TEST(llast==i.end());
  BOOST_TEST(clfirst==i.cbegin());
  BOOST_TEST(clfirst==ci.begin());
  BOOST_TEST(cllast==i.cend());
  BOOST_TEST(cllast==ci.end());

  for(;lbfirst!=lblast;++lbfirst,++clbfirst,++lfirst,++clfirst){
    BOOST_TEST(lfirst==static_cast<local_iterator>(lbfirst));
    BOOST_TEST(static_cast<local_base_iterator>(lfirst)==lbfirst);
    BOOST_TEST(clfirst==static_cast<const_local_iterator>(clbfirst));
    BOOST_TEST(static_cast<const_local_base_iterator>(clfirst)==clbfirst);
    BOOST_TEST(clfirst==lfirst);
    BOOST_TEST(&*lfirst==&*static_cast<local_iterator>(lbfirst));
    BOOST_TEST(&*clfirst==&*static_cast<const_local_iterator>(clbfirst));
    BOOST_TEST(&*clfirst==&*lfirst);

    Type&       r=p.template begin<Type>()[n];
    const Type& cr=cp.template begin<Type>()[n];

    BOOST_TEST(&*lfirst==&r);
    BOOST_TEST(&*clfirst==&cr);

    ++n;
  }
  BOOST_TEST(clbfirst==clblast);
  BOOST_TEST(lfirst==llast);
  BOOST_TEST(clfirst==cllast);
  BOOST_TEST(lfirst==static_cast<local_iterator>(llast));
  BOOST_TEST(clfirst==static_cast<const_local_iterator>(cllast));
  BOOST_TEST(clfirst==llast);
  BOOST_TEST((std::ptrdiff_t)n==p.end(typeid(Type))-p.begin(typeid(Type)));
  BOOST_TEST(
    (std::ptrdiff_t)n==p.template end<Type>()-p.template begin<Type>());

  for(auto s:p.segment_traversal()){
    if(s.type_info()==typeid(Type)){
      const auto& cs=s;

      BOOST_TEST(
        s.template begin<Type>()==
        static_cast<local_iterator>(s.begin()));
      BOOST_TEST(
        s.template end<Type>()==
        static_cast<local_iterator>(s.end()));
      BOOST_TEST(
        cs.template begin<Type>()==
        static_cast<const_local_iterator>(cs.begin()));
      BOOST_TEST(
        cs.template end<Type>()==
        static_cast<const_local_iterator>(cs.end()));
      BOOST_TEST(
        cs.template cbegin<Type>()==
        static_cast<const_local_iterator>(cs.cbegin()));
      BOOST_TEST(
        cs.template cend<Type>()==
        static_cast<const_local_iterator>(cs.cend()));
    }
  }
}

template<typename PolyCollection,typename ValueFactory,typename... Types>
void test_iterators()
{
  using value_type=typename PolyCollection::value_type;
  using iterator=typename PolyCollection::iterator;
  using const_iterator=typename PolyCollection::const_iterator;
  using local_base_iterator=typename PolyCollection::local_base_iterator;
  using const_local_base_iterator=
    typename PolyCollection::const_local_base_iterator;
  using const_base_segment_info=
    typename PolyCollection::const_base_segment_info;
  using base_segment_info=typename PolyCollection::base_segment_info;
  using base_segment_info_iterator=
    typename PolyCollection::base_segment_info_iterator;
  using const_base_segment_info_iterator=
    typename PolyCollection::const_base_segment_info_iterator;
  using const_segment_traversal_info=
    typename PolyCollection::const_segment_traversal_info;
  using segment_traversal_info=
    typename PolyCollection::segment_traversal_info;

  static_assert(is_forward<iterator>::value,
                "iterator must be forward");
  static_assert(is_forward<const_iterator>::value,
                "const_iterator must be forward");
  static_assert(is_random_access<local_base_iterator>::value,
                "local_base_iterator must be random access");
  static_assert(is_random_access<const_local_base_iterator>::value,
                "const_local_base_iterator must be random access");
  static_assert(std::is_base_of<
                  const_base_segment_info,base_segment_info>::value,
                "base_segment_info must derive from const_base_segment_info");
  static_assert(is_input<base_segment_info_iterator>::value,
                "base_segment_info_iterator must be input");
  static_assert(is_input<const_base_segment_info_iterator>::value,
                "const_base_segment_info_iterator must be input");
  static_assert(std::is_base_of<
                  const_segment_traversal_info,segment_traversal_info>::value,
                "const_segment_traversal_info must derive "\
                "from segment_traversal_info");

  {
    iterator                         it,it2;
    const_iterator                   cit,cit2(it);
    local_base_iterator              lbit,lbit2;
    const_local_base_iterator        clbit,clbit2(lbit);
    base_segment_info_iterator       sit,sit2;
    const_base_segment_info_iterator csit,csit2(csit);

    it=it2;
    cit=cit2;
    cit=it;
    lbit=lbit2;
    clbit=clbit2;
    clbit=lbit;
    sit=sit2;
    csit=csit2;
    csit=sit;
  }

  PolyCollection        p;
  const PolyCollection& cp=p;
  ValueFactory          v;

  fill<constraints<>,Types...>(p,v,2);

  {
    std::size_t    n=0;
    iterator       first=p.begin(),last=p.end();
    const_iterator cfirst=cp.begin(),clast=cp.end();

    BOOST_TEST(cfirst==cp.cbegin());
    BOOST_TEST(clast==cp.cend());

    for(;first!=last;++first,++cfirst){
      BOOST_TEST(first==cfirst);
      BOOST_TEST(&*first==&*cfirst);

      ++n;
    }
    BOOST_TEST(cfirst==clast);
    BOOST_TEST(last==clast);
    BOOST_TEST(n==p.size());
  }

  {
    std::size_t                      n=0;
    base_segment_info_iterator       first=p.segment_traversal().begin(),
                                     last=p.segment_traversal().end();
    const_base_segment_info_iterator cfirst=cp.segment_traversal().begin(),
                                     clast=cp.segment_traversal().end();

    BOOST_TEST(cfirst==cp.segment_traversal().cbegin());
    BOOST_TEST(clast==cp.segment_traversal().cend());

    for(;first!=last;++first,++cfirst){
      BOOST_TEST(first==cfirst);

      std::size_t               m=0;
      local_base_iterator       lbfirst=first->begin(),lblast=first->end();
      const_local_base_iterator clbfirst=cfirst->begin(),clblast=cfirst->end();

      BOOST_TEST(clbfirst==cfirst->cbegin());
      BOOST_TEST(clblast==cfirst->cend());
      BOOST_TEST(lbfirst==p.begin(first->type_info()));
      BOOST_TEST(lblast==p.end(first->type_info()));
      BOOST_TEST(clbfirst==cp.begin(first->type_info()));
      BOOST_TEST(clblast==cp.end(first->type_info()));
      BOOST_TEST(clbfirst==cp.cbegin(first->type_info()));
      BOOST_TEST(clblast==cp.cend(first->type_info()));

      for(;lbfirst!=lblast;++lbfirst,++clbfirst){
        BOOST_TEST(lbfirst==clbfirst);
        BOOST_TEST(&*lbfirst==&*clbfirst);

        value_type&       r=first->begin()[m];
        const value_type& cr=cfirst->begin()[m];

        BOOST_TEST(&*lbfirst==&r);
        BOOST_TEST(&*clbfirst==&cr);

        ++m;
      }
      BOOST_TEST(clbfirst==clblast);
      BOOST_TEST(lblast==clblast);
      BOOST_TEST((std::ptrdiff_t)m==first->end()-first->begin());
      BOOST_TEST((std::ptrdiff_t)m==cfirst->end()-cfirst->begin());
      BOOST_TEST((std::ptrdiff_t)m==cfirst->cend()-cfirst->cbegin());

      n+=m;
    }
    BOOST_TEST(cfirst==clast);
    BOOST_TEST(last==clast);
    BOOST_TEST(n==p.size());
  }

  do_((test_iterators<Types>(p),0)...);
}

void test_iterators()
{
  test_iterators<
    any_types::collection,auto_increment,
    any_types::t1,any_types::t2,any_types::t3,
    any_types::t4,any_types::t5>();
  test_iterators<
    base_types::collection,auto_increment,
    base_types::t1,base_types::t2,base_types::t3,
    base_types::t4,base_types::t5>();
  test_iterators<
    function_types::collection,auto_increment,
    function_types::t1,function_types::t2,function_types::t3,
    function_types::t4,function_types::t5>();
}
