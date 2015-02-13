package Digest::file;

use strict;

use Exporter ();
use Carp qw(croak);
use Digest ();

use vars qw($VERSION @ISA @EXPORT_OK);

$VERSION = "1.00";
@ISA = qw(Exporter);
@EXPORT_OK = qw(digest_file_ctx digest_file digest_file_hex digest_file_base64);

sub digest_file_ctx {
    my $file = shift;
    croak("No digest algorithm specified") unless @_;
    local *F;
    open(F, $file) || croak("Can't open '$file': $!");
    binmode(F);
    my $ctx = Digest->new(@_);
    $ctx->addfile(*F);
    close(F);
    return $ctx;
}

sub digest_file {
    digest_file_ctx(@_)->digest;
}

sub digest_file_hex {
    digest_file_ctx(@_)->hexdigest;
}

sub digest_file_base64 {
    digest_file_ctx(@_)->b64digest;
}

1;

__END__

=head1 NAME

Digest::file - Calculate digests of files

=head1 SYNOPSIS

  # Poor mans "md5sum" command
  use Digest::file qw(digest_file_hex);
  for (@ARGV) {
      print digest_file_hex($_, "MD5"), "  $_\n";
  }

=head1 DESCRIPTION

This module provide 3 convenience functions to calculate the digest
of files.  The following functions are provided:

=over

=item digest_file( $file, $algorithm, [$arg,...] )

This function will calculate and return the binary digest of the bytes
of the given file.  The function will croak if it fails to open or
read the file.

The $algorithm is a string like "MD2", "MD5", "SHA-1", "SHA-512".
Additional arguments are passed to the constructor for the
implementation of the given algorithm.

=item digest_file_hex( $file, $algorithm, [$arg,...] )

Same as digest_file(), but return the digest in hex form.

=item digest_file_base64( $file, $algorithm, [$arg,...] )

Same as digest_file(), but return the digest as a base64 encoded
string.

=back

=head1 SEE ALSO

L<Digest>
