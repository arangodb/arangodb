//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision$
//
//  Description : datasets test helpers
// ***************************************************************************

#ifndef BOOST_TEST_TEST_DATASETS_HPP
#define BOOST_TEST_TEST_DATASETS_HPP

// Boost
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/is_convertible.hpp>

#include <iostream>
#include <vector>
#include <list>

//____________________________________________________________________________//

class copy_count {
public:
    copy_count() {}
    copy_count( copy_count const& ) {
      value()++;
    }
    copy_count( copy_count&& ) BOOST_NOEXCEPT_OR_NOTHROW {} // noexcept is important indeed
    copy_count( copy_count const&& ) {}
//    ~copy_count() { std::cout << "~copy_count" << std::endl; }

    static int& value()         { static int s_value; return s_value; };

    static copy_count make()    { return copy_count(); }
    static copy_count const make_const() { return copy_count(); }
};

//____________________________________________________________________________//

template<typename T>
struct check_arg_type {
    template<typename S>
    void    operator()( S const& ) const
    {
        BOOST_CHECK_MESSAGE( (boost::is_same<S,T>::value), "Sample type does not match expected" );
    }
};

//____________________________________________________________________________//

template<typename T1, typename T2>
struct check_arg_type<std::tuple<T1,T2>> {
    template<typename S1, typename S2>
    void    operator()( S1 const&, S2 const& ) const
    {
        BOOST_CHECK_MESSAGE( (boost::is_same<S1,T1>::value), "S1 type does not match expected" );
        BOOST_CHECK_MESSAGE( (boost::is_same<S2,T2>::value), "S2 type does not match expected" );
    }
};

//____________________________________________________________________________//

template<typename T1, typename T2, typename T3>
struct check_arg_type<std::tuple<T1,T2,T3>> {
    template<typename S1, typename S2, typename S3>
    void    operator()( S1 const&, S2 const&, S3 const& ) const
    {
        BOOST_CHECK_MESSAGE( (boost::is_same<S1,T1>::value), "S1 type does not match expected" );
        BOOST_CHECK_MESSAGE( (boost::is_same<S2,T2>::value), "S2 type does not match expected" );
        BOOST_CHECK_MESSAGE( (boost::is_same<S3,T3>::value), "S3 type does not match expected" );
    }
};

//____________________________________________________________________________//

template<typename T>
struct check_arg_type_like {
    template<typename S>
    void    operator()( S const& ) const
    {
        BOOST_CHECK_MESSAGE( (boost::is_convertible<S,T>::value), "Sample type does not match expected" );
    }
};

//____________________________________________________________________________//

template<typename T1, typename T2>
struct check_arg_type_like<std::tuple<T1,T2>> {
    template<typename S1, typename S2>
    void    operator()( S1 const&, S2 const& ) const
    {
        BOOST_CHECK_MESSAGE( (boost::is_convertible<S1,T1>::value), "S1 type does not match expected" );
        BOOST_CHECK_MESSAGE( (boost::is_convertible<S2,T2>::value), "S2 type does not match expected" );
    }
};

//____________________________________________________________________________//

template<typename T1, typename T2, typename T3>
struct check_arg_type_like<std::tuple<T1,T2,T3>> {
    template<typename S1, typename S2, typename S3>
    void    operator()( S1 const&, S2 const&, S3 const& ) const
    {
        BOOST_CHECK_MESSAGE( (boost::is_convertible<S1,T1>::value), "S1 type does not match expected" );
        BOOST_CHECK_MESSAGE( (boost::is_convertible<S2,T2>::value), "S2 type does not match expected" );
        BOOST_CHECK_MESSAGE( (boost::is_convertible<S3,T3>::value), "S3 type does not match expected" );
    }
};

//____________________________________________________________________________//

struct invocation_count {
    invocation_count() : m_value( 0 ) {}

    template<typename S>
    void    operator()( S const& ) const
    {
        m_value++;
    }
    template<typename S1,typename S2>
    void    operator()( S1 const&, S2 const& ) const
    {
        m_value++;
    }
    template<typename S1,typename S2,typename S3>
    void    operator()( S1 const&, S2 const&, S3 const& ) const
    {
        m_value++;
    }

    mutable int    m_value;

private:
    invocation_count(invocation_count const&);
};

//____________________________________________________________________________//

struct expected_call_count {
    explicit expected_call_count( int exp_count )
    : m_call_count( 0 )
    , m_exp_count( exp_count )
    {}
    expected_call_count(expected_call_count const&) = delete;
    void operator=(expected_call_count const&) = delete;

    ~expected_call_count()
    {
        BOOST_TEST( m_exp_count == m_call_count );
    }

    template<typename S>
    void    operator()( S const& ) const
    {
        m_call_count++;
    }
    template<typename S1,typename S2>
    void    operator()( S1 const&, S2 const& ) const
    {
        m_call_count++;
    }
    template<typename S1,typename S2,typename S3>
    void    operator()( S1 const&, S2 const&, S3 const& ) const
    {
        m_call_count++;
    }

    mutable int    m_call_count;
    int            m_exp_count;

};

//____________________________________________________________________________//

struct print_sample {
    template<typename T>
    void    operator()( T const& sample ) const
    {
        std::cout << "S has type: " << typeid(T).name() << " and value " << sample << std::endl;
    }
};

//____________________________________________________________________________//

inline std::vector<copy_count>
make_copy_count_collection()
{
    return std::vector<copy_count>( 3 );
}

//____________________________________________________________________________//

inline std::list<copy_count> const
make_copy_count_const_collection()
{
    return std::list<copy_count>( 3 );
}

//____________________________________________________________________________//

#endif // BOOST_TEST_TEST_DATASETS_HPP
