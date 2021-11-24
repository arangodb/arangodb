#include "grammar.hpp"

auto const grammar_def = x3::int_;

BOOST_SPIRIT_DEFINE(grammar)
BOOST_SPIRIT_INSTANTIATE(grammar_type, char const*, x3::unused_type)
