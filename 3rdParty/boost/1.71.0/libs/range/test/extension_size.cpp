// Boost.Range library
//
//  Copyright Thorsten Ottosen 2003-2004. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/range/
//


#include <boost/detail/workaround.hpp>

#if BOOST_WORKAROUND(__BORLANDC__, BOOST_TESTED_AT(0x564))
#  pragma warn -8091 // suppress warning in Boost.Test
#  pragma warn -8057 // unused argument argc/argv in Boost.Test
#endif

#include <boost/range.hpp>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/static_assert.hpp>
#include <boost/cstdint.hpp>
#include <list>
#include <vector>

namespace boost_range_extension_size_test
{
    class FooWithoutSize
    {
        typedef std::list<int> impl_t;
        
        BOOST_STATIC_ASSERT((
            boost::is_same<
                boost::range_size<std::list<int> >::type,
                std::list<int>::size_type
            >::value));
        
        typedef impl_t::const_iterator const_iterator;
        typedef impl_t::iterator iterator;

    public:
        friend inline const_iterator range_begin(const FooWithoutSize& obj) { return obj.m_impl.begin(); }
        friend inline iterator range_begin(FooWithoutSize& obj) { return obj.m_impl.begin(); }
        friend inline const_iterator range_end(const FooWithoutSize& obj) { return obj.m_impl.end(); }
        friend inline iterator range_end(FooWithoutSize& obj){ return obj.m_impl.end(); }

    private:
        impl_t m_impl;
    };
    
    template<typename SizeType>
    class FooWithSize
    {
    public:
        typedef SizeType size_type;
        typedef boost::uint8_t* iterator;
        typedef const boost::uint8_t* const_iterator;
        
        const_iterator begin() const;
        iterator begin();
        const_iterator end() const;
        iterator end();
    };
    
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::uint8_t,
            boost::range_size<FooWithSize<boost::uint8_t> >::type
        >::value
    ));
    
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::uint16_t,
            boost::range_size<FooWithSize<boost::uint16_t> >::type
        >::value
    ));
    
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::uint32_t,
            boost::range_size<FooWithSize<boost::uint32_t> >::type
        >::value
    ));
    
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::uint64_t,
            boost::range_size<FooWithSize<boost::uint64_t> >::type
        >::value
    ));
    
    class UdtSizeType
    {
    public:
        typedef boost::uint16_t value_type;
        
        UdtSizeType() : value_(0) { }
        UdtSizeType(value_type value) : value_(value) { }
        
        operator value_type() const { return value_; }
        
    private:
        value_type value_;
    };
    
    BOOST_STATIC_ASSERT((
        boost::is_same<
            UdtSizeType,
            boost::range_size<FooWithSize<UdtSizeType> >::type
        >::value
    ));
    
    class Foo2WithoutSize
    {
    public:
        struct const_iterator
        {
            typedef std::forward_iterator_tag iterator_category;
            typedef boost::int8_t difference_type;
            typedef boost::int16_t value_type;
            typedef value_type* pointer;
            typedef value_type& reference;
            
            reference operator*() const;
            pointer operator->() const;
            const_iterator& operator++();
            const_iterator operator++(int);
            bool operator==(const const_iterator&) const;
            bool operator!=(const const_iterator&) const;
        };
        
        struct iterator : const_iterator
        {
            typedef const value_type* pointer;
            typedef const value_type& reference;
            
            reference operator*() const;
            pointer operator->() const;
            
            iterator& operator++();
            iterator operator++(int);
            
            bool operator==(const iterator&) const;
            bool operator!=(const iterator&) const;
        };
        
        const_iterator begin() const;
        iterator begin();
        const_iterator end() const;
        iterator end();
    };
    
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::uint8_t,
            boost::range_size<
                ::boost_range_extension_size_test::Foo2WithoutSize>::type
        >::value
    ));
}

namespace boost
{
    template<> struct range_iterator<const ::boost_range_extension_size_test::FooWithoutSize>
    {
        typedef std::list<int>::const_iterator type;
    };

    template<> struct range_iterator< ::boost_range_extension_size_test::FooWithoutSize>
    {
        typedef std::list<int>::iterator type;
    };
}

namespace boost_range_extension_size_test
{
    inline boost::range_size<FooWithoutSize>::type
    range_calculate_size(const FooWithoutSize& rng)
    {
        return 2u;
    }
}

BOOST_STATIC_ASSERT((
    boost::is_same<
        boost::make_unsigned<std::ptrdiff_t>::type,
        boost::range_size<
                    boost_range_extension_size_test::FooWithoutSize>::type
    >::value
));

typedef boost::make_unsigned<std::ptrdiff_t>::type t1;
typedef boost::range_size<boost_range_extension_size_test::FooWithoutSize>::type t1;

namespace
{

void check_size_works_with_random_access()
{
    std::vector<int> container;
    container.push_back(1);
    BOOST_CHECK_EQUAL( boost::size(container), 1u );
}

void check_extension_size()
{
    BOOST_CHECK_EQUAL( boost::size(boost_range_extension_size_test::FooWithoutSize()), 2u );
}

} // anonymous namespace

using boost::unit_test::test_suite;

test_suite* init_unit_test_suite( int argc, char* argv[] )
{
    test_suite* test = BOOST_TEST_SUITE( "Range Test Suite" );

    test->add( BOOST_TEST_CASE( &check_size_works_with_random_access ));
    test->add( BOOST_TEST_CASE( &check_extension_size ) );

    return test;
}
