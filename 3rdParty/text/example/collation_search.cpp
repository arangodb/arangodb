// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/text/collation_search.hpp>
#include <boost/text/text.hpp>
#include <boost/text/data/da.hpp>

#include <iostream>

//[ collation_search_string
boost::text::text const string =
    (char const *)
    u8"Århus changed the way they spell the name of their town, which has had "
    u8"the same name for centuries.  What's going on in those city council "
    u8"meetings?";
//]


int main ()
{

{
//[ collation_search_covenience
boost::text::collation_table default_table =
    boost::text::default_collation_table();

boost::text::text const pattern = "What";

auto const result =
    boost::text::collation_search(string, pattern, default_table);

// Prints "Found 'What' at [100, 104): What".
std::cout << "Found '" << pattern << "' at ["
          << std::distance(string.begin(), result.begin()) << ", "
          << std::distance(string.begin(), result.end())
          << "): " << boost::text::text(result) << "\n";
//]
}

{
//[ collation_search_simple
boost::text::collation_table default_table =
    boost::text::default_collation_table();

boost::text::text const pattern = "What";

auto const searcher =
    boost::text::make_simple_collation_searcher(pattern, default_table);
auto const result = boost::text::collation_search(string, searcher);

// Prints "Found 'What' at [100, 104): What".
std::cout << "Found '" << pattern << "' at ["
          << std::distance(string.begin(), result.begin()) << ", "
          << std::distance(string.begin(), result.end())
          << "): " << boost::text::text(result) << "\n";
//]
}

{
//[ collation_search_bmh
boost::text::collation_table default_table =
    boost::text::default_collation_table();

boost::text::text const pattern = "What";

auto const searcher = boost::text::make_boyer_moore_horspool_collation_searcher(
    pattern, default_table);
auto const result = boost::text::collation_search(string, searcher);

// Prints "Found 'What' at [100, 104): What".
std::cout << "Found '" << pattern << "' at ["
          << std::distance(string.begin(), result.begin()) << ", "
          << std::distance(string.begin(), result.end())
          << "): " << boost::text::text(result) << "\n";
//]
}

{
//[ collation_search_ignore_case
boost::text::collation_table default_table =
    boost::text::default_collation_table();

boost::text::text const pattern = "what";

auto const searcher = boost::text::make_boyer_moore_horspool_collation_searcher(
    pattern, default_table, boost::text::collation_flags::ignore_case);
auto const result = boost::text::collation_search(string, searcher);

// Prints "Found 'what' at [100, 104): What".
std::cout << "Found '" << pattern << "' at ["
          << std::distance(string.begin(), result.begin()) << ", "
          << std::distance(string.begin(), result.end())
          << "): " << boost::text::text(result) << "\n";
//]
}

{
//[ collation_search_da
boost::text::collation_table da_table = boost::text::tailored_collation_table(
    boost::text::data::da::standard_collation_tailoring());

boost::text::text const pattern = "Aarhus";
assert(pattern.distance() == 6); // 6 graphemes.

// The tailoring for Danish creates a tertiary-difference between Å and Aa;
// this implies that they are the same at secondary and primary strengths.  By
// ignoring case, we ensure that we only use secondary strength or higher.
auto const result = boost::text::collation_search(
    string, pattern, da_table, boost::text::collation_flags::ignore_case);

// We found what we were looking for, but it is not what we started with.
// Collation matches can be longer or shorter than the pattern matched, so we
// return a range instead of an iterator from all the collation search
// functions.
assert(std::distance(result.begin(), result.end()) == 5); // 5 graphemes!

// Prints "Found 'Aarhus' at [0, 5), but it looks like this: Århus".
std::cout << "Found '" << pattern << "' at ["
          << std::distance(string.begin(), result.begin()) << ", "
          << std::distance(string.begin(), result.end())
          << "), but it looks like this: " << boost::text::text(result) << "\n";
//]
}

}
