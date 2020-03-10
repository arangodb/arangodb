# Chromium inspector (devtools) protocol

This package contains code generators and templates for the Chromium
inspector protocol.

The canonical location of this package is at
https://chromium.googlesource.com/deps/inspector_protocol/

In the Chromium tree, it's rolled into
https://cs.chromium.org/chromium/src/third_party/inspector_protocol/

In the V8 tree, it's rolled into
https://cs.chromium.org/chromium/src/v8/third_party/inspector_protocol/

See also [Contributing to Chrome Devtools Protocol](https://docs.google.com/document/d/1c-COD2kaK__5iMM5SEx-PzNA7HFmgttcYfOHHX0HaOM/edit).

We're working on enabling standalone builds for parts of this package for
testing and development.
If you're familiar with
[Chromium's development process](https://www.chromium.org/developers/contributing-code)
and have the depot_tools installed, you may use these commands
to fetch the package (and dependencies) and build and run the tests:

    fetch inspector_protocol
    cd src
    gn gen out/Release
    ninja -C out/Release encoding_test bindings_test
    out/Release/encoding_test
    out/Release/bindings_test

You'll probably also need to install g++, since Clang uses this to find the
standard C++ headers. E.g.,

    sudo apt-get install g++-8
