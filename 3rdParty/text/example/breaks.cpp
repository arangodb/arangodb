// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/text/grapheme_break.hpp>
#include <boost/text/word_break.hpp>
#include <boost/text/line_break.hpp>
#include <boost/text/text.hpp>
#include <boost/text/string_utility.hpp>
#include <boost/text/estimated_width.hpp>

#include <iostream>


int main ()
{

{
//[ grapheme_breaks

// U+0308 COMBINING ACUTE ACCENT
std::array<uint32_t, 3> cps = {{'a', 0x0308, 'b'}};

auto const first = cps.begin();
auto const last = cps.end();

auto at_or_before_1 = boost::text::prev_grapheme_break(first, first + 1, last);
assert(at_or_before_1 == first + 0);

auto at_or_before_2 = boost::text::prev_grapheme_break(cps, first + 2);
assert(at_or_before_2 == first + 2);

auto at_or_before_3 = boost::text::prev_grapheme_break(first, first + 3, last);
assert(at_or_before_3 == first + 2);

auto after_0 = boost::text::next_grapheme_break(first, last);
assert(after_0 == first + 2);

// Prints "[0, 2) [2, 3)".
for (auto range : boost::text::graphemes(cps)) {
    std::cout << '[' << (range.begin() - first) << ", " << (range.end() - first)
              << ") ";
}
std::cout << "\n";

// Prints "[2, 3) [0, 2)".
for (auto range : boost::text::reversed_graphemes(cps)) {
    std::cout << '[' << (range.begin() - first) << ", " << (range.end() - first)
              << ") ";
}
std::cout << "\n";
//]
}

{



//[ word_breaks_1
// Using GraphemeRange/GraphemeIterator overloads...
boost::text::text cps("The quick (\"brown\") fox can’t jump 32.3 feet, right?");

auto const first = cps.begin();

//auto at_or_before_1 = boost::text::prev_word_break(cps, std::next(first, 1));
//assert(at_or_before_1 == std::next(first, 0));
//
//auto at_or_before_3 = boost::text::prev_word_break(cps, std::next(first, 3));
//assert(at_or_before_3 == std::next(first, 3));
//
//auto after_0 = boost::text::next_word_break(cps, first);
//assert(after_0 == std::next(first, 3));
//
//auto around_7 = boost::text::word(cps, std::next(first, 7));
//assert(around_7.begin() == std::next(first, 4));
//assert(around_7.end() == std::next(first, 9));

// Prints the indices of the words from the table above.
for (auto range : boost::text::words(cps)) {
    std::cerr<< '[' << std::distance(first, range.begin()) << ", "
              << std::distance(first, range.end()) << ") ";
}
std::cout << "\n";

// Prints the indices of the words from the table above, backward.
for (auto range : boost::text::reversed_words(cps)) {
    std::cout << '[' << std::distance(first, range.begin()) << ", "
              << std::distance(first, range.end()) << ") ";
}
std::cout << "\n";
//]
}

{
//[ word_breaks_2
boost::text::text cps("out-of-the-box");

// Prints "out - of - the - box".
for (auto range : boost::text::words(cps)) {
    std::cout << boost::text::text_view(range.begin(), range.end()) << " ";
}
std::cout << "\n";

auto const custom_word_prop = [](uint32_t cp) {
    if (cp == '-')
        return boost::text::word_property::MidLetter; // '-' becomes a MidLetter
    return boost::text::word_prop(cp); // Otherwise, just use the default implementation.
};

// Prints "out-of-the-box".
for (auto range : boost::text::words(cps, custom_word_prop)) {
    std::cout << boost::text::text_view(range.begin(), range.end()) << " ";
}
std::cout << "\n";
//]
}

{
//[ word_breaks_3
boost::text::text cps("snake_case camelCase");

// Prints "snake_case   camelCase".
for (auto range : boost::text::words(cps)) {
    std::cout << boost::text::text_view(range.begin(), range.end()) << " ";
}
std::cout << "\n";

// Break up words into chunks as if they were parts of identifiers in a
// popular programming language.
auto const identifier_break = [](uint32_t prev_prev,
                                 uint32_t prev,
                                 uint32_t curr,
                                 uint32_t next,
                                 uint32_t next_next) {
    if ((prev == '_') != (curr == '_'))
        return true;
    if (0x61 <= prev && prev <= 0x7a && 0x41 <= curr && curr <= 0x5a)
        return true;
    return false;
};

// Prints "snake _ case   camel Case".
for (auto range :
     boost::text::words(cps, boost::text::word_prop, identifier_break)) {
    std::cout << boost::text::text_view(range.begin(), range.end()) << " ";
}
std::cout << "\n";
//]
}

{
//[ line_breaks_1
std::array<uint32_t, 5> cps = {{'a', ' ', 'b', '\n', 'c'}};

auto const first = cps.begin();
auto const last = cps.end();

// prev_/next_hard_line_break() returns an iterator.
auto at_or_before_2_hard =
    boost::text::prev_hard_line_break(first, first + 2, last);
assert(at_or_before_2_hard == first + 0);

auto after_0_hard = boost::text::next_hard_line_break(first, last);
assert(after_0_hard == first + 4);

// prev_/next_allowed_line_break() returns a line_break_result<CPIter>.
auto at_or_before_2_allowed =
    boost::text::prev_allowed_line_break(first, first + 2, last);
assert(at_or_before_2_allowed.iter == first + 2);
assert(!at_or_before_2_allowed.hard_break);

auto after_0_allowed = boost::text::next_allowed_line_break(first, last);
assert(after_0_allowed.iter == first + 2);
assert(!after_0_allowed.hard_break);

// operator==() and operator!=() are defined between line_break_result<CPIter>
// and CPIter.
assert(at_or_before_2_allowed == first + 2);
assert(first + 2 == after_0_allowed);
//]
}

{
//[ line_breaks_2
std::array<uint32_t, 5> cps = {{'a', ' ', 'b', '\n', 'c'}};

/* Prints:
"c"
"b
"a "
*/
for (auto line : boost::text::reversed_allowed_lines(cps)) {
    std::cout << '"' << boost::text::to_string(line.begin(), line.end()) << '"';
    // Don't add \n to a hard line break; it already has one!
    if (!line.hard_break())
        std::cout << "\n";
}
//]
}

{
//[ line_breaks_3
boost::text::text const cps =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
    "commodo consequat. Duis aute irure dolor in reprehenderit in voluptate "
    "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
    "occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
    "mollit anim id est laborum.";

/* Prints:
************************************************************
Lorem ipsum dolor sit amet, consectetur adipiscing elit, 
sed do eiusmod tempor incididunt ut labore et dolore magna 
aliqua. Ut enim ad minim veniam, quis nostrud exercitation 
ullamco laboris nisi ut aliquip ex ea commodo consequat. 
Duis aute irure dolor in reprehenderit in voluptate velit 
esse cillum dolore eu fugiat nulla pariatur. Excepteur sint 
occaecat cupidatat non proident, sunt in culpa qui officia 
deserunt mollit anim id est laborum.
************************************************************
*/
std::cout << "************************************************************\n";
for (auto line : boost::text::lines(
         cps,
         60,
         [](boost::text::text::const_iterator::iterator_type first,
            boost::text::text::const_iterator::iterator_type last)
             -> std::ptrdiff_t {
             // estimated_width_of_graphemes() uses the same table-based width
             // determination that std::format() uses.  You can use anything
             // here, even the width of individual code points in a particular
             // font.
             return boost::text::estimated_width_of_graphemes(first, last);
         })) {
    std::cout << boost::text::text_view(line.begin(), line.end());
    if (!line.hard_break())
        std::cout << "\n";
}
std::cout << "************************************************************\n";
//]
}

}
