package Digest::MD5;

use strict;
use vars qw($VERSION @ISA @EXPORT_OK);

$VERSION = '2.36_01';  # $Date: 2005/11/30 13:46:47 $

require Exporter;
*import = \&Exporter::import;
@EXPORT_OK = qw(md5 md5_hex md5_base64);

eval {
    require Digest::base;
    push(@ISA, 'Digest::base');
};
if ($@) {
    my $err = $@;
    *add_bits = sub { die $err };
}


eval {
    require XSLoader;
    XSLoader::load('Digest::MD5', $VERSION);
};
if ($@) {
    my $olderr = $@;
    eval {
	# Try to load the pure perl version
	require Digest::Perl::MD5;

	Digest::Perl::MD5->import(qw(md5 md5_hex md5_base64));
	push(@ISA, "Digest::Perl::MD5");  # make OO interface work
    };
    if ($@) {
	# restore the original error
	die $olderr;
    }
}
else {
    *reset = \&new;
}

1;
__END__

=head1 NAME

Digest::MD5 - Perl interface to the MD5 Algorithm

=head1 SYNOPSIS

 # Functional style
 use Digest::MD5 qw(md5 md5_hex md5_base64);

 $digest = md5($data);
 $digest = md5_hex($data);
 $digest = md5_base64($data);

 # OO style
 use Digest::MD5;

 $ctx = Digest::MD5->new;

 $ctx->add($data);
 $ctx->addfile(*FILE);

 $digest = $ctx->digest;
 $digest = $ctx->hexdigest;
 $digest = $ctx->b64digest;

=head1 DESCRIPTION

The C<Digest::MD5> module allows you to use the RSA Data Security
Inc. MD5 Message Digest algorithm from within Perl programs.  The
algorithm takes as input a message of arbitrary length and produces as
output a 128-bit "fingerprint" or "message digest" of the input.

Note that the MD5 algorithm is not as strong as it used to be.  It has
since 2005 been easy to generate different messages that produce the
same MD5 digest.  It still seems hard to generate messages that
produce a given digest, but it is probably wise to move to stronger
algorithms for applications that depend on the digest to uniquely identify
a message.

The C<Digest::MD5> module provide a procedural interface for simple
use, as well as an object oriented interface that can handle messages
of arbitrary length and which can read files directly.

=head1 FUNCTIONS

The following functions are provided by the C<Digest::MD5> module.
None of these functions are exported by default.

=over 4

=item md5($data,...)

This function will concatenate all arguments, calculate the MD5 digest
of this "message", and return it in binary form.  The returned string
will be 16 bytes long.

The result of md5("a", "b", "c") will be exactly the same as the
result of md5("abc").

=item md5_hex($data,...)

Same as md5(), but will return the digest in hexadecimal form. The
length of the returned string will be 32 and it will only contain
characters from this set: '0'..'9' and 'a'..'f'.

=item md5_base64($data,...)

Same as md5(), but will return the digest as a base64 encoded string.
The length of the returned string will be 22 and it will only contain
characters from this set: 'A'..'Z', 'a'..'z', '0'..'9', '+' and
'/'.

Note that the base64 encoded string returned is not padded to be a
multiple of 4 bytes long.  If you want interoperability with other
base64 encoded md5 digests you might want to append the redundant
string "==" to the result.

=back

=head1 METHODS

The object oriented interface to C<Digest::MD5> is described in this
section.  After a C<Digest::MD5> object has been created, you will add
data to it and finally ask for the digest in a suitable format.  A
single object can be used to calculate multiple digests.

The following methods are provided:

=over 4

=item $md5 = Digest::MD5->new

The constructor returns a new C<Digest::MD5> object which encapsulate
the state of the MD5 message-digest algorithm.

If called as an instance method (i.e. $md5->new) it will just reset the
state the object to the state of a newly created object.  No new
object is created in this case.

=item $md5->reset

This is just an alias for $md5->new.

=item $md5->clone

This a copy of the $md5 object. It is useful when you do not want to
destroy the digests state, but need an intermediate value of the
digest, e.g. when calculating digests iteratively on a continuous data
stream.  Example:

    my $md5 = Digest::MD5->new;
    while (<>) {
	$md5->add($_);
	print "Line $.: ", $md5->clone->hexdigest, "\n";
    }

=item $md5->add($data,...)

The $data provided as argument are appended to the message we
calculate the digest for.  The return value is the $md5 object itself.

All these lines will have the same effect on the state of the $md5
object:

    $md5->add("a"); $md5->add("b"); $md5->add("c");
    $md5->add("a")->add("b")->add("c");
    $md5->add("a", "b", "c");
    $md5->add("abc");

=item $md5->addfile($io_handle)

The $io_handle will be read until EOF and its content appended to the
message we calculate the digest for.  The return value is the $md5
object itself.

