
# fu2::function an improved drop-in replacement to std::function

![](https://img.shields.io/badge/Version-3.0.0-0091EA.svg) ![](https://img.shields.io/badge/License-Boost-blue.svg) [![Build Status](https://travis-ci.org/Naios/function2.svg?branch=master)](https://travis-ci.org/Naios/function2) [![Build status](https://ci.appveyor.com/api/projects/status/1tl0vqpg8ndccats/branch/master?svg=true)](https://ci.appveyor.com/project/Naios/function2/branch/master)

Provides two improved implementations of `std::function`:

- **copyable** `fu2::function`
- **move-only** `fu2::unique_function` (capable of holding move only types)

that provide many benefits and improvements over `std::function`:

- [x] **const**, **volatile** and **reference** correct (qualifiers are part of the `operator()` signature).
- [x] **convertible** to and from `std::function` as well as other callable types.
- [x] **adaptable** through `fu2::function_base` (internal capacity, copyable and exception guarantees)
- [x] **overloadable** with an arbitrary count of signatures (`fu2::function<bool(int), bool(float)>`)
- [x] **full allocator support** in contrast of `std::function` which doesn't provide support anymore
- [x] **covered** by unit tests and continuous integration (*GCC*, *Clang* and *MSVC*).
- [x] **header only**, just copy and include `function.hpp` in your project, **permissive licensed** under **boost**.


## Table of Contents

* **[Documentation](#documentation)**
  * **[How to use](#how-to-use)**
  * **[Constructing a function](#constructing-a-function)**
  * **[Non copyable unique functions](#non-copyable-unique-functions)**
  * **[Convertibility of functions](#convertibility-of-functions)**
  * **[Adapt function2](#adapt-function2)**
* **[Performance and optimization](#performance-and-optimization)**
  * **[Small functor optimization](#small-functor-optimization)**
  * **[Compiler optimization](#compiler-optimization)**
  * **[std::function vs fu2::function](#stdfunction-vs-fu2function)**
* **[Coverage and runtime checks](#coverage-and-runtime-checks)**
* **[Compatibility](#compatibility)**
* **[License](#licence)**
* **[Similar implementations](#similar-implementations)**

## Documentation

### How to use

**function2** is implemented in one header (`function.hpp`), no compilation is required.
Just copy the `function.hpp` header in your project and include it to start.
It's recommended to import the library as git submodule using CMake:

```sh
# Shell:
git submodule add https://github.com/Naios/function2.git
```

```cmake
# CMake file:
add_subdirectory(function2)
# function2 provides an interface target which makes it's
# headers available to all projects using function2
target_link_libraries(my_project function2)
```

Use `fu2::function` as a wrapper for copyable function wrappers and `fu2::unique_function` for move only types.
The standard implementation `std::function` and `fu2::function` are convertible to each other, see [the chapter convertibility of functions](#convertibility-of-functions) for details.

A function wrapper is declared as following:
```c++
fu2::function<void(int, float) const>
// Return type ~^   ^     ^     ^
// Parameters  ~~~~~|~~~~~|     ^
// Qualifier ~~~~~~~~~~~~~~~~~~~|
```

* **Return type**: The return type of the function to wrap.
* **Arguments**: The argument types of the function to wrap.
  Any argument types are allowed.
* **Qualifiers**: There are several qualifiers allowed:
  - **no qualifier** provides `ReturnType operator() (Args...)`
    - Can be assigned from const and no const objects (*mutable lambdas* for example).
  - **const** provides `ReturnType operator() (Args...) const`
    - Requires that the assigned functor is const callable (won't work with *mutable lambdas*),
  - **volatile** provides `ReturnType operator() (Args...) volatile`
    - Can only be assigned from volatile qualified functors.
  - **const volatile** provides `ReturnType operator() (Args...) const volatile`
    - Same as const and volatile together.
  - Also there is support for **r-value functions** `ReturnType operator() (Args...) &&`
    - one-shot functions which are invalidated after the first call.
* **Multiple overloads**: The library is capable of providing multiple overloads:
  ```cpp
  fu2::function<int(std::vector<int> const&),
                int(std::set<int> const&) const> fn = [] (auto const& container) {
                  return container.size());
                };
  ```

### Constructing a function

`fu2::function` and `fu2::unique_function` (non copyable) are easy to use:

```c++
fu2::function<void() const> fun = [] {
  // ...
};

// fun provides void operator()() const now
fun();
```

### Non copyable unique functions

`fu2::unique_function` also works with non copyable functors/ lambdas.

```c++
fu2::unique_function<bool() const> fun = [ptr = std::make_unique<bool>(true)] {
  return *ptr;
};

// unique functions are move only
fu2::unique_function<bool() const> otherfun = std::move(fun):

otherfun();
```


### Non owning functions

A `fu2::function_view` can be used to create a non owning view on a persistent object. Note that the view is only valid as long as the object lives.

```c++
auto callable = [ptr = std::make_unique<bool>(true)] {
  return *ptr;
};

fu2::function_view<bool() const> view(callable);
```

### Convertibility of functions

`fu2::function`, `fu2::unique_function` and `std::function` are convertible to each other when:

- The return type and parameter type match.
- The functions are both volatile or not.
- The functions are const correct:
  - `noconst = const`
  - `const = const`
  - `noconst = noconst`
- The functions are copyable correct when:
  - `unique = unique`
  - `unique = copyable`
  - `copyable = copyable`
- The functions are reference correct when:
  - `lvalue = lvalue`
  - `lvalue = rvalue`
  - `rvalue = rvalue`

| Convertibility from \ to | fu2::function | fu2::unique_function | std::function |
| ------------------------ | ------------- | -------------------- | ------------- |
| fu2::function            | Yes           | Yes                  | Yes           |
| fu2::unique_function     | No            | Yes                  | No            |
| std::function            | Yes           | Yes                  | Yes           |

```c++
fu2::function<void()> fun = []{};
// OK
std::function<void()> std_fun = fun;
// OK
fu2::unique_function<void()> un_fun = fun;

// Error (non copyable -> copyable)
fun = un_fun;
// Error (non copyable -> copyable)
fun = un_fun;

```

### Adapt function2

function2 is adaptable through `fu2::function_base` which allows you to set:

- **Signature:** defines the signature of the function.
- **Copyable:** defines if the function is copyable or not.
- **Capacity:** defines the internal capacity used for [sfo optimization](#small-functor-optimization).
- **Throwing** defines if empty function calls throw an `fu2::bad_function_call` exception, otherwise `std::abort` is called.

The following code defines a function with a variadic signature which is copyable and sfo optimization is disabled:

```c++
template<typename Signature>
using my_function = fu2::function_base<Signature, 0UL, true>;
```

The following code defines a non copyable function which just takes 1 argument, and has a huge capacity for internal sfo optimization.
Also it must be called as r-value.

```c++
template<typename Arg>
using my_consumer = fu2::function_base<void(Arg)&&, 100UL, false>;

// Example
my_consumer<int, float> consumer = [](int, float) { }
std::move(consumer)(44, 1.7363f);
```

## Performance and optimization

### Small functor optimization

function2 uses small functor optimization like the most common `std::function` implementations which means it allocates a small internal capacity to evade heap allocation for small functors.

Smart heap allocation moves the inplace allocated functor automatically to the heap to speed up moving between objects.

It's possible to disable small functor optimization through setting the internal capacity to 0.


## Coverage and runtime checks

Function2 is checked with unit tests and valgrind (for memory leaks), where the unit tests provide coverage for all possible template parameter assignments.

## Compatibility

Tested with:

- Visual Studio 2017+ Update 3
- Clang 3.8+
- GCC 5.4+

Every compiler with modern C++14 support should work.
*function2* only depends on the standard library.

## License
*function2* is licensed under the very permissive Boost 1.0 License.

## Similar implementations

There are similar implementations of a function wrapper:

- [pmed/fixed_size_function](https://github.com/pmed/fixed_size_function)
- stdex::function - A multi-signature function implementation.
- multifunction - Example from [Boost.TypeErasure](http://www.boost.org/doc/html/boost_typeerasure/examples.html#boost_typeerasure.examples.multifunction), another multi-signature function.
- std::function - [Standard](http://en.cppreference.com/w/cpp/utility/functional/function).
- boost::function - The one from [Boost](http://www.boost.org/doc/libs/1_55_0/doc/html/function.html).
- func::function - From this [blog](http://probablydance.com/2013/01/13/a-faster-implementation-of-stdfunction/).
- generic::delegate - [Fast delegate in C++11](http://codereview.stackexchange.com/questions/14730/impossibly-fast-delegate-in-c11), also see [here](https://code.google.com/p/cpppractice/source/browse/trunk/).
- ssvu::FastFunc - Another Don Clugston's FastDelegate, as shown [here](https://groups.google.com/a/isocpp.org/forum/#!topic/std-discussion/QgvHF7YMi3o).
- [cxx_function::function](https://github.com/potswa/cxx_function) - By David Krauss

Also check out the amazing [**CxxFunctionBenchmark**](https://github.com/jamboree/CxxFunctionBenchmark) which compares several implementations.
