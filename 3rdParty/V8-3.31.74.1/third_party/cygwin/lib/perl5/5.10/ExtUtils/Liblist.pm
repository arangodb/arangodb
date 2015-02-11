package ExtUtils::Liblist;

use strict;

our $VERSION = '6.44';

use File::Spec;
require ExtUtils::Liblist::Kid;
our @ISA = qw(ExtUtils::Liblist::Kid File::Spec);

# Backwards compatibility with old interface.
sub ext {
    goto &ExtUtils::Liblist::Kid::ext;
}

sub lsdir {
  shift;
  my $rex = qr/$_[1]/;
  opendir DIR, $_[0];
  my @out = grep /$rex/, readdir DIR;
  closedir DIR;
  return @out;
}

__END__

=head1 NAME

ExtUtils::Liblist - determine libraries to use and how to use them

=head1 SYNOPSIS

  require ExtUtils::Liblist;

  $MM->ext($potential_libs, $verbose, $need_names);

  # Usually you can get away with:
  ExtUtils::Liblist->ext($potential_libs, $verbose, $need_names)

=head1 DESCRIPTION

This utility takes a list of libraries in the form C<-llib1 -llib2
-llib3> and returns lines suitable for inclusion in an extension
Makefile.  Extra library paths may be included with the form
C<-L/another/path> this will affect the searches for all subsequent
libraries.

It returns an array of four or five scalar values: EXTRALIBS,
BSLOADLIBS, LDLOADLIBS, LD_RUN_PATH, and, optionally, a reference to
the array of the filenames of actual libraries.  Some of these don't
mean anything unless on Unix.  See the details about those platform
specifics below.  The list of the filenames is returned only if
$need_names argument is true.

Dependent libraries can be linked in one of three ways:

=over 2

=item * For static extensions

by the ld command when the perl binary is linked with the extension
library. See EXTRALIBS below.

=item * For dynamic extensions at build/link time

by the ld command when the shared object is built/linked. See
LDLOADLIBS below.

=item * For dynamic extensions at load time

by the DynaLoader when the shared object is loaded. See BSLOADLIBS
below.

=back

=head2 EXTRALIBS

List of libraries that need to be linked with when linking a perl
binary which includes this extension. Only those libraries that
actually exist are included.  These are written to a file and used
when linking perl.

=head2 LDLOADLIBS and LD_RUN_PATH

List of those libraries which can or must be linked into the shared
library when created using ld. These may be static or dynamic
libraries.  LD_RUN_PATH is a colon separated list of the directories
in LDLOADLIBS. It is passed as an environment variable to the process
that links the shared library.

=head2 BSLOADLIBS

List of those libraries that are needed but can be linked in
dynamically at run time on this platform.  SunOS/Solaris does not need
this because ld records the information (from LDLOADLIBS) into the
object file.  This list is used to create a .bs (bootstrap) file.

=head1 PORTABILITY

This module deals with a lot of system dependencies and has quite a
few architecture specific C<if>s in the code.

=head2 VMS implementation

The version of ext() which is executed under VMS differs from the
Unix-OS/2 version in several respects:

=over 2

=item *

Input library and path specifications are accepted with or without the
C<-l> and C<-L> prefixes used by Unix linkers.  If neither prefix is
present, a token is considered a directory to search if it is in fact
a directory, and a library to search for otherwise.  Authors who wish
their extensions to be portable to Unix or OS/2 should use the Unix
prefixes, since the Unix-OS/2 version of ext() requires them.

=item *

Wherever possible, shareable images are preferred to object libraries,
and object libraries to plain object files.  In accordance with VMS
naming conventions, ext() looks for files named I<lib>shr and I<lib>rtl;
it also looks for I<lib>lib and libI<lib> to accommodate Unix conventions
used in some ported software.

=item *

For each library that is found, an appropriate directive for a linker options
file is generated.  The return values are space-separated strings of
these directives, rather than elements used on the linker command line.

=item *

LDLOADLIBS contains both the libraries found based on C<$potential_libs> and
the CRTLs, if any, specified in Config.pm.  EXTRALIBS contains just those
libraries found based on C<$potential_libs>.  BSLOADLIBS and LD_RUN_PATH
are always empty.

=back

In addition, an attempt is made to recognize several common Unix library
names, and filter them out or convert them to their VMS equivalents, as
appropriate.

In general, the VMS version of ext() should properly handle input from
extensions originally designed for a Unix or VMS environment.  If you
encounter problems, or discover cases where the search could be improved,
please let us know.

=head2 Win32 implementation

