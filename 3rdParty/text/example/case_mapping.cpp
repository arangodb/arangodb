// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/text/case_mapping.hpp>
#include <boost/text/in_place_case_mapping.hpp>

#include <iostream>


int main ()
{

{
//[ case_mapping_1
std::array<uint32_t, 4> cps = {{'A', 'n', 'd'}};

assert(!boost::text::is_lower(cps));
assert(boost::text::is_title(cps));
assert(!boost::text::is_upper(cps));

std::array<uint32_t, 4> lowered_cps;
boost::text::to_lower(cps, lowered_cps.begin());
//]
}

{
//[ case_mapping_2
boost::text::text t = "ijssel";

boost::text::text default_titled_t;
boost::text::to_title(
    t, std::inserter(default_titled_t, default_titled_t.end()));
assert(default_titled_t == boost::text::text("Ijssel"));

std::string dutch_titled_t;
boost::text::to_title(
    t,
    boost::text::from_utf32_inserter(dutch_titled_t, dutch_titled_t.end()),
    boost::text::case_language::dutch);
assert(dutch_titled_t == "IJssel");
//]
}

{
//[ case_mapping_3
boost::text::text t = "a title";
boost::text::in_place_to_upper(t);
assert(t == boost::text::text("A TITLE"));

boost::text::rope r = "another title";
boost::text::in_place_to_title(r);
assert(r == boost::text::text("Another Title"));
//]
}

}
