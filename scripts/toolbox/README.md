# Python Developer Toolbox

Python based Toolbox for various development tasks

## Description

This project started to bundle some development helpers into a single toolbox.

## Getting Started

### Dependencies

* Python3 (please install the requirements.txt via pip)
* Go
* Self compiled ArangoDB in the default build directory ("build")

### Installing

* Currently, no specific installation is needed. Just clone the repository and run the scripts.

### Executing program

* Example of how to run the program, more to follow...

```
python3 main.py --mode=single --init=true --startupParameters='{"rocksdb.block-cache-size": "133713371337"}'
```

### Organization

* The project is organized into the following folders:
    * `main.py`: The main entry point for the toolbox.
    * `config.py`: The configuration file for the toolbox.
    * `modules`: Contains the python scripts for the toolbox (e.g. start ArangoDB, run the Feed tool).
      * All separated into its own module.
    * `tests`: Contains the tests for the toolbox.
    * `work`: Will be the output folder for everything the toolbox does.
    * `README.md`: This file.
