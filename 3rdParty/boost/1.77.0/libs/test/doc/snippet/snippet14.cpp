//  (C) Copyright Gennadiy Rozental 2001-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at 
//  http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://www.boost.org/libs/test for the library home page.
//

//[snippet14
class const_string {
public:
  // Constructors
  const_string();
  const_string( std::string const& s )
  const_string( char const* s );
  const_string( char const* s, size_t length );
  const_string( char const* begin, char const* end );

  // Access methods
  char const* data() const;
  size_t      length() const;
  bool        is_empty() const;

  // ...
};
//]
