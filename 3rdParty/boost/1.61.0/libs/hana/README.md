# Boost.Hana <a target="_blank" href="http://semver.org">![Version][badge.version]</a> <a target="_blank" href="https://travis-ci.org/boostorg/hana">![Travis status][badge.Travis]</a> <a target="_blank" href="https://ci.appveyor.com/project/ldionne/hana">![Appveyor status][badge.Appveyor]</a> <a target="_blank" href="http://melpon.org/wandbox/permlink/MZqKhMF7tiaNZdJg">![Try it online][badge.wandbox]</a> <a target="_blank" href="https://gitter.im/boostorg/hana">![Gitter Chat][badge.Gitter]</a>

> Your standard library for metaprogramming

## Overview
<!-- Important: keep this in sync with example/overview.cpp -->
```cpp
#include <boost/hana.hpp>
#include <cassert>
#include <string>
namespace hana = boost::hana;
using namespace hana::literals;

struct Fish { std::string name; };
struct Cat  { std::string name; };
struct Dog  { std::string name; };

int main() {
  // Sequences capable of holding heterogeneous objects, and algorithms
  // to manipulate them.
  auto animals = hana::make_tuple(Fish{"Nemo"}, Cat{"Garfield"}, Dog{"Snoopy"});
  auto names = hana::transform(animals, [](auto a) {
    return a.name;
  });
  assert(hana::reverse(names) == hana::make_tuple("Snoopy", "Garfield", "Nemo"));

  // No compile-time information is lost: even if `animals` can't be a
  // constant expression because it contains strings, its length is constexpr.
  static_assert(hana::length(animals) == 3u, "");

  // Computations on types can be performed with the same syntax as that of
  // normal C++. Believe it or not, everything is done at compile-time.
  auto animal_types = hana::make_tuple(hana::type_c<Fish*>, hana::type_c<Cat&>, hana::type_c<Dog*>);
  auto animal_ptrs = hana::filter(animal_types, [](auto a) {
    return hana::traits::is_pointer(a);
  });
  static_assert(animal_ptrs == hana::make_tuple(hana::type_c<Fish*>, hana::type_c<Dog*>), "");

  // And many other goodies to make your life easier, including:
  // 1. Access to elements in a tuple with a sane syntax.
  static_assert(animal_ptrs[0_c] == hana::type_c<Fish*>, "");
  static_assert(animal_ptrs[1_c] == hana::type_c<Dog*>, "");

  // 2. Unroll loops at compile-time without hassle.
  std::string s;
  hana::int_c<10>.times([&]{ s += "x"; });
  // equivalent to s += "x"; s += "x"; ... s += "x";

  // 3. Easily check whether an expression is valid.
  //    This is usually achieved with complex SFINAE-based tricks.
  auto has_name = hana::is_valid([](auto&& x) -> decltype((void)x.name) { });
  static_assert(has_name(animals[0_c]), "");
  static_assert(!has_name(1), "");
}
```


## Documentation
You can browse the documentation online at http://boostorg.github.io/hana.
You can also get an offline version of the documentation by checking out
the `gh-pages` branch. To avoid overwriting the current directory, you
can clone the `gh-pages` branch into a subdirectory like `doc/html`:
```shell
git clone http://github.com/boostorg/hana --branch=gh-pages --depth=1 doc/html
```

After issuing this, `doc/html` will contain exactly the same static website
that's [available online][Hana.docs]. Note that `doc/html` is automatically
ignored by Git so updating the documentation won't pollute your index.


## Hacking on Hana
Setting yourself up to work on Hana is easy. First, you will need an
installation of [CMake][]. Once this is done, you can `cd` to the root
of the project and setup the build directory:
```shell
mkdir build
cd build
cmake ..
```

Usually, you'll want to specify a custom compiler because the system's
compiler is too old:
```shell
cmake .. -DCMAKE_CXX_COMPILER=/path/to/compiler
```

Usually, this will work just fine. However, on some systems, the standard
library and/or compiler provided by default does not support C++14. If
this is your case, the [wiki][Hana.wiki] has more information about
setting you up on different systems.

Normally, Hana tries to find Boost headers if you have them on your system.
It's also fine if you don't have them; a few tests requiring the Boost headers
will be disabled in that case. However, if you'd like Hana to use a custom
installation of Boost, you can specify the path to this custom installation:
```shell
cmake .. -DCMAKE_CXX_COMPILER=/path/to/compiler -DBOOST_ROOT=/path/to/boost
```

You can now build and run the unit tests and the examples:
```shell
cmake --build . --target check
```

You should be aware that compiling the unit tests is pretty time and RAM
consuming, especially the tests for external adapters. This is due to the
fact that Hana's unit tests are very thorough, and also that heterogeneous
sequences in other libraries tend to have horrible compile-time performance.

