package PerlIO::scalar;
our $VERSION = '0.06';
use XSLoader ();
XSLoader::load 'PerlIO::scalar';
1;
__END__

=head1 NAME

PerlIO::scalar - in-memory IO, scalar IO

=head1 SYNOPSIS

   my $scalar = '';
   ...
   open my $fh, "<",  \$scalar or die;
   open my $fh, ">",  \$scalar or die;
   open my $fh, ">>", \$scalar or die;

or

   my $scalar = '';
   ...
   open my $fh, "<:scalar",  \$scalar or die;
   open my $fh, ">:scalar",  \$scalar or die;
   open my $fh, ">>:scalar", \$scalar or die;

=head1 DESCRIPTION

A filehandle is opened but the file operations are performed "in-memory"
on a scalar variable.  All the normal file operations can be performed
on the handle. The scalar is considered a stream of bytes.  Currently
fileno($fh) returns -1.

=head1 IMPLEMENTATION NOTE

C<PerlIO::scalar> only exists to use XSLoader to load C code that
provides support for treating a scalar as an "in memory" file.
One does not need to explicitly C<use PerlIO::scalar>.

=cut
