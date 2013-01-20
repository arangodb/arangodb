# mrbgems

mrbgems is a library manager to integrate C and Ruby extension in an easy and
standardised way into mruby.

## Usage

By default mrbgems is currently deactivated. As soon as you add a GEM to your
build configuration (*build_config.rb*), mrbgems will be activated and the
extension integrated.

To add a GEM into the build_config.rb add the following line for example:

```
conf.gem '/path/to/your/gem/dir'
```

You can also use a relative path which would be relative from the mruby root:

```
conf.gem 'doc/mrbgems/ruby_extension_example'
```

A remote GIT repository location for a GEM is also supported:

```
conf.gem :git => 'https://github.com/masuidrive/mrbgems-example.git', :branch => 'master'
```

```
conf.gem :github => 'masuidrive/mrbgems-example', :branch => 'master'
```


## GEM Structure

The maximal GEM structure looks like this:

```
+- GEM_NAME         <- Name of GEM
   |
   +- mrblib/       <- Source for Ruby extension
   |
   +- src/          <- Source for C extension
   |
   +- test/         <- Test code (Ruby)
   |
   +- mrbgem.rake   <- GEM Specification
   |
   +- README.md     <- Readme for GEM
```

The folder *mrblib* contains pure Ruby files to extend mruby. The folder *src*
contains C files to extend mruby. The folder *test* contains C and pure Ruby files
for testing purposes which will be used by ```mrbtest```. *mrbgem.rake* contains
the specification to compile C and Ruby files. *README.md* is a short description
of your GEM.

## Build process

mrbgems expects a specifcation file called *mrbgem.rake* inside of your
GEM direcotry. A typical GEM specification could look like this for example:

```
MRuby::Gem::Specification.new('c_and_ruby_extension_example') do |spec|
  spec.license = 'MIT'
  spec.authors = 'mruby developers'
end
```

The mrbgems build process will use this specification to compile Object and Ruby
files. The compilation results will be add to *lib/libmruby.a*. This file is used
by tools like ```mruby``` and ```mirb``` to empower the GEM functionality.

In case your GEM has more complex build requirements you can use
the following options additionally inside of your GEM specification:

* spec.cflags (C compiler flags for this GEM)
* spec.mruby_cflags (global C compiler flags for everything)
* spec.mruby_ldflags (global linker flags for everything)
* spec.mruby_libs (global libraries for everything)
* spec.mruby_includes (global includes for everything)
* spec.rbfiles (Ruby files to compile)
* spec.objs (Object files to compile)
* spec.test_rbfiles (Ruby test files for integration into mrbtest)
* spec.test_objs (Object test files for integration into mrbtest)
* spec.test_preload (Initialization files for mrbtest)

## C Extension

mruby can be extended with C. This is possible by using the C API to
integrate C libraries into mruby.

### Pre-Conditions

mrbgems expects that you have implemented a C method called
```mrb_YOURGEMNAME_gem_init(mrb_state)```. ```YOURGEMNAME``` will be replaced
by the name of your GEM. If you call your GEM *c_extension_example*, your
initialisation method could look like this:

```
void
mrb_c_extension_example_gem_init(mrb_state* mrb) {
  struct RClass *class_cextension = mrb_define_module(mrb, "CExtension");
  mrb_define_class_method(mrb, class_cextension, "c_method", mrb_c_method, ARGS_NONE());
}
```

### Example

```
+- c_extension_example/
   |
   +- src/
   |  |
   |  +- example.c         <- C extension source
   |
   +- test/
   |  |
   |  +- example.rb        <- Test code for C extension
   |
   +- mrbgem.rake          <- GEM specification
   |
   +- README.md
```

## Ruby Extension

mruby can be extended with pure Ruby. It is possible to override existing
classes or add new ones in this way. Put all Ruby files into the *mrblib*
folder.

### Pre-Conditions

none

### Example

```
+- ruby_extension_example/
   |
   +- mrblib/
   |  |
   |  +- example.rb        <- Ruby extension source
   |
   +- test/
   |  |
   |  +- example.rb        <- Test code for Ruby extension
   |
   +- mrbgem.rake          <- GEM specification
   |
   +- README.md
```

## C and Ruby Extension

mruby can be extended with C and Ruby at the same time. It is possible to
override existing classes or add new ones in this way. Put all Ruby files
into the *mrblib* folder and all C files into the *src* folder.

### Pre-Conditions

See C and Ruby example.

### Example

```
+- c_and_ruby_extension_example/
   |
   +- mrblib/
   |  |
   |  +- example.rb        <- Ruby extension source
   |
   +- src/
   |  |
   |  +- example.c         <- C extension source
   |
   +- test/
   |  |
   |  +- example.rb        <- Test code for C and Ruby extension
   |
   +- mrbgem.rake          <- GEM specification
   |
   +- README.md
