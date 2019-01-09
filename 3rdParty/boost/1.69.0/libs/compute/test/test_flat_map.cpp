//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestFlatMap
#include <boost/test/unit_test.hpp>

#include <utility>

#include <boost/concept_check.hpp>

#include <boost/compute/source.hpp>
#include <boost/compute/container/flat_map.hpp>
#include <boost/compute/type_traits/type_name.hpp>
#include <boost/compute/type_traits/type_definition.hpp>

#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(concept_check)
{
    BOOST_CONCEPT_ASSERT((boost::Container<boost::compute::flat_map<int, float> >));
//    BOOST_CONCEPT_ASSERT((boost::SimpleAssociativeContainer<boost::compute::flat_map<int, float> >));
//    BOOST_CONCEPT_ASSERT((boost::UniqueAssociativeContainer<boost::compute::flat_map<int, float> >));
    BOOST_CONCEPT_ASSERT((boost::RandomAccessIterator<boost::compute::flat_map<int, float>::iterator>));
    BOOST_CONCEPT_ASSERT((boost::RandomAccessIterator<boost::compute::flat_map<int, float>::const_iterator>));
}

BOOST_AUTO_TEST_CASE(insert)
{
    boost::compute::flat_map<int, float> map(context);
    map.insert(std::make_pair(1, 1.1f), queue);
    map.insert(std::make_pair(-1, -1.1f), queue);
    map.insert(std::make_pair(3, 3.3f), queue);
    map.insert(std::make_pair(2, 2.2f), queue);
    BOOST_CHECK_EQUAL(map.size(), size_t(4));
    BOOST_CHECK(map.find(-1) == map.begin() + 0);
    BOOST_CHECK(map.find(1) == map.begin() + 1);
    BOOST_CHECK(map.find(2) == map.begin() + 2);
    BOOST_CHECK(map.find(3) == map.begin() + 3);

    map.insert(std::make_pair(2, -2.2f), queue);
    BOOST_CHECK_EQUAL(map.size(), size_t(4));
}

BOOST_AUTO_TEST_CASE(at)
{
    boost::compute::flat_map<int, float> map(context);
    map.insert(std::make_pair(1, 1.1f), queue);
    map.insert(std::make_pair(4, 4.4f), queue);
    map.insert(std::make_pair(3, 3.3f), queue);
    map.insert(std::make_pair(2, 2.2f), queue);
    BOOST_CHECK_EQUAL(float(map.at(1)), float(1.1f));
    BOOST_CHECK_EQUAL(float(map.at(2)), float(2.2f));
    BOOST_CHECK_EQUAL(float(map.at(3)), float(3.3f));
    BOOST_CHECK_EQUAL(float(map.at(4)), float(4.4f));
}

BOOST_AUTO_TEST_CASE(index_operator)
{
    boost::compute::flat_map<int, float> map;
    map[1] = 1.1f;
    map[2] = 2.2f;
    map[3] = 3.3f;
    map[4] = 4.4f;
    BOOST_CHECK_EQUAL(float(map[1]), float(1.1f));
    BOOST_CHECK_EQUAL(float(map[2]), float(2.2f));
    BOOST_CHECK_EQUAL(float(map[3]), float(3.3f));
    BOOST_CHECK_EQUAL(float(map[4]), float(4.4f));
}

BOOST_AUTO_TEST_CASE(custom_kernel)
{
    typedef boost::compute::flat_map<int, float> MapType;

    // map from int->float on device
    MapType map(context);
    map.insert(std::make_pair(1, 1.2f), queue);
    map.insert(std::make_pair(3, 3.4f), queue);
    map.insert(std::make_pair(5, 5.6f), queue);
    map.insert(std::make_pair(7, 7.8f), queue);

    // simple linear search for key in map
    const char lookup_source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void lookup(__global const MapType *map,
                             const int map_size,
                             const KeyType key,
                             __global ValueType *result)
        {
            for(int i = 0; i < map_size; i++){
                if(map[i].first == key){
                    *result = map[i].second;
                    break;
                }
            }
        }
    );

    // create program source
    std::stringstream source;

    // add type definition for map type
    source << boost::compute::type_definition<MapType::value_type>();

    // add lookup function source
    source << lookup_source;

    // create lookup program
    boost::compute::program lookup_program =
        boost::compute::program::create_with_source(source.str(), context);

    // program build options
    std::stringstream options;
    options << "-DMapType=" << boost::compute::type_name<MapType::value_type>()
            << " -DKeyType=" << boost::compute::type_name<MapType::key_type>()
            << " -DValueType=" << boost::compute::type_name<MapType::mapped_type>();

    // build lookup program with options
    lookup_program.build(options.str());

    // create buffer for result value
    boost::compute::vector<float> result(1, context);

    // create lookup kernel
    boost::compute::kernel lookup_kernel = lookup_program.create_kernel("lookup");

    // set kernel arguments
    lookup_kernel.set_arg(0, map.begin().get_buffer()); // map buffer
    lookup_kernel.set_arg<boost::compute::int_>(
        1, static_cast<boost::compute::int_>(map.size())
    ); // map size
    lookup_kernel.set_arg<MapType::key_type>(2, 5); // key
    lookup_kernel.set_arg(3, result.get_buffer()); // result buffer

    // run kernel with a single work-item
    queue.enqueue_task(lookup_kernel);

    // check result from buffer
    BOOST_CHECK_EQUAL(result.begin().read(queue), 5.6f);
}

BOOST_AUTO_TEST_SUITE_END()
