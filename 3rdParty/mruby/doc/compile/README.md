# Compile

mruby is using Rake to compile and cross-compile all libraries and
binaries.

## Prerequisites

To compile mruby out of the source code you need the following tools:
* C Compiler (i.e. ```gcc```)
* Linker (i.e. ```gcc```)
* Archive utility (i.e. ```ar```)
* Parser generator (i.e. ```bison```)
* Ruby 1.8 or 1.9

Optional:
* GIT (to update mruby source and integrate mrbgems easier)
* C++ compiler (to use GEMs which include *.cpp)
* Assembler (to use GEMs which include *.asm)

## Usage

Inside of the root directory of the mruby source exist a file
called *build_config.rb*. This file contains the build configuration
of mruby and looks like this for example:

```
MRuby::Build.new do |conf|
  conf.cc = ENV['CC'] || 'gcc'
  conf.ld = ENV['LD'] || 'gcc'
  conf.ar = ENV['AR'] || 'ar'

  conf.cflags << (ENV['CFLAGS'] || %w(-g -O3 -Wall -Werror-implicit-function-declaration))
  conf.ldflags << (ENV['LDFLAGS'] || %w(-lm))
end
```

All tools necessary to compile mruby can be set or modified here.
The following options can be configurated:

* conf.cc (C compiler)
* conf.ld (Linker)
* conf.ar (Archive utility)
* conf.cxx (C++ compiler)
* conf.objcc (Object compiler)
* conf.asm (Assembler)
* conf.yacc (Parser Generator)
* conf.gperf (Hash function Generator)
* conf.cat (Concatenate utility)
* conf.git (GIT content tracker)
* conf.cflags (C compiler flags)
* conf.ldflags (Linker flags)
* conf.cxxflags (C++ compiler flags) 
* conf.objccflags (Object compiler flags)
* conf.asmflags (Assembler flags)
* conf.gem (A GEM which should be integrated - can be set several times)
* conf.bins (Build binaries)

To compile just call ```./minirake``` inside of the mruby source root. To
generate the test tool environment call ```./minirake test```. To clean
all build files call ```./minirake clean```.

### Cross-Compilation

mruby can also be cross-compiled from one platform to another. To
achive this the *build_config.rb* needs to contain an instance of
```MRuby::CrossBuild```. This instance defines the compilation
tools and flags for the target platform. An example could look
like this for example:

```
MRuby::CrossBuild.new('i386') do |conf|
  conf.cc = ENV['CC'] || 'gcc'
  conf.ld = ENV['LD'] || 'gcc'
  conf.ar = ENV['AR'] || 'ar'

  if ENV['OS'] == 'Windows_NT' # MinGW
    conf.cflags = %w(-g -O3 -Wall -Werror-implicit-function-declaration -Di386_MARK)
    conf.ldflags = %w(-s -static)
  else
    conf.cflags << %w(-g -O3 -Wall -Werror-implicit-function-declaration -arch i386)
    conf.ldflags << %w(-arch i386)
  end
end
```

You can configurate the same options as for a normal build. You can specified your own build_config.rb with *$MRUBY_CONFIG*.

## Build process

During the build process the directory *build* will be created. The
directory structure will look like this:

```
+- build
   |
   +-  host
       |
       +- bin          <- Binaries (mirb, mrbc and mruby)
       |
       +- lib          <- Libraries (libmruby.a and libmruby_core.a)
       |
       +- mrblib
       |
       +- src
       |
       +- test         <- mrbtest tool
       |
       +- tools
          |
          +- mirb
          |
          +- mrbc
          | 
          +- mruby
```

The compilation workflow will look like this:
* compile all files under *src* (object files will be stored 
in *build/host/src*
* generate parser grammar out of *src/parse.y* (generated
result will be stored in *build/host/src/y.tab.c*
* compile  *build/host/src/y.tab.c* to  *build/host/src/y.tab.o*
* create *build/host/lib/libmruby_core.a* out of all object files (C only)
* create ```build/host/bin/mrbc``` by compile *tools/mrbc/mrbc.c* and
link with *build/host/lib/libmruby_core.a* 
* create *build/host/mrblib/mrblib.c* by compiling all *.rb files
under *mrblib* with ```build/host/bin/mrbc```
* compile *build/host/mrblib/mrblib.c* to *build/host/mrblib/mrblib.o* 
* create *build/host/lib/libmruby.a* out of all object files (C and Ruby)
* create ```build/host/bin/mruby``` by compile *tools/mruby/mruby.c* and
link with *build/host/lib/libmruby.a*
* create ```build/host/bin/mirb``` by compile *tools/mirb/mirb.c* and
link with *build/host/lib/libmruby.a*

### Cross-Compilation

In case of a cross-compilation to *i386* the *build* directory structure looks
like this:

```
+- build
   |
   +-  host
   |   |
   |   +- bin           <- Native Binaries
   |   |
   |   +- lib           <- Native Libraries
   |   |
   |   +- mrblib
   |   |
   |   +- src
   |   |
   |   +- test          <- Native mrbtest tool
   |   |
   |   +- tools
   |      |
   |      +- mirb
   |      |
   |      +- mrbc
   |      | 
   |      +- mruby 
   +- i386
      |
      +- bin            <- Cross-compiled Binaries
      |
      +- lib            <- Cross-compiled Libraries
      |
      +- mrblib
      |
      +- src
      |
      +- test           <- Cross-compiled mrbtest tool
      |
      +- tools
         |
         +- mirb
         |
         +- mrbc
         | 
         +- mruby
```

An extra directory is created for the target platform. In case you
compile for *i386* a directory called *i386* is created under the
build direcotry.

The cross compilation workflow starts in the same way as the normal
compilation by compiling all *native* libraries and binaries.
Aftwards the cross compilation process proceeds like this:
* cross-compile all files under *src* (object files will be stored 
in *build/i386/src*
* generate parser grammar out of *src/parse.y* (generated
result will be stored in *build/i386/src/y.tab.c*
* cross-compile  *build/i386/src/y.tab.c* to  *build/i386/src/y.tab.o*
* create *build/i386/mrblib/mrblib.c* by compiling all *.rb files
under *mrblib* with the native ```build/host/bin/mrbc```
* cross-compile *build/host/mrblib/mrblib.c* to *build/host/mrblib/mrblib.o*
* create *build/i386/lib/libmruby.a* out of all object files (C and Ruby)
* create ```build/i386/bin/mruby``` by cross-compile *tools/mruby/mruby.c* and
link with *build/i386/lib/libmruby.a*
* create ```build/i386/bin/mirb``` by cross-compile *tools/mirb/mirb.c* and
link with *build/i386/lib/libmruby.a*
* create *build/i386/lib/libmruby_core.a* out of all object files (C only)
* create ```build/i386/bin/mrbc``` by cross-compile *tools/mrbc/mrbc.c* and
link with *build/i386/lib/libmruby_core.a* 

## Test Environment

mruby's build process includes a test environment. In case you start the testing
of mruby, a native binary called ```mrbtest``` will be generated and executed.
This binary contains all test cases which are defined under *test/t*. In case
of a cross-compilation an additional cross-compiled *mrbtest* binary is 
generated. This binary you can copy and run on your target system.
