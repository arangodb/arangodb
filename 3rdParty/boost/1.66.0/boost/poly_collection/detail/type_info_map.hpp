/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_TYPE_INFO_MAP_HPP
#define BOOST_POLY_COLLECTION_DETAIL_TYPE_INFO_MAP_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/detail/workaround.hpp>
#include <boost/poly_collection/detail/newdelete_allocator.hpp>
#include <functional>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace boost{

namespace poly_collection{

namespace detail{

/* To cope with dynamic modules/libs, the standard allows for different
 * std::type_info instances to describe the same type, which implies that
 * std::type_info::operator== and std::type_info::hash_code are costly
 * operations typically relying on the stored type name.
 * type_info_ptr_hash<T> behaves roughly as a
 * std::unordered_map<std::type_index,T> but maintains an internal cache of
 * passed std::type_info instances so that lookup is performed (when there's a
 * cache hit) without invoking std::type_info equality and hashing ops.
 */

struct type_info_ptr_hash
{
  std::size_t operator()(const std::type_info* p)const noexcept
  {return p->hash_code();}
};

struct type_info_ptr_equal_to
{
  bool operator()(
    const std::type_info* p,const std::type_info* q)const noexcept
  {return *p==*q;}
};


template<typename T,typename Allocator>
class type_info_map
{
  using map_type=std::unordered_map<
    const std::type_info*,T,
    type_info_ptr_hash,type_info_ptr_equal_to,
    typename std::allocator_traits<Allocator>::template
      rebind_alloc<std::pair<const std::type_info* const,T>>
  >;

public:
  using key_type=std::type_info;
  using mapped_type=T;
  using value_type=typename map_type::value_type;
  using allocator_type=typename map_type::allocator_type;
  using iterator=typename map_type::iterator;
  using const_iterator=typename map_type::const_iterator;

#if BOOST_WORKAROUND(BOOST_LIBSTDCXX_VERSION,<40900)
  /* std::unordered_map(const unordered_map&,const allocator_type&),
   * std::unordered_map(unordered_map&&,const allocator_type&) and
   * std::unordered_map(const allocator_type&) not available.
   */

#define BOOST_POLY_COLLECTION_MAP_CONT_ALLOC_CTOR(x,al)              \
x.begin(),x.end(),                                                   \
0,typename map_type::hasher{},typename map_type::key_equal{},al
#define BOOST_POLY_COLLECTION_MAP_ALLOC_CTOR(al)                     \
10,typename map_type::hasher{},typename map_type::key_equal{},al
#define BOOST_POLY_COLLECTION_CACHE_ALLOC_CTOR(al)                   \
10,typename cache_type::hasher{},typename cache_type::key_equal{},al

#else

#define BOOST_POLY_COLLECTION_MAP_CONT_ALLOC_CTOR(x,al) x,al
#define BOOST_POLY_COLLECTION_MAP_ALLOC_CTOR(al) al
#define BOOST_POLY_COLLECTION_CACHE_ALLOC_CTOR(al) al

#endif

  type_info_map()=default;
  type_info_map(const type_info_map& x):
    map{x.map},
    cache{BOOST_POLY_COLLECTION_CACHE_ALLOC_CTOR(x.cache.get_allocator())}
    {build_cache(x.cache);}
  type_info_map(type_info_map&& x)=default;
  type_info_map(const allocator_type& al):
    map{BOOST_POLY_COLLECTION_MAP_ALLOC_CTOR(al)},
    cache{BOOST_POLY_COLLECTION_CACHE_ALLOC_CTOR(cache_allocator_type{al})}
    {}
  type_info_map(const type_info_map& x,const allocator_type& al):
    map{BOOST_POLY_COLLECTION_MAP_CONT_ALLOC_CTOR(x.map,al)},
    cache{BOOST_POLY_COLLECTION_CACHE_ALLOC_CTOR(cache_allocator_type{al})}
    {build_cache(x.cache);}
  type_info_map(type_info_map&& x,const allocator_type& al):
    map{BOOST_POLY_COLLECTION_MAP_CONT_ALLOC_CTOR(std::move(x.map),al)},
    cache{BOOST_POLY_COLLECTION_CACHE_ALLOC_CTOR(cache_allocator_type{al})}
  {
    if(al==x.map.get_allocator()&&
       cache_allocator_type{al}==x.cache.get_allocator()){
      cache=std::move(x.cache);
    }
    else{
      build_cache(x.cache);
      x.cache.clear();
    }
  }

#undef BOOST_POLY_COLLECTION_MAP_CONT_ALLOC_CTOR
#undef BOOST_POLY_COLLECTION_MAP_ALLOC_CTOR
#undef BOOST_POLY_COLLECTION_CACHE_ALLOC_CTOR

  type_info_map& operator=(const type_info_map& x)
  {
    if(this!=&x){
      type_info_map c{x};
      swap(c);
    }
    return *this;
  }

  type_info_map& operator=(type_info_map&& x)=default;

  allocator_type get_allocator()const noexcept{return map.get_allocator();}

  iterator       begin()noexcept{return map.begin();}
  iterator       end()noexcept{return map.end();}
  const_iterator begin()const noexcept{return map.begin();}
  const_iterator end()const noexcept{return map.end();}
  const_iterator cbegin()const noexcept{return map.cbegin();}
  const_iterator cend()const noexcept{return map.cend();}

  iterator find(const key_type& key)
  {
    auto cit=cache.find(&key);
    if(cit!=cache.end())return cit->second;
    auto mit=map.find(&key);
    if(mit!=map.end())cache.insert({&key,mit});
    return mit; 
  }

  const_iterator find(const key_type& key)const
  {
    auto cit=cache.find(&key);
    if(cit!=cache.end())return cit->second;
    return map.find(&key);
  }

  template<typename P>
  std::pair<iterator,bool> insert(const key_type& key,P&& x)
  {
    auto p=map.insert({&key,std::forward<P>(x)});
    cache.insert({&key,p.first});
    return p;
  }

  void swap(type_info_map& x){map.swap(x.map);cache.swap(x.cache);}

private:
  using cache_type=std::unordered_map<
    const std::type_info*,iterator,
    std::hash<const std::type_info*>,std::equal_to<const std::type_info*>,
    newdelete_allocator_adaptor<
      typename std::allocator_traits<Allocator>::template
        rebind_alloc<std::pair<const std::type_info* const,iterator>>
    >
  >;
  using cache_allocator_type=typename cache_type::allocator_type;

  void build_cache(const cache_type& x)
  {
    for(const auto& p:x)cache.insert({p.first,map.find(p.first)});
  }

  map_type   map;
  cache_type cache;
};

template<typename T,typename Allocator>
void swap(type_info_map<T,Allocator>& x,type_info_map<T,Allocator>& y)
{
  x.swap(y);
}

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */

#endif
