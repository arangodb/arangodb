package locale;

our $VERSION = '1.00';

=head1 NAME

locale - Perl pragma to use and avoid POSIX locales for built-in operations

=head1 SYNOPSIS

    @x = sort @y;	# ASCII sorting order
    {
        use locale;
        @x = sort @y;   # Locale-defined sorting order
    }
    @x = sort @y;	# ASCII sorting order again

=head1 DESCRIPTION

This pragma tells the compiler to enable (or disable) the use of POSIX
locales for built-in operations (LC_CTYPE for regular expressions, and
LC_COLLATE for string comparison).  Each "use locale" or "no locale"
affects statements to the end of the enclosing BLOCK.

See L<perllocale> for more detailed information on how Perl supports
locales.

=cut

$locale::hint_bits = 0x4;

sub import {
    $^H |= $locale::hint_bits;
}

sub unimport {
    $^H &= ~$locale::hint_bits;
}

1;
