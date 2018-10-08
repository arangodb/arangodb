Compiling ArangoDB from Source
===============================

ArangoDB can be compiled directly from source. It will compile on most Linux and
Mac OS X systems, as well as on Windows.

We assume that you use the GNU C/C++ compiler or clang/clang++ to compile the
source. ArangoDB has been tested with these compilers, but should be able to
compile with any Posix-compliant, C++14-enabled compiler. 

By default, cloning the GitHub repository will checkout the _devel_ branch.
This branch contains the development version of the ArangoDB. Use this branch if
you want to make changes to the ArangoDB source.

On Windows you first [need to allow and enable symlinks for your user](https://github.com/git-for-windows/git/wiki/Symbolic-Links#allowing-non-administrators-to-create-symbolic-links). 

Please checkout the [cookbook](../../Cookbook/Compiling/index.html) on how to
compile ArangoDB.
