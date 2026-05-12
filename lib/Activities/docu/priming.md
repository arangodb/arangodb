# Documenting which activities exist

## Quick Overview
Finds every Activity subclass (arangodb::activities::Activity in lib/Activities/include/Activities/Activity.h) that is declared as a member or local variable either in a single source file or a directory.
Concrete subclasses inherit via the CRTP intermediate `GuardedActivity<Self, Data>` which provides a data member of per-subclass type `Data`.

Intermediate representation of an activity (`ActivityDeclaration`):
- owner: source code location of the declaration that holds the activity (FieldDecl or non-param VarDecl)
- `data_type`: fully qualified `Data` type name
- `field_types`: description of the `Data` type (not the full Activity type as the envelope is the same for every activity)

## Tech Stack
- LibTooling
- clang-19

## Patterns
- test driven development: write tests before implementation
- pure functions instead of side effects (better readability and testability)
- use `auto` as often as you can (also for fn definitions)
- instantiate variables only when using them, as few mutations as possible
- no one-letter variables
- write short and concise documentation for every struct or function with
  /**
   * Short sentence explaining what it is (prefered one line)
	 *
	 * If needed, more detailed explanation
	 *
	 * For pulic components: at least one simple code example
	 *
	 * Even more advanced explanations if necessary
	 */

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
- run_docs.sh and generated/ currently unused

## State
- [x] first implementation with local tests
- [ ] GenericActivityData content is also shown in field_types
- [ ] show all data types recursively (including enums)
- [ ] use inspectors for data members
- [ ] improve GenericActivity output
- [ ] convert the results to one markdown file (future session)
