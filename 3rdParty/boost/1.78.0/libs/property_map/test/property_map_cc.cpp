//  (C) Copyright Jeremy Siek 2001.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/property_map/property_map.hpp>
#include <boost/property_map/shared_array_property_map.hpp>
#include <map>

// This file checks the property map concepts against the property map
// archetypes to make sure they are consistent and that they compile.
// This also checks all the property map classes defined in
// property_map.hpp against the concept checking classes.

int
main()
{
  using namespace boost;
  {
    typedef null_archetype<> Key;
    typedef assignable_archetype<copy_constructible_archetype<> > Value;
    typedef readable_property_map_archetype<Key, Value> PMap;
    BOOST_CONCEPT_ASSERT((ReadablePropertyMapConcept<PMap, Key>));
  }
  {
    typedef null_archetype<> Key;
    typedef assignable_archetype<copy_constructible_archetype<> > Value;
    typedef writable_property_map_archetype<Key, Value> PMap;
    BOOST_CONCEPT_ASSERT((WritablePropertyMapConcept<PMap, Key>));
  }
  {
    typedef null_archetype<> Key;
    typedef assignable_archetype<copy_constructible_archetype<> > Value;
    typedef read_write_property_map_archetype<Key, Value> PMap;
    BOOST_CONCEPT_ASSERT((ReadWritePropertyMapConcept<PMap, Key>));
  }
  {
    typedef null_archetype<> Key;
    typedef assignable_archetype<copy_constructible_archetype<> > Value;
    typedef lvalue_property_map_archetype<Key, Value> PMap;
    BOOST_CONCEPT_ASSERT((LvaluePropertyMapConcept<PMap, Key>));
  }
  {
    typedef null_archetype<> Key;
    typedef assignable_archetype<copy_constructible_archetype<> > Value;
    typedef mutable_lvalue_property_map_archetype<Key, Value> PMap;
    BOOST_CONCEPT_ASSERT((Mutable_LvaluePropertyMapConcept<PMap, Key>));
  }
  {
    typedef std::ptrdiff_t Key;
    typedef int* PMap;
    BOOST_CONCEPT_ASSERT((Mutable_LvaluePropertyMapConcept<PMap, Key>));
  }
  {
    typedef std::ptrdiff_t Key;
    typedef const int* PMap;
    BOOST_CONCEPT_ASSERT((LvaluePropertyMapConcept<PMap, Key>));
  }
  {
    typedef sgi_assignable_archetype<> Key; // ?
    typedef sgi_assignable_archetype<> Value;
    typedef random_access_iterator_archetype<Value> Iterator;
    typedef readable_property_map_archetype<Key, std::ptrdiff_t> IndexMap;
    typedef iterator_property_map<Iterator, IndexMap
#ifdef BOOST_NO_STD_ITERATOR_TRAITS
      , Value, const Value&
#endif
      > PMap;
    BOOST_CONCEPT_ASSERT((LvaluePropertyMapConcept<PMap, Key>));
  }
  {
    typedef sgi_assignable_archetype<> Key;
    typedef sgi_assignable_archetype<> Value;
    typedef mutable_random_access_iterator_archetype<Value> Iterator;
    typedef readable_property_map_archetype<Key, std::ptrdiff_t> IndexMap;
    typedef iterator_property_map<Iterator, IndexMap
#ifdef BOOST_NO_STD_ITERATOR_TRAITS
      , Value, Value&
#endif
      > PMap;
    BOOST_CONCEPT_ASSERT((Mutable_LvaluePropertyMapConcept<PMap, Key>));
  }
  {
    typedef sgi_assignable_archetype< less_than_comparable_archetype<> > Key;
    typedef default_constructible_archetype< sgi_assignable_archetype<> > 
      Value;
    typedef std::map<Key, Value> Container;
    typedef associative_property_map<Container> PMap;
    BOOST_CONCEPT_ASSERT((Mutable_LvaluePropertyMapConcept<PMap, Key>));
  }
  {
    typedef sgi_assignable_archetype< less_than_comparable_archetype<> > Key;
    typedef default_constructible_archetype< sgi_assignable_archetype<> > 
      Value;
    typedef std::map<Key, Value> Container;
    typedef const_associative_property_map<Container> PMap;
    BOOST_CONCEPT_ASSERT((LvaluePropertyMapConcept<PMap, Key>));
  }
  {
    typedef identity_property_map PMap;
    BOOST_CONCEPT_ASSERT((ReadablePropertyMapConcept<PMap, int>));
  }
  {
    typedef dummy_property_map PMap;
    BOOST_CONCEPT_ASSERT((ReadWritePropertyMapConcept<PMap, int>));
  }
  {
    typedef sgi_assignable_archetype<> Key; // ?
    typedef sgi_assignable_archetype<> Value;
    typedef readable_property_map_archetype<Key, std::ptrdiff_t> IndexMap;
    typedef shared_array_property_map<Value, IndexMap> PMap;
    BOOST_CONCEPT_ASSERT((Mutable_LvaluePropertyMapConcept<PMap, Key>));
  }
  return 0;
}
