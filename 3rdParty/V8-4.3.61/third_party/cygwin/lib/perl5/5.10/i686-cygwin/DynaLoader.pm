
# Generated from DynaLoader_pm.PL

package DynaLoader;

#   And Gandalf said: 'Many folk like to know beforehand what is to
#   be set on the table; but those who have laboured to prepare the
#   feast like to keep their secret; for wonder makes the words of
#   praise louder.'

#   (Quote from Tolkien suggested by Anno Siegel.)
#
# See pod text at end of file for documentation.
# See also ext/DynaLoader/README in source tree for other information.
#
# Tim.Bunce@ig.co.uk, August 1994

BEGIN {
    $VERSION = '1.09';
}

require AutoLoader;
*AUTOLOAD = \&AutoLoader::AUTOLOAD;

use Config;

# enable debug/trace messages from DynaLoader perl code
$dl_debug = $ENV{PERL_DL_DEBUG} || 0 unless defined $dl_debug;

#
# Flags to alter dl_load_file behaviour.  Assigned bits:
#   0x01  make symbols available for linking later dl_load_file's.
#         (only known to work on Solaris 2 using dlopen(RTLD_GLOBAL))
#         (ignored under VMS; effect is built-in to image linking)
#
# This is called as a class method $module->dl_load_flags.  The
# definition here will be inherited and result on "default" loading
# behaviour unless a sub-class of DynaLoader defines its own version.
#

sub dl_load_flags { 0x00 }

($dl_dlext, $dl_so, $dlsrc) = @Config::Config{qw(dlext so dlsrc)};


$do_expand = 0;



@dl_require_symbols = ();       # names of symbols we need
@dl_resolve_using   = ();       # names of files to link with
@dl_library_path    = ();       # path to look for files

#XSLoader.pm may have added elements before we were required
#@dl_shared_objects  = ();       # shared objects for symbols we have 
#@dl_librefs         = ();       # things we have loaded
#@dl_modules         = ();       # Modules we have loaded

# This is a fix to support DLD's unfortunate desire to relink -lc
@dl_resolve_using = dl_findfile('-lc') if $dlsrc eq "dl_dld.xs";

# Initialise @dl_library_path with the 'standard' library path
# for this platform as determined by Configure.

push(@dl_library_path, split(' ', $Config::Config{libpth}));


my $ldlibpthname         = $Config::Config{ldlibpthname};
my $ldlibpthname_defined = defined $Config::Config{ldlibpthname};
my $pthsep               = $Config::Config{path_sep};

# Add to @dl_library_path any extra directories we can gather from environment
# during runtime.

if ($ldlibpthname_defined &&
    exists $ENV{$ldlibpthname}) {
    push(@dl_library_path, split(/$pthsep/, $ENV{$ldlibpthname}));
}

# E.g. HP-UX supports both its native SHLIB_PATH *and* LD_LIBRARY_PATH.

if ($ldlibpthname_defined &&
    $ldlibpthname ne 'LD_LIBRARY_PATH' &&
    exists $ENV{LD_LIBRARY_PATH}) {
    push(@dl_library_path, split(/$pthsep/, $ENV{LD_LIBRARY_PATH}));
}


# No prizes for guessing why we don't say 'bootstrap DynaLoader;' here.
# NOTE: All dl_*.xs (including dl_none.xs) define a dl_error() XSUB
boot_DynaLoader('DynaLoader') if defined(&boot_DynaLoader) &&
                                !defined(&dl_error);

if ($dl_debug) {
    print STDERR "DynaLoader.pm loaded (@INC, @dl_library_path)\n";
    print STDERR "DynaLoader not linked into this perl\n"
	    unless defined(&boot_DynaLoader);
}

1; # End of main code


sub croak   { require Carp; Carp::croak(@_)   }

sub bootstrap_inherit {
    my $module = $_[0];
    local *isa = *{"$module\::ISA"};
    local @isa = (@isa, 'DynaLoader');
    # Cannot goto due to delocalization.  Will report errors on a wrong line?
    bootstrap(@_);
}

# The bootstrap function cannot be autoloaded (without complications)
# so we define it here:

