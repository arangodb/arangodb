package ExtUtils::CBuilder;

use File::Spec ();
use File::Path ();
use File::Basename ();

use vars qw($VERSION @ISA);
$VERSION = '0.21';
$VERSION = eval $VERSION;

# Okay, this is the brute-force method of finding out what kind of
# platform we're on.  I don't know of a systematic way.  These values
# came from the latest (bleadperl) perlport.pod.

my %OSTYPES = qw(
		 aix       Unix
		 bsdos     Unix
		 dgux      Unix
		 dynixptx  Unix
		 freebsd   Unix
		 linux     Unix
		 hpux      Unix
		 irix      Unix
		 darwin    Unix
		 machten   Unix
		 next      Unix
		 openbsd   Unix
		 netbsd    Unix
		 dec_osf   Unix
		 svr4      Unix
		 svr5      Unix
		 sco_sv    Unix
		 unicos    Unix
		 unicosmk  Unix
		 solaris   Unix
		 sunos     Unix
		 cygwin    Unix
		 os2       Unix
		 
		 dos       Windows
		 MSWin32   Windows

		 os390     EBCDIC
		 os400     EBCDIC
		 posix-bc  EBCDIC
		 vmesa     EBCDIC

		 MacOS     MacOS
		 VMS       VMS
		 VOS       VOS
		 riscos    RiscOS
		 amigaos   Amiga
		 mpeix     MPEiX
		);

# We only use this once - don't waste a symbol table entry on it.
# More importantly, don't make it an inheritable method.
my $load = sub {
  my $mod = shift;
  eval "use $mod";
  die $@ if $@;
  @ISA = ($mod);
};

{
  my @package = split /::/, __PACKAGE__;
  
  if (grep {-e File::Spec->catfile($_, @package, 'Platform', $^O) . '.pm'} @INC) {
    $load->(__PACKAGE__ . "::Platform::$^O");
    
  } elsif (exists $OSTYPES{$^O} and
	   grep {-e File::Spec->catfile($_, @package, 'Platform', $OSTYPES{$^O}) . '.pm'} @INC) {
    $load->(__PACKAGE__ . "::Platform::$OSTYPES{$^O}");
    
  } else {
    $load->(__PACKAGE__ . "::Base");
  }
}

sub os_type { $OSTYPES{$^O} }

1;
__END__

=head1 NAME

ExtUtils::CBuilder - Compile and link C code for Perl modules

=head1 SYNOPSIS

  use ExtUtils::CBuilder;

  my $b = ExtUtils::CBuilder->new(%options);
  $obj_file = $b->compile(source => 'MyModule.c');
  $lib_file = $b->link(objects => $obj_file);

=head1 DESCRIPTION

This module can build the C portions of Perl modules by invoking the
appropriate compilers and linkers in a cross-platform manner.  It was
motivated by the C<Module::Build> project, but may be useful for other
purposes as well.  However, it is I<not> intended as a general
cross-platform interface to all your C building needs.  That would
have been a much more ambitious goal!

=head1 METHODS

=over 4

=item new

Returns a new C<ExtUtils::CBuilder> object.  A C<config> parameter
lets you override C<Config.pm> settings for all operations performed
by the object, as in the following example:

  # Use a different compiler than Config.pm says
  my $b = ExtUtils::CBuilder->new( config =>
                                   { ld => 'gcc' } );

A C<quiet> parameter tells C<CBuilder> to not print its C<system()>
commands before executing them:

  # Be quieter than normal
  my $b = ExtUtils::CBuilder->new( quiet => 1 );

=item have_compiler

Returns true if the current system has a working C compiler and
linker, false otherwise.  To determine this, we actually compile and
link a sample C library.

=item compile

Compiles a C source file and produces an object file.  The name of the
object file is returned.  The source file is specified in a C<source>
parameter, which is required; the other parameters listed below are
optional.

=over 4

=item C<object_file>

Specifies the name of the output file to create.  Otherwise the
C<object_file()> method will be consulted, passing it the name of the
C<source> file.

=item C<include_dirs>

Specifies any additional directories in which to search for header
files.  May be given as a string indicating a single directory, or as
a list reference indicating multiple directories.

=item C<extra_compiler_flags>

Specifies any additional arguments to pass to the compiler.  Should be
given as a list reference containing the arguments individually, or if
this is not possible, as a string containing all the arguments
together.

=back

