# ArangoDB coding guidelines

This document is mostly derived from [Google C++ Style
Guide](https://google.github.io/styleguide/cppguide.html#C++_Version) with some
ArangoDB specific adjustments.

## Preamble

These coding guidelines represent the C++ code style that we want to follow at ArangoDB.
As the ArangoDB code base has evolved over a period of around 10 years, there still exists
some code which does not or not fully follow these guidelines. Older code that is not following
the guidelines described here will eventually be adjusted to follow these guidelines. This will
take time.
Any _newly_ added code however should follow the guidelines described here. When in doubt,
please always follow the coding guidelines described here.

## Naming

The most important consistency rules are those that govern naming. The style of
a name immediately informs us what sort of thing the named entity is: a type, a
variable, a function, a constant, a macro, etc., without requiring us to search
for the declaration of that entity. The pattern-matching engine in our brains
relies a great deal on these naming rules.

Naming rules are pretty arbitrary, but we feel that consistency is more
important than individual preferences in this area, so regardless of whether you
find them sensible or not, the rules are the rules.

### General Naming Rules

Optimize for readability using names that would be clear even to people on a
different team.

Use names that describe the purpose or intent of the object. Do not worry about
saving horizontal space as it is far more important to make your code
immediately understandable by a new reader. Minimize the use of abbreviations
that would likely be unknown to someone outside your project (especially
acronyms and initialisms). Do not abbreviate by deleting letters within a word.
As a rule of thumb, an abbreviation is probably OK if it's listed in Wikipedia.
Generally speaking, descriptiveness should be proportional to the name's scope
of visibility. For example, n may be a fine name within a 5-line function, but
within the scope of a class, it's likely too vague.

```
class MyClass {
 public:
  int countFooErrors(std::vector<Foo> const& foos) {
    int n = 0;  // Clear meaning given limited scope and context
    for (auto const& foo : foos) {
      ...
      ++n;
    }
    return n;
  }
  void doSomethingImportant() {
    std::string fqdn = ...;  // Well-known abbreviation for Fully Qualified Domain Name
  }
 private:
  int const kMaxAllowedConnections = ...;  // Clear meaning within context
};

class MyClass {
 public:
  int countFooErrors(std::vector<Foo> const& foos) {
    int totalNumberOfFooErrors = 0;  // Overly verbose given limited scope and context
    for (size_t fooIndex = 0; fooIndex < foos.size(); ++fooIndex) {  // Use idiomatic `i`
      ...
      ++totalNumberOfFooErrors;
    }
    return totalNumberOfFooErrors;
  }
  void doSomethingImportant() {
    int cstmrId = ...;  // Deletes internal letters
  }
 private:
  int const kNum = ...;  // Unclear meaning within broad scope
};
```

Note that certain universally-known abbreviations are OK, such as i for an
iteration variable and T for a template parameter.

For the purposes of the naming rules below, a "word" is anything that you would
write in English without internal spaces. This includes abbreviations, such as
acronyms and initialisms. For names written in mixed case (also sometimes
referred to as "[camel case](https://en.wikipedia.org/wiki/Camel_case)" or
"[Pascal case](https://en.wiktionary.org/wiki/Pascal_case)"), in which the first
letter of each word except the first one is capitalized, prefer to capitalize
abbreviations as single words, e.g., startRpc() rather than startRPC().

Template parameters should follow the naming style for their category: type
template parameters should follow the rules for type names, and non-type
template parameters should follow the rules for variable names.

### File Names

Filenames start with a capital letter and have a capital letter for each new
word, with no underscores: `MyUsefulClass.cpp`, `VeryUsefulClass.cpp`

C++ files should end in `.cpp` and header files should end in `.h`. Files that
rely on being textually included at specific points should end in `.inc` (see
also the section on self-contained headers). Do not use filenames that already
exist in `/usr/include`, such as `db.h`. In general, make your filenames very
specific. For example, use `HttpServerLogs.h` rather than `Logs.h`. A very
common case is to have a pair of files called, e.g., `FooBar.h` and
`FooBar.cpp`, defining a class called `FooBar`.

### Type Names

Type names start with a capital letter and have a capital letter for each new
word, with no underscores: `MyExcitingClass`, `MyExcitingEnum`.

The names of all types — classes, structs, type aliases, enums, and type
template parameters — have the same naming convention. Type names should start
with a capital letter and have a capital letter for each new word. No
underscores. For example:

```
// classes and structs
class UrlTable { ...
class UrlTableTester { ...
struct UrlTableProperties { ...

// typedefs
typedef hash_map<UrlTableProperties*, std::string> PropertiesMap;

// using aliases
using PropertiesMap = hash_map<UrlTableProperties*, std::string>;

// enums
enum class UrlTableError { ...
```

### Variable Names

The names of variables (including function parameters) should follow "[camel
case](https://en.wikipedia.org/wiki/Camel_case)" practice with no underscores
between words. Data members of classes (but not structs) additionally have
leading underscores. For instance: `aLocalVariable`, `aStructDataMember`,
`_aClassDataMember`.

#### Common Variable names

For example:

```
std::string table_name;  // BAD - lowercase with underscore.

std::string tableName;   // Ok - mixed case.
```

#### Class Data Members

Data members of classes, both static and non-static, are named like ordinary
nonmember variables, but with a leading underscore.

```
class TableInfo {
  ...
 private:
  std::string _tableName;  // OK - mixed case with leading underscore
  static Pool<TableInfo>* _pool;  // OK.
};
```

#### Struct Data Members

Data members of structs, both static and non-static, are named like ordinary
nonmember variables. They do not have the leading underscores that data members
in classes have.

```
struct UrlTableProperties {
  std::string name;
  int numEntries;
  static Pool<UrlTableProperties>* pool;
};
```

#### Constant Names

Variables declared constexpr or const, and whose value is fixed for the duration
of the program, are named with a leading "k" followed by mixed case. Underscores
can be used as separators in the rare cases where capitalization cannot be used
for separation. For example:

```
int const kDaysInAWeek = 7;
int const kAndroid8_0_0 = 24;  // Android 8.0.0
```

All such variables with static storage duration (i.e., statics and globals, see
Storage Duration for details) should be named this way. This convention is
optional for variables of other storage classes, e.g., automatic variables,
otherwise the usual variable naming rules apply.

#### Function Names

Regular functions have mixed case; accessors and mutators may be named like
variables.

Ordinarily, functions should start with a non-capital letter and have a capital
letter for each new word.

```
addTableEntry()
deleteUrl()
openFileOrDie()
```

(The same naming rule applies to class- and namespace-scope constants that are
exposed as part of an API and that are intended to look like functions, because
the fact that they're objects rather than functions is an unimportant
implementation detail.)

Accessors and mutators (get and set functions) may be named like variables.
These often correspond to actual member variables, but this is not required. For
example, `int count()` and void `setCount(int count)`.

#### Namespace Names

Namespace names are all lower-case, with words separated by underscores.
Top-level namespace names are based on the project name . Avoid collisions
between nested namespaces and well-known top-level namespaces.

The code in that namespace should usually be in a directory whose basename
matches the namespace name (or in subdirectories thereof).

Avoid nested namespaces that match well-known top-level namespaces. Collisions
between namespace names can lead to surprising build breaks because of name
lookup rules. In particular, do not create any nested std namespaces. Prefer
unique project identifiers (`websearch::index`, `websearch::index_util`) over
collision-prone names like `websearch::util`. Also avoid overly deep nesting
namespaces ([TotW #130](https://abseil.io/tips/130)).

For `internal` namespaces, be wary of other code being added to the same `internal`
namespace causing a collision (internal helpers within a team tend to be related
and may lead to collisions). In such a situation, using the filename to make a
unique internal name is helpful (`websearch::index::frobber_internal` for use in
`frobber.h`).

#### Enumerator Names

Enumerators (for both scoped and unscoped enums) should be named like constants,
not like macros. That is, use `kEnumName` not `ENUM_NAME`.

```
enum class UrlTableError {
  kOk = 0,
  kOutOfMemory,
  kMalformedInput,
};
```
```
// BAD practice
enum class AlternateUrlTableError {
  OK = 0,
  OUT_OF_MEMORY = 1,
  MALFORMED_INPUT = 2,
};
```

Until January 2009, the style was to name enum values like macros. This caused
problems with name collisions between enum values and macros. Hence, the change
to prefer constant-style naming was put in place. New code should use
constant-style naming.

#### Macro Names

You're not really going to define a macro, are you? If you do, they're like this: 
`MY_MACRO_THAT_SCARES_SMALL_CHILDREN_AND_ADULTS_ALIKE`.

Please see the description of macros; in general macros should not be used.
However, if they are absolutely needed, then they should be named with all
capitals and underscores.

```
#define ROUND(x) ...
#define PI_ROUNDED 3.0
```

### Comments

Comments are absolutely vital to keeping our code readable. The following rules
describe what you should comment and where. But remember: while comments are
very important, the best code is self-documenting. Giving sensible names to
types and variables is much better than using obscure names that you must then
explain through comments.

When writing your comments, write for your audience: the next contributor who
will need to understand your code. Be generous — the next one may be you!

#### Comment Style

Use either the `//` or `/* */` syntax, as long as you are consistent.

You can use either the `//` or the `/* */` syntax; however, `//` is much more
common. Be consistent with how you comment and what style you use where.

#### File Comments

Start each file with license boilerplate. 

File comments describe the contents of a file. If a file declares, implements,
or tests exactly one abstraction that is documented by a comment at the point of
declaration, file comments are not required. All other files must have file
comments.

#### Legal Notice and Author Line

Every file should contain license boilerplate. If you make significant changes
to a file with an author line, consider deleting the author line. Use
[non-enterprise](#non-enterprise-template) template for public arangodb
codebase, [enterprise template](#enterprise-template) for the enterprise
part.

##### Non-enterprise template

```
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////
```

##### Enterprise template
```
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
///
/// The Programs (which include both the software and documentation) contain
/// proprietary information of ArangoDB GmbH; they are provided under a license
/// agreement containing restrictions on use and disclosure and are also
/// protected by copyright, patent and other intellectual and industrial
/// property laws. Reverse engineering, disassembly or decompilation of the
/// Programs, except to the extent required to obtain interoperability with
/// other independently created software or as specified by law, is prohibited.
///
/// It shall be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of
/// applications if the Programs are used for purposes such as nuclear,
/// aviation, mass transit, medical, or other inherently dangerous applications,
/// and ArangoDB GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of ArangoDB
/// GmbH. You shall not disclose such confidential and proprietary information
/// and shall use it only in accordance with the terms of the license agreement
/// you entered into with ArangoDB GmbH.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////
```

#### File Contents

If a `.h` declares multiple abstractions, the file-level comment should broadly
describe the contents of the file, and how the abstractions are related. A 1 or
2 sentence file-level comment may be sufficient. The detailed documentation
about individual abstractions belongs with those abstractions, not at the file
level.

Do not duplicate comments in both the .h and the .cpp. Duplicated comments
diverge.
