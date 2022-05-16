# [Boost.PFR](https://boost.org/libs/pfr)

This is a C++14 library for very basic reflection that gives you access to structure elements by index and provides other `std::tuple` like methods for user defined types without any macro or boilerplate code.

[Boost.PFR](https://boost.org/libs/pfr) is a part of the [Boost C++ Libraries](https://github.com/boostorg). However, Boost.PFR is a header only library that does not depend on Boost. You can just copy the content of the "include" folder from the github into your project, and the library will work fine.

For a version of the library without `boost::` namespace see [PFR](https://github.com/apolukhin/pfr_non_boost).

### Test results

Branches        | Build         | Tests coverage | More info
----------------|-------------- | -------------- |-----------
Develop:        | [![CI](https://github.com/boostorg/pfr/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/boostorg/pfr/actions/workflows/ci.yml) [![Build status](https://ci.appveyor.com/api/projects/status/0mavmnkdmltcdmqa/branch/develop?svg=true)](https://ci.appveyor.com/project/apolukhin/pfr/branch/develop) | [![Coverage Status](https://coveralls.io/repos/github/apolukhin/magic_get/badge.png?branch=develop)](https://coveralls.io/github/apolukhin/magic_get?branch=develop) | [details...](https://www.boost.org/development/tests/develop/developer/pfr.html)
Master:         | [![CI](https://github.com/boostorg/pfr/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/boostorg/pfr/actions/workflows/ci.yml) [![Build status](https://ci.appveyor.com/api/projects/status/0mavmnkdmltcdmqa/branch/master?svg=true)](https://ci.appveyor.com/project/apolukhin/pfr/branch/master) | [![Coverage Status](https://coveralls.io/repos/github/apolukhin/magic_get/badge.png?branch=master)](https://coveralls.io/github/apolukhin/magic_get?branch=master) | [details...](https://www.boost.org/development/tests/master/developer/pfr.html)

[Latest developer documentation](https://www.boost.org/doc/libs/develop/doc/html/boost_pfr.html)

### Motivating Example #0
```c++
#include <iostream>
#include <fstream>
#include <string>

#include "boost/pfr.hpp"

struct some_person {
  std::string name;
  unsigned birth_year;
};

int main(int argc, const char* argv[]) {
  some_person val{"Edgar Allan Poe", 1809};

  std::cout << boost::pfr::get<0>(val)                // No macro!
      << " was born in " << boost::pfr::get<1>(val);  // Works with any aggregate initializables!

  if (argc > 1) {
    std::ofstream ofs(argv[1]);
    ofs << boost::pfr::io(val);                       // File now contains: {"Edgar Allan Poe", 1809}
  }
}
```
Outputs:
```
Edgar Allan Poe was born in 1809
```


### Motivating Example #1
```c++
#include <iostream>
#include "boost/pfr/precise.hpp"

struct my_struct { // no ostream operator defined!
    int i;
    char c;
    double d;
};

int main() {
    my_struct s{100, 'H', 3.141593};
    std::cout << "my_struct has " << boost::pfr::tuple_size<my_struct>::value
        << " fields: " << boost::pfr::io(s) << "\n";
}

```

Outputs:
```
my_struct has 3 fields: {100, H, 3.14159}
```

### Motivating Example #2

```c++
#include <iostream>
#include "boost/pfr/precise.hpp"

struct my_struct { // no ostream operator defined!
    std::string s;
    int i;
};

int main() {
    my_struct s{{"Das ist fantastisch!"}, 100};
    std::cout << "my_struct has " << boost::pfr::tuple_size<my_struct>::value
        << " fields: " << boost::pfr::io(s) << "\n";
}

```

Outputs:
```
my_struct has 2 fields: {"Das ist fantastisch!", 100}
```


### Requirements and Limitations

[See docs](https://www.boost.org/doc/libs/develop/doc/html/boost_pfr.html).

### License

Distributed under the [Boost Software License, Version 1.0](https://boost.org/LICENSE_1_0.txt).
