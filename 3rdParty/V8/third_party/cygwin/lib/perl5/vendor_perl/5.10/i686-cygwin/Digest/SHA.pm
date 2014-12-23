package Digest::SHA;

require 5.003000;

use strict;
use integer;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);

$VERSION = '5.47';

require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
@EXPORT_OK = qw(
	hmac_sha1	hmac_sha1_base64	hmac_sha1_hex
	hmac_sha224	hmac_sha224_base64	hmac_sha224_hex
	hmac_sha256	hmac_sha256_base64	hmac_sha256_hex
	hmac_sha384	hmac_sha384_base64	hmac_sha384_hex
	hmac_sha512	hmac_sha512_base64	hmac_sha512_hex
	sha1		sha1_base64		sha1_hex
	sha224		sha224_base64		sha224_hex
	sha256		sha256_base64		sha256_hex
	sha384		sha384_base64		sha384_hex
	sha512		sha512_base64		sha512_hex);

# If possible, inherit from Digest::base (which depends on MIME::Base64)

*addfile = \&Addfile;

eval {
	require MIME::Base64;
	require Digest::base;
	push(@ISA, 'Digest::base');
};
if ($@) {
	*hexdigest = \&Hexdigest;
	*b64digest = \&B64digest;
}

# The following routines aren't time-critical, so they can be left in Perl

sub new {
	my($class, $alg) = @_;
	$alg =~ s/\D+//g if defined $alg;
	if (ref($class)) {	# instance method
		unless (defined($alg) && ($alg != $class->algorithm)) {
			sharewind($$class);
			return($class);
		}
		shaclose($$class) if $$class;
		$$class = shaopen($alg) || return;
		return($class);
	}
	$alg = 1 unless defined $alg;
	my $state = shaopen($alg) || return;
	my $self = \$state;
	bless($self, $class);
	return($self);
}

sub DESTROY {
	my $self = shift;
	shaclose($$self) if $$self;
}

sub clone {
	my $self = shift;
	my $state = shadup($$self) || return;
	my $copy = \$state;
	bless($copy, ref($self));
	return($copy);
}

*reset = \&new;

sub add_bits {
	my($self, $data, $nbits) = @_;
	unless (defined $nbits) {
		$nbits = length($data);
		$data = pack("B*", $data);
	}
	shawrite($data, $nbits, $$self);
	return($self);
}

sub _bail {
	my $msg = shift;

        require Carp;
        Carp::croak("$msg: $!");
}

sub _addfile {  # this is "addfile" from Digest::base 1.00
    my ($self, $handle) = @_;

    my $n;
    my $buf = "";

    while (($n = read($handle, $buf, 4096))) {
        $self->add($buf);
    }
    _bail("Read failed") unless defined $n;

    $self;
}

sub Addfile {
	my ($self, $file, $mode) = @_;

	return(_addfile($self, $file)) unless ref(\$file) eq 'SCALAR';

	$mode = defined($mode) ? $mode : "";
	my ($binary, $portable) = map { $_ eq $mode } ("b", "p");
	my $text = -T $file;

	local *FH;
		# protect any leading or trailing whitespace in $file;
		# otherwise, 2-arg "open" will ignore them
	$file =~ s#^(\s)#./$1#;
	open(FH, "< $file\0") or _bail("Open failed");
	binmode(FH) if $binary || $portable;

	unless ($portable && $text) {
		$self->_addfile(*FH);
		close(FH);
		return($self);
	}

	my ($n1, $n2);
	my ($buf1, $buf2) = ("", "");

	while (($n1 = read(FH, $buf1, 4096))) {
		while (substr($buf1, -1) eq "\015") {
			$n2 = read(FH, $buf2, 4096);
			_bail("Read failed") unless defined $n2;
			last unless $n2;
			$buf1 .= $buf2;
		}
		$buf1 =~ s/\015?\015\012/\012/g; 	# DOS/Windows
		$buf1 =~ s/\015/\012/g;          	# early MacOS
		$self->add($buf1);
	}
	_bail("Read failed") unless defined $n1;
	close(FH);

	$self;
}

