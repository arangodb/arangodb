// Boost.Range library
//
//  Copyright Neil Groves 2014. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/range/
//

#include <boost/detail/workaround.hpp>
#include <boost/range/iterator_range_core.hpp>
#include <boost/cstdint.hpp>

#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>

#include <vector>

namespace boost_range_test
{
    namespace
    {

class single_pass_iterator
        : public boost::iterator_facade<
            single_pass_iterator,
            boost::int32_t,
            boost::single_pass_traversal_tag,
            const boost::int32_t&
        >
{
    friend class boost::iterator_core_access;

    typedef std::vector<boost::int32_t>::const_iterator iterator_t;

public:
    single_pass_iterator() { }

    explicit single_pass_iterator(iterator_t it)
        : m_it(it)
    {
    }

private:
    void increment()
    {
        ++m_it;
    }

    bool equal(single_pass_iterator other) const
    {
        return m_it == other.m_it;
    }

    reference dereference() const
    {
        return *m_it;
    }

    iterator_t m_it;
};

class bidirectional_iterator
        : public boost::iterator_facade<
            bidirectional_iterator,
            boost::int32_t,
            boost::bidirectional_traversal_tag,
            const boost::int32_t&
        >
{
    friend class boost::iterator_core_access;

    typedef std::vector<boost::int32_t>::const_iterator iterator_t;

public:
    bidirectional_iterator() { }

    explicit bidirectional_iterator(iterator_t it)
        : m_it(it)
    {
    }

private:
    void increment()
    {
        ++m_it;
    }

    void decrement()
    {
        --m_it;
    }

    bool equal(bidirectional_iterator other) const
    {
        return m_it == other.m_it;
    }

    reference dereference() const
    {
        return *m_it;
    }

    iterator_t m_it;
};

template<typename SinglePassRange>
boost::iterator_range<single_pass_iterator>
single_pass_range(const SinglePassRange& rng)
{
    return boost::iterator_range<single_pass_iterator>(
                single_pass_iterator(boost::begin(rng)),
                single_pass_iterator(boost::end(rng)));
}

template<typename BidirectionalRange>
boost::iterator_range<bidirectional_iterator>
bidirectional_range(const BidirectionalRange& rng)
{
    return boost::iterator_range<bidirectional_iterator>(
                bidirectional_iterator(boost::begin(rng)),
                bidirectional_iterator(boost::end(rng)));
}

void test_drop_front()
{
    std::vector<boost::int32_t> v;
    std::vector<boost::int32_t> ref_output;

    for (boost::int32_t i = 0; i < 10; ++i)
    {
        v.push_back(i);
        ref_output.push_back(i);
    }

    boost::iterator_range<single_pass_iterator> rng = single_pass_range(v);

    BOOST_CHECK_EQUAL_COLLECTIONS(rng.begin(), rng.end(),
                                  ref_output.begin(), ref_output.end());

    rng.drop_front();

    ref_output.erase(ref_output.begin());

    BOOST_CHECK_EQUAL_COLLECTIONS(rng.begin(), rng.end(),
                                  ref_output.begin(), ref_output.end());

    rng.drop_front(5);

    ref_output.erase(ref_output.begin(), ref_output.begin() + 5);

    BOOST_CHECK_EQUAL_COLLECTIONS(rng.begin(), rng.end(),
                                  ref_output.begin(), ref_output.end());
}

void test_drop_back()
{
    std::vector<boost::int32_t> v;
    std::vector<boost::int32_t> ref_output;

    for (boost::int32_t i = 0; i < 10; ++i)
    {
        v.push_back(i);
        ref_output.push_back(i);
    }

    boost::iterator_range<bidirectional_iterator> rng = bidirectional_range(v);

    BOOST_CHECK_EQUAL_COLLECTIONS(rng.begin(), rng.end(),
                                  ref_output.begin(), ref_output.end());

    rng.drop_back();

    ref_output.pop_back();

    BOOST_CHECK_EQUAL_COLLECTIONS(rng.begin(), rng.end(),
                                  ref_output.begin(), ref_output.end());

    rng.drop_back(5);

    ref_output.erase(ref_output.end() - 5, ref_output.end());

    BOOST_CHECK_EQUAL_COLLECTIONS(rng.begin(), rng.end(),
                                  ref_output.begin(), ref_output.end());
}

    } // anonymous namespace
} // namespace boost_range_test

boost::unit_test::test_suite* init_unit_test_suite( int argc, char* argv[] )
{
    boost::unit_test::test_suite* test =
        BOOST_TEST_SUITE("Boost.Range iterator_range drop functions");

    test->add(BOOST_TEST_CASE(&boost_range_test::test_drop_front));
    test->add(BOOST_TEST_CASE(&boost_range_test::test_drop_back));

    return test;
}
