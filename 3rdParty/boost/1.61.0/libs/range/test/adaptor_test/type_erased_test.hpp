// Boost.Range library
//
//  Copyright Neil Groves 2010. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef BOOST_RANGE_ADAPTOR_TEST_TYPE_ERASED_TEST_HPP
#define BOOST_RANGE_ADAPTOR_TEST_TYPE_ERASED_TEST_HPP

#include <boost/range/algorithm/fill.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>
#include <boost/assign.hpp>
#include <boost/test/test_tools.hpp>

namespace boost_range_adaptor_type_erased_test
{

class MockType
{
public:
    MockType()
        : m_x(0)
    {
    }

    MockType(boost::int32_t x)
        : m_x(x)
    {
    }

    boost::int32_t get() const { return m_x; }

    inline bool operator==(const MockType& other) const
    {
        return m_x == other.m_x;
    }

    inline bool operator!=(const MockType& other) const
    {
        return m_x != other.m_x;
    }

private:
    boost::int32_t m_x;
};

class MockType2 : public MockType
{
public:
    MockType2() {}
    MockType2(boost::int32_t x) : MockType(x) { }
    MockType2(const MockType& other) : MockType(other) { }
};

inline std::ostream& operator<<(std::ostream& out, const MockType& obj)
{
    out << obj.get();
    return out;
}

template<class Container>
void test_type_erased_impl(Container& c)
{
    using namespace boost::adaptors;
    typedef typename boost::range_value<Container>::type value_type;
    typedef typename boost::adaptors::type_erased<> type_erased_t;


    std::vector<value_type> output;

    boost::push_back(output, boost::adaptors::type_erase(c, type_erased_t()));

    BOOST_CHECK_EQUAL_COLLECTIONS( output.begin(), output.end(),
                                   c.begin(), c.end() );

    output.clear();
    boost::push_back(output, c | type_erased_t());

    BOOST_CHECK_EQUAL_COLLECTIONS( output.begin(), output.end(),
                                   c.begin(), c.end() );
}

template<class Container>
void test_const_and_mutable(Container& c)
{
    test_type_erased_impl(c);

    const Container& const_c = c;
    test_type_erased_impl(const_c);
}

template<class Container>
void test_driver()
{
    using namespace boost::assign;

    typedef typename boost::range_value<Container>::type value_type;

    Container c;
    test_const_and_mutable(c);

    c += value_type(1);
    test_const_and_mutable(c);

    c += value_type(2);
    test_const_and_mutable(c);
}

template<
    class Traversal
  , class Container
>
void test_writeable(Container&, boost::single_pass_traversal_tag)
{}

template<
    class Traversal
  , class Container
>
void test_writeable(Container& source, boost::forward_traversal_tag)
{
    using namespace boost::adaptors;

    typedef typename boost::range_value<Container>::type value_type;
    typedef typename boost::range_difference<Container>::type difference_type;
    typedef typename boost::range_reference<Container>::type mutable_reference_type;
    typedef boost::any_range<
                value_type
              , Traversal
              , mutable_reference_type
              , difference_type
            > mutable_any_range;

    mutable_any_range r = source | boost::adaptors::type_erased<>();
    std::vector<value_type> output_test;
    boost::fill(r, value_type(1));
    BOOST_CHECK_EQUAL( boost::distance(r), boost::distance(source) );
    std::vector<value_type> reference_output(source.size(), value_type(1));
    BOOST_CHECK_EQUAL_COLLECTIONS( reference_output.begin(), reference_output.end(),
                                   r.begin(), r.end() );

}

template<
    class Container
  , class Traversal
  , class Buffer
>
void test_type_erased_impl()
{
    using namespace boost::adaptors;

    typedef typename boost::range_value<Container>::type value_type;

    typedef typename boost::any_range_type_generator<
        Container
      , boost::use_default
      , Traversal
      , boost::use_default
      , boost::use_default
      , Buffer
    >::type mutable_any_range;

    typedef typename boost::any_range_type_generator<
        const Container
      , boost::use_default
      , Traversal
      , boost::use_default
      , boost::use_default
      , Buffer
    >::type const_any_range;

    typedef boost::adaptors::type_erased<
                boost::use_default
              , Traversal
              , boost::use_default
              , boost::use_default
              , Buffer
            > type_erased_t;

    Container source;
    for (int i = 0; i < 10; ++i)
        source.push_back(value_type(i));

    mutable_any_range r(source);
    BOOST_CHECK_EQUAL_COLLECTIONS( source.begin(), source.end(),
                                   r.begin(), r.end() );

    r = mutable_any_range();
    BOOST_CHECK_EQUAL( r.empty(), true );

    r = source | type_erased_t();
    BOOST_CHECK_EQUAL_COLLECTIONS( source.begin(), source.end(),
                                   r.begin(), r.end() );
    r = mutable_any_range();

    r = boost::adaptors::type_erase(source, type_erased_t());
    BOOST_CHECK_EQUAL_COLLECTIONS( source.begin(), source.end(),
                                   r.begin(), r.end() );
    r = mutable_any_range();

    test_writeable<Traversal>(source, Traversal());

    // convert and construct a const any_range from a mutable source
    // range
    const_any_range cr(source);
    BOOST_CHECK_EQUAL_COLLECTIONS( source.begin(), source.end(),
                                   cr.begin(), cr.end() );
    // assign an empty range and ensure that this correctly results
    // in an empty range. This is important for the validity of
    // the rest of the tests.
    cr = const_any_range();
    BOOST_CHECK_EQUAL( cr.empty(), true );

    // Test the pipe type_erased adaptor from a constant source
    // range to a constant any_range
    const Container& const_source = source;
    cr = const_any_range();
    cr = const_source | type_erased_t();
    BOOST_CHECK_EQUAL_COLLECTIONS( const_source.begin(), const_source.end(),
                                   cr.begin(), cr.end() );

    // Test the pipe type erased adaptor from a mutable source
    // range to a constant any_range
    cr = const_any_range();
    cr = source | type_erased_t();
    BOOST_CHECK_EQUAL_COLLECTIONS( source.begin(), source.end(),
                                   cr.begin(), cr.end() );

    // Use the function form of the type_erase adaptor from a constant
    // source range
    cr = const_any_range();
    cr = boost::adaptors::type_erase(const_source, type_erased_t());
    BOOST_CHECK_EQUAL_COLLECTIONS( const_source.begin(), const_source.end(),
                                   cr.begin(), cr.end() );

    // Assignment from mutable to const...
    cr = const_any_range();
    cr = r;
    BOOST_CHECK_EQUAL_COLLECTIONS( cr.begin(), cr.end(),
                                   r.begin(), r.end() );

    // Converting copy from mutable to const...
    cr = const_any_range();
    cr = const_any_range(r);
    BOOST_CHECK_EQUAL_COLLECTIONS( cr.begin(), cr.end(),
                                   r.begin(), r.end() );
}

template<
    class Container
  , class Traversal
  , class Buffer
>
class test_type_erased_impl_fn
{
public:
    typedef void result_type;
    void operator()()
    {
        test_type_erased_impl< Container, Traversal, Buffer >();
    }
};

template<
    class Container
  , class Traversal
>
void test_type_erased_exercise_buffer_types()
{
    using boost::any_iterator_default_buffer;
    using boost::any_iterator_buffer;
    using boost::any_iterator_heap_only_buffer;
    using boost::any_iterator_stack_only_buffer;

    test_type_erased_impl_fn< Container, Traversal, any_iterator_default_buffer >()();
    test_type_erased_impl_fn< Container, Traversal, any_iterator_heap_only_buffer >()();
    test_type_erased_impl_fn< Container, Traversal, any_iterator_buffer<1> >()();
    test_type_erased_impl_fn< Container, Traversal, any_iterator_buffer<2> >()();
    test_type_erased_impl_fn< Container, Traversal, any_iterator_buffer<32> >()();
    test_type_erased_impl_fn< Container, Traversal, any_iterator_buffer<64> >()();
    test_type_erased_impl_fn< Container, Traversal, any_iterator_buffer<128> >()();
    test_type_erased_impl_fn< Container, Traversal, any_iterator_stack_only_buffer<128> >()();
}

} // namespace boost_range_adaptor_type_erased_test

#endif // include guard
