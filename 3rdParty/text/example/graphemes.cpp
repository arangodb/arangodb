// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/text/text.hpp>
#include <boost/text/rope.hpp>

#include <iostream>
#include <vector>

//[ graphemes_grapheme
// These functions don't do anything interesting; Just pay attention to the
// interfaces.

// This returns a view into a temporary.  Don't do this.
boost::text::grapheme_ref<boost::text::text::iterator::iterator_type>
find_first_dot_bad(boost::text::text t)
{
    for (auto grapheme : t) {
        uint32_t target[1] = {'.'};
        // Using the one from Boost.Algorithm, so we get the 4-param version
        // even if we're not using a C++14 compiler.
        if (boost::algorithm::equal(
                grapheme.begin(), grapheme.end(), target, target + 1)) {
            // This line compiles without warnings.  The compiler doesn't know
            // how to tell me I'm returning a dangling reference, because it's
            // a view, not a builtin reference.
            return grapheme;
        }
    }
    return boost::text::grapheme_ref<
        boost::text::text::iterator::iterator_type>();
}

// This returns a grapheme that owns its code point storage, so it cannot
// dangle.  Do this.
boost::text::grapheme find_first_dot_good(boost::text::text t)
{
    for (auto grapheme : t) {
        uint32_t target[1] = {'.'};
        if (boost::algorithm::equal(
                grapheme.begin(), grapheme.end(), target, target + 1)) {
            return boost::text::grapheme(grapheme);
        }
    }
    return boost::text::grapheme();
}
//]

int main ()
{

{
//[ graphemes_grapheme_ref
boost::text::text t = "A bit of text.";

// grapheme_refs should be declared as values in range-based for loops, since
// they are small value types.
for (auto grapheme : t) {
    std::cout << grapheme; // grapheme_ref is directly streamable.
    // grapheme_ref is also a code point range, of course
    for (auto cp : grapheme) {
        // Do something with code point cp here....
    }
}
std::cout << "\n";
//]
}

{
//[ graphemes_iterator_bases
// t is a GraphemeRange.
boost::text::text t = "This is a short sentence.";

// This is a code point range that contains all the same code points as t.
boost::text::utf32_view<boost::text::utf_8_to_32_iterator<char const *>> cps(
    t.begin().base(), t.end().base());

// This is achar range that contains all the same code units as t, though it
// is not null-terminated like t's underlying storage is.
std::vector<char> chars(t.begin().base().base(), t.end().base().base());
//]
}

{
//[ graphemes_text_null_termination
boost::text::text t = "This is a short sentence.";

assert(strlen(t.begin().base().base()) == 25);

t = "This is a short séance."; // é occupies two UTF-8 code units.
assert(strlen(t.begin().base().base()) == 24);
//]
}

}
