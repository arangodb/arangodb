//go:generate go run generate.go ../algorithms algorithms.go
//go:generate gofmt -s -w algorithms.go

package main

import (
	"bufio"
	"flag"
	"fmt"
	"log"
	"os"

	snowballRuntime "github.com/snowballstem/snowball/go"
)

var language = flag.String("l", "", "language")
var input = flag.String("i", "", "input file")
var output = flag.String("o", "", "output file")

func main() {
	flag.Parse()

	if *language == "" {
		log.Fatal("must specify language")
	}

	stemmer, ok := languages[*language]
	if !ok {
		log.Fatalf("no language support for %s", *language)
	}

	var reader = os.Stdin
	if *input != "" {
		var err error
		reader, err = os.Open(*input)
		if err != nil {
			log.Fatal(err)
		}
		defer reader.Close()
	}

	var writer = os.Stdout
	if *output != "" {
		var err error
		writer, err = os.Create(*output)
		if err != nil {
			log.Fatal(err)
		}
		defer writer.Close()
	}

	var err error
	scanner := bufio.NewScanner(reader)
	for scanner.Scan() {
		word := scanner.Text()
		env := snowballRuntime.NewEnv(word)
		stemmer(env)
		fmt.Fprintf(writer, "%s\n", env.Current())
	}

	if err = scanner.Err(); err != nil {
		log.Fatal(err)
	}
}

type StemFunc func(env *snowballRuntime.Env) bool

var languages = make(map[string]StemFunc)
