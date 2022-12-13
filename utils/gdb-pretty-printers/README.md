Run `./utils/build_and_run_tests.sh` to test the pretty printers. The tests currently don't run automatically in the CI. I've tested it only on Linux. It's to be expected that the tests might break if not built with `CMAKE_BUILD_TYPE=Debug` (we're testing `gdb`, after all).

See the `gdbinit` files (e.g. `./utils/gdb-pretty-printers/immer/gdbinit`) for an example to write in your `~/.gdbinit` to load the pretty printers. You will have to adapt the path.

Currently there's only one pretty printer, for `immer::flex_vector`. We might want to include the existing VelocyPack one, and possibly add more later.
