# Activity documentation generator

Looks at all files in a given path and finds every concrete subclass instance (either a field in a record or a local variable) of `arangodb::activities::Activity`.

It walks the project's `compile_commands.json` with Clang LibTooling to find these instances and produces markdown output on stdout describing each activity's serialized shape and its owner.

## Prerequisites

LLVM/Clang **19** (development headers + libraries):

```sh
sudo apt install libclang-19-dev llvm-19-dev clang-19
```

You also need a configured ArangoDB build directory whose
`compile_commands.json` covers the sources you want to scan (arangodb configures
this automatically). You pass that directory with `--build-path`.

`git` has to be on `PATH`: the generated document names the commit the scan was
made from, which is looked up on every run.

## Build

Building is currently hidden behind the feature-flag `USE_ACTIVITY_DOCS`:
```sh
cmake --build <build-dir> -DUSE_ACTIVITY_DOCS=On find-activity-subclasses
```

## Run

```sh
./find-activity-subclasses --build-path <build-path> <source-path> [<source-path> ...]
```

- `--build-path` (or `-p`, required) is the arangodb build directory that holds
  `compile_commands.json`.
- Every following positional argument is a source path to search for activities.
  At least one is required; directories are searched recursively.

Each source path must be part of the compilation database, otherwise it is
skipped with a warning on stderr. The two directories `<root>/Documentation` and
`<root>/3rdParty` are always ignored; naming one explicitly is skipped with a
warning as well (`<root>` is the arangodb repository, found via git from the
build path). When nothing is left to scan the program exits with a non-zero
status. `--help` (`-h`) prints usage.

Output is the markdown document on stdout. Its header names the commit the
arangodb repository is checked out at, so the list can be reproduced later; the
commit reads `unknown` when it cannot be determined. When a source path lies in
the `enterprise` submodule, that submodule's own commit is listed as well.

## Test

Building is currently hidden behind the feature-flag `USE_ACTIVITY_DOCS`:
```sh
cmake --build <build-dir> -DUSE_ACTIVITY_DOCS=On arangodbtests_activities_documentation
./arangodbtests_activities_documentation
```
