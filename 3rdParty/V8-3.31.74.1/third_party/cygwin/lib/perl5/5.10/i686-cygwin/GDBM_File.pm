# GDBM_File.pm -- Perl 5 interface to GNU gdbm library.

=head1 NAME

GDBM_File - Perl5 access to the gdbm library.

=head1 SYNOPSIS

    use GDBM_File ;
    tie %hash, 'GDBM_File', $filename, &GDBM_WRCREAT, 0640;
    # Use the %hash array.
    untie %hash ;

=head1 DESCRIPTION

B<GDBM_File> is a module which allows Perl programs to make use of the
facilities provided by the GNU gdbm library.  If you intend to use this
module you should really have a copy of the gdbm manualpage at hand.

Most of the libgdbm.a functions are available through the GDBM_File
interface.

=head1 AVAILABILITY

gdbm is available from any GNU archive.  The master site is
C<ftp.gnu.org>, but you are strongly urged to use one of the many
mirrors.  You can obtain a list of mirror sites from
http://www.gnu.org/order/ftp.html.

=head1 BUGS

The available functions and the gdbm/perl interface need to be documented.

The GDBM error number and error message interface needs to be added.

=head1 SEE ALSO

L<perl(1)>, L<DB_File(3)>, L<perldbmfilter>. 

=cut

package GDBM_File;

use strict;
use warnings;
our($VERSION, @ISA, @EXPORT, $AUTOLOAD);

require Carp;
require Tie::Hash;
require Exporter;
use XSLoader ();
@ISA = qw(Tie::Hash Exporter);
@EXPORT = qw(
	GDBM_CACHESIZE
	GDBM_CENTFREE
	GDBM_COALESCEBLKS
	GDBM_FAST
	GDBM_FASTMODE
	GDBM_INSERT
	GDBM_NEWDB
	GDBM_NOLOCK
	GDBM_OPENMASK
	GDBM_READER
	GDBM_REPLACE
	GDBM_SYNC
	GDBM_SYNCMODE
	GDBM_WRCREAT
	GDBM_WRITER
);

# This module isn't dual life, so no need for dev version numbers.
$VERSION = '1.09';

sub AUTOLOAD {
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    my ($error, $val) = constant($constname);
    Carp::croak $error if $error;
    no strict 'refs';
    *{$AUTOLOAD} = sub { $val };
    goto &{$AUTOLOAD};
}

XSLoader::load 'GDBM_File', $VERSION;

1;
