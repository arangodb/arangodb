package MIME::QuotedPrint;

# $Id: QuotedPrint.pm,v 3.7 2005/11/29 20:49:46 gisle Exp $

use strict;
use vars qw(@ISA @EXPORT $VERSION);

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(encode_qp decode_qp);

$VERSION = "3.07";

use MIME::Base64;  # will load XS version of {en,de}code_qp()

*encode = \&encode_qp;
*decode = \&decode_qp;

1;

__END__

=head1 NAME

MIME::QuotedPrint - Encoding and decoding of quoted-printable strings

=head1 SYNOPSIS

 use MIME::QuotedPrint;

 $encoded = encode_qp($decoded);
 $decoded = decode_qp($encoded);

=head1 DESCRIPTION

This module provides functions to encode and decode strings into and from the
quoted-printable encoding specified in RFC 2045 - I<MIME (Multipurpose
Internet Mail Extensions)>.  The quoted-printable encoding is intended
to represent data that largely consists of bytes that correspond to
printable characters in the ASCII character set.  Each non-printable
character (as defined by English Americans) is represented by a
triplet consisting of the character "=" followed by two hexadecimal
digits.

The following functions are provided:

=over 4

=item encode_qp($str)

=item encode_qp($str, $eol)

=item encode_qp($str, $eol, $binmode)

This function returns an encoded version of the string ($str) given as
argument.

The second argument ($eol) is the line-ending sequence to use.  It is
optional and defaults to "\n".  Every occurrence of "\n" is replaced
with this string, and it is also used for additional "soft line
breaks" to ensure that no line end up longer than 76 characters.  Pass
it as "\015\012" to produce data suitable for external consumption.
The string "\r\n" produces the same result on many platforms, but not
all.

The third argument ($binmode) will select binary mode if passed as a
TRUE value.  In binary mode "\n" will be encoded in the same way as
any other non-printable character.  This ensures that a decoder will
end up with exactly the same string whatever line ending sequence it
uses.  In general it is preferable to use the base64 encoding for
binary data; see L<MIME::Base64>.

An $eol of "" (the empty string) is special.  In this case, no "soft
line breaks" are introduced and binary mode is effectively enabled so
that any "\n" in the original data is encoded as well.

=item decode_qp($str);

This function returns the plain text version of the string given
as argument.  The lines of the result are "\n" terminated, even if
the $str argument contains "\r\n" terminated lines.

=back


If you prefer not to import these routines into your namespace, you can
call them as:

  use MIME::QuotedPrint ();
  $encoded = MIME::QuotedPrint::encode($decoded);
  $decoded = MIME::QuotedPrint::decode($encoded);

Perl v5.8 and better allow extended Unicode characters in strings.
Such strings cannot be encoded directly, as the quoted-printable
encoding is only defined for single-byte characters.  The solution is
to use the Encode module to select the byte encoding you want.  For
example:

    use MIME::QuotedPrint qw(encode_qp);
    use Encode qw(encode);

    $encoded = encode_qp(encode("UTF-8", "\x{FFFF}\n"));
    print $encoded;

=head1 COPYRIGHT

Copyright 1995-1997,2002-2004 Gisle Aas.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<MIME::Base64>

=cut