sub dump {
	my $self = shift;
	my $file = shift || "";

	shadump($file, $$self) || return;
	return($self);
}

sub load {
	my $class = shift;
	my $file = shift || "";
	if (ref($class)) {	# instance method
		shaclose($$class) if $$class;
		$$class = shaload($file) || return;
		return($class);
	}
	my $state = shaload($file) || return;
	my $self = \$state;
	bless($self, $class);
	return($self);
}

Digest::SHA->bootstrap($VERSION);

1;
__END__

=head1 NAME

Digest::SHA - Perl extension for SHA-1/224/256/384/512

=head1 SYNOPSIS

In programs:

		# Functional interface

	use Digest::SHA qw(sha1 sha1_hex sha1_base64 ...);

	$digest = sha1($data);
	$digest = sha1_hex($data);
	$digest = sha1_base64($data);

	$digest = sha256($data);
	$digest = sha384_hex($data);
	$digest = sha512_base64($data);

		# Object-oriented

	use Digest::SHA;

	$sha = Digest::SHA->new($alg);

	$sha->add($data);		# feed data into stream

	$sha->addfile(*F);
	$sha->addfile($filename);

	$sha->add_bits($bits);
	$sha->add_bits($data, $nbits);

	$sha_copy = $sha->clone;	# if needed, make copy of
	$sha->dump($file);		#	current digest state,
	$sha->load($file);		#	or save it on disk

	$digest = $sha->digest;		# compute digest
	$digest = $sha->hexdigest;
	$digest = $sha->b64digest;

From the command line:

	$ shasum files

	$ shasum --help

=head1 SYNOPSIS (HMAC-SHA)

		# Functional interface only

	use Digest::SHA qw(hmac_sha1 hmac_sha1_hex ...);

	$digest = hmac_sha1($data, $key);
	$digest = hmac_sha224_hex($data, $key);
	$digest = hmac_sha256_base64($data, $key);

=head1 ABSTRACT

Digest::SHA is a complete implementation of the NIST Secure Hash
Standard.  It gives Perl programmers a convenient way to calculate
SHA-1, SHA-224, SHA-256, SHA-384, and SHA-512 message digests.
The module can handle all types of input, including partial-byte
data.

=head1 DESCRIPTION

Digest::SHA is written in C for speed.  If your platform lacks a
C compiler, you can install the functionally equivalent (but much
slower) L<Digest::SHA::PurePerl> module.

The programming interface is easy to use: it's the same one found
in CPAN's L<Digest> module.  So, if your applications currently
use L<Digest::MD5> and you'd prefer the stronger security of SHA,
it's a simple matter to convert them.