sub bootstrap {
    # use local vars to enable $module.bs script to edit values
    local(@args) = @_;
    local($module) = $args[0];
    local(@dirs, $file);

    unless ($module) {
	require Carp;
	Carp::confess("Usage: DynaLoader::bootstrap(module)");
    }

    # A common error on platforms which don't support dynamic loading.
    # Since it's fatal and potentially confusing we give a detailed message.
    croak("Can't load module $module, dynamic loading not available in this perl.\n".
	"  (You may need to build a new perl executable which either supports\n".
	"  dynamic loading or has the $module module statically linked into it.)\n")
	unless defined(&dl_load_file);


    
    my @modparts = split(/::/,$module);
    my $modfname = $modparts[-1];

    # Some systems have restrictions on files names for DLL's etc.
    # mod2fname returns appropriate file base name (typically truncated)
    # It may also edit @modparts if required.
    $modfname = &mod2fname(\@modparts) if defined &mod2fname;

    

    my $modpname = join('/',@modparts);

    print STDERR "DynaLoader::bootstrap for $module ",
		       "(auto/$modpname/$modfname.$dl_dlext)\n"
	if $dl_debug;

    foreach (@INC) {
	
	
	    my $dir = "$_/auto/$modpname";
	
	
	next unless -d $dir; # skip over uninteresting directories
	
	# check for common cases to avoid autoload of dl_findfile
	my $try =  "$dir/$modfname.$dl_dlext";
	last if $file = (-f $try) && $try;
	
	
	# no luck here, save dir for possible later dl_findfile search
	push @dirs, $dir;
    }
    # last resort, let dl_findfile have a go in all known locations
    $file = dl_findfile(map("-L$_",@dirs,@INC), $modfname) unless $file;

    croak("Can't locate loadable object for module $module in \@INC (\@INC contains: @INC)")
	unless $file;	# wording similar to error from 'require'

    
    my $bootname = "boot_$module";
    $bootname =~ s/\W/_/g;
    @dl_require_symbols = ($bootname);

    # Execute optional '.bootstrap' perl script for this module.
    # The .bs file can be used to configure @dl_resolve_using etc to
    # match the needs of the individual module on this architecture.
    my $bs = $file;
    $bs =~ s/(\.\w+)?(;\d*)?$/\.bs/; # look for .bs 'beside' the library
    if (-s $bs) { # only read file if it's not empty
        print STDERR "BS: $bs ($^O, $dlsrc)\n" if $dl_debug;
        eval { do $bs; };
        warn "$bs: $@\n" if $@;
    }

    my $boot_symbol_ref;

    

    # Many dynamic extension loading problems will appear to come from
    # this section of code: XYZ failed at line 123 of DynaLoader.pm.
    # Often these errors are actually occurring in the initialisation
    # C code of the extension XS file. Perl reports the error as being
    # in this perl code simply because this was the last perl code
    # it executed.

    my $libref = dl_load_file($file, $module->dl_load_flags) or
	croak("Can't load '$file' for module $module: ".dl_error());

    push(@dl_librefs,$libref);  # record loaded object

    my @unresolved = dl_undef_symbols();
    if (@unresolved) {
	require Carp;
	Carp::carp("Undefined symbols present after loading $file: @unresolved\n");
    }

    $boot_symbol_ref = dl_find_symbol($libref, $bootname) or
         croak("Can't find '$bootname' symbol in $file\n");

    push(@dl_modules, $module); # record loaded module

  boot:
    my $xs = dl_install_xsub("${module}::bootstrap", $boot_symbol_ref, $file);

    # See comment block above

	push(@dl_shared_objects, $file); # record files loaded

    &$xs(@args);
}


#sub _check_file {   # private utility to handle dl_expandspec vs -f tests
#    my($file) = @_;
#    return $file if (!$do_expand && -f $file); # the common case
#    return $file if ( $do_expand && ($file=dl_expandspec($file)));
#    return undef;
#}


# Let autosplit and the autoloader deal with these functions:
__END__


