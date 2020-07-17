<!--
|Branch      |Status   |
|------------|---------|
|master      |[![Build Status][travisMasterBadge]][travisLink] [![Build status][AppveyorMasterBadge]][AppveyorLink] |

[travisMasterBadge]: https://travis-ci.org/iresearch-toolkit/iresearch.svg?branch=master "Linux"
[travisLink]: https://travis-ci.org/iresearch-toolkit/iresearch "Linux"
[AppveyorMasterBadge]: https://ci.appveyor.com/api/projects/status/umr1pa805v7xa54a/branch/master?svg=true "Windows"
[AppveyorLink]: https://ci.appveyor.com/project/gnusi/iresearch/branch/master "Windows"
-->

# IResearch search engine
### Version 1.0

## Table of contents
- [Overview](#overview)
- [High level architecture and main concepts](#high-level-architecture-and-main-concepts)
- [Build](#build)
- [Pyresearch](#pyresearch)
- [Included 3rd party dependencies](#included-3rd-party-dependencies)
- [External 3rd party dependencies](#external-3rd-party-dependencies)
- [Query filter building blocks](#query-filter-building-blocks)
- [Index Query Language](#index-query-language)
- [Supported compilers](#supported-compilers)
- [License](#license)

## Overview
The IResearch library is meant to be treated as a standalone index that is capable of both indexing and storing individual values verbatim.
Indexed data is treated on a per-version/per-revision basis, i.e. existing data version/revision is never modified and updates/removals
are treated as new versions/revisions of the said data. This allows for trivial multi-threaded read/write operations on the index.
The index exposes its data processing functionality via a multi-threaded 'writer' interface that treats each document abstraction as a
collection of fields to index and/or store.
The index exposes its data retrieval functionality via 'reader' interface that returns records from an index matching a specified query.
The queries themselves are constructed from either string IQL (index query language) requests or query trees built directly using the query
building blocks available in the API.
The querying infrastructure provides the capability of ordering the result set by one or more ranking/scoring implementations.
The ranking/scoring implementation logic is plugin-based and lazy-initialized during runtime as needed, allowing for addition of custom
ranking/scoring logic without the need to even recompile the IResearch library.


## High level architecture and main concepts
### Index
An index consists of multiple independent parts, called segments and index metadata. Index metadata stores information about
active index segments for the particular index version/revision.  Each index segment is an index itself and consists of the
following logical components:

- segment metadata
- field metadata
- term dictionary
- postings lists
- list of deleted documents
- stored values

Read/write access to the components carried via plugin-based formats. Index may contain segments created using different formats.

### Document
A database record is represented as an abstraction called a document.
A document is actually a collection of indexed/stored fields.
In order to be processed each field should satisfy at least `IndexedField` or `StoredField` concept.

#### IndexedField concept
For type `T` to be `IndexedField`, the following conditions have to be satisfied for an object m of type `T`:

|Expression|Requires|Effects|
|----|----|----|
|`m.name()`|The output type must be convertible to `iresearch::string_ref`|A value uses as a key name.|
|`m.get_tokens()`|The output type must be convertible to `iresearch::token_stream*`|A token stream uses for populating in invert procedure. If value is `nullptr` field is treated as non-indexed.|
|`m.features()`|The output type must be convertible to `const iresearch::flags&`|A set of features requested for evaluation during indexing. E.g. it may contain request of processing positions and frequencies. Later the evaluated information can be used during querying.|

#### StoredField concept
For type `T` to be `StoredField`, the following conditions have to be satisfied for an object m of type `T`:

|Expression|Requires|Effects|
|----|----|----|
|`m.name()`|The output type must be convertible to `iresearch::string_ref`|A value uses as a key name.|
|`m.write(iresearch::data_output& out)`|The output type must be convertible to bool.|One may write arbitrary data to stream denoted by `out` in order to retrieve written value using index_reader API later. If nothing has written but returned value is `true` then stored value is treated as flag. If returned value is `false` then nothing is stored even if something has been written to `out` stream.|

### Directory
A data storage abstraction that can either store data in memory or on the filesystem depending on which implementation is instantiated.
A directory stores at least all the currently in-use index data versions/revisions. For the case where there are no active users of the
directory then at least the last data version/revision is stored. Unused data versions/revisions may be removed via the directory_cleaner.
A single version/revision of the index is composed of one or more segments associated, and possibly shared, with the said version/revision.

### Writer
A single instance per-directory object that is used for indexing data. Data may be indexed in a per-document basis or sourced from
another reader for trivial directory merge functionality.
Each `commit()` of a writer produces a new version/revision of the view of the data in the corresponding directory.
Additionally the interface also provides directory defragmentation capabilities to allow compacting multiple smaller version/revision
segments into larger more compact representations.
A writer supports two-phase transactions via `begin()`/`commit()`/`rollback()` methods.

### Reader
A reusable/refreshable view of an index at a given point in time. Multiple readers can use the same directory and may point to different
versions/revisions of data in the said directory.

## Build prerequisites

### [CMake](https://cmake.org)
v3.2 or later

### [Boost](http://www.boost.org/doc/libs/1_57_0/more/getting_started/index.html)
v1.57.0 or later (locale system thread)

#### install (*nix)
> It looks like it is important to pass arguments to the bootstrap script in one 
> line

```bash
./bootstrap.sh --with-libraries=locale,system,regex,thread
./b2
```

#### install (MacOS)
> Do not link Boost against 'iconv' because on MacOS it causes problems when
> linking against Boost locale. Unfortunately this requires linking against ICU.

```bash
./bootstrap.sh --with-libraries=locale,system,regex,thread
./b2 -sICU_PATH="${ICU_ROOT}" boost.locale.iconv=off boost.locale.icu=on
```

#### install (win32)
```bash
bootstrap.bat --with-libraries=test
bootstrap.bat --with-libraries=thread
b2 --build-type=complete stage address-model=64
```

#### set environment
```bash
BOOST_ROOT=<path-to>/boost_1_57_0
```

### [Lz4](https://code.google.com/p/lz4)

#### install (*nix)
```bash
make
make install
```
or
point LZ4_ROOT at the source directory to build together with IResearch

#### install (win32)

> If compiling IResearch with /MT add add_definitions("/MTd") to the end of
> cmake_unofficial/CMakeLists.txt since cmake will ignore the command line argument
> -DCMAKE_C_FLAGS=/MTd

```bash
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=<install-path> -DBUILD_STATIC_LIBS=on -g "Visual studio 17" -Ax64 ../contrib/cmake_unofficial
cmake --build .
cmake --build . --target install
```
or
point LZ4_ROOT at the source directory to build together with IResearch

#### set environment
```bash
LZ4_ROOT=<install-path>
```

### [Bison](https://www.gnu.org/software/bison)
v2.4 or later

win32 binaries also available in:
- https://git-scm.com/download/win
- http://sourceforge.net/projects/mingw/files
- http://sourceforge.net/projects/mingwbuilds/files/external-binary-packages

### [ICU](http://site.icu-project.org/download)
v53 or higher

#### install (*nix)
```bash
./configure --disable-samples --disable-tests --enable-static --srcdir="$(pwd)" --prefix=<install-path> --exec-prefix=<install-path>
make install
```
or
point ICU_ROOT at the source directory to build together with IResearch
or
via the distributions' package manager: libicu<version>

#### install (win32)
look for link: "ICU4C Binaries"

#### set environment
```bash
ICU_ROOT=<path-to-icu>
```

### [Snowball](http://snowball.tartarus.org)

#### install (*nix)

> the custom CMakeLists.txt is intended to be used with snowball v2.0.0 
> and later versions. At least it was tested to work on commit
> 53739a805cfa6c77ff8496dc711dc1c106d987c1

```bash
git clone https://github.com/snowballstem/snowball.git
mkdir build && cd build
cmake -DENABLE_STATIC=OFF -DNO_SHARED=OFF -g "Unix Makefiles" ..
cmake --build .
cmake -DENABLE_STATIC=OFF -DNO_SHARED=ON -g "Unix Makefiles" ..
cmake --build .
```
or
point SNOWBALL_ROOT at the source directory to build together with IResearch
or
via the distributions' package manager: libstemmer

#### install (win32)

> the custom CMakeLists.txt was based on revision 5137019d68befd633ce8b1cd48065f41e77ed43e
> later versions may be used at your own risk of compilation failure

```bash
git clone https://github.com/snowballstem/snowball.git
git reset --hard 5137019d68befd633ce8b1cd48065f41e77ed43e
mkdir build && cd build
set PATH=%PATH%;<path-to>/build/Debug
cmake -DENABLE_STATIC=OFF -DNO_SHARED=OFF -g "Visual studio 12" -Ax64 ..
cmake --build .
cmake -DENABLE_STATIC=OFF -DNO_SHARED=ON -g "Visual studio 12" -Ax64 ..
cmake --build .
```
or
point SNOWBALL_ROOT at the source directory to build together with IResearch

> For static builds:
> 1. in MSVC open: build/snowball.sln
> 2. set: stemmer -> Properties -> Configuration Properties -> C/C++ -> Code Generation -> Runtime Library = /MTd
> 3. BUILD -> Build Solution

#### set environment
```bash
SNOWBALL_ROOT=<path-to-snowball>
```

### [BFD](https://sourceware.org/binutils/) <optional>

#### install (*nix)
via the distributions' package manager: libbfd
or
build from source via:
```bash
cd libiberty
env CFLAGS=-fPIC ./configure
make

cd ../zlib
env CFLAGS=-fPIC ./configure
make

cd ../bfd
env LDFLAGS='-L../libiberty -liberty' ./configure --enable-targets=all --enable-shared
make
```

#### install (win32)
not yet available for win32

#### set environment
Note: BINUTILS_ROOT is a "reserved" variable internally used by some of the gcc compiler tools. 
```bash
BFD_ROOT=<path-to-binutils>
```

### [Unwind](http://www.nongnu.org/libunwind/) <optional>

#### install (*nix)
via the distributions' package manager: libunwind
or
build from source via:
```bash
configure
make
make install
```

#### install (win32)
not yet available for win32

#### set environment
```bash
UNWIND_ROOT=<path-to-unwind>
```

### [Gooogle test](https://code.google.com/p/googletest)

#### install (*nix)
```bash
mkdir build && cd build
cmake ..
make
```
or
point GTEST_ROOT at the source directory to build together with IResearch

#### install (win32)
```bash
mkdir build && cd build
cmake -g "Visual studio 12" -Ax64 -Dgtest_force_shared_crt=ON -DCMAKE_DEBUG_POSTFIX="" ..
cmake --build .
mv Debug ../lib
```
or
point GTEST_ROOT at the source directory to build together with IResearch

#### set environment
```bash
GTEST_ROOT=<path-to-gtest>
```

### Stopword list (for use with analysis::text_analyzer)
> download any number of lists of stopwords, e.g. from:
> https://github.com/snowballstem/snowball-website/tree/master/algorithms/*/stop.txt
> https://code.google.com/p/stop-words/

#### install
  1. mkdir <path-to-stopword-lists>
  2. for each language, (e.g. "c", "en", "es", "ru"), create a corresponding subdirectory
     (a directory name has 2 letters except the default locale "c" which has 1 letter)
  3. place the files with stopwords, (utf8 encoded with one word per line, any text after
     the first whitespace is ignored), in the directory corresponding to its language
     (multiple files per language are supported and will be interpreted as a single list)

#### set environment
```bash
IRESEARCH_TEXT_STOPWORD_PATH=<path-to-stopword-lists>
```

> If the variable IRESEARCH_TEXT_STOPWORD_PATH is left unset then locale specific
> stopword-list subdirectories are deemed to be located in the current working directory

## Build
```bash
git clone <IResearch code repository>/iresearch.git iresearch
cd iresearch
mkdir build && cd build
```

generate build file <*nix>:
```bash    
cmake -DCMAKE_BUILD_TYPE=[Debug|Release|Coverage] -g "Unix Makefiles" ..
```

> 1. if some libraries are not found by the build then set the needed environment 
> variables (e.g. BOOST_ROOT, BOOST_LIBRARYDIR, LZ4_ROOT, OPENFST_ROOT, GTEST_ROOT)
> 2. if ICU or Snowball from the distribution paths are not found, the following additional
> environment variables might be required:
> ICU_ROOT_SUFFIX=x86_64-linux-gnu SNOWBALL_ROOT_SUFFIX=x86_64-linux-gnu

generate build file (win32):
```bash
cmake -g "Visual studio 12" -Ax64 ..
```

> If some libraries are not found by the build then set the needed environment
> variables (e.g. BOOST_ROOT, BOOST_LIBRARYDIR, LZ4_ROOT, OPENFST_ROOT, GTEST_ROOT)

set Build Identifier for this build (optional)
```bash
echo "<build_identifier>" > BUILD_IDENTIFIER
```

build library:
```bash
cmake --build .
```

test library:
```bash
cmake --build . --target iresearch-check
```

install library:
```bash   
cmake --build . --target install
```

code coverage:
```bash
cmake --build . --target iresearch-coverage
```

## Pyresearch
There is Python wrapper for IResearch. Wrapper gives access to directory reader object.
For usage example see <src-path>/python/scripts
### Build
To build Pyresearch SWIG generator should be available.
Add -DUSE_PYRESEARCH=ON to cmake command-line to generate Pyresearch targets
### Install
Run target pyresearch-install
#### win32 install notes:
Some version of ICU installers seems to fail to make available all icu dlls through 
PATH enviroment variable, manual adjustment may be needed.
#### (*nix) install notes:
Shared version of libiresearch is used. Install IResearch before running Pyresearch.

## Included 3rd party dependencies
Code for all included 3rd party dependencies is located in the "external" directory.
#### [MurMurHash](https://sites.google.com/site/murmurhash)
used for fast computation of hashes for byte arrays
#### [OpenFST](http://www.openfst.org/twiki/bin/view/FST/WebHome)
used to generate very compact term dictionary prefix tries which can to be loaded
in memory even for huge dictionaries

## External 3rd party dependencies
External 3rd party dependencies must be made available to the IResearch library separately.
They may either be installed through the distribution package management system or build
from source and the appropriate environment variables set accordingly.

### [Boost](http://www.boost.org/doc/libs/1_57_0/more/getting_started/index.html)
v1.57.0 or later (locale system thread)
used for functionality not available in the STL (excluding functionality available in ICU)

### [Lz4](https://code.google.com/p/lz4)
used for compression/decompression of byte/string data

### [Bison](https://www.gnu.org/software/bison)
v2.4 or later
used for compilation of the IQL (index query language) grammar

### [ICU](http://site.icu-project.org/download)
used by locale_utils as a back-end for locale facets
used by analysis::text_analyzer for parsing, transforming and tokenising string data

### [Snowball](http://snowball.tartarus.org)
used by analysis::text_analyzer for computing word stems (i.e. roots) for more flexible matching
matching of words from languages not supported by 'snowball' are done verbatim

### [Google Test](https://code.google.com/p/googletest)
used for writing tests for the IResearch library

### Stopword list
used by analysis::text_analyzer for filtering out noise words that should not impact text ranging
e.g. for 'en' these are usualy 'a', 'the', etc...
download any number of lists of stopwords, e.g. from:
https://github.com/snowballstem/snowball-website/tree/master/algorithms/*/stop.txt
https://code.google.com/p/stop-words/
or create a custom language-specific list of stopwords
place the files with stopwords, (utf8 encoded with one word per line, any text after
the first whitespace is ignored), in the directory corresponding to its language
(multiple files per language are supported and will be interpreted as a single list)


## Query filter building blocks
| Filter    |       Description    |          
|-----------|----------------------|
|iresearch::by_edit_distance|for filtering of values based on Levenshtein distance
|iresearch::by_granular_range|for faster filtering of numeric values within a given range, with the possibility of specifying open/closed ranges
|iresearch::by_ngram_similarity|for filtering of values based on NGram model
|iresearch::by_phrase|for word-position-sensitive filtering of values, with the possibility of skipping selected positions
|iresearch::by_prefix|for filtering of exact value prefixes
|iresearch::by_range|for filtering of values within a given range, with the possibility of specifying open/closed ranges
|iresearch::by_same_position|for term-insertion-order sensitive filtering of exact values
|iresearch::by_term|for filtering of exact values
|iresearch::by_wildcard|for filtering of values based on matching pattern
|iresearch::And|boolean conjunction of multiple filters, influencing document ranks/scores as appropriate
|iresearch::Or|boolean disjunction of multiple filters, influencing document ranks/scores as appropriate (including "minimum match" functionality)
|iresearch::Not|boolean negation of multiple filters

## Index Query Language
The IResearch index may be queries either via query trees built directly using the query building blocks available
in the API or via the IQL query builder that generates a comparable query from a string representation of the query
expressed using the IQL syntax.

#### API
The IQL parser is defined via Bison grammar and is accessible via <b>iresearch::iql::parser</b>
and <b>iresearch::iql::parser_context</b> classes. The latter class is intended to be extended to expose at least the
following methods as required:
- query_state current_state() const;
- query_node const& find_node(parser::semantic_type const& value) const;

The <b>iresearch::iql::parser_context::query_state</b> object provides access to the results of the query parsing as well
as any parse errors reported by Bison.
- nOffset  - next offset position to be parsed (size_t)
- pnFilter - the filter portion (nodeID) of the query, or nullptr if unset (size_t const*)
- order    - the order portion (nodeID, ascending) of the query (std::vector<std::pair<size_t, bool>> const&)
- pnLimit  - the limit value of the query, or nullptr if unset (size_t const*)
- pError   - the last encountered error, or nullptr if no errors seen (iresearch::iql::query_position const*)


#### Grammar
The following grammar is currently defined via Bison (the root is <query>):
```
	<query> ::= <sep>? <union> <order> <limit> <sep>?

	<sep> ::= [[:space:]]+
	        | <sep> "/*" ... "*/"

	<list_sep> ::= <sep>? "," <sep>?

	<union> ::= intersection
	          | <union> <sep> "OR" <sep> <intersection>
	          | <union> <sep>? "||" <sep>? <intersection>

	<intersection> ::= <expression>
	                 | <intersection> <sep> "AND" <sep> <expression>
	                 | <intersection> <sep>? "&&" <sep> <expression>

	<expression> ::= <boost>
	               | <compare>
	               | <negation>
	               | <subexpression>

	<boost> ::= <{float value}> <sep>? "*" <sep>? <subexpression>
	          | <subexpression> <sep>? "*" <sep>? <{float value}>

	<negation> ::= "NOT" <sep>? <subexpression>
	             | "!" <sep>? <subexpression>

	<subexpression> ::= "(" <sep>? <union> <sep>? ")"
	                  | <sequence> "(" <sep>? ")"
	                  | <sequence> "(" <sep>? <term_list> <sep>? ")"

	<compare> ::= <term> <sep>? "~=" <sep>? <term>
	            | <term> <sep>? "!=" <sep>? <term>
	            | <term> <sep>? "<"  <sep>? <term>
	            | <term> <sep>? "<=" <sep>? <term>
	            | <term> <sep>? "==" <sep>? <term>
	            | <term> <sep>? ">=" <sep>? <term>
	            | <term> <sep>? ">"  <sep>? <term>
	            | <term> <sep>? "!=" <sep>? <range>
	            | <term> <sep>? "==" <sep>? <range>

	<range> ::= "[" <sep>? <term> <list_sep> <term> <sep>? "]"
	          | "[" <sep>? <term> <list_sep> <term> <sep>? ")"
	          | "(" <sep>? <term> <list_sep> <term> <sep>? ")"
	          | "(" <sep>? <term> <list_sep> <term> <sep>? "]"

	<term> ::= <function>
	         | <sequence>

	<function> ::= <sequence> "(" <sep>? ")"
	             | <sequence> "(" <sep>? <term_list> <sep>? ")"

	<term_list> ::= <term>
	              | <term_list> <list_sep> <term>

	<sequence> ::= <plain_literal>
	             | <dquoted_literal>
	             | <squoted_literal>

	<plain_literal> ::= <sequence>
	                  | <plain_literal> <sequence>

	<sequence> ::= [^[:space:][:punct:]]+
	             | [[:punct:]][^[:space:][:punct:]]*

	<dquoted_literal> ::= """ [^"]* """
	                    | <dquoted_literal> """ [^"]* """

	<squoted_literal> ::= "'" [^']* "'"
	                    | <squoted_literal> "'" [^']* "'"

	<limit> ::= ""
	          | <sep> "LIMIT" <sep> <{term expanding to an unsigned int value}>

	<order> ::= ""
	          | <sep> "ORDER" <sep> <order_list>

	<order_list> ::= <order_term>
	               | <order_list> <list_sep> <order_term>

	<order_term> ::= <term>
	               | <term> <sep> "ASC"
	               | <term> <sep> "DESC"
```

## Supported compilers

- GCC: 7.3-7.5, 8.1-8.4, 9.1-9.3
- MSVC: 17 (VS 2015), 19 (VS 2017)
- Apple Clang: 9

## License
Copyright (c) 2017-2020 ArangoDB GmbH

Copyright (c) 2016-2017 EMC Corporation

This software is provided under the Apache 2.0 Software license provided in the
[LICENSE.md](LICENSE.md) file. Licensing information for third-party products used
by IResearch search engine can be found in
[THIRD_PARTY_README.md](THIRD_PARTY_README.md)
