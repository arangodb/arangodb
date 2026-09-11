# Documenting which activities exist

## Quick Overview
Finds every Activity subclass (arangodb::activities::Activity in lib/Activities/include/Activities/Activity.h) that is declared as a member or local variable either in a single source file or a directory.
Concrete subclasses inherit via the CRTP intermediate `GuardedActivity<Self, Data>` which provides a data member of per-subclass type `Data`.

Intermediate representation of an activity (`ActivityDeclaration`):
- `owner`: name of the declaration that holds the activity (FieldDecl or non-param VarDecl)
- `type`: fully qualified Activity subclass type name
- `data_type_definition`: description of the `Data` type (not the full Activity type as the envelope is the same for every activity)

## Tech Stack
- LibTooling
- clang-19

## Naming conventions
- Files: snake_case
- Functions: snake_case
- Types: PascalCase
- Constants: SCREAMING_SNAKE_CASE
- Booleans: is/has/can prefix

## Non-obvious behavior
- header input files are routed to their sibling .cpp files
- one activity class (e.g. `GenericActivity`) can be used for more than one activity at different source locations
- skips activity declarations done inside the activity library (only internal plumbing)
- skips source paths in `<root>/Documentation` and `<root>/3rdParty` (with a warning); `<root>` is found via git from the `--build-path` build directory, not from a source path (a path inside a submodule like `enterprise` or `3rdParty/*` would resolve to the submodule root instead)
- a source path inside the `enterprise` submodule adds that submodule's own commit id to the markdown output next to arangodb's

## State
- [x] first implementation with local tests
- [x] show all data types recursively
- [x] convert the results to one markdown file (future session)
- [ ] describe not only structs/classes but also enums and aliases in data_type_definition
- [ ] also show the activity's "type" string, given by the user
- [ ] use inspectors for data members
- [ ] stream results: give results as soon as they are found
