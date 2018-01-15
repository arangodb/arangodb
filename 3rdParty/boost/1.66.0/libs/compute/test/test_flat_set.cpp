//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestFlatSet
#include <boost/test/unit_test.hpp>

#include <utility>

#include <boost/concept_check.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/container/flat_set.hpp>

#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(concept_check)
{
    BOOST_CONCEPT_ASSERT((boost::Container<bc::flat_set<int> >));
//    BOOST_CONCEPT_ASSERT((boost::SimpleAssociativeContainer<bc::flat_set<int> >));
//    BOOST_CONCEPT_ASSERT((boost::UniqueAssociativeContainer<bc::flat_set<int> >));
    BOOST_CONCEPT_ASSERT((boost::RandomAccessIterator<bc::flat_set<int>::iterator>));
    BOOST_CONCEPT_ASSERT((boost::RandomAccessIterator<bc::flat_set<int>::const_iterator>));
}

BOOST_AUTO_TEST_CASE(insert)
{
    bc::flat_set<int> set(context);
    typedef bc::flat_set<int>::iterator iterator;
    std::pair<iterator, bool> location = set.insert(12, queue);
    queue.finish();
    BOOST_CHECK(location.first == set.begin());
    BOOST_CHECK(location.second == true);
    BOOST_CHECK_EQUAL(*location.first, 12);
    BOOST_CHECK_EQUAL(set.size(), size_t(1));

    location = set.insert(12, queue);
    queue.finish();
    BOOST_CHECK(location.first == set.begin());
    BOOST_CHECK(location.second == false);
    BOOST_CHECK_EQUAL(set.size(), size_t(1));

    location = set.insert(4, queue);
    queue.finish();
    BOOST_CHECK(location.first == set.begin());
    BOOST_CHECK(location.second == true);
    BOOST_CHECK_EQUAL(set.size(), size_t(2));

    location = set.insert(12, queue);
    queue.finish();
    BOOST_CHECK(location.first == set.begin() + 1);
    BOOST_CHECK(location.second == false);
    BOOST_CHECK_EQUAL(set.size(), size_t(2));

    location = set.insert(9, queue);
    queue.finish();
    BOOST_CHECK(location.first == set.begin() + 1);
    BOOST_CHECK(location.second == true);
    BOOST_CHECK_EQUAL(set.size(), size_t(3));
}

BOOST_AUTO_TEST_CASE(erase)
{
    bc::flat_set<int> set(context);
    typedef bc::flat_set<int>::iterator iterator;
    set.insert(1, queue);
    set.insert(2, queue);
    set.insert(3, queue);
    set.insert(4, queue);
    set.insert(5, queue);
    queue.finish();
    BOOST_CHECK_EQUAL(set.size(), size_t(5));

    iterator i = set.erase(set.begin(), queue);
    queue.finish();
    BOOST_CHECK(i == set.begin() + 1);
    BOOST_CHECK_EQUAL(set.size(), size_t(4));
    BOOST_CHECK_EQUAL(*set.begin(), 2);

    size_t count = set.erase(3, queue);
    queue.finish();
    BOOST_CHECK_EQUAL(count, size_t(1));
    BOOST_CHECK_EQUAL(set.size(), size_t(3));
    BOOST_CHECK_EQUAL(*set.begin(), 2);

    count = set.erase(9, queue);
    queue.finish();
    BOOST_CHECK_EQUAL(count, size_t(0));
    BOOST_CHECK_EQUAL(set.size(), size_t(3));
    BOOST_CHECK_EQUAL(*set.begin(), 2);

    i = set.erase(set.begin() + 1, queue);
    queue.finish();
    BOOST_CHECK(i == set.begin() + 2);
    BOOST_CHECK_EQUAL(set.size(), size_t(2));
    BOOST_CHECK_EQUAL(*set.begin(), 2);
    BOOST_CHECK_EQUAL(*(set.end() - 1), 5);

    set.erase(set.begin(), set.end(), queue);
    queue.finish();
    BOOST_CHECK_EQUAL(set.size(), size_t(0));
}

BOOST_AUTO_TEST_CASE(clear)
{
    bc::flat_set<float> set;
    BOOST_CHECK(set.empty() == true);
    BOOST_CHECK_EQUAL(set.size(), size_t(0));

    set.clear();
    BOOST_CHECK(set.empty() == true);
    BOOST_CHECK_EQUAL(set.size(), size_t(0));

    set.insert(3.14f);
    BOOST_CHECK(set.empty() == false);
    BOOST_CHECK_EQUAL(set.size(), size_t(1));

    set.insert(4.184f);
    BOOST_CHECK(set.empty() == false);
    BOOST_CHECK_EQUAL(set.size(), size_t(2));

    set.clear();
    BOOST_CHECK(set.empty() == true);
    BOOST_CHECK_EQUAL(set.size(), size_t(0));
}

BOOST_AUTO_TEST_SUITE_END()
