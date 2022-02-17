// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/text/trie.hpp>

#include <iostream>
#include <string>


int main ()
{

{
//[ trie_intro
// Starting with a trie, we can:
boost::text::trie<std::string, int> trie(
    {{"foo", 13}, {"bar", 17}, {"foos", 19}, {"", 42}});

// look up a specific element;
assert(trie["foo"] == 13);

// look for the node that contains as much of the given sequence as possible;
auto fo_subsequence = trie.longest_subsequence("fo");
assert(!fo_subsequence.match);

// try to extend the subsequence by one node;
auto foo_subsequence = trie.extend_subsequence(fo_subsequence, 'o');
assert(foo_subsequence.match);

// or try to extend the subsequence by multiple nodes;
auto const os_str = "os";
auto foos_subsequence =
    trie.extend_subsequence(fo_subsequence, os_str, os_str + 2);
assert(foos_subsequence.match);
//]
}

{
//[ trie_index_results
boost::text::trie<std::string, int> trie(
    {{"foo", 13}, {"bar", 17}, {"foos", 19}, {"", 42}});

// This is the kind of thing you expect from a `std::map`.
assert(trie["foo"] == 13);
trie["foo"] = 111;
assert(trie["foo"] == 111);

// What operator[] actually returns though is an optional_ref.
boost::text::optional_ref<int> element = trie["foo"];
// You can ask the optional_ref if it refers to something or not, because it
// has an explicit conversion to bool.
if (element) {
    // You can read from and write to it as if it was its template parameter
    // type T, because it has an implicit conversion to that:
    int element_as_int = element;
    assert(element_as_int == 111);
    // Use element_as_int here....
}

boost::text::trie<std::string, int> const const_trie(
    {{"foo", 13}, {"bar", 17}, {"foos", 19}, {"", 42}});

// Because it returns this optional_ref, it's okay to use with constant tries.
// operator[] does not mutate the trie, even for mutable tries.
auto element_2 = const_trie["foo"]; // Whaaaaat?
if (element_2) {
    int element_as_int = element_2;
    assert(element_as_int == 13);
    // Use element_as_int here....
}

boost::text::trie<std::string, bool> const bool_trie(
    {{"foo", true}, {"bar", true}, {"foos", false}, {"", true}});

// What about optional_ref<bool> you ask?  There are specializations for it
// that have no implcit conversions.  You just have to use the familiar
// operator* to get its bool value as you would with boost::optional or
// std::optional.
auto element_3 = bool_trie["foos"];
if (element_3) {
    // bool element_as_bool = element_3; // Error! No implicit conversion to bool.
    bool element_as_bool = *element_3;
    assert(element_as_bool == false);
    // Use element_as_bool here....
}
//]
}

}
