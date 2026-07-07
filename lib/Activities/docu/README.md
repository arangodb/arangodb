# Activity documentation generator

Looks at all files in a given path and finds every concrete subclass instance (either a field in a record or a local variable) of `arangodb::activities::Activity`.

It walks the project's `compile_commands.json` with Clang LibTooling to find these instances and markdown output on stdout describing each activity's serialized shape and its owner.

## Prerequisites

LLVM/Clang **19** (development headers + libraries):

```sh
sudo apt install libclang-19-dev llvm-19-dev clang-19
```

You also need a configured ArangoDB build directory whose
`compile_commands.json` covers the sources you want to scan. Make sure that
this file is located at the repository root. 

## Build

```sh
cmake --build find-activity-subclasses
```

## Run

```sh
./find-activity-subclasses <path in which to to search for activities (directory or file)>
```

Output is written to stdout.
