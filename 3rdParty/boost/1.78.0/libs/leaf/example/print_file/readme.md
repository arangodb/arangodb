# Print File Example

This directory has three versions of the same simple program, which reads a
file, prints it to standard out and handles errors using LEAF, each using a
different variation on error handling:

* [print_file_result.cpp](./print_file_result.cpp) reports errors with
  `leaf::result<T>`, using an error code `enum` for classification of failures.

* [print_file_outcome_result.cpp](./print_file_outcome_result.cpp) is the same
  as the above, but using `outcome::result<T>`. This demonstrates the ability
  to transport arbitrary error objects through APIs that do not use
  `leaf::result<T>`.

* [print_file_eh.cpp](./print_file_eh.cpp) throws on error, using an error code
  `enum` for classification of failures.
