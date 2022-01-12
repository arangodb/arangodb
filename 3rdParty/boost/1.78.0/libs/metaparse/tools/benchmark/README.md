This directory contains benchmarks for the library.

The characters to use in the benchmarks and their distribution is coming from
`chars.py`. This is an automatically generated file and can be regenerated using
`char_stat.py`. It represents the distribution of characters of the Boost 1.61.0
header files.

To regenerate the benchmarks:

* Generate the source files by running `generate.py`. Unless specified
  otherwise, it will generate the source files found in `src` to `generated`.
* Run the benchmarks by running `benchmark.py`. Unless specified otherwise, it
  will benchmark the compilation of the source files in `generated` and generate
  the diagrams into the library's documentation.
