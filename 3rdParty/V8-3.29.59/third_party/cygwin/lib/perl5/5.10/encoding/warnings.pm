package encoding::warnings;
$encoding::warnings::VERSION = '0.11';

use strict;
use 5.007;

=head1 NAME

encoding::warnings - Warn on implicit encoding conversions

=head1 VERSION

This document describes version 0.11 of encoding::warnings, released
June 5, 2007.

=head1 SYNOPSIS

    use encoding::warnings; # or 'FATAL' to raise fatal exceptions

    utf8::encode($a = chr(20000));  # a byte-string (raw bytes)
    $b = chr(20000);		    # a unicode-string (wide characters)

    # "Bytes implicitly upgraded into wide characters as iso-8859-1"
    $c = $a . $b;

=head1 DESCRIPTION

=head2 Overview of the problem

By default, there is a fundamental asymmetry in Perl's unicode model:
implicit upgrading from byte-strings to unicode-strings assumes that
they were encoded in I<ISO 8859-1 (Latin-1)>, but unicode-strings are
downgraded with UTF-8 encoding.  This happens because the first 256
codepoints in Unicode happens to agree with Latin-1.  

However, this silent upgrading can easily cause problems, if you happen
to mix unicode strings with non-Latin1 data -- i.e. byte-strings encoded
in UTF-8 or other encodings.  The error will not manifest until the
combined string is written to output, at which time it would be impossible
to see where did the silent upgrading occur.

=head2 Detecting the problem

This module simplifies the process of diagnosing such problems.  Just put
this line on top of your main program:

    use encoding::warnings;

Afterwards, implicit upgrading of high-bit bytes will raise a warning.
Ex.: C<Bytes implicitly upgraded into wide characters as iso-8859-1 at
- line 7>.

However, strings composed purely of ASCII code points (C<0x00>..C<0x7F>)
will I<not> trigger this warning.

You can also make the warnings fatal by importing this module as:

    use encoding::warnings 'FATAL';

=head2 Solving the problem

Most of the time, this warning occurs when a byte-string is concatenated
with a unicode-string.  There are a number of ways to solve it:

=over 4

=item * Upgrade both sides to unicode-strings

If your program does not need compatibility for Perl 5.6 and earlier,
the recommended approach is to apply appropriate IO disciplines, so all
data in your program become unicode-strings.  See L<encoding>, L<open> and
L<perlfunc/binmode> for how.

=item * Downgrade both sides to byte-strings

The other way works too, especially if you are sure that all your data
are under the same encoding, or if compatibility with older versions
of Perl is desired.

You may downgrade strings with C<Encode::encode> and C<utf8::encode>.
See L<Encode> and L<utf8> for details.

=item * Specify the encoding for implicit byte-string upgrading

If you are confident that all byte-strings will be in a specific
encoding like UTF-8, I<and> need not support older versions of Perl,
use the C<encoding> pragma:

    use encoding 'utf8';

Similarly, this will silence warnings from this module, and preserve the
default behaviour:

    use encoding 'iso-8859-1';

However, note that C<use encoding> actually had three distinct effects:

=over 4

=item * PerlIO layers for B<STDIN> and B<STDOUT>

This is similar to what L<open> pragma does.

=item * Literal conversions

This turns I<all> literal string in your program into unicode-strings
(equivalent to a C<use utf8>), by decoding them using the specified
encoding.

=item * Implicit upgrading for byte-strings

This will silence warnings from this module, as shown above.

=back

Because literal conversions also work on empty strings, it may surprise
some people:

    use encoding 'big5';

    my $byte_string = pack("C*", 0xA4, 0x40);
    print length $a;	# 2 here.
    $a .= "";		# concatenating with a unicode string...
    print length $a;	# 1 here!

In other words, do not C<use encoding> unless you are certain that the
program will not deal with any raw, 8-bit binary data at all.

However, the C<Filter =E<gt> 1> flavor of C<use encoding> will I<not>
affect implicit upgrading for byte-strings, and is thus incapable of
silencing warnings from this module.  See L<encoding> for more details.

=back

=head1 CAVEATS

For Perl 5.9.4 or later, this module's effect is lexical.

For Perl versions prior to 5.9.4, this module affects the whole script,
instead of inside its lexical block.

=cut

# Constants.
sub ASCII  () { 0 }
sub LATIN1 () { 1 }
sub FATAL  () { 2 }

# Install a ${^ENCODING} handler if no other one are already in place.
sub import {
    my $class = shift;
    my $fatal = shift || '';

    local $@;
    return if ${^ENCODING} and ref(${^ENCODING}) ne $class;
    return unless eval { require Encode; 1 };

    my $ascii  = Encode::find_encoding('us-ascii') or return;
    my $latin1 = Encode::find_encoding('iso-8859-1') or return;

    # Have to undef explicitly here
    undef ${^ENCODING};

    # Install a warning handler for decode()
    my $decoder = bless(
	[
	    $ascii,
	    $latin1,
	    (($fatal eq 'FATAL') ? 'Carp::croak' : 'Carp::carp'),
	], $class,
    );

    ${^ENCODING} = $decoder;
    $^H{$class} = 1;
}

sub unimport {
    my $class = shift;
    $^H{$class} = undef;
    undef ${^ENCODING};
}

# Don't worry about source code literals.
sub cat_decode {
    my $self = shift;
    return $self->[LATIN1]->cat_decode(@_);
}

# Warn if the data is not purely US-ASCII.
sub decode {
    my $self = shift;

    DO_WARN: {
        if ($] >= 5.009004) {
            my $hints = (caller(0))[10];
            $hints->{ref($self)} or last DO_WARN;
        }

        local $@;
        my $rv = eval { $self->[ASCII]->decode($_[0], Encode::FB_CROAK()) };
        return $rv unless $@;

        require Carp;
        no strict 'refs';
        $self->[FATAL]->(
            "Bytes implicitly upgraded into wide characters as iso-8859-1"
        );

    }

    return $self->[LATIN1]->decode(@_);
}

sub name { 'iso-8859-1' }

1;

__END__

=head1 SEE ALSO

L<perlunicode>, L<perluniintro>

L<open>, L<utf8>, L<encoding>, L<Encode>

=head1 AUTHORS

Audrey Tang

=head1 COPYRIGHT

Copyright 2004, 2005, 2006, 2007 by Audrey Tang E<lt>cpan@audreyt.orgE<gt>.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut
