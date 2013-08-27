# mrbgems

mrbgems is a library manager to integrate C and Ruby extension in an easy and
standardised way into mruby.

## Usage

By default mrbgems is currently deactivated. As soon as you add a GEM to your
build configuration (i.e. *build_config.rb*), mrbgems will be activated and the
extension integrated.

To add a GEM into the *build_config.rb* add the following line for example:

	conf.gem '/path/to/your/gem/dir'

You can also use a relative path which would be relative from the mruby root:

	conf.gem 'examples/mrbgems/ruby_extension_example'

A remote GIT repository location for a GEM is also supported:

	conf.gem :git => 'https://github.com/masuidrive/mrbgems-example.git', :branch => 'master'

	conf.gem :github => 'masuidrive/mrbgems-example', :branch => 'master'

	conf.gem :bitbucket => 'mruby/mrbgems-example', :branch => 'master'

To pull all gems from remote GIT repository on build, call ```./minirake -p```, 
or ```./minirake --pull-gems```.

NOTE: `:bitbucket` option supports only git. Hg is unsupported in this version.

## GemBox

There are instances when you wish to add a collection of mrbgems into mruby at 
once, or be able to substitute mrbgems based on configuration, without having to
add each gem to the *build_config.rb* file.  A packaged collection of mrbgems 
is called a GemBox.  A GemBox is a file that contains a list of mrbgems to load 
into mruby, in the same format as if you were adding them to *build_config.rb*
via `config.gem`, but wrapped in an `MRuby::GemBox` object.  GemBoxes are 
loaded into mruby via `config.gembox 'boxname'`.

Below we have created a GemBox containing *mruby-time* and *mrbgems-example*:

    MRuby::GemBox.new do |conf|
      conf.gem "#{root}/mrbgems/mruby-time"
      conf.gem :github => 'masuidrive/mrbgems-example'
    end

As mentioned, the GemBox uses the same conventions as `MRuby::Build`.  The GemBox
must be saved with a *.gembox* extension inside the *mrbgems* directory to to be
picked up by mruby.

To use this example GemBox, we save it as `custom.gembox` inside the *mrbgems* 
directory in mruby, and add the following to our *build_config.rb* file inside 
the build block:

    conf.gembox 'custom'

This will cause the *custom* GemBox to be read in during the build process,
adding *mruby-time* and *mrbgems-example* to the build.

If you want, you can put GemBox outside of mruby directory. In that case you must 
specify absolute path like below.

	conf.gembox "#{ENV["HOME"]}/mygemboxes/custom"

There are two GemBoxes that ship with mruby: [default](../../mrbgems/default.gembox)
and [full-core](../../mrbgems/full-core.gembox). The [default](../../mrbgems/default.gembox) GemBox
contains several core components of mruby, and [full-core](../../mrbgems/full-core.gembox)
contains every gem found in the *mrbgems* directory.

## GEM Structure

The maximal GEM structure looks like this:

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

The folder *mrblib* contains pure Ruby files to extend mruby. The folder *src*
contains C files to extend mruby. The folder *test* contains C and pure Ruby files
for testing purposes which will be used by `mrbtest`. *mrbgem.rake* contains
the specification to compile C and Ruby files. *README.md* is a short description
of your GEM.

## Build process

mrbgems expects a specification file called *mrbgem.rake* inside of your
GEM directory. A typical GEM specification could look like this for example:

	MRuby::Gem::Specification.new('c_and_ruby_extension_example') do |spec|
	  spec.license = 'MIT'
	  spec.author  = 'mruby developers'
	end

The mrbgems build process will use this specification to compile Object and Ruby
files. The compilation results will be added to *lib/libmruby.a*. This file exposes
the GEM functionality to tools like `mruby` and `mirb`.

The following properties can be set inside of your `MRuby::Gem::Specification` for
information purpose:

* `spec.license` or `spec.licenses` (A single license or a list of them under which this GEM is licensed)
* `spec.author` or `spec.authors` (Developer name or a list of them)
* `spec.version` (Current version)
* `spec.description` (Detailed description)
* `spec.summary` (Short summary)
* `spec.homepage` (Homepage)
* `spec.requirements` (External requirements as information for user)

The license and author properties are required in every GEM!

In case your GEM is depending on other GEMs please use
`spec.add_dependency(gem, *requirements)` like:

	MRuby::Gem::Specification.new('c_and_ruby_extension_example') do |spec|
	  spec.license = 'MIT'
	  spec.author  = 'mruby developers'

	  # add GEM dependency mruby-parser.
	  # Version has to be between 1.0.0 and 1.5.2
	  spec.add_dependency('mruby-parser', '> 1.0.0', '< 1.5.2')
	end

The usage of versions is optional.

__ATTENTION:__
The dependency system is currently (May 2013) under development and doesn't check
or resolve dependencies!

In case your GEM has more complex build requirements you can use
the following options additionally inside of your GEM specification:

* `spec.cflags` (C compiler flags)
* `spec.mruby_cflags` (global C compiler flags for everything)
* `spec.mruby_ldflags` (global linker flags for everything)
* `spec.mruby_libs` (global libraries for everything)
* `spec.mruby_includes` (global includes for everything)
* `spec.rbfiles` (Ruby files to compile)
* `spec.objs` (Object files to compile)
* `spec.test_rbfiles` (Ruby test files for integration into mrbtest)
* `spec.test_objs` (Object test files for integration into mrbtest)
* `spec.test_preload` (Initialization files for mrbtest)

## C Extension

mruby can be extended with C. This is possible by using the C API to
integrate C libraries into mruby.

### Preconditions

mrbgems expects that you have implemented a C method called
`mrb_YOURGEMNAME_gem_init(mrb_state)`. `YOURGEMNAME` will be replaced
by the name of your GEM. If you call your GEM *c_extension_example*, your
initialisation method could look like this:

	void
	mrb_c_extension_example_gem_init(mrb_state* mrb) {
	  struct RClass *class_cextension = mrb_define_module(mrb, "CExtension");
	  mrb_define_class_method(mrb, class_cextension, "c_method", mrb_c_method, MRB_ARGS_NONE());
	}

### Finalize

mrbgems expects that you have implemented a C method called
`mrb_YOURGEMNAME_gem_final(mrb_state)`. `YOURGEMNAME` will be replaced
by the name of your GEM. If you call your GEM *c_extension_example*, your
finalizer method could look like this:

	void
	mrb_c_extension_example_gem_final(mrb_state* mrb) {
	  free(someone);
	}

### Example

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

## Ruby Extension

mruby can be extended with pure Ruby. It is possible to override existing
classes or add new ones in this way. Put all Ruby files into the *mrblib*
folder.

### Pre-Conditions

none

### Example

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

## C and Ruby Extension

mruby can be extended with C and Ruby at the same time. It is possible to
override existing classes or add new ones in this way. Put all Ruby files
into the *mrblib* folder and all C files into the *src* folder.

### Pre-Conditions

See C and Ruby example.

### Example

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
