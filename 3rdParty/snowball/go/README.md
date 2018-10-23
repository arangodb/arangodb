# Go Target for Snowball

The initial implementation was built as a port of the Rust target.  The initial focus has been on getting it to function, and making it work correctly.  No attempt has been made to beautify the implementation, generated code, or address performance issues.

## Usage

To generate Go source for a Snowball algorithm:
```
$ snowball path/to/algorithm.sbl -go -o algorithm
```

### Go specific options

`-gop[ackage]` the package name used in the generated go file (defaults to `snowball`)

`-gor[untime]` the import path used for the Go Snowball runtime (defaults to `github.com/snowballstem/snowball/go`)

## Code Organization

`compiler/generator_go.c` has the Go code generation logic

`go/` contains the default Go Snowball runtime support

`go/stemwords` contains the source for a Go version of the stemwords utility

`go/algorithms` location where the makefile generated code will end up

## Using the Generated Stemmers

Assuming you generated a stemmer, put that code in a package which is imported by this code as `english`.

```
env := snowball.NewEnv("beautiful")
english.Stem(env)
fmt.Printf("stemmed word is: %s", env.Current())
```

NOTE: you can use the env.SetCurrent("new_word") to reuse the env on subsequent calls to the stemmer.

## Testing

Only the existing Snowball algorithms have been used for testing.  This does not exercise all features of the language.

Run:

```
$ make check_go
```

An initial pass of fuzz-testing has been performed on the generated stemmers for the algorithms in this repo.  Each ran for 5 minutes and used an initial corpus seeded with 10k words from the algorithm's snowballstem-data voc.txt file.

## Known Limitations

- Code going through generate_dollar production has not been tested
- Code going through generate_debug production has not been tested
