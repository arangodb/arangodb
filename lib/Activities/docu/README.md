# Activity documentation generator

Walks the project's `compile_commands.json` with Clang LibTooling, finds every
concrete subclass of `arangodb::activities::Activity`, and emits a single
`generated/Activities.md` describing each one's serialized shape (Snapshot
envelope + Data fields, recursively one level for project-local nested types).

The generated file is git-ignored and meant to be regenerated on demand.

## Prerequisites

LLVM/Clang **19** (development headers + libraries) and `jq`:

```sh
sudo apt install libclang-19-dev llvm-19-dev clang-19 jq
```

You also need a configured ArangoDB build directory whose
`compile_commands.json` covers the sources you want to scan. By default the
target reads from `build-presets/my-edition`.

## Build

```sh
cmake -S lib/Activities/docu -B lib/Activities/docu/build
cmake --build lib/Activities/docu/build
```

Override Clang location if needed:

```sh
cmake -S lib/Activities/docu -B lib/Activities/docu/build \
      -DClang_DIR=/usr/lib/llvm-19/lib/cmake/clang \
      -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm
```

## Run

```sh
cmake --build lib/Activities/docu/build --target activity-docs
```

Output lands at `lib/Activities/docu/generated/Activities.md`.

To point at a different build directory:

```sh
cmake -S lib/Activities/docu -B lib/Activities/docu/build \
      -DACTIVITY_DOCS_COMPILE_DB=/path/to/build-presets/community-developer
cmake --build lib/Activities/docu/build --target activity-docs
```

Tunables (env vars read by `run_docs.sh`):

- `ACTIVITY_DOCS_PARALLEL` — number of parallel `xargs` workers (default 8)
- `ACTIVITY_DOCS_BATCH` — files per worker invocation (default 50)

## How it finds subclasses

The tool's AST matcher is:

```cpp
cxxRecordDecl(
    isDefinition(),
    isDerivedFrom(hasName("::arangodb::activities::Activity")),
    unless(hasName("GuardedActivity")))
  .bind("activity");
```

For each match it walks the bases, finds the `GuardedActivity<Self, Data>`
specialization, and resolves the second template argument to the `Data`
record. The Data's public `FieldDecl`s become the documented fields. If a
field's type is a record defined inside the project, one level of nested
fields is also emitted.

To enumerate subclasses of a different base class without recompiling, pass
`--base-class=::your::Base --guarded-template=YourCRTPName` directly to the
binary.