The operation of this method is also affected by the
C<archlibexp>, C<cccdlflags>, C<ccflags>, C<optimize>, and C<cc>
entries in C<Config.pm>.

=item link

Invokes the linker to produce a library file from object files.  In
scalar context, the name of the library file is returned.  In list
context, the library file and any temporary files created are
returned.  A required C<objects> parameter contains the name of the
object files to process, either in a string (for one object file) or
list reference (for one or more files).  The following parameters are
optional:


=over 4

=item lib_file

Specifies the name of the output library file to create.  Otherwise
the C<lib_file()> method will be consulted, passing it the name of
the first entry in C<objects>.

=item module_name

Specifies the name of the Perl module that will be created by linking.
On platforms that need to do prelinking (Win32, OS/2, etc.) this is a
required parameter.

=item extra_linker_flags

Any additional flags you wish to pass to the linker.

=back

On platforms where C<need_prelink()> returns true, C<prelink()>
will be called automatically.

The operation of this method is also affected by the C<lddlflags>,
C<shrpenv>, and C<ld> entries in C<Config.pm>.

=item link_executable

Invokes the linker to produce an executable file from object files.  In
scalar context, the name of the executable file is returned.  In list
context, the executable file and any temporary files created are
returned.  A required C<objects> parameter contains the name of the
object files to process, either in a string (for one object file) or
list reference (for one or more files).  The optional parameters are
the same as C<link> with exception for


=over 4

=item exe_file

Specifies the name of the output executable file to create.  Otherwise
the C<exe_file()> method will be consulted, passing it the name of the
first entry in C<objects>.

=back

=item object_file

 my $object_file = $b->object_file($source_file);

Converts the name of a C source file to the most natural name of an
output object file to create from it.  For instance, on Unix the
source file F<foo.c> would result in the object file F<foo.o>.

=item lib_file

 my $lib_file = $b->lib_file($object_file);

Converts the name of an object file to the most natural name of a
output library file to create from it.  For instance, on Mac OS X the
object file F<foo.o> would result in the library file F<foo.bundle>.

=item exe_file

 my $exe_file = $b->exe_file($object_file);

Converts the name of an object file to the most natural name of an
executable file to create from it.  For instance, on Mac OS X the
object file F<foo.o> would result in the executable file F<foo>, and
on Windows it would result in F<foo.exe>.


=item prelink

On certain platforms like Win32, OS/2, VMS, and AIX, it is necessary
to perform some actions before invoking the linker.  The
C<ExtUtils::Mksymlists> module does this, writing files used by the
linker during the creation of shared libraries for dynamic extensions.
The names of any files written will be returned as a list.

Several parameters correspond to C<ExtUtils::Mksymlists::Mksymlists()>
options, as follows:

    Mksymlists()   prelink()          type
   -------------|-------------------|-------------------
    NAME        |  dl_name          | string (required)
    DLBASE      |  dl_base          | string
    FILE        |  dl_file          | string
    DL_VARS     |  dl_vars          | array reference
    DL_FUNCS    |  dl_funcs         | hash reference
    FUNCLIST    |  dl_func_list     | array reference
    IMPORTS     |  dl_imports       | hash reference
    VERSION     |  dl_version       | string

Please see the documentation for C<ExtUtils::Mksymlists> for the
details of what these parameters do.

=item need_prelink

Returns true on platforms where C<prelink()> should be called
during linking, and false otherwise.

=item extra_link_args_after_prelink

Returns list of extra arguments to give to the link command; the arguments
are the same as for prelink(), with addition of array reference to the
results of prelink(); this reference is indexed by key C<prelink_res>.

=back

=head1 TO DO

Currently this has only been tested on Unix and doesn't contain any of
the Windows-specific code from the C<Module::Build> project.  I'll do
that next.

=head1 HISTORY

This module is an outgrowth of the C<Module::Build> project, to which
there have been many contributors.  Notably, Randy W. Sims submitted
lots of code to support 3 compilers on Windows and helped with various
other platform-specific issues.  Ilya Zakharevich has contributed
fixes for OS/2; John E. Malmberg and Peter Prymmer have done likewise
for VMS.

=head1 AUTHOR

Ken Williams, kwilliams@cpan.org

=head1 COPYRIGHT

Copyright (c) 2003-2005 Ken Williams.  All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=head1 SEE ALSO

perl(1), Module::Build(3)

=cut