There are also optional targets which are enabled only when the required
software is available on your computer. For example, generating the
documentation requires [Doxygen][] to be installed. An informative message
will be printed during the CMake generation step whenever an optional target
is disabled. You can install any missing software and then re-run the CMake
generation to update the list of available targets.

> #### Tip
> You can use the `help` target to get a list of all the available targets.

If you want to add unit tests or examples, just add a source file in `test/`
or `example/` and then re-run the CMake generation step so the new source
file is known to the build system. Let's suppose the relative path from the
root of the project to the new source file is `path/to/file.cpp`. When you
re-run the CMake generation step, a new target named `path.to.file` will be
created, and a test of the same name will also be created. Hence,
```shell
cd build # Go back to the build directory
cmake --build . --target path.to.file # Builds the program associated to path/to/file.cpp
ctest -R path.to.file # Runs the program as a test
```

> #### Tip for Sublime Text users
> If you use the provided [hana.sublime-project](hana.sublime-project) file,
> you can select the "[Hana] Build current file" build system. When viewing a
> file to which a target is associated (like a test or an example), you can
> then compile it by pressing ⌘B, or compile and then run it using ⇧⌘B.


## Project organization
The project is organized in a couple of subdirectories.
- The [benchmark](benchmark) directory contains compile-time and runtime
  benchmarks to make sure the library is as fast as advertised. The benchmark
  code is written mostly in the form of [eRuby][] templates. The templates
  are used to generate C++ files which are then compiled while gathering
  compilation and execution statistics.
- The [cmake](cmake) directory contains various CMake modules and other
  scripts needed by the build system.
- The [doc](doc) directory contains configuration files needed to generate
  the documentation. The `doc/html` subdirectory is automatically ignored
  by Git; you can conveniently store a local copy of the documentation by
  cloning the `gh-pages` branch into that directory, as explained above.
- The [example](example) directory contains the source code for all the
  examples of both the tutorial and the reference documentation.
- The [experimental](experimental) directory contains various experiments that
  might or might not make it into Hana at some point.
- The [include](include) directory contains the library itself, which is
  header only.
- The [test](test) directory contains the source code for all the unit tests.


## Related material
- Talk on metaprogramming and Hana at [CppCon][] 2015 ([slides](http://ldionne.com/hana-cppcon-2015)/[video](https://youtu.be/cg1wOINjV9U))
- Talk on metaprogramming and Hana at [C++Now][] 2015 ([slides](http://ldionne.com/hana-cppnow-2015))
- Talk on Hana at [CppCon][] 2014 ([slides](http://ldionne.com/hana-cppcon-2014)/[video](https://youtu.be/L2SktfaJPuU))
- The [MPL11][] library, which is how Hana started out
- Talk on the MPL11 at [C++Now][] 2014 ([slides](http://ldionne.com/mpl11-cppnow-2014)/[video](https://youtu.be/8c0aWLuEO0Y))
- Louis Dionne's bachelor's thesis was a formalization of C++ metaprogramming through
  category theory. The thesis is available [here](https://github.com/ldionne/hana-thesis/blob/gh-pages/main.pdf),
  and the slides of a related presentation are available [here](http://ldionne.com/hana-thesis).
  Unfortunately, both are in french only.


## Contributing
Please see [CONTRIBUTING.md](CONTRIBUTING.md).


## License
Please see [LICENSE.md](LICENSE.md).


## Releasing
This section acts as a reminder of the few simple steps required to release a
new version of the library. This is only relevant to Hana's developers. To
release a new version of the library, create an annotated tag using `git tag -a`.
Then, push the tag and create a new GitHub release pointing to that tag.
Once that is done, bump the version number in `include/boost/hana/version.hpp`
so that it matches the next planned release.


<!-- Links -->
[badge.Appveyor]: https://ci.appveyor.com/api/projects/status/github/boostorg/hana?svg=true&branch=master
[badge.Gitter]: https://img.shields.io/badge/gitter-join%20chat-blue.svg
[badge.Travis]: https://travis-ci.org/boostorg/hana.svg?branch=master
[badge.version]: https://badge.fury.io/gh/boostorg%2Fhana.svg
[badge.Wandbox]: https://img.shields.io/badge/try%20it-online-blue.svg
[C++Now]: http://cppnow.org
[CMake]: http://www.cmake.org
[CppCon]: http://cppcon.org
[Doxygen]: http://www.doxygen.org
[eRuby]: http://en.wikipedia.org/wiki/ERuby
[Hana.docs]: http://boostorg.github.io/hana
[Hana.wiki]: https://github.com/boostorg/hana/wiki
[MPL11]: http://github.com/ldionne/mpl11
