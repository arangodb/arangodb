string(REPLACE " " ";" replacement_pairs ${replacement_pairs})

file(READ "${input_file}" text)

list(LENGTH replacement_pairs length)
math(EXPR last_index "${length} - 1")

foreach(regex_index RANGE 0 ${last_index} 2)
  math(EXPR replacement_index "${regex_index} + 1")
  LIST(GET replacement_pairs ${regex_index} regex_expression)
  LIST(GET replacement_pairs ${replacement_index} replacement_expression)
  string(REPLACE
         "${regex_expression}"
         "${replacement_expression}"
         text "${text}")
endforeach()

file(WRITE ${output_file} "${text}")
