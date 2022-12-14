# Enable pretty printers

See the `gdbinit` files (e.g. `./utils/gdb-pretty-printers/immer/gdbinit`) for an example to write in your `~/.gdbinit`
to load the pretty printers. Copy its content to (the end of your) `~/.gdbinit` file, and adapt the path passed
to `sys.path.insert`.

# Run tests

Run `./utils/build_and_run_tests.sh` to test the pretty printers. The tests currently don't run automatically in the CI.
I've tested them only on Linux. It's to be expected that the tests might break if not built
with `CMAKE_BUILD_TYPE=Debug` (we're testing `gdb`, after all).

# Extend tests

The `flex_vector_test` is executed by `ctest`, and consists of two files. `flex_vector_test.cpp`, which sets up both `testee` and `expected` data structures. And `flex_vector_test.gdbscript`, which runs the `flex_vector` pretty printer on each `testee` and compares it to `expected`. 

# Future work

Currently there's only one pretty printer, for `immer::flex_vector` (to be precise, there's also one
for `immer::details::rbts::rrbtree`, which is the implementation of `immer::flex_vector`). We might want to include the
existing VelocyPack one, and possibly add more later.
