#include <boost/spirit/home/x3.hpp>

namespace x3 = boost::spirit::x3;

x3::rule<struct grammar_r, int> const grammar;
using grammar_type = decltype(grammar);
BOOST_SPIRIT_DECLARE(grammar_type)
