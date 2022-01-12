# Contributing to Boost.GIL

Boost.GIL is a member of [Boost](https://www.boost.org) libraries.

If you wish to contribute a new feature or a bug fix,
please follow the workflow explained in this document.

## Table of Content

- [Prerequisites](#prerequisites)
- [Pull Requests](#pull-requests)
- [Getting started with Git workflow](#getting-started-with-git-workflow)
  - [1. Clone Boost super-project](#1-clone-boost-super-project)
  - [2. Checkout Boost.GIL development branch](#2-checkout-boostgil-development-branch)
  - [3. Fork Boost.GIL repository on GitHub](#3-fork-boostgil-repository-on-github)
  - [4. Submit a pull request](#4-submit-a-pull-request)
  - [5. Update your pull request](#5-update-your-pull-request)
- [Development](#development)
  - [Install dependencies](#install-dependencies)
  - [Using Boost.Build](#using-boostbuild)
  - [Using CMake](#using-cmake)
  - [Running clang-tidy](#running-clang-tidy)
- [Guidelines](#guidelines)

## Prerequisites

- C++11 compiler
- Build and run-time dependencies for tests and examples:
  - Boost.Filesystem
  - Boost.Test
  - Headers and libraries of libjpeg, libpng, libtiff, libraw for the I/O extension.
- Experience with `git` command line basics.
- Familiarity with build toolset and development environment of your choice.
- Although this document tries to present all commands with necessary options,
  it may be a good idea to skim through the
  [Boost Getting Started](https://www.boost.org/more/getting_started/index.html)
  chapters, especially if you are going to use
  [Boost.Build](https://boostorg.github.io/build/) for the first time.

## Pull Requests

- **DO** base your work against the `develop` branch, not the `master`.
- **DO** submit all major changes to code via pull requests (PRs) rather than through
  a direct commit. PRs will be CI-checked first, then reviewed and potentially merged
  by the repo maintainers after a peer review that includes at least one maintainer.
  Contributors with commit access may submit trivial patches or changes to the project
  infrastructure configuration via direct commits (CAUTION!)
- **DO NOT** mix independent, unrelated changes in one PR.
  Separate unrelated fixes into separate PRs, especially if they are in different components
  (e.g. core headers versus extensions).
  Separate real product/test code changes from larger code formatting/dead code removal changes,
  unless the former are extensive enough to justify such refactoring, then also mention it.
- **DO** start PR subject with "WIP:" tag if you submit it as "work in progress".
  A PR should preferably be submitted when it is considered ready for review and subsequent
  merging by the contributor. Otherwise, clearly indicate it is not yet ready.
  The "WIP:" tag will also help maintainers to label your PR with [status/work-in-progress].
- **DO** give PRs short-but-descriptive names (e.g. "Add test for algorithm XXX", not "Fix #1234").
- **DO** [refer] to any relevant issues, and include the [keywords] that automatically
  close issues when the PR is merged.
- **DO** [mention] any users that should know about and/or review the change.
- **DO** ensure each commit successfully builds. The entire PR must pass all tests in
  the Continuous Integration (CI) system before it'll be merged.
- **DO** ensure any new features or changes to existing behaviours are covered with test cases.
- **DO** address PR feedback in an additional commit(s) rather than amending the existing
  commits, and only rebase/squash them when necessary. This makes it easier for reviewers
  to track changes.
- **DO** assume that the [Squash and Merge] will be used to merge your commit unless you
  request otherwise in the PR.
- **DO** NOT fix merge conflicts using a merge commit. Prefer git rebase.
- **DO** NOT submit changes to the original legacy tests, see
  [test/legacy/README.md](test/legacy/README.md).

### Merging Pull Requests (for maintainers with write access)

- **DO** use [Squash and Merge] by default for individual contributions unless requested
  by the PR author. Do so, even if the PR contains only one commit. It creates a simpler
  history than [Create a Merge Commit]. Reasons that PR authors may request the true
  merge recording a merge commit may include (but are not limited to):
  - The change is easier to understand as a series of focused commits.
    Each commit in the series must be buildable so as not to break git bisect.
  - Contributor is using an e-mail address other than the primary GitHub address
    and wants that preserved in the history.
    Contributor must be willing to squash the commits manually before acceptance.

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
    git clone --recurse-submodules --jobs 8 https://github.com/boostorg/boost.git
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

4. Create full content of `/boost` virtual directory with
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
(i.e. topic branches) on Boost.GIL `develop` branch.

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
git remote add <username> https://github.com/<username>/gil.git
```

or, if you cloned from your fork already, add the upstream as `origin` remote:

```shell
git remote add upstream https://github.com/boostorg/gil.git
# or
git remote rename origin <username>
git remote add origin https://github.com/boostorg/gil.git
```

### 4. Submit a pull request

All Boost.GIL contributions should be developed inside a topic branch created by
branching off the `develop` branch of [boostorg/gil](https://github.com/boostorg/gil).

**IMPORTANT:** Pull Requests *must* come from a branch based on `develop`,
and *never* on `master`.

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
git push <username> feature/foo
```

Finally, sign in to your GitHub account and
[create a pull request](https://help.github.com/articles/creating-a-pull-request/).

Your pull request will be automatically built and tests will run on GitHub ACtions,
Azure Pipelines, AppVeyor and Circle CI (see [README](README.md) for builds status).
Please, keep an eye on those CI builds and correct any problems detected in your
contribution by updating your pull request.

### 5. Update your pull request

Depending on actual purpose of the update, you can follow a different
strategy to update your pull request:

- Use `git commit --amend`, `git rebase` and `git push --force` when your
   pull request is still *work-in-progress* and not ready for review yet.
- Use `git commit`, `git merge` and `git push` to update your pull request
   during review, in response to requests from reviewers.

**NOTE:** Once review of your work has started, you should not rebase your work.
You should create new commits and update your topic branch. This helps with
traceability in the pull request and prevents the accidental history breakage.
Those who review your work may be fetching it into their fork for local review.

#### Synchronise pull request branch

Keep your topic branch up to date and synchronized with the upstream `develop` branch:

```shell
cd libs/gil
git checkout develop
git pull origin develop
git checkout feature/foo
```

If review of your work has not started, *prefer* to merge:

```shell
git merge develop
git push <username> feature/foo
```

If your PR is still *work-in-progress*, you may rebase if you like:

```shell
git rebase develop
git push --force <username> feature/foo
```

#### Amend last commit of pull request

If your pull request is a *work-in-progress* and has not been reviewed yet,
you may amend your commit or rebase onto the `develop` branch:

```shell
cd libs/gil
git checkout feature/foo
git add -A
git commit --amend
git push --force <username> feature/foo
```

#### Add new commits to pull request

In order to update your pull request, for example in response to a change
request from reviewer, just add new commits:

```shell
cd libs/gil
git checkout feature/foo
git add -A
git commit -m "Fix CI build failure"
git push <username> feature/foo
```

## Development

Boost.GIL is a [header-only library](https://en.wikipedia.org/wiki/Header-only)
which does not require sources compilation. Only test runners and
[example](example/README.md) programs have to be compiled.

By default, Boost.GIL uses Boost.Build to build all the executables.

We also provide configuration for two alternative build systems:

- [CMake](https://cmake.org)

**NOTE:** The CMake is optional and the corresponding build configurations
for Boost.GIL do not offer equivalents for all Boost.Build features.
Most important difference to recognise is that Boost.Build will automatically
build any other Boost libraries required by Boost.GIL as dependencies.

### Install dependencies

Boost.GIL tests and examples use the GIL I/O extension which depends on
third-party libraries for read and write support of specific image formats:

```shell
sudo apt-get install libjpeg-dev libpng-dev libtiff5-dev libraw-dev
```

**TIP:** On Windows, use vcpkg with `user-config.jam` configuration provided in [example/b2/user-config-windows-vcpkg.jam](example/b2/).

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
../../b2 cxxstd=11 test/core
```

Run all tests for selected extension (from Boost root directory, as alternative):

```shell
./b2 cxxstd=11 libs/gil/test/io
./b2 cxxstd=11 libs/gil/test/numeric
./b2 cxxstd=11 libs/gil/test/toolbox
```

Run I/O extension tests bundled in target called `simple`:

```shell
./b2 cxxstd=11 libs/gil/test/io//simple
```

**TIP:** Pass `b2` feature `cxxstd=11,14,17,2a` to request compilation for
multiple C++ standard versions in single build run.

### Using CMake

Maintainer: [@mloskot](https://github.com/mloskot)

**WARNING:** The CMake configuration is only provided for convenience
of contributors. It does not export or install any targets, deploy
config files or support subproject workflow.

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

For CMake, you only need to build Boost libraries required by Boost.Test library
which is used to run GIL tests. Since the `CMAKE_CXX_STANDARD` option in the current
[CMakeLists.txt](CMakeLists.txt) defaults to C++11, pass the default `cxxstd=11` to `b2`:

  ```shell
  ./b2 headers
  ./b2 variant=debug,release cxxstd=11 --with-filesystem stage
  ```

  or, depending on specific requirements, more complete build:

  ```shell
  ./b2 variant=debug,release address-model=32,64 cxxstd=11 --layout=versioned --with-filesystem stage
  ```

If you wish to build tests using different C++ standard version, then adjust the `cxxstd` accordingly.

Using the installed Boost enables a lightweight mode for the library development,
inside a stand-alone clone Boost.GIL repository and without any need to clone the
whole Boost super-project.

**TIP:** For the lightweight setup, prefer latest release of Boost.

For available custom CMake options, open the top-level `CMakeLists.txt` and search for `option`.

Here is an example of such lightweight workflow in Linux environment (Debian-based):

- Install required Boost libraries

    ```shell
    sudo apt-get update
    sudo apt-get install libboost-dev libboost-test-dev libboost-filesystem-dev
    ```

- Clone Boost.GIL repository

    ```shell
    git clone https://github.com/boostorg/gil.git
    cd gil
    ```

- Configure build with CMake

    ```shell
    mkdir _build
    cd _build/
    cmake ..
    ```

    **TIP:** By default, tests and [examples](example/README.md) are compiled using
    the minimum required C++11.
    Specify `-DCMAKE_CXX_STANDARD=14|17|20` to use newer version.
    For more CMake options available for GIL, check `option`-s defined
    in the top-level `CMakeLists.txt`.

    **TIP:** If CMake is failing to find Boost libraries, especially built
    with `--layout=versioned`, you can try a few hacks:
      - option `-DBoost_ARCHITECTURE=-x64` to help CMake find Boost 1.66 and above
        add an architecture tag to the library file names in versioned build
        The option added in CMake 3.13.0.
      - option `-DBoost_COMPILER=-gcc5` or `-DBoost_COMPILER=-vc141` to help CMake earlier
        than 3.13 match your compiler with toolset used in the Boost library file names
        (i.e. `libboost_filesystem-gcc5-mt-x64-1_69` and not `-gcc55-`).
        Fixed in CMake 3.13.0.
      - if CMake is still failing to find Boost, you may try `-DBoost_DEBUG=ON` to
        get detailed diagnostics output from `FindBoost.cmake` module.

- List available CMake targets

    ```shell
    cmake --build . --target help
    ```

- Build selected target with CMake

    ```shell
    cmake --build . --target gil_test_pixel
    ```

- List available CTest targets

    ```shell
    ctest --show-only | grep Test
    ```

- Run selected test with CTest

    ```shell
    ctest -R gil.tests.core.pixel
    ```

#### CMake configuration for Visual Studio

We provide [example/cmake/CMakeSettings.json](https://github.com/boostorg/gil/blob/develop/example/cmake/CMakeSettings.json)
with reasonable default settings for the [CMake support in Visual Studio](https://go.microsoft.com//fwlink//?linkid=834763).
See [example/cmake/README.md](example/cmake/README.md) for more details.

#### CMake configuration for Visual Studio Code

We provide [example/cmake/cmake-variants.yaml](https://github.com/boostorg/gil/blob/develop/example/cmake/cmake-variants.yaml)
with reasonable default settings for the [CMake Tools](https://github.com/vector-of-bool/vscode-cmake-tools) extension.
See [example/cmake/README.md](example/cmake/README.md) for more details.

### Running clang-tidy

[clang-tidy](http://clang.llvm.org/extra/clang-tidy/) can be run on demand to
diagnose or diagnose and fix or refactor source code issues.

Since the CMake configuration is provided for building tests and [examples](example/README.md),
it is easy to run `clang-tidy` using either the integration built-in CMake 3.6+
as target property `CXX_CLANG_TIDY` or the compile command database which
can be easily generated.

#### Linting

This mode uses the CMake built-in integration and runs `clang-tidy` checks configured
in [.clang-tidy](https://github.com/boostorg/gil/blob/develop/.clang-tidy).
All custom compilation warning levels (e.g. `-Wall`) are disabled and
compiler defaults are used.

```shell
cd libs/gil
cmake -S . -B _build -DGIL_USE_CLANG_TIDY=ON

# all targets
cmake --build _build

# selected target
cmake --build _build --target test_headers_all_in_one
```

#### Refactoring

**WARNING:** This is advanced processing and depending on checks, it may fail to deliver
expected results, especially if run against all configured translation units at ones.

1. Generate `compile_commands.json` database

    ```shell
    cd libs/gil
    cmake -S . -B _build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    ```

2. Edit `compile_commands.json` and remove entries of commands for all but the `.cpp`
    files you wish to refactor. For example, keep `test_headers_all_in_one.cpp` only
    to refactor all headers.

3. Run the parallel `clang-tidy` runner script to apply the desired checks (and fixes)
    across the library source code:

    ```shell
    run-clang-tidy.py -p=_build -header-filter='boost\/gil\/.*' -checks='-*,modernize-use-using' -fix > cl.log 2>&1
    ```

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

- Name files in meaningful way.
- Put copyright and license information in every file
- If your changes [meet a certain threshold of originality](https://www.boost.org/users/license.html),
  add yourself to the copyright notice. Do not put any additional authorship or
  file comments (eg. no `\file` for Doxygen), revision information, etc.
- In header, put `#include` guard based on header path and file name

    ```cpp
    #ifndef BOOST_GIL_<DIR1>_<DIR2>_<FILE>_HPP
    #define BOOST_GIL_<DIR1>_<DIR2>_<FILE>_HPP
    ...
    #endif
    ```

- Make sure each [header is self-contained](https://github.com/boostorg/gil/wiki/Include-Directives-Order), i.e. that they include all headers they need.
- All public headers should be placed in `boost/gil/` or `boost/gil/<component>/`.
- All non-public headers should be placed `boost/gil/detail` or `boost/gil/<component>/detail`.
- All public definitions should reside in scope of `namespace boost { namespace gil {...}}`.
- All non-public definitions should reside in scope of `namespace boost { namespace gil { namespace detail {...}}}`.
- Write your code to fit within **100** columns of text.
- Use [EditorConfig](https://editorconfig.org) for your editor and enable [.editorconfig](https://github.com/boostorg/gil/blob/develop/.editorconfig) to:
      - Indent with **4 spaces** and no tabs.
      - Trim any trailing whitespaces.
- Do not increases the indentation level within namespace.

[status/work-in-progress]: https://github.com/boostorg/gil/labels/status%2Fwork-in-progress
[refer]: https://help.github.com/articles/autolinked-references-and-urls/
[keywords]: https://help.github.com/articles/closing-issues-using-keywords/
[mention]: https://help.github.com/articles/basic-writing-and-formatting-syntax/#mentioning-people-and-teams
[squash and merge]: https://help.github.com/articles/merging-a-pull-request/
[create a merge commit]: https://help.github.com/articles/merging-a-pull-request/
