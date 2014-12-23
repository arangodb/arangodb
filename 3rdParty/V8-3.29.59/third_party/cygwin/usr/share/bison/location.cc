m4_divert(-1)

# C++ skeleton for Bison

# Copyright (C) 2002, 2003, 2004, 2005, 2006 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301  USA

# We do want M4 expansion after # for CPP macros.
m4_changecom()
m4_divert(0)dnl
@output b4_dir_prefix[]position.hh
b4_copyright([Positions for Bison parsers in C++],
  [2002, 2003, 2004, 2005, 2006])[

/**
 ** \file position.hh
 ** Define the ]b4_namespace[::position class.
 */

#ifndef BISON_POSITION_HH
# define BISON_POSITION_HH

# include <iostream>
# include <string>

namespace ]b4_namespace[
{
  /// Abstract a position.
  class position
  {
  public:
]m4_ifdef([b4_location_constructors], [
    /// Construct a position.
    position ()
      : filename (0), line (1), column (0)
    {
    }

])[
    /// Initialization.
    inline void initialize (]b4_filename_type[* fn)
    {
      filename = fn;
      line = 1;
      column = 0;
    }

    /** \name Line and Column related manipulators
     ** \{ */
  public:
    /// (line related) Advance to the COUNT next lines.
    inline void lines (int count = 1)
    {
      column = 0;
      line += count;
    }

    /// (column related) Advance to the COUNT next columns.
    inline void columns (int count = 1)
    {
      int leftmost = 0;
      int current  = column;
      if (leftmost <= current + count)
	column += count;
      else
	column = 0;
    }
    /** \} */

  public:
    /// File name to which this position refers.
    ]b4_filename_type[* filename;
    /// Current line number.
    unsigned int line;
    /// Current column number.
    unsigned int column;
  };

  /// Add and assign a position.
  inline const position&
  operator+= (position& res, const int width)
  {
    res.columns (width);
    return res;
  }

  /// Add two position objects.
  inline const position
  operator+ (const position& begin, const int width)
  {
    position res = begin;
    return res += width;
  }

  /// Add and assign a position.
  inline const position&
  operator-= (position& res, const int width)
  {
    return res += -width;
  }

  /// Add two position objects.
  inline const position
  operator- (const position& begin, const int width)
  {
    return begin + -width;
  }

  /** \brief Intercept output stream redirection.
   ** \param ostr the destination output stream
   ** \param pos a reference to the position to redirect
   */
  inline std::ostream&
  operator<< (std::ostream& ostr, const position& pos)
  {
    if (pos.filename)
      ostr << *pos.filename << ':';
    return ostr << pos.line << '.' << pos.column;
  }

}
#endif // not BISON_POSITION_HH]
@output b4_dir_prefix[]location.hh
b4_copyright([Locations for Bison parsers in C++],
  [2002, 2003, 2004, 2005, 2006])[

/**
 ** \file location.hh
 ** Define the ]b4_namespace[::location class.
 */

#ifndef BISON_LOCATION_HH
# define BISON_LOCATION_HH

# include <iostream>
# include <string>
# include "position.hh"

namespace ]b4_namespace[
{

  /// Abstract a location.
  class location
  {
  public:
]m4_ifdef([b4_location_constructors], [
    /// Construct a location.
    location ()
      : begin (), end ()
    {
    }

])[
    /// Initialization.
    inline void initialize (]b4_filename_type[* fn)
    {
      begin.initialize (fn);
      end = begin;
    }

    /** \name Line and Column related manipulators
     ** \{ */
  public:
    /// Reset initial location to final location.
    inline void step ()
    {
      begin = end;
    }

    /// Extend the current location to the COUNT next columns.
    inline void columns (unsigned int count = 1)
    {
      end += count;
    }

    /// Extend the current location to the COUNT next lines.
    inline void lines (unsigned int count = 1)
    {
      end.lines (count);
    }
    /** \} */


  public:
    /// Beginning of the located region.
    position begin;
    /// End of the located region.
    position end;
  };

  /// Join two location objects to create a location.
  inline const location operator+ (const location& begin, const location& end)
  {
    location res = begin;
    res.end = end.end;
    return res;
  }

  /// Add two location objects.
  inline const location operator+ (const location& begin, unsigned int width)
  {
    location res = begin;
    res.columns (width);
    return res;
  }

  /// Add and assign a location.
  inline location& operator+= (location& res, unsigned int width)
  {
    res.columns (width);
    return res;
  }

  /** \brief Intercept output stream redirection.
   ** \param ostr the destination output stream
   ** \param loc a reference to the location to redirect
   **
   ** Avoid duplicate information.
   */
  inline std::ostream& operator<< (std::ostream& ostr, const location& loc)
  {
    position last = loc.end - 1;
    ostr << loc.begin;
    if (last.filename
	&& (!loc.begin.filename
	    || *loc.begin.filename != *last.filename))
      ostr << '-' << last;
    else if (loc.begin.line != last.line)
      ostr << '-' << last.line  << '.' << last.column;
    else if (loc.begin.column != last.column)
      ostr << '-' << last.column;
    return ostr;
  }

}

#endif // not BISON_LOCATION_HH]
m4_divert(-1)
m4_changecom([#])
