// +build ignore

package main

import (
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"os"
)

// tool to register all algorithms built with the stemwords tool

func main() {
	flag.Parse()

	if flag.NArg() < 1 {
		log.Fatal("must specify algorithms directory")
	}

	var w io.Writer
	if flag.NArg() > 1 {
		var err error
		w, err = os.Create(flag.Arg(1))
		if err != nil {
			log.Fatalf("error creating output file %v", err)
		}
	} else {
		w = os.Stdout
	}

	fmt.Fprintf(w, "%s", header)

	files, err := ioutil.ReadDir(flag.Arg(0))
	if err != nil {
		log.Fatal(err)
	}

	for _, file := range files {
		fmt.Fprintf(w, "  %s \"github.com/snowballstem/snowball/go/algorithms/%s\"\n",
			file.Name(), file.Name())
	}

	fmt.Fprintf(w, closeImportStartInit)

	for _, file := range files {
		fmt.Fprintf(w, "  languages[\"%s\"] = %s.Stem\n",
			file.Name(), file.Name())
	}

	fmt.Fprintf(w, "%s", footer)
}

var header = `// generated list of supported algorithms, DO NOT EDIT

package main

import (
`

var closeImportStartInit = `)

func init() {`

var footer = `}
`