The version of ext() which is executed under Win32 differs from the
Unix-OS/2 version in several respects:

=over 2

=item *

If C<$potential_libs> is empty, the return value will be empty.
Otherwise, the libraries specified by C<$Config{perllibs}> (see Config.pm)
will be appended to the list of C<$potential_libs>.  The libraries
will be searched for in the directories specified in C<$potential_libs>,
C<$Config{libpth}>, and in C<$Config{installarchlib}/CORE>.
For each library that is found,  a space-separated list of fully qualified
library pathnames is generated.

=item *

Input library and path specifications are accepted with or without the
C<-l> and C<-L> prefixes used by Unix linkers.

An entry of the form C<-La:\foo> specifies the C<a:\foo> directory to look
for the libraries that follow.

An entry of the form C<-lfoo> specifies the library C<foo>, which may be
spelled differently depending on what kind of compiler you are using.  If
you are using GCC, it gets translated to C<libfoo.a>, but for other win32
compilers, it becomes C<foo.lib>.  If no files are found by those translated
names, one more attempt is made to find them using either C<foo.a> or
C<libfoo.lib>, depending on whether GCC or some other win32 compiler is
being used, respectively.

If neither the C<-L> or C<-l> prefix is present in an entry, the entry is
considered a directory to search if it is in fact a directory, and a
library to search for otherwise.  The C<$Config{lib_ext}> suffix will
be appended to any entries that are not directories and don't already have
the suffix.

Note that the C<-L> and C<-l> prefixes are B<not required>, but authors
who wish their extensions to be portable to Unix or OS/2 should use the
prefixes, since the Unix-OS/2 version of ext() requires them.

=item *

Entries cannot be plain object files, as many Win32 compilers will
not handle object files in the place of libraries.

=item *

Entries in C<$potential_libs> beginning with a colon and followed by
alphanumeric characters are treated as flags.  Unknown flags will be ignored.

An entry that matches C</:nodefault/i> disables the appending of default
libraries found in C<$Config{perllibs}> (this should be only needed very rarely).

An entry that matches C</:nosearch/i> disables all searching for
the libraries specified after it.  Translation of C<-Lfoo> and
C<-lfoo> still happens as appropriate (depending on compiler being used,
as reflected by C<$Config{cc}>), but the entries are not verified to be
valid files or directories.

An entry that matches C</:search/i> reenables searching for
the libraries specified after it.  You can put it at the end to
enable searching for default libraries specified by C<$Config{perllibs}>.

=item *

The libraries specified may be a mixture of static libraries and
import libraries (to link with DLLs).  Since both kinds are used
pretty transparently on the Win32 platform, we do not attempt to
distinguish between them.

=item *

LDLOADLIBS and EXTRALIBS are always identical under Win32, and BSLOADLIBS
and LD_RUN_PATH are always empty (this may change in future).

=item *

You must make sure that any paths and path components are properly
surrounded with double-quotes if they contain spaces. For example,
C<$potential_libs> could be (literally):

	"-Lc:\Program Files\vc\lib" msvcrt.lib "la test\foo bar.lib"

Note how the first and last entries are protected by quotes in order
to protect the spaces.

=item *

Since this module is most often used only indirectly from extension
C<Makefile.PL> files, here is an example C<Makefile.PL> entry to add
a library to the build process for an extension:

        LIBS => ['-lgl']

When using GCC, that entry specifies that MakeMaker should first look
for C<libgl.a> (followed by C<gl.a>) in all the locations specified by
C<$Config{libpth}>.

When using a compiler other than GCC, the above entry will search for
C<gl.lib> (followed by C<libgl.lib>).

If the library happens to be in a location not in C<$Config{libpth}>,
you need:

        LIBS => ['-Lc:\gllibs -lgl']

Here is a less often used example:

        LIBS => ['-lgl', ':nosearch -Ld:\mesalibs -lmesa -luser32']

This specifies a search for library C<gl> as before.  If that search
fails to find the library, it looks at the next item in the list. The
C<:nosearch> flag will prevent searching for the libraries that follow,
so it simply returns the value as C<-Ld:\mesalibs -lmesa -luser32>,
since GCC can use that value as is with its linker.

When using the Visual C compiler, the second item is returned as
C<-libpath:d:\mesalibs mesa.lib user32.lib>.

When using the Borland compiler, the second item is returned as
C<-Ld:\mesalibs mesa.lib user32.lib>, and MakeMaker takes care of
moving the C<-Ld:\mesalibs> to the correct place in the linker
command line.

=back


=head1 SEE ALSO

L<ExtUtils::MakeMaker>

=cut

