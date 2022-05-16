# Boost.GIL Documentation

A simple guide about writing and building documentation for Boost.GIL.

## Prerequisites

- Python 3
- [Install Sphinx](#install-sphinx) (see `requirements.txt`)
- Install [Doxygen](http://www.doxygen.org)


## Installation

Create Python virtual environment:

```console
$ python3 -m venv .venv
$ source ~/.venv/bin/activate
```

Install Sphinx and Sphinx extensions:

```console
(.venv)$ cd boost-root
(.venv)$ pip install -r libs/gil/doc/requirements.txt
```

## Build

```console
$ echo "using doxygen ;" > ~/user-config.jam
$ cd boost-root
$ b2 libs/gil/doc
```
