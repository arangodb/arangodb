![logo](https://raw.githubusercontent.com/boostorg/gil/develop/doc/_static/gil.png)

# Contributing to Boost.GIL

Boost.GIL is a member of [Boost](https://www.boost.org) libraries.

If you wish to contribute a new feature or a bug fix,
please follow the workflow explained in this document.

## Table of Content

* [Prerequisites](#prerequisites)
* [Getting started with Git workflow](#getting-started-with-git-workflow)
  * [1. Clone Boost super-project](#1-clone-boost-super-project)
  * [2. Checkout Boost.GIL development branch](#2-checkout-boostgil-development-branch)
  * [3. Fork Boost.GIL repository on GitHub](#3-fork-boostgil-repository-on-github)
  * [4. Submit a pull request](#4-submit-a-pull-request)
  * [5. Update your pull request](#5-update-your-pull-request)
* [Development](#development)
  * [Using Boost.Build](#using-boostbuild)
  * [Using CMake](#using-cmake)
  * [Using Faber](#using-faber)
* [Guidelines](#guidelines)

## Prerequisites

* C++11 compiler
* Experience with `git` command line basics.
* Familiarity with build toolset and development environment of your choice.
* Although this document tries to present all commands with necessary options,
  it may be a good idea to skim through the
  [Boost Getting Started](https://www.boost.org/more/getting_started/index.html)
  chapters, especially if you are going to use
  [Boost.Build](https://boostorg.github.io/build/) for the first time.

## Getting started with Git workflow

First, you need learn some minimal basics of the
[modular Boost](https://svn.boost.org/trac/boost/wiki/ModularBoost)
super-project workflow.

The following steps are based on the official Boost
[Getting Started](https://github.com/boostorg/boost/wiki/Getting-Started).

**NOTE:** For brevity, commands below use notation for POSIX-like operating
systems and you may need to tweak them for Windows systems.

### 1. Clone Boost super-project

The preparation involves the following steps:

1. Clone the Boost super-project

    ```shell
    git clone --recursive --jobs 8 https://github.com/boostorg/boost.git
    ```

2. Switch the Boost super-project to desired branch, `master` or `develop`

    ```shell
    cd boost
    git checkout master
    ```

    **TIP:** [Modular Boost Library Maintenance](https://svn.boost.org/trac10/wiki/StartModMaint)
    guide, for more realistic test environment, recommends to develop and test
    individual Boost library against other Boost libraries as defined by
    the Boost super-project `master` branch:

    ```shell
    cd boost
    git checkout master
    git pull
    git submodule update --init --recursive --jobs 8
    ```

3. Build the `b2` driver program for Boost.Build engine.

    ```shell
    ./bootstrap.sh
    ./b2 --version
    ```

    **TIP:** For more convenient path-less invocation, you can copy the `b2`
    program to a location in your `PATH`.

4. Optionally, create full content of `/boost` virtual directory with
all Boost headers linked from the individual modular Boost libraries.
If you skip this step, executing `b2` to run tests will automatically
create the directory with all headers required by Boost.GIL and tests.

    ```shell
    ./b2 headers
    ```

**TIP:** If something goes wrong, you end up with incomplete or accidentally
modified files in your clone of the super-project repository, or you simply
wish to start fresh, then you can clean and reset the whole repository and
its modules:

```shell
git clean -xfd
git submodule foreach --recursive git clean -xfd
git reset --hard
git submodule foreach --recursive git reset --hard
git submodule update --init --recursive --jobs 8
```

### 2. Checkout Boost.GIL development branch

Regardless if you decide to develop again `master` (recommended) or `develop`
branch of the Boost super-project, you should *always* base your contributions
(ie. topic branches) on Boost.GIL `develop` branch.

1. Go to the Boost.GIL library submodule.

    ```shell
    cd libs/gil
    ```

2. Checkout the `develop` branch and bring it up to date

    ```shell
    git checkout develop
    git branch -vv
    git pull origin develop
    ```

### 3. Fork Boost.GIL repository on GitHub

Follow [Forking Projects](https://guides.github.com/activities/forking/) guide
to get personal copy of [boostorg/gil](https://github.com/boostorg/gil)
repository from where you will be able to submit new contributions as
[pull requests](https://help.github.com/articles/about-pull-requests/).

Add your fork as git remote to the Boost.GIL submodule:

```shell
cd libs/gil
git remote add username https://github.com/username/gil.git
```

### 4. Submit a pull request

All Boost.GIL contributions should be developed inside a topic branch created by
branching off the `develop` branch of [boostorg/gil](https://github.com/boostorg/gil).

**IMPORTANT:** Pull Requests *must* come from a branch based on `develop`, and *never* on `master`.

**NOTE:** The branching workflow model
[Boost recommends](https://svn.boost.org/trac10/wiki/StartModWorkflow)
is called Git Flow.

For example:

```shell
cd libs/gil
git checkout develop
git checkout -b feature/foo
```

Now, you are set to to develop a new feature for Boost.GIL,
then [git add](https://git-scm.com/docs/git-add) and
[git commit](https://git-scm.com/docs/git-commit) your changes.

Once it's finished, you can submit it as pull request for review:

```shell
cd libs/gil
git checkout feature/foo
git push username feature/foo
```

Finally, sign in to your GitHub account and
[create a pull request](https://help.github.com/articles/creating-a-pull-request/).

Your pull request will be automatically built and tests will run on Travis CI
and AppVeyor (see [README](README.md) for builds status). Please, keep an eye
on those CI builds and correct any problems detected in your contribution
by updating your pull request.

### 5. Update your pull request

In simplest (and recommended) case , your the pull request you submitted earlier
has *a single commit*, so you can simply update the existing commit with any
modifications required to fix failing CI builds or requested by reviewers.

First, it is a good idea to synchronize your topic branch with the latest
changes in the upstream `develop` branch:

```shell
cd libs/gil
git checkout develop
git pull origin develop
git checkout feature/foo
git rebase develop
```

Next, make your edits.

Finally, `git commit --amend` the *single-commit* in your topic branch and
update the pull request:

```shell
cd libs/gil
git checkout feature/foo
git add -A
git commit --amend
git push --force username feature/foo
```

**WARNING:** Ensure your pull request has a single commit, otherwise the
force push can corrupt your pull request.

If you wish to update pull request adding a new commit, then create new
commit and issue regular push:

```shell
git commit -m "Fix variable name"
git push username feature/foo
```

## Development

Boost.GIL is a [header-only library](https://en.wikipedia.org/wiki/Header-only)
which does not require sources compilation. Only test runners and example
programs have to be compiled.

By default, Boost.GIL uses Boost.Build to build all the executables.

We also provide configuration for two alternative build systems:

* [CMake](https://cmake.org)
* [Faber](http://stefan.seefeld.name/faber/)

**NOTE:** The CMake and Faber are optional and the corresponding build
configurations for Boost.GIL do not offer equivalents for all Boost.Build features. Most important difference to recognise is that Boost.Build will
automatically build any other Boost libraries required by Boost.GIL as dependencies.

### Using Boost.Build

The [b2 invocation](https://boostorg.github.io/build/manual/develop/index.html#bbv2.overview.invocation)
explains available options like `toolset`, `variant` and others.

Simply, just execute `b2` to run all tests built using default
`variant=debug` and default `toolset` determined for your
development environment.

**TIP:** Pass `b2` option `-d 2` to output complete action text and commands,
as they are executed. It is useful to inspect compilation flags.

If no target or directory is specified, everything in the current directory
is built. For example, all Boost.GIL tests can be built and run using:

```shell
cd libs/gil
../../b2
```

Run core tests only specifying location of directory with tests:

```shell
cd libs/gil
../../b2 test
```

Run all tests for selected extension (from Boost root directory, as alternative):

```shell
./b2 libs/gil/io/test
./b2 libs/gil/numeric/test
./b2 libs/gil/toolbox/test
```

Run I/O extension tests bundled in target called `simple`:

```shell
./b2 libs/gil/io/test//simple
```

*TODO:* _Explain I/O dependencies (libjpeg, etc.)_

### Using CMake

Maintainer: [@mloskot](https://github.com/mloskot)

**NOTE:** CMake configuration does not build any dependencies required by
Boost.GIL like Boost.Test and Boost.Filesystem libraries or any
third-party image format libraries used by the I/O extension.

The provided CMake configuration allows a couple of ways to develop Boost.GIL:

1. Using Boost installed from binary packages in default system-wide location.
2. Using Boost installed from sources in arbitrary location (CMake may need
   `-DBOOST_ROOT=/path/to/boost/root`, see
   [FindBoost](https://cmake.org/cmake/help/latest/module/FindBoost.html)
   documentation for details).
3. Using [cloned Boost super-project](#cloned-boost-super-project), inside modular
   `libs/gil`. This mode requires prior deployment of `boost` virtual directory
   with headers and stage build of required libraries, for example:
    ```shell
    ./b2 headers
    ./b2 variant=debug --with-test --with-filesystem stage
    ./b2 variant=release --with-test --with-filesystem stage
    ```
    or, depending on specific requirements, more complete build:
    ```shell
    ./b2 variant=debug,release address-model=32,64 --layout=versioned --with-test --with-filesystem stage
    ```

Using the installed Boost enables a lightweight mode for the library development,
inside a stand-alone clone Boost.GIL repository and without any need to clone the
whole Boost super-project.

Here is an example of such lightweight workflow in Linux environment (Debian-based):

* Install required Boost libraries

    ```shell
    sudo apt-get update
    sudo apt-get install libboost-dev libboost-test-dev libboost-filesystem-dev
    ```

* Optionally, install libraries required by the I/O extension

    ```shell
    sudo apt-get update
    sudo apt install libtiff-dev libpng-dev libjpeg-dev
    ```

* Clone Boost.GIL repository

    ```shell
    git clone https://github.com/boostorg/gil.git
    cd gil
    ```

* Configure build with CMake

    ```shell
    mkdir _build
    cd _build/
    cmake ..
    -- The CXX compiler identification is GNU 7.3.0
    -- Check for working CXX compiler: /usr/bin/c++
    -- Check for working CXX compiler: /usr/bin/c++ -- works
    -- Detecting CXX compiler ABI info
    -- Detecting CXX compiler ABI info - done
    -- Detecting CXX compile features
    -- Detecting CXX compile features - done
    -- Boost version: 1.65.1
    -- Found the following Boost libraries:
    --   unit_test_framework
    --   filesystem
    --   system
    -- Boost_INCLUDE_DIRS=/usr/include
    -- Boost_LIBRARY_DIRS=/usr/lib/x86_64-linux-gnu
    -- Found JPEG: /usr/lib/x86_64-linux-gnu/libjpeg.so
    -- Found ZLIB: /usr/lib/x86_64-linux-gnu/libz.so (found version "1.2.11")
    -- Found PNG: /usr/lib/x86_64-linux-gnu/libpng.so (found version "1.6.34")
    -- Found TIFF: /usr/lib/x86_64-linux-gnu/libtiff.so (found version "4.0.9")
    -- Configuring Boost.GIL core tests
    -- Configuring Boost.GIL IO tests
    -- Configuring Boost.GIL Numeric tests
    -- Configuring Boost.GIL Toolbox tests
    -- Configuring done
    -- Generating done
    -- Build files have been written to: /home/mloskot/gil/_build
    ```

    **TIP:** If CMake is failing to find Boost libraries, you can try a few hacks:

     - `-DGIL_ENABLE_FINDBOOST_DOWNLOAD=ON` to use very latest version of
       `FindBoost.cmake` without upgrading your CMake installation.
     - `-DBoost_COMPILER=-gcc5` or `-DBoost_COMPILER=-vc141` to help CMake match
        your compiler with Boost libraries naming in versioned layout
        (ie. `libboost_unit_test_framework-gcc5-mt-x64-1_69` and not `-gcc55-`).
     - `-DCMAKE_CXX_COMPILER_ARCHITECTURE_ID=x64` to help CMake match the target
        architecture, in case it fails to determine it for your compiler, which is
        also crucial for matching `-x64-` in the versioned layout names.
     - if CMake is still failing to find Boost, you may try `-DBoost_DEBUG=ON` to
       get detailed diagnostics output from `FindBoost.cmake` module.

* List available CMake targets

    ```shell
    cmake --build . --target help
    ```

* Build selected target with CMake

    ```shell
    cmake --build . --target gil_test_pixel
    ```

* List available CTest targets

    ```shell
    ctest --show-only | grep Test
    ```
* Run selected test with CTest

    ```shell
    ctest -R gil.tests.core.pixel
    ```

### Using Faber

Maintainer: [@stefanseefeld](https://github.com/stefanseefeld)

*TODO:* _Describe_

## Guidelines

Boost.GIL is a more than a decade old mature library maintained by several
developers with help from a couple of dozens contributors.
It is important to maintain consistent design, look and feel.
Thus, below a few basic guidelines are listed.

First and foremost, make sure you are familiar with the official
[Boost Library Requirements and Guidelines](https://www.boost.org/development/requirements.html).

Second, strive for writing idiomatic C++11, clean and elegant code.

**NOTE:** *The Boost.GIL source code does not necessary represent clean and elegant
code to look up to. The library has recently entered the transition to C++11.
Major refactoring overhaul is ongoing.*

Maintain structure your source code files according to the following guidelines:

* Name files in meaningful way.
* Put copyright and license information in every file
* If your changes [meet a certain threshold of originality](https://www.boost.org/users/license.html),
  add yourself to the copyright notice. Do not put any additional authorship or
  file comments (eg. no `\file` for Doxygen), revision information, etc.
* In header, put `#include` guard based on header path and file name
    ```cpp
    #ifndef BOOST_GIL_<DIR1>_<DIR2>_<FILE>_HPP
    #define BOOST_GIL_<DIR1>_<DIR2>_<FILE>_HPP
    ...
    #endif
    ```
* Make sure each [header is self-contained](https://github.com/boostorg/gil/wiki/Include-Directives-Order), i.e. that they include all headers they need.
* All public headers should be placed in `boost/gil/` or `boost/gil/<component>/`.
* All non-public headers should be placed `boost/gil/detail` or `boost/gil/<component>/detail`.
* All public definitions should reside in scope of `namespace boost { namespace gil {...}}`.
* All non-public definitions should reside in scope of `namespace boost { namespace gil { namespace detail {...}}}`.
* Write your code to fit within **90** columns of text (see discussion on [preferred line length](https://lists.boost.org/boost-gil/2018/04/0028.php) in GIL).
* Indent with **4 spaces**, not tabs. See the [.editorconfig](https://github.com/boostorg/gil/blob/develop/.editorconfig) file for details. Please, do not increases the indentation level within namespace.
