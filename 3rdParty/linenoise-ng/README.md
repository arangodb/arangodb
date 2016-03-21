# Linenoise Next Generation

A small, portable GNU readline replacement for Linux, Windows and
MacOS which is capable of handling UTF-8 characters. Unlike GNU
readline, which is GPL, this library uses a BSD license and can be
used in any kind of program.

## Origin

This linenoise implementation is based on the work by 
[Salvatore Sanfilippo](https://github.com/antirez/linenoise) and
10gen Inc.  The goal is to create a zero-config, BSD
licensed, readline replacement usable in Apache2 or BSD licensed
programs.

## Features

* single-line and multi-line editing mode with the usual key bindings implemented
* history handling
* completion
* BSD license source code
* Only uses a subset of VT100 escapes (ANSI.SYS compatible)
* UTF8 aware
* support for Linux, MacOS and Windows

It deviates from Salvatore's original goal to have a minimal readline
replacement for the sake of supporting UTF8 and Windows. It deviates
from 10gen Inc.'s goal to create a C++ interface to linenoise. This
library uses C++ internally, but to the user it provides a pure C
interface that is compatible with the original linenoise API.
C interface. 

## Requirements

To build this library, you will need a C++11-enabled compiler and
some recent version of CMake.

## Build instructions

To build this library on Linux, first create a build directory

```bash
mkdir -p build
```

and then build the library:

```bash
(cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make)
```

To build and install the library at the default target location, use

```bash
(cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make && sudo make install)
```

The default installation location can be adjusted by setting the `DESTDIR`
variable when invoking `make install`:

```bash
(cd build && make DESTDIR=/tmp install)
```

To build the library on Windows, use these commands in an MS-DOS command 
prompt:

```
md build
cd build
```

After that, invoke the appropriate command to create the files for your
target environment:

* 32 bit: `cmake -G "Visual Studio 12 2013" -DCMAKE_BUILD_TYPE=Release ..`
* 64 bit: `cmake -G "Visual Studio 12 2013 Win64" -DCMAKE_BUILD_TYPE=Release ..`

After that, open the generated file `linenoise.sln` from the `build`
subdirectory with Visual Studio.


*note: the following sections of the README.md are from the original
linenoise repository and are partly outdated*

## Can a line editing library be 20k lines of code?

Line editing with some support for history is a really important
feature for command line utilities. Instead of retyping almost the
same stuff again and again it's just much better to hit the up arrow
and edit on syntax errors, or in order to try a slightly different
command. But apparently code dealing with terminals is some sort of
Black Magic: readline is 30k lines of code, libedit 20k. Is it
reasonable to link small utilities to huge libraries just to get a
minimal support for line editing?

So what usually happens is either:

 * Large programs with configure scripts disabling line editing if
   readline is not present in the system, or not supporting it at all
   since readline is GPL licensed and libedit (the BSD clone) is not
   as known and available as readline is (Real world example of this
   problem: Tclsh).

 * Smaller programs not using a configure script not supporting line
   editing at all (A problem we had with Redis-cli for instance).
 
The result is a pollution of binaries without line editing support.

So Salvatore spent more or less two hours doing a reality check
resulting in this little library: is it *really* needed for a line
editing library to be 20k lines of code? Apparently not, it is possibe
to get a very small, zero configuration, trivial to embed library,
that solves the problem. Smaller programs will just include this,
supporing line editing out of the box. Larger programs may use this
little library or just checking with configure if readline/libedit is
available and resorting to linenoise if not.

## Terminals, in 2010.

Apparently almost every terminal you can happen to use today has some
kind of support for basic VT100 escape sequences. So Salvatore tried
to write a lib using just very basic VT100 features. The resulting
library appears to work everywhere Salvatore tried to use it, and now
can work even on ANSI.SYS compatible terminals, since no VT220
specific sequences are used anymore.

The original library has currently about 1100 lines of code. In order
to use it in your project just look at the *example.c* file in the
source distribution, it is trivial. Linenoise is BSD code, so you can
use both in free software and commercial software.

## Tested with...

 * Linux text only console ($TERM = linux)
 * Linux KDE terminal application ($TERM = xterm)
 * Linux xterm ($TERM = xterm)
 * Linux Buildroot ($TERM = vt100)
 * Mac OS X iTerm ($TERM = xterm)
 * Mac OS X default Terminal.app ($TERM = xterm)
 * OpenBSD 4.5 through an OSX Terminal.app ($TERM = screen)
 * IBM AIX 6.1
 * FreeBSD xterm ($TERM = xterm)
 * ANSI.SYS
 * Emacs comint mode ($TERM = dumb)
 * Windows

Please test it everywhere you can and report back!

## Let's push this forward!

Patches should be provided in the respect of linenoise sensibility for
small and easy to understand code that and the license
restrictions. Extensions must be submitted under a BSD license-style.
A contributor license is required for contributions.

