#

package IO::Seekable;

=head1 NAME

IO::Seekable - supply seek based methods for I/O objects

=head1 SYNOPSIS

    use IO::Seekable;
    package IO::Something;
    @ISA = qw(IO::Seekable);

=head1 DESCRIPTION

C<IO::Seekable> does not have a constructor of its own as it is intended to
be inherited by other C<IO::Handle> based objects. It provides methods
which allow seeking of the file descriptors.

=over 4

=item $io->getpos

Returns an opaque value that represents the current position of the
IO::File, or C<undef> if this is not possible (eg an unseekable stream such
as a terminal, pipe or socket). If the fgetpos() function is available in
your C library it is used to implements getpos, else perl emulates getpos
using C's ftell() function.

=item $io->setpos

Uses the value of a previous getpos call to return to a previously visited
position. Returns "0 but true" on success, C<undef> on failure.

=back

See L<perlfunc> for complete descriptions of each of the following
supported C<IO::Seekable> methods, which are just front ends for the
corresponding built-in functions:

=over 4

=item $io->seek ( POS, WHENCE )

Seek the IO::File to position POS, relative to WHENCE:

=over 8

=item WHENCE=0 (SEEK_SET)

POS is absolute position. (Seek relative to the start of the file)

=item WHENCE=1 (SEEK_CUR)

POS is an offset from the current position. (Seek relative to current)

=item WHENCE=2 (SEEK_END)

POS is an offset from the end of the file. (Seek relative to end)

=back

The SEEK_* constants can be imported from the C<Fcntl> module if you
don't wish to use the numbers C<0> C<1> or C<2> in your code.

Returns C<1> upon success, C<0> otherwise.

=item $io->sysseek( POS, WHENCE )

Similar to $io->seek, but sets the IO::File's position using the system
call lseek(2) directly, so will confuse most perl IO operators except
sysread and syswrite (see L<perlfunc> for full details)

Returns the new position, or C<undef> on failure.  A position
of zero is returned as the string C<"0 but true">

=item $io->tell

Returns the IO::File's current position, or -1 on error.

=back

=head1 SEE ALSO

L<perlfunc>, 
L<perlop/"I/O Operators">,
L<IO::Handle>
L<IO::File>

=head1 HISTORY

Derived from FileHandle.pm by Graham Barr E<lt>gbarr@pobox.comE<gt>

=cut

use 5.006_001;
use Carp;
use strict;
our($VERSION, @EXPORT, @ISA);
use IO::Handle ();
# XXX we can't get these from IO::Handle or we'll get prototype
# mismatch warnings on C<use POSIX; use IO::File;> :-(
use Fcntl qw(SEEK_SET SEEK_CUR SEEK_END);
require Exporter;

@EXPORT = qw(SEEK_SET SEEK_CUR SEEK_END);
@ISA = qw(Exporter);

$VERSION = "1.10";
$VERSION = eval $VERSION;

sub seek {
    @_ == 3 or croak 'usage: $io->seek(POS, WHENCE)';
    seek($_[0], $_[1], $_[2]);
}

sub sysseek {
    @_ == 3 or croak 'usage: $io->sysseek(POS, WHENCE)';
    sysseek($_[0], $_[1], $_[2]);
}

sub tell {
    @_ == 1 or croak 'usage: $io->tell()';
    tell($_[0]);
}

1;
