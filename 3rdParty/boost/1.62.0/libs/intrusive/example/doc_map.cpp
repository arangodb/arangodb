/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2015-2015
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////
//[doc_map_code
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/unordered_set.hpp>
#include <vector>
#include <cassert>

using namespace boost::intrusive;

class MyClass : public set_base_hook<>
              , public unordered_set_base_hook<>
{
   public:
   int first;
   explicit MyClass(int i) : first(i){}
};

//key_of_value function object, must:
//- be default constructible (and lightweight)
//- define the key type using "type"
//- define an operator() taking "const value_type&" and
//    returning "const type &"
struct first_int_is_key
{
   typedef int type;

   const type & operator()(const MyClass& v) const
   {  return v.first;  }
};

//Define omap like ordered and unordered classes 
typedef set< MyClass, key_of_value<first_int_is_key> > OrderedMap;
typedef unordered_set< MyClass, key_of_value<first_int_is_key> > UnorderedMap;

int main()
{
   BOOST_STATIC_ASSERT((boost::is_same<  OrderedMap::key_type, int>::value));
   BOOST_STATIC_ASSERT((boost::is_same<UnorderedMap::key_type, int>::value));

   //Create several MyClass objects, each one with a different value
   //and insert them into the omap
   std::vector<MyClass> values;
   for(int i = 0; i < 100; ++i)  values.push_back(MyClass(i));

   //Create ordered/unordered maps and insert values
   OrderedMap   omap(values.begin(), values.end());
   UnorderedMap::bucket_type buckets[100];
   UnorderedMap umap(values.begin(), values.end(), UnorderedMap::bucket_traits(buckets, 100));

   //Test each element using the key_type (int)
   for(int i = 0; i != 100; ++i){
      assert(omap.find(i) != omap.end());
      assert(umap.find(i) != umap.end());
      assert(omap.lower_bound(i) != omap.end());
      assert(++omap.lower_bound(i) == omap.upper_bound(i));
      assert(omap.equal_range(i).first != omap.equal_range(i).second);
      assert(umap.equal_range(i).first != umap.equal_range(i).second);
   }

   //Count and erase by key
   for(int i = 0; i != 100; ++i){
      assert(1 == omap.count(i));
      assert(1 == umap.count(i));
      assert(1 == omap.erase(i));
      assert(1 == umap.erase(i));
   }
   assert(omap.empty());
   assert(umap.empty());

   return 0;
}
//]
