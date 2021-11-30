# Print File Example

This directory has several versions of the same simple program (which reads a
file, prints it to standard out and handles errors using LEAF) presented in the
[Five Minute Introduction](https://boostorg.github.io/leaf/#introduction), each
using a different error handling style:

## Using exception handling:

* [print_file_eh.cpp](./print_file_eh.cpp) throws on error, using an exception
  type hierarchy for classification of failures.

* [print_file_eh_error_tags.cpp](./print_file_eh_error_tags.cpp) throws
  `std::exception` on error, using a
  [novel mechanism](https://boostorg.github.io/leaf/#tutorial-classification)
  for classification of failures.

## Using a `result<T>` type:

* [print_file_result.cpp](./print_file_result.cpp) reports errors with
  `leaf::result<T>`, using an error code `enum` for classification of failures.

* [print_file_outcome_result.cpp](./print_file_outcome_result.cpp) is the same
  as the above, but using `outcome::result<T>`. This demonstrates the ability
  to transport arbitrary error objects through APIs that do not use
  `leaf::result<T>`.

* [print_file_result_error_tags.cpp](./print_file_result_error_tags.cpp)
  reports errors with `leaf::result<T>`, using a
  [novel mechanism](https://boostorg.github.io/leaf/#tutorial-classification)
  for classification of failures.
