//  Copyright (c) 2016 Jeffrey E. Trull
// 
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// A series of simple tests for the istream_iterator

#include <boost/detail/lightweight_test.hpp>

#include <sstream>

#include <boost/spirit/include/support_istream_iterator.hpp>

int main()
{
  std::stringstream ss("HELO\n");
  boost::spirit::istream_iterator it(ss);

  // Check iterator concepts
  boost::spirit::istream_iterator it2(it);  // CopyConstructible
  BOOST_TEST( it2 == it );                  // EqualityComparable
  BOOST_TEST( *it2 == 'H' );

  boost::spirit::istream_iterator end;      // DefaultConstructible
  BOOST_TEST( it != end );
  it = end;                                 // CopyAssignable
  BOOST_TEST( it == end );

  std::swap(it, it2);                       // Swappable
  BOOST_TEST( it2 == end );
  BOOST_TEST( *it == 'H' );
  
  ++it;
  BOOST_TEST( *it == 'E' );
  BOOST_TEST( *it++ == 'E' );

  // "Incrementing a copy of a does not change the value read from a"
  boost::spirit::istream_iterator it3 = it;
  BOOST_TEST( *it == 'L' );
  BOOST_TEST( *it3 == 'L' );
  ++it;
  BOOST_TEST( *it == 'O' );
  BOOST_TEST( *it3 == 'L' );

  it3 = it;
  // "a == b implies ++a == ++b"
  BOOST_TEST( ++it3 == ++it );

  // Usage of const iterators
  boost::spirit::istream_iterator const itc = it;
  BOOST_TEST( itc == it );
  BOOST_TEST( *itc == *it );
  ++it;
  BOOST_TEST( itc != it );

  // Skipping le/gt comparisons as unclear what they are for in forward iterators...

  return boost::report_errors();
}
// Destructible