The interface provides two ways to calculate digests:  all-at-once,
or in stages.  To illustrate, the following short program computes
the SHA-256 digest of "hello world" using each approach:

	use Digest::SHA qw(sha256_hex);

	$data = "hello world";
	@frags = split(//, $data);

	# all-at-once (Functional style)
	$digest1 = sha256_hex($data);

	# in-stages (OOP style)
	$state = Digest::SHA->new(256);
	for (@frags) { $state->add($_) }
	$digest2 = $state->hexdigest;

	print $digest1 eq $digest2 ?
		"whew!\n" : "oops!\n";

To calculate the digest of an n-bit message where I<n> is not a
multiple of 8, use the I<add_bits()> method.  For example, consider
the 446-bit message consisting of the bit-string "110" repeated
148 times, followed by "11".  Here's how to display its SHA-1
digest:

	use Digest::SHA;
	$bits = "110" x 148 . "11";
	$sha = Digest::SHA->new(1)->add_bits($bits);
	print $sha->hexdigest, "\n";

Note that for larger bit-strings, it's more efficient to use the
two-argument version I<add_bits($data, $nbits)>, where I<$data> is
in the customary packed binary format used for Perl strings.

The module also lets you save intermediate SHA states to disk, or
display them on standard output.  The I<dump()> method generates
portable, human-readable text describing the current state of
computation.  You can subsequently retrieve the file with I<load()>
to resume where the calculation left off.

To see what a state description looks like, just run the following:

	use Digest::SHA;
	Digest::SHA->new->add("Shaw" x 1962)->dump;

As an added convenience, the Digest::SHA module offers routines to
calculate keyed hashes using the HMAC-SHA-1/224/256/384/512
algorithms.  These services exist in functional form only, and
mimic the style and behavior of the I<sha()>, I<sha_hex()>, and
I<sha_base64()> functions.

	# Test vector from draft-ietf-ipsec-ciph-sha-256-01.txt

	use Digest::SHA qw(hmac_sha256_hex);
	print hmac_sha256_hex("Hi There", chr(0x0b) x 32), "\n";

=head1 NIST STATEMENT ON SHA-1

I<NIST was recently informed that researchers had discovered a way
to "break" the current Federal Information Processing Standard SHA-1
algorithm, which has been in effect since 1994. The researchers
have not yet published their complete results, so NIST has not
confirmed these findings. However, the researchers are a reputable
research team with expertise in this area.>

I<Due to advances in computing power, NIST already planned to phase
out SHA-1 in favor of the larger and stronger hash functions (SHA-224,
SHA-256, SHA-384 and SHA-512) by 2010. New developments should use
the larger and stronger hash functions.>

ref. L<http://www.csrc.nist.gov/pki/HashWorkshop/NIST%20Statement/Burr_Mar2005.html>

=head1 PADDING OF BASE64 DIGESTS

By convention, CPAN Digest modules do B<not> pad their Base64 output.
Problems can occur when feeding such digests to other software that
expects properly padded Base64 encodings.

For the time being, any necessary padding must be done by the user.
Fortunately, this is a simple operation: if the length of a Base64-encoded
digest isn't a multiple of 4, simply append "=" characters to the end
of the digest until it is:

	while (length($b64_digest) % 4) {
		$b64_digest .= '=';
	}

To illustrate, I<sha256_base64("abc")> is computed to be

	ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0

which has a length of 43.  So, the properly padded version is

	ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=

=head1 EXPORT

None by default.

=head1 EXPORTABLE FUNCTIONS

Provided your C compiler supports a 64-bit type (e.g. the I<long
long> of C99, or I<__int64> used by Microsoft C/C++), all of these
functions will be available for use.  Otherwise, you won't be able
to perform the SHA-384 and SHA-512 transforms, both of which require
64-bit operations.

I<Functional style>

=over 4

=item B<sha1($data, ...)>

=item B<sha224($data, ...)>

=item B<sha256($data, ...)>

=item B<sha384($data, ...)>

=item B<sha512($data, ...)>

Logically joins the arguments into a single string, and returns
its SHA-1/224/256/384/512 digest encoded as a binary string.

=item B<sha1_hex($data, ...)>

=item B<sha224_hex($data, ...)>

=item B<sha256_hex($data, ...)>

=item B<sha384_hex($data, ...)>

=item B<sha512_hex($data, ...)>

Logically joins the arguments into a single string, and returns
its SHA-1/224/256/384/512 digest encoded as a hexadecimal string.

=item B<sha1_base64($data, ...)>

=item B<sha224_base64($data, ...)>

=item B<sha256_base64($data, ...)>

=item B<sha384_base64($data, ...)>

=item B<sha512_base64($data, ...)>

Logically joins the arguments into a single string, and returns
its SHA-1/224/256/384/512 digest encoded as a Base64 string.

It's important to note that the resulting string does B<not> contain
the padding characters typical of Base64 encodings.  This omission is
deliberate, and is done to maintain compatibility with the family of
CPAN Digest modules.  See L</"PADDING OF BASE64 DIGESTS"> for details.

=back

I<OOP style>

=over 4

=item B<new($alg)>

Returns a new Digest::SHA object.  Allowed values for I<$alg> are
1, 224, 256, 384, or 512.  It's also possible to use common string
representations of the algorithm (e.g. "sha256", "SHA-384").  If
the argument is missing, SHA-1 will be used by default.

Invoking I<new> as an instance method will not create a new object;
instead, it will simply reset the object to the initial state
associated with I<$alg>.  If the argument is missing, the object
will continue using the same algorithm that was selected at creation.

=item B<reset($alg)>

This method has exactly the same effect as I<new($alg)>.  In fact,
I<reset> is just an alias for I<new>.

=item B<hashsize>

Returns the number of digest bits for this object.  The values are
160, 224, 256, 384, and 512 for SHA-1, SHA-224, SHA-256, SHA-384,
and SHA-512, respectively.

=item B<algorithm>

Returns the digest algorithm for this object.  The values are 1,
224, 256, 384, and 512 for SHA-1, SHA-224, SHA-256, SHA-384, and
SHA-512, respectively.

=item B<clone>

Returns a duplicate copy of the object.

=item B<add($data, ...)>

Logically joins the arguments into a single string, and uses it to
update the current digest state.  In other words, the following
statements have the same effect:

	$sha->add("a"); $sha->add("b"); $sha->add("c");
	$sha->add("a")->add("b")->add("c");
	$sha->add("a", "b", "c");
	$sha->add("abc");

The return value is the updated object itself.

=item B<add_bits($data, $nbits)>

=item B<add_bits($bits)>

Updates the current digest state by appending bits to it.  The
return value is the updated object itself.

The first form causes the most-significant I<$nbits> of I<$data>
to be appended to the stream.  The I<$data> argument is in the
customary binary format used for Perl strings.

The second form takes an ASCII string of "0" and "1" characters as
its argument.  It's equivalent to

	$sha->add_bits(pack("B*", $bits), length($bits));

So, the following two statements do the same thing:

	$sha->add_bits("111100001010");
	$sha->add_bits("\xF0\xA0", 12);

=item B<addfile(*FILE)>

Reads from I<FILE> until EOF, and appends that data to the current
state.  The return value is the updated object itself.

=item B<addfile($filename [, $mode])>

Reads the contents of I<$filename>, and appends that data to the current
state.  The return value is the updated object itself.

By default, I<$filename> is simply opened and read; no special modes
or I/O disciplines are used.  To change this, set the optional I<$mode>
argument to one of the following values:

	"b"	read file in binary mode

	"p"	use portable mode

The "p" mode is handy since it ensures that the digest value of
I<$filename> will be the same when computed on different operating
systems.  It accomplishes this by internally translating all newlines in
text files to UNIX format before calculating the digest.  Binary files
are read in raw mode with no translation whatsoever.

For a fuller discussion of newline formats, refer to CPAN module
L<File::LocalizeNewlines>.  Its "universal line separator" regex forms
the basis of I<addfile>'s portable mode processing.

=item B<dump($filename)>

Provides persistent storage of intermediate SHA states by writing
a portable, human-readable representation of the current state to
I<$filename>.  If the argument is missing, or equal to the empty
string, the state information will be written to STDOUT.

=item B<load($filename)>

Returns a Digest::SHA object representing the intermediate SHA
state that was previously dumped to I<$filename>.  If called as a
class method, a new object is created; if called as an instance
method, the object is reset to the state contained in I<$filename>.
If the argument is missing, or equal to the empty string, the state
information will be read from STDIN.

=item B<digest>

Returns the digest encoded as a binary string.

Note that the I<digest> method is a read-once operation. Once it
has been performed, the Digest::SHA object is automatically reset
in preparation for calculating another digest value.  Call
I<$sha-E<gt>clone-E<gt>digest> if it's necessary to preserve the
original digest state.

=item B<hexdigest>

Returns the digest encoded as a hexadecimal string.

Like I<digest>, this method is a read-once operation.  Call
I<$sha-E<gt>clone-E<gt>hexdigest> if it's necessary to preserve
the original digest state.

This method is inherited if L<Digest::base> is installed on your
system.  Otherwise, a functionally equivalent substitute is used.

=item B<b64digest>

Returns the digest encoded as a Base64 string.

Like I<digest>, this method is a read-once operation.  Call
I<$sha-E<gt>clone-E<gt>b64digest> if it's necessary to preserve
the original digest state.

This method is inherited if L<Digest::base> is installed on your
system.  Otherwise, a functionally equivalent substitute is used.

It's important to note that the resulting string does B<not> contain
the padding characters typical of Base64 encodings.  This omission is
deliberate, and is done to maintain compatibility with the family of
CPAN Digest modules.  See L</"PADDING OF BASE64 DIGESTS"> for details.

=back

I<HMAC-SHA-1/224/256/384/512>

=over 4

=item B<hmac_sha1($data, $key)>

=item B<hmac_sha224($data, $key)>

=item B<hmac_sha256($data, $key)>

=item B<hmac_sha384($data, $key)>

=item B<hmac_sha512($data, $key)>

Returns the HMAC-SHA-1/224/256/384/512 digest of I<$data>/I<$key>,
with the result encoded as a binary string.  Multiple I<$data>
arguments are allowed, provided that I<$key> is the last argument
in the list.

=item B<hmac_sha1_hex($data, $key)>

=item B<hmac_sha224_hex($data, $key)>

=item B<hmac_sha256_hex($data, $key)>

=item B<hmac_sha384_hex($data, $key)>

=item B<hmac_sha512_hex($data, $key)>

Returns the HMAC-SHA-1/224/256/384/512 digest of I<$data>/I<$key>,
with the result encoded as a hexadecimal string.  Multiple I<$data>
arguments are allowed, provided that I<$key> is the last argument
in the list.

=item B<hmac_sha1_base64($data, $key)>

=item B<hmac_sha224_base64($data, $key)>

=item B<hmac_sha256_base64($data, $key)>

=item B<hmac_sha384_base64($data, $key)>

=item B<hmac_sha512_base64($data, $key)>

Returns the HMAC-SHA-1/224/256/384/512 digest of I<$data>/I<$key>,
with the result encoded as a Base64 string.  Multiple I<$data>
arguments are allowed, provided that I<$key> is the last argument
in the list.

It's important to note that the resulting string does B<not> contain
the padding characters typical of Base64 encodings.  This omission is
deliberate, and is done to maintain compatibility with the family of
CPAN Digest modules.  See L</"PADDING OF BASE64 DIGESTS"> for details.

=back

=head1 SEE ALSO

L<Digest>, L<Digest::SHA::PurePerl>

The Secure Hash Standard (FIPS PUB 180-2) can be found at:

L<http://csrc.nist.gov/publications/fips/fips180-2/fips180-2withchangenotice.pdf>

The Keyed-Hash Message Authentication Code (HMAC):

L<http://csrc.nist.gov/publications/fips/fips198/fips-198a.pdf>

=head1 AUTHOR

	Mark Shelor	<mshelor@cpan.org>

=head1 ACKNOWLEDGMENTS

The author is particularly grateful to

	Gisle Aas
	Chris Carey
	Alexandr Ciornii
	Jim Doble
	Julius Duque
	Jeffrey Friedl
	Robert Gilmour
	Brian Gladman
	Adam Kennedy
	Andy Lester
	Alex Muntada
	Steve Peters
	Chris Skiscim
	Martin Thurn
	Gunnar Wolf
	Adam Woodbury

for their valuable comments and suggestions.

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2003-2008 Mark Shelor

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

L<perlartistic>

=cut
