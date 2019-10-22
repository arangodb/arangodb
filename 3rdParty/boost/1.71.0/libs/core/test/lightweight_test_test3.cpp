//
// Test for lightweight_test.hpp
//
// Copyright (c) 2014, 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/core/lightweight_test.hpp>
#include <boost/core/noncopyable.hpp>
#include <ostream>

// EQ

struct eq1: private boost::noncopyable {};
struct eq2: private boost::noncopyable {};

std::ostream& operator<<( std::ostream& os, eq1 const& )
{
    return os << "eq1()";
}

std::ostream& operator<<( std::ostream& os, eq2 const& )
{
    return os << "eq2()";
}

bool operator==( eq1 const&, eq2 const& )
{
    return true;
}

// NE

struct ne1: private boost::noncopyable {};
struct ne2: private boost::noncopyable {};

std::ostream& operator<<( std::ostream& os, ne1 const& )
{
    return os << "ne1()";
}

std::ostream& operator<<( std::ostream& os, ne2 const& )
{
    return os << "ne2()";
}

bool operator!=( ne1 const&, ne2 const& )
{
    return true;
}

// LT

struct lt1: private boost::noncopyable {};
struct lt2: private boost::noncopyable {};

std::ostream& operator<<( std::ostream& os, lt1 const& )
{
    return os << "lt1()";
}

std::ostream& operator<<( std::ostream& os, lt2 const& )
{
    return os << "lt2()";
}

bool operator<( lt1 const&, lt2 const& )
{
    return true;
}

// LE

struct le1: private boost::noncopyable {};
struct le2: private boost::noncopyable {};

std::ostream& operator<<( std::ostream& os, le1 const& )
{
    return os << "le1()";
}

std::ostream& operator<<( std::ostream& os, le2 const& )
{
    return os << "le2()";
}

bool operator<=( le1 const&, le2 const& )
{
    return true;
}

// GT

struct gt1: private boost::noncopyable {};
struct gt2: private boost::noncopyable {};

std::ostream& operator<<( std::ostream& os, gt1 const& )
{
    return os << "gt1()";
}

std::ostream& operator<<( std::ostream& os, gt2 const& )
{
    return os << "gt2()";
}

bool operator>( gt1 const&, gt2 const& )
{
    return true;
}

// GE

struct ge1: private boost::noncopyable {};
struct ge2: private boost::noncopyable {};

std::ostream& operator<<( std::ostream& os, ge1 const& )
{
    return os << "ge1()";
}

std::ostream& operator<<( std::ostream& os, ge2 const& )
{
    return os << "ge2()";
}

bool operator>=( ge1 const&, ge2 const& )
{
    return true;
}

//

int main()
{
    BOOST_TEST_EQ( eq1(), eq2() );
    BOOST_TEST_NE( ne1(), ne2() );
    BOOST_TEST_LT( lt1(), lt2() );
    BOOST_TEST_LE( le1(), le2() );
    BOOST_TEST_GT( gt1(), gt2() );
    BOOST_TEST_GE( ge1(), ge2() );

    return boost::report_errors();
}