sub dl_findfile {
    # Read ext/DynaLoader/DynaLoader.doc for detailed information.
    # This function does not automatically consider the architecture
    # or the perl library auto directories.
    my (@args) = @_;
    my (@dirs,  $dir);   # which directories to search
    my (@found);         # full paths to real files we have found
    #my $dl_ext= 'dll'; # $Config::Config{'dlext'} suffix for perl extensions
    #my $dl_so = 'dll'; # $Config::Config{'so'} suffix for shared libraries

    print STDERR "dl_findfile(@args)\n" if $dl_debug;

    # accumulate directories but process files as they appear
    arg: foreach(@args) {
        #  Special fast case: full filepath requires no search
	
	
	
        if (m:/: && -f $_) {
	    push(@found,$_);
	    last arg unless wantarray;
	    next;
	}
	

        # Deal with directories first:
        #  Using a -L prefix is the preferred option (faster and more robust)
        if (m:^-L:) { s/^-L//; push(@dirs, $_); next; }

	
	
        #  Otherwise we try to try to spot directories by a heuristic
        #  (this is a more complicated issue than it first appears)
        if (m:/: && -d $_) {   push(@dirs, $_); next; }

	

        #  Only files should get this far...
        my(@names, $name);    # what filenames to look for
        if (m:-l: ) {          # convert -lname to appropriate library name
            s/-l//;
            push(@names,"lib$_.$dl_so");
            push(@names,"lib$_.a");
        } else {                # Umm, a bare name. Try various alternatives:
            # these should be ordered with the most likely first
            push(@names,"$_.$dl_dlext")    unless m/\.$dl_dlext$/o;
            push(@names,"$_.$dl_so")     unless m/\.$dl_so$/o;
            push(@names,"cyg$_.")  if !m:/: and $^O eq 'cygwin';
            push(@names,"lib$_.$dl_so")  unless m:/:;
            push(@names,"$_.a")          if !m/\.a$/ and $dlsrc eq "dl_dld.xs";
            push(@names, $_);
        }
	my $dirsep = '/';
	
        foreach $dir (@dirs, @dl_library_path) {
            next unless -d $dir;
	    
            foreach $name (@names) {
		my($file) = "$dir$dirsep$name";
                print STDERR " checking in $dir for $name\n" if $dl_debug;
		$file = ($do_expand) ? dl_expandspec($file) : (-f $file && $file);
		#$file = _check_file($file);
		if ($file) {
                    push(@found, $file);
                    next arg; # no need to look any further
                }
            }
        }
    }
    if ($dl_debug) {
        foreach(@dirs) {
            print STDERR " dl_findfile ignored non-existent directory: $_\n" unless -d $_;
        }
        print STDERR "dl_findfile found: @found\n";
    }
    return $found[0] unless wantarray;
    @found;
}


sub dl_expandspec {
    my($spec) = @_;
    # Optional function invoked if DynaLoader.pm sets $do_expand.
    # Most systems do not require or use this function.
    # Some systems may implement it in the dl_*.xs file in which case
    # this autoload version will not be called but is harmless.

    # This function is designed to deal with systems which treat some
    # 'filenames' in a special way. For example VMS 'Logical Names'
    # (something like unix environment variables - but different).
    # This function should recognise such names and expand them into
    # full file paths.
    # Must return undef if $spec is invalid or file does not exist.

    my $file = $spec; # default output to input

    
	return undef unless -f $file;
    
    print STDERR "dl_expandspec($spec) => $file\n" if $dl_debug;
    $file;
}

sub dl_find_symbol_anywhere
{
    my $sym = shift;
    my $libref;
    foreach $libref (@dl_librefs) {
	my $symref = dl_find_symbol($libref,$sym);
	return $symref if $symref;
    }
    return undef;
}

=head1 NAME

DynaLoader - Dynamically load C libraries into Perl code

=head1 SYNOPSIS

    package YourPackage;
    require DynaLoader;
    @ISA = qw(... DynaLoader ...);
    bootstrap YourPackage;

    # optional method for 'global' loading
    sub dl_load_flags { 0x01 }     


=head1 DESCRIPTION

This document defines a standard generic interface to the dynamic
linking mechanisms available on many platforms.  Its primary purpose is
to implement automatic dynamic loading of Perl modules.

This document serves as both a specification for anyone wishing to
implement the DynaLoader for a new platform and as a guide for
anyone wishing to use the DynaLoader directly in an application.

The DynaLoader is designed to be a very simple high-level
interface that is sufficiently general to cover the requirements
of SunOS, HP-UX, NeXT, Linux, VMS and other platforms.

It is also hoped that the interface will cover the needs of OS/2, NT
etc and also allow pseudo-dynamic linking (using C<ld -A> at runtime).

It must be stressed that the DynaLoader, by itself, is practically
useless for accessing non-Perl libraries because it provides almost no
Perl-to-C 'glue'.  There is, for example, no mechanism for calling a C
library function or supplying arguments.  A C::DynaLib module
is available from CPAN sites which performs that function for some
common system types.  And since the year 2000, there's also Inline::C,
a module that allows you to write Perl subroutines in C.  Also available
from your local CPAN site.

DynaLoader Interface Summary

  @dl_library_path
  @dl_resolve_using
  @dl_require_symbols
  $dl_debug
  @dl_librefs
  @dl_modules
  @dl_shared_objects
                                                  Implemented in:
  bootstrap($modulename)                               Perl
  @filepaths = dl_findfile(@names)                     Perl
  $flags = $modulename->dl_load_flags                  Perl
  $symref  = dl_find_symbol_anywhere($symbol)          Perl

  $libref  = dl_load_file($filename, $flags)           C
  $status  = dl_unload_file($libref)                   C
  $symref  = dl_find_symbol($libref, $symbol)          C
  @symbols = dl_undef_symbols()                        C
  dl_install_xsub($name, $symref [, $filename])        C
  $message = dl_error                                  C

=over 4

=item @dl_library_path

The standard/default list of directories in which dl_findfile() will
search for libraries etc.  Directories are searched in order:
$dl_library_path[0], [1], ... etc

@dl_library_path is initialised to hold the list of 'normal' directories
(F</usr/lib>, etc) determined by B<Configure> (C<$Config{'libpth'}>).  This should
ensure portability across a wide range of platforms.

@dl_library_path should also be initialised with any other directories
that can be determined from the environment at runtime (such as
LD_LIBRARY_PATH for SunOS).

After initialisation @dl_library_path can be manipulated by an
application using push and unshift before calling dl_findfile().
Unshift can be used to add directories to the front of the search order
either to save search time or to override libraries with the same name
in the 'normal' directories.

The load function that dl_load_file() calls may require an absolute
pathname.  The dl_findfile() function and @dl_library_path can be
used to search for and return the absolute pathname for the
library/object that you wish to load.

=item @dl_resolve_using

A list of additional libraries or other shared objects which can be
used to resolve any undefined symbols that might be generated by a
later call to load_file().

This is only required on some platforms which do not handle dependent
libraries automatically.  For example the Socket Perl extension
library (F<auto/Socket/Socket.so>) contains references to many socket
functions which need to be resolved when it's loaded.  Most platforms
will automatically know where to find the 'dependent' library (e.g.,
F</usr/lib/libsocket.so>).  A few platforms need to be told the
location of the dependent library explicitly.  Use @dl_resolve_using
for this.

Example usage:

    @dl_resolve_using = dl_findfile('-lsocket');

=item @dl_require_symbols

A list of one or more symbol names that are in the library/object file
to be dynamically loaded.  This is only required on some platforms.

=item @dl_librefs

An array of the handles returned by successful calls to dl_load_file(),
made by bootstrap, in the order in which they were loaded.
Can be used with dl_find_symbol() to look for a symbol in any of
the loaded files.

=item @dl_modules

An array of module (package) names that have been bootstrap'ed.

=item @dl_shared_objects

An array of file names for the shared objects that were loaded.

=item dl_error()

Syntax:

    $message = dl_error();

Error message text from the last failed DynaLoader function.  Note
that, similar to errno in unix, a successful function call does not
reset this message.

Implementations should detect the error as soon as it occurs in any of
the other functions and save the corresponding message for later
retrieval.  This will avoid problems on some platforms (such as SunOS)
where the error message is very temporary (e.g., dlerror()).

=item $dl_debug

Internal debugging messages are enabled when $dl_debug is set true.
Currently setting $dl_debug only affects the Perl side of the
DynaLoader.  These messages should help an application developer to
resolve any DynaLoader usage problems.

$dl_debug is set to C<$ENV{'PERL_DL_DEBUG'}> if defined.

For the DynaLoader developer/porter there is a similar debugging
variable added to the C code (see dlutils.c) and enabled if Perl was
built with the B<-DDEBUGGING> flag.  This can also be set via the
PERL_DL_DEBUG environment variable.  Set to 1 for minimal information or
higher for more.

=item dl_findfile()

Syntax:

    @filepaths = dl_findfile(@names)

Determine the full paths (including file suffix) of one or more
loadable files given their generic names and optionally one or more
directories.  Searches directories in @dl_library_path by default and
returns an empty list if no files were found.

Names can be specified in a variety of platform independent forms.  Any
names in the form B<-lname> are converted into F<libname.*>, where F<.*> is
an appropriate suffix for the platform.

If a name does not already have a suitable prefix and/or suffix then
the corresponding file will be searched for by trying combinations of
prefix and suffix appropriate to the platform: "$name.o", "lib$name.*"
and "$name".

If any directories are included in @names they are searched before
@dl_library_path.  Directories may be specified as B<-Ldir>.  Any other
names are treated as filenames to be searched for.

Using arguments of the form C<-Ldir> and C<-lname> is recommended.

Example: 

    @dl_resolve_using = dl_findfile(qw(-L/usr/5lib -lposix));


=item dl_expandspec()

Syntax:

    $filepath = dl_expandspec($spec)

Some unusual systems, such as VMS, require special filename handling in
order to deal with symbolic names for files (i.e., VMS's Logical Names).

To support these systems a dl_expandspec() function can be implemented
either in the F<dl_*.xs> file or code can be added to the autoloadable
dl_expandspec() function in F<DynaLoader.pm>.  See F<DynaLoader.pm> for
more information.

=item dl_load_file()

Syntax:

    $libref = dl_load_file($filename, $flags)

Dynamically load $filename, which must be the path to a shared object
or library.  An opaque 'library reference' is returned as a handle for
the loaded object.  Returns undef on error.

The $flags argument to alters dl_load_file behaviour.  
Assigned bits:

 0x01  make symbols available for linking later dl_load_file's.
       (only known to work on Solaris 2 using dlopen(RTLD_GLOBAL))
       (ignored under VMS; this is a normal part of image linking)

(On systems that provide a handle for the loaded object such as SunOS
and HPUX, $libref will be that handle.  On other systems $libref will
typically be $filename or a pointer to a buffer containing $filename.
The application should not examine or alter $libref in any way.)

This is the function that does the real work.  It should use the
current values of @dl_require_symbols and @dl_resolve_using if required.

    SunOS: dlopen($filename)
    HP-UX: shl_load($filename)
    Linux: dld_create_reference(@dl_require_symbols); dld_link($filename)
    NeXT:  rld_load($filename, @dl_resolve_using)
    VMS:   lib$find_image_symbol($filename,$dl_require_symbols[0])

(The dlopen() function is also used by Solaris and some versions of
Linux, and is a common choice when providing a "wrapper" on other
mechanisms as is done in the OS/2 port.)

=item dl_unload_file()

Syntax:

    $status = dl_unload_file($libref)

Dynamically unload $libref, which must be an opaque 'library reference' as
returned from dl_load_file.  Returns one on success and zero on failure.

This function is optional and may not necessarily be provided on all platforms.
If it is defined, it is called automatically when the interpreter exits for
every shared object or library loaded by DynaLoader::bootstrap.  All such
library references are stored in @dl_librefs by DynaLoader::Bootstrap as it
loads the libraries.  The files are unloaded in last-in, first-out order.

This unloading is usually necessary when embedding a shared-object perl (e.g.
one configured with -Duseshrplib) within a larger application, and the perl
interpreter is created and destroyed several times within the lifetime of the
application.  In this case it is possible that the system dynamic linker will
unload and then subsequently reload the shared libperl without relocating any
references to it from any files DynaLoaded by the previous incarnation of the
interpreter.  As a result, any shared objects opened by DynaLoader may point to
a now invalid 'ghost' of the libperl shared object, causing apparently random
memory corruption and crashes.  This behaviour is most commonly seen when using
Apache and mod_perl built with the APXS mechanism.

    SunOS: dlclose($libref)
    HP-UX: ???
    Linux: ???
    NeXT:  ???
    VMS:   ???

(The dlclose() function is also used by Solaris and some versions of
Linux, and is a common choice when providing a "wrapper" on other
mechanisms as is done in the OS/2 port.)

=item dl_load_flags()

Syntax:

    $flags = dl_load_flags $modulename;

Designed to be a method call, and to be overridden by a derived class
(i.e. a class which has DynaLoader in its @ISA).  The definition in
DynaLoader itself returns 0, which produces standard behavior from
dl_load_file().

=item dl_find_symbol()

Syntax:

    $symref = dl_find_symbol($libref, $symbol)

Return the address of the symbol $symbol or C<undef> if not found.  If the
target system has separate functions to search for symbols of different
types then dl_find_symbol() should search for function symbols first and
then other types.

The exact manner in which the address is returned in $symref is not
currently defined.  The only initial requirement is that $symref can
be passed to, and understood by, dl_install_xsub().

    SunOS: dlsym($libref, $symbol)
    HP-UX: shl_findsym($libref, $symbol)
    Linux: dld_get_func($symbol) and/or dld_get_symbol($symbol)
    NeXT:  rld_lookup("_$symbol")
    VMS:   lib$find_image_symbol($libref,$symbol)


=item dl_find_symbol_anywhere()

Syntax:

    $symref = dl_find_symbol_anywhere($symbol)

Applies dl_find_symbol() to the members of @dl_librefs and returns
the first match found.

=item dl_undef_symbols()

Example

    @symbols = dl_undef_symbols()

Return a list of symbol names which remain undefined after load_file().
Returns C<()> if not known.  Don't worry if your platform does not provide
a mechanism for this.  Most do not need it and hence do not provide it,
they just return an empty list.


=item dl_install_xsub()

Syntax:

    dl_install_xsub($perl_name, $symref [, $filename])

Create a new Perl external subroutine named $perl_name using $symref as
a pointer to the function which implements the routine.  This is simply
a direct call to newXSUB().  Returns a reference to the installed
function.

The $filename parameter is used by Perl to identify the source file for
the function if required by die(), caller() or the debugger.  If
$filename is not defined then "DynaLoader" will be used.


=item bootstrap()

Syntax:

bootstrap($module)

This is the normal entry point for automatic dynamic loading in Perl.

It performs the following actions:

=over 8

=item *

locates an auto/$module directory by searching @INC

=item *

uses dl_findfile() to determine the filename to load

=item *

sets @dl_require_symbols to C<("boot_$module")>

=item *

executes an F<auto/$module/$module.bs> file if it exists
(typically used to add to @dl_resolve_using any files which
are required to load the module on the current platform)

=item *

calls dl_load_flags() to determine how to load the file.

=item *

calls dl_load_file() to load the file

=item *

calls dl_undef_symbols() and warns if any symbols are undefined

=item *

calls dl_find_symbol() for "boot_$module"

=item *

calls dl_install_xsub() to install it as "${module}::bootstrap"

=item *

calls &{"${module}::bootstrap"} to bootstrap the module (actually
it uses the function reference returned by dl_install_xsub for speed)

=back

=back


=head1 AUTHOR

Tim Bunce, 11 August 1994.

This interface is based on the work and comments of (in no particular
order): Larry Wall, Robert Sanders, Dean Roehrich, Jeff Okamoto, Anno
Siegel, Thomas Neumann, Paul Marquess, Charles Bailey, myself and others.

Larry Wall designed the elegant inherited bootstrap mechanism and
implemented the first Perl 5 dynamic loader using it.

Solaris global loading added by Nick Ing-Simmons with design/coding
assistance from Tim Bunce, January 1996.

=cut