The addfile() method will croak() if it fails reading data for some
reason.  If it croaks it is unpredictable what the state of the $md5
object will be in. The addfile() method might have been able to read
the file partially before it failed.  It is probably wise to discard
or reset the $md5 object if this occurs.

In most cases you want to make sure that the $io_handle is in
C<binmode> before you pass it as argument to the addfile() method.

=item $md5->add_bits($data, $nbits)

=item $md5->add_bits($bitstring)

Since the MD5 algorithm is byte oriented you might only add bits as
multiples of 8, so you probably want to just use add() instead.  The
add_bits() method is provided for compatibility with other digest
implementations.  See L<Digest> for description of the arguments
that add_bits() take.

=item $md5->digest

Return the binary digest for the message.  The returned string will be
16 bytes long.

Note that the C<digest> operation is effectively a destructive,
read-once operation. Once it has been performed, the C<Digest::MD5>
object is automatically C<reset> and can be used to calculate another
digest value.  Call $md5->clone->digest if you want to calculate the
digest without resetting the digest state.

=item $md5->hexdigest

Same as $md5->digest, but will return the digest in hexadecimal
form. The length of the returned string will be 32 and it will only
contain characters from this set: '0'..'9' and 'a'..'f'.

=item $md5->b64digest

Same as $md5->digest, but will return the digest as a base64 encoded
string.  The length of the returned string will be 22 and it will only
contain characters from this set: 'A'..'Z', 'a'..'z', '0'..'9', '+'
and '/'.


The base64 encoded string returned is not padded to be a multiple of 4
bytes long.  If you want interoperability with other base64 encoded
md5 digests you might want to append the string "==" to the result.

=back


=head1 EXAMPLES

The simplest way to use this library is to import the md5_hex()
function (or one of its cousins):

    use Digest::MD5 qw(md5_hex);
    print "Digest is ", md5_hex("foobarbaz"), "\n";

The above example would print out the message:

    Digest is 6df23dc03f9b54cc38a0fc1483df6e21

The same checksum can also be calculated in OO style:

    use Digest::MD5;
    
    $md5 = Digest::MD5->new;
    $md5->add('foo', 'bar');
    $md5->add('baz');
    $digest = $md5->hexdigest;
    
    print "Digest is $digest\n";

With OO style you can break the message arbitrary.  This means that we
are no longer limited to have space for the whole message in memory, i.e.
we can handle messages of any size.

This is useful when calculating checksum for files:

    use Digest::MD5;

    my $file = shift || "/etc/passwd";
    open(FILE, $file) or die "Can't open '$file': $!";
    binmode(FILE);

    $md5 = Digest::MD5->new;
    while (<FILE>) {
        $md5->add($_);
    }
    close(FILE);
    print $md5->b64digest, " $file\n";

Or we can use the addfile method for more efficient reading of
the file:

    use Digest::MD5;

    my $file = shift || "/etc/passwd";
    open(FILE, $file) or die "Can't open '$file': $!";
    binmode(FILE);

    print Digest::MD5->new->addfile(*FILE)->hexdigest, " $file\n";

Perl 5.8 support Unicode characters in strings.  Since the MD5
algorithm is only defined for strings of bytes, it can not be used on
strings that contains chars with ordinal number above 255.  The MD5
functions and methods will croak if you try to feed them such input
data:

    use Digest::MD5 qw(md5_hex);

    my $str = "abc\x{300}";
    print md5_hex($str), "\n";  # croaks
    # Wide character in subroutine entry

What you can do is calculate the MD5 checksum of the UTF-8
representation of such strings.  This is achieved by filtering the
string through encode_utf8() function:

    use Digest::MD5 qw(md5_hex);
    use Encode qw(encode_utf8);

    my $str = "abc\x{300}";
    print md5_hex(encode_utf8($str)), "\n";
    # 8c2d46911f3f5a326455f0ed7a8ed3b3

=head1 SEE ALSO

L<Digest>,
L<Digest::MD2>,
L<Digest::SHA1>,
L<Digest::HMAC>

L<md5sum(1)>

RFC 1321

http://en.wikipedia.org/wiki/MD5

The paper "How to Break MD5 and Other Hash Functions" by Xiaoyun Wang
and Hongbo Yu.

=head1 COPYRIGHT

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

 Copyright 1998-2003 Gisle Aas.
 Copyright 1995-1996 Neil Winton.
 Copyright 1991-1992 RSA Data Security, Inc.

The MD5 algorithm is defined in RFC 1321. This implementation is
derived from the reference C code in RFC 1321 which is covered by
the following copyright statement:

=over 4

=item

Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.

=back

This copyright does not prohibit distribution of any version of Perl
containing this extension under the terms of the GNU or Artistic
licenses.

=head1 AUTHORS

The original C<MD5> interface was written by Neil Winton
(C<N.Winton@axion.bt.co.uk>).

The C<Digest::MD5> module is written by Gisle Aas <gisle@ActiveState.com>.

=cut
