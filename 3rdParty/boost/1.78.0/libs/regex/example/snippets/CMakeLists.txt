# Copyright 2019 Mike Dev
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
#
# NOTE: CMake support for Boost.Regex is currently experimental at best
#       and we are currently only building a few examples

set(examples 
    partial_regex_grep
    partial_regex_iterate
    partial_regex_match
    regex_grep_example_1
    regex_grep_example_2
    regex_grep_example_3
    regex_grep_example_4
    regex_iterator_example
    regex_match_example
    regex_merge_example
    regex_replace_example
    regex_search_example
    regex_split_example_1
    regex_split_example_2
    regex_token_iterator_eg_1
    regex_token_iterator_eg_2
)

foreach( example IN LISTS examples )
    add_executable( boost_regex_ex_${example} ${example}.cpp )
    target_link_libraries( boost_regex_ex_${example} Boost::regex )
endforeach()
