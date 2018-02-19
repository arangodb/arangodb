The headers in this directory provide facilities for automatic unit testing.
Basically, each header defines unit tests for an algorithm or a set of related
algorithms. To get the tests for these algorithms, simply include the header
at global scope. However, before including the header, you must define the
following macros:

    `MAKE_TUPLE(...)`
        Must expand to a sequence holding `__VA_ARGS__`. A valid definition
        would be `hana::make_tuple(__VA_ARGS__)`.

    `TUPLE_TYPE(...)`
        Must expand to the type of a sequence holding objects of type `__VA_ARGS__`.
        A valid definition would be `hana::tuple<__VA_ARGS__>`.

    `TUPLE_TAG`
        Must expand to the tag of the sequence. A valid definition would
        be `hana::tuple_tag`.


The following macros may or may not be defined:

    `MAKE_TUPLE_NO_CONSTEXPR`
        Must be defined if the `MAKE_TUPLE` macro can't be used inside a
        constant expression. Otherwise, `MAKE_TUPLE` is assumed to be able
        to construct a `constexpr` container.

The following directories contain automatic unit tests, and the following is
sufficient when adding a new automatic unit test (in a file `${FILE}`):

```sh
DIRECTORIES=(
    test/basic_tuple/auto
    test/ext/boost/fusion/deque/auto
    test/ext/boost/fusion/list/auto
    test/ext/boost/fusion/tuple/auto
    test/ext/boost/fusion/vector/auto
    test/ext/boost/tuple/auto
    test/ext/std/tuple/auto
    test/tuple/auto
)

for d in ${DIRECTORIES}; do
    cat > ${d}/${FILE}.cpp <<EOF
// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include "_specs.hpp"
#include <auto/${FILE}.hpp>

int main() { }
EOF
done
```
