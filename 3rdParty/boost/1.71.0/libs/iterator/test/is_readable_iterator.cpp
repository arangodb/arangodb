// Copyright David Abrahams 2003. Use, modification and distribution is
// subject to the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <deque>
#include <iterator>
#include <iostream>
#include <cstddef> // std::ptrdiff_t
#include <boost/static_assert.hpp>
#include <boost/noncopyable.hpp>
#include <boost/iterator/is_readable_iterator.hpp>

// Last, for BOOST_NO_LVALUE_RETURN_DETECTION
#include <boost/iterator/detail/config_def.hpp>

struct v
{
    v();
    ~v();
};


struct value_iterator
{
    typedef std::input_iterator_tag iterator_category;
    typedef v value_type;
    typedef std::ptrdiff_t difference_type;
    typedef v* pointer;
    typedef v& reference;

    v operator*() const;
};

struct noncopyable_iterator
{
    typedef std::forward_iterator_tag iterator_category;
    typedef boost::noncopyable value_type;
    typedef std::ptrdiff_t difference_type;
    typedef boost::noncopyable* pointer;
    typedef boost::noncopyable& reference;
	
    boost::noncopyable const& operator*() const;
};

struct proxy_iterator
{
    typedef std::output_iterator_tag iterator_category;
    typedef v value_type;
    typedef std::ptrdiff_t difference_type;
    typedef v* pointer;
    typedef v& reference;
    
    struct proxy
    {
        operator v&();
        proxy& operator=(v) const;
    };
        
    proxy operator*() const;
};

struct proxy_iterator2
{
    typedef std::output_iterator_tag iterator_category;
    typedef v value_type;
    typedef std::ptrdiff_t difference_type;
    typedef v* pointer;
    typedef v& reference;
    
    struct proxy
    {
        proxy& operator=(v) const;
    };
        
    proxy operator*() const;
};


int main()
{
    BOOST_STATIC_ASSERT(boost::is_readable_iterator<v*>::value);
    BOOST_STATIC_ASSERT(boost::is_readable_iterator<v const*>::value);
    BOOST_STATIC_ASSERT(boost::is_readable_iterator<std::deque<v>::iterator>::value);
    BOOST_STATIC_ASSERT(boost::is_readable_iterator<std::deque<v>::const_iterator>::value);
    BOOST_STATIC_ASSERT(!boost::is_readable_iterator<std::back_insert_iterator<std::deque<v> > >::value);
    BOOST_STATIC_ASSERT(!boost::is_readable_iterator<std::ostream_iterator<v> >::value);
    BOOST_STATIC_ASSERT(boost::is_readable_iterator<proxy_iterator>::value);
    BOOST_STATIC_ASSERT(!boost::is_readable_iterator<proxy_iterator2>::value);
    BOOST_STATIC_ASSERT(boost::is_readable_iterator<value_iterator>::value);
    
    // Make sure inaccessible copy constructor doesn't prevent
    // readability
    BOOST_STATIC_ASSERT(boost::is_readable_iterator<noncopyable_iterator>::value);
    
    return 0;
}
