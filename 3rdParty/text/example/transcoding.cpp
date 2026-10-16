// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/text/transcode_iterator.hpp>
#include <boost/text/transcode_view.hpp>


int main ()
{

{
//[ to_utf32_verbose
uint32_t const utf32[] = {0x004d, 0x0430, 0x4e8c, 0x10302};
char const utf8[] = {
    0x4d,
    char(0xd0),
    char(0xb0),
    char(0xe4),
    char(0xba),
    char(0x8c),
    char(0xf0),
    char(0x90),
    char(0x8c),
    char(0x82)};
int i = 0;
for (auto it = boost::text::utf32_iterator(
              std::begin(utf8), std::begin(utf8), std::end(utf8)),
          end = boost::text::utf32_iterator(
              std::begin(utf8), std::end(utf8), std::end(utf8));
     it != end;
     ++it) {
    uint32_t cp = *it;
    assert(cp == utf32[i++]);
}
//]
}

//[ to_utf32_terse
uint32_t const utf32[] = {0x004d, 0x0430, 0x4e8c, 0x10302};
char const utf8[] = {
    0x4d,
    char(0xd0),
    char(0xb0),
    char(0xe4),
    char(0xba),
    char(0x8c),
    char(0xf0),
    char(0x90),
    char(0x8c),
    char(0x82)};
int i = 0;
for (auto cp : boost::text::as_utf32(utf8)) {
    assert(cp == utf32[i++]);
}
//]

{
//[ to_utf32_ub

// That the full UTF-8 encoding for U+8D00 is 0xe8, 0xb4, 0x80.
char const broken_utf8_for_0x8d00[] = {
    char(0xe8), char(0xb4) /* Incomplete! */};
auto it = boost::text::utf32_iterator(
    std::begin(broken_utf8_for_0x8d00),
    std::begin(broken_utf8_for_0x8d00),
    std::end(broken_utf8_for_0x8d00));

assert(*it == boost::text::replacement_character());

// --it; // Undefined behavior!
++it; // Ok.  Incrementing through a broken sequence is defined.
// ++it; // Undefined behavior!  A subsequent increment past the end is not defined.

}
//]

}
