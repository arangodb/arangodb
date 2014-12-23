# $Id: encoding.pm,v 2.7 2008/03/12 09:51:11 dankogai Exp $
package encoding;
our $VERSION = '2.6_01';

use Encode;
use strict;
use warnings;

sub DEBUG () { 0 }

BEGIN {
    if ( ord("A") == 193 ) {
        require Carp;
        Carp::croak("encoding: pragma does not support EBCDIC platforms");
    }
}

our $HAS_PERLIO = 0;
eval { require PerlIO::encoding };
unless ($@) {
    $HAS_PERLIO = ( PerlIO::encoding->VERSION >= 0.02 );
}

sub _exception {
    my $name = shift;
    $] > 5.008 and return 0;    # 5.8.1 or higher then no
    my %utfs = map { $_ => 1 }
      qw(utf8 UCS-2BE UCS-2LE UTF-16 UTF-16BE UTF-16LE
      UTF-32 UTF-32BE UTF-32LE);
    $utfs{$name} or return 0;    # UTFs or no
    require Config;
    Config->import();
    our %Config;
    return $Config{perl_patchlevel} ? 0 : 1    # maintperl then no
}

sub in_locale { $^H & ( $locale::hint_bits || 0 ) }

sub _get_locale_encoding {
    my $locale_encoding;

    # I18N::Langinfo isn't available everywhere
    eval {
        require I18N::Langinfo;
        I18N::Langinfo->import(qw(langinfo CODESET));
        $locale_encoding = langinfo( CODESET() );
    };

    my $country_language;

    no warnings 'uninitialized';

    if ( (not $locale_encoding) && in_locale() ) {
        if ( $ENV{LC_ALL} =~ /^([^.]+)\.([^.@]+)(@.*)?$/ ) {
            ( $country_language, $locale_encoding ) = ( $1, $2 );
        }
        elsif ( $ENV{LANG} =~ /^([^.]+)\.([^.@]+)(@.*)?$/ ) {
            ( $country_language, $locale_encoding ) = ( $1, $2 );
        }

        # LANGUAGE affects only LC_MESSAGES only on glibc
    }
    elsif ( not $locale_encoding ) {
        if (   $ENV{LC_ALL} =~ /\butf-?8\b/i
            || $ENV{LANG} =~ /\butf-?8\b/i )
        {
            $locale_encoding = 'utf8';
        }

        # Could do more heuristics based on the country and language
        # parts of LC_ALL and LANG (the parts before the dot (if any)),
        # since we have Locale::Country and Locale::Language available.
        # TODO: get a database of Language -> Encoding mappings
        # (the Estonian database at http://www.eki.ee/letter/
        # would be excellent!) --jhi
    }
    if (   defined $locale_encoding
        && lc($locale_encoding) eq 'euc'
        && defined $country_language )
    {
        if ( $country_language =~ /^ja_JP|japan(?:ese)?$/i ) {
            $locale_encoding = 'euc-jp';
        }
        elsif ( $country_language =~ /^ko_KR|korean?$/i ) {
            $locale_encoding = 'euc-kr';
        }
        elsif ( $country_language =~ /^zh_CN|chin(?:a|ese)$/i ) {
            $locale_encoding = 'euc-cn';
        }
        elsif ( $country_language =~ /^zh_TW|taiwan(?:ese)?$/i ) {
            $locale_encoding = 'euc-tw';
        }
        else {
            require Carp;
            Carp::croak(
                "encoding: Locale encoding '$locale_encoding' too ambiguous"
            );
        }
    }

    return $locale_encoding;
}

sub import {
    my $class = shift;
    my $name  = shift;
    if ( $name eq ':_get_locale_encoding' ) {    # used by lib/open.pm
        my $caller = caller();
        {
            no strict 'refs';
            *{"${caller}::_get_locale_encoding"} = \&_get_locale_encoding;
        }
        return;
    }
    $name = _get_locale_encoding() if $name eq ':locale';
    my %arg = @_;
    $name = $ENV{PERL_ENCODING} unless defined $name;
    my $enc = find_encoding($name);
    unless ( defined $enc ) {
        require Carp;
        Carp::croak("encoding: Unknown encoding '$name'");
    }
    $name = $enc->name;    # canonize
    unless ( $arg{Filter} ) {
        DEBUG and warn "_exception($name) = ", _exception($name);
        _exception($name) or ${^ENCODING} = $enc;
        $HAS_PERLIO or return 1;
    }
    else {
        defined( ${^ENCODING} ) and undef ${^ENCODING};

        # implicitly 'use utf8'
        require utf8;      # to fetch $utf8::hint_bits;
        $^H |= $utf8::hint_bits;
        eval {
            require Filter::Util::Call;
            Filter::Util::Call->import;
            filter_add(
                sub {
                    my $status = filter_read();
                    if ( $status > 0 ) {
                        $_ = $enc->decode( $_, 1 );
                        DEBUG and warn $_;
                    }
                    $status;
                }
            );
        };
        $@ eq '' and DEBUG and warn "Filter installed";
    }
    defined ${^UNICODE} and ${^UNICODE} != 0 and return 1;
    for my $h (qw(STDIN STDOUT)) {
        if ( $arg{$h} ) {
            unless ( defined find_encoding( $arg{$h} ) ) {
                require Carp;
                Carp::croak(
                    "encoding: Unknown encoding for $h, '$arg{$h}'");
            }
            eval { binmode( $h, ":raw :encoding($arg{$h})" ) };
        }
        else {
            unless ( exists $arg{$h} ) {
                eval {
                    no warnings 'uninitialized';
                    binmode( $h, ":raw :encoding($name)" );
                };
            }
        }
        if ($@) {
            require Carp;
            Carp::croak($@);
        }
    }
    return 1;    # I doubt if we need it, though
}

sub unimport {
    no warnings;
    undef ${^ENCODING};
    if ($HAS_PERLIO) {
        binmode( STDIN,  ":raw" );
        binmode( STDOUT, ":raw" );
    }
    else {
        binmode(STDIN);
        binmode(STDOUT);
    }
    if ( $INC{"Filter/Util/Call.pm"} ) {
        eval { filter_del() };
    }
}

1;
__END__

=pod

=head1 NAME

encoding - allows you to write your script in non-ascii or non-utf8

=head1 SYNOPSIS

  use encoding "greek";  # Perl like Greek to you?
  use encoding "euc-jp"; # Jperl!

  # or you can even do this if your shell supports your native encoding

  perl -Mencoding=latin2 -e '...' # Feeling centrally European?
  perl -Mencoding=euc-kr -e '...' # Or Korean?

  # more control

  # A simple euc-cn => utf-8 converter
  use encoding "euc-cn", STDOUT => "utf8";  while(<>){print};

  # "no encoding;" supported (but not scoped!)
  no encoding;

  # an alternate way, Filter
  use encoding "euc-jp", Filter=>1;
  # now you can use kanji identifiers -- in euc-jp!

  # switch on locale -
  # note that this probably means that unless you have a complete control
  # over the environments the application is ever going to be run, you should
  # NOT use the feature of encoding pragma allowing you to write your script
  # in any recognized encoding because changing locale settings will wreck
  # the script; you can of course still use the other features of the pragma.
  use encoding ':locale';

=head1 ABSTRACT

Let's start with a bit of history: Perl 5.6.0 introduced Unicode
support.  You could apply C<substr()> and regexes even to complex CJK
characters -- so long as the script was written in UTF-8.  But back
then, text editors that supported UTF-8 were still rare and many users
instead chose to write scripts in legacy encodings, giving up a whole
new feature of Perl 5.6.

Rewind to the future: starting from perl 5.8.0 with the B<encoding>
pragma, you can write your script in any encoding you like (so long
as the C<Encode> module supports it) and still enjoy Unicode support.
This pragma achieves that by doing the following:

=over

=item *

Internally converts all literals (C<q//,qq//,qr//,qw///, qx//>) from
the encoding specified to utf8.  In Perl 5.8.1 and later, literals in
C<tr///> and C<DATA> pseudo-filehandle are also converted.

=item *

Changing PerlIO layers of C<STDIN> and C<STDOUT> to the encoding
 specified.

=back

=head2 Literal Conversions

You can write code in EUC-JP as follows:

  my $Rakuda = "\xF1\xD1\xF1\xCC"; # Camel in Kanji
               #<-char-><-char->   # 4 octets
  s/\bCamel\b/$Rakuda/;

And with C<use encoding "euc-jp"> in effect, it is the same thing as
the code in UTF-8:

  my $Rakuda = "\x{99F1}\x{99DD}"; # two Unicode Characters
  s/\bCamel\b/$Rakuda/;

=head2 PerlIO layers for C<STD(IN|OUT)>

The B<encoding> pragma also modifies the filehandle layers of
STDIN and STDOUT to the specified encoding.  Therefore,

  use encoding "euc-jp";
  my $message = "Camel is the symbol of perl.\n";
  my $Rakuda = "\xF1\xD1\xF1\xCC"; # Camel in Kanji
  $message =~ s/\bCamel\b/$Rakuda/;
  print $message;

Will print "\xF1\xD1\xF1\xCC is the symbol of perl.\n",
not "\x{99F1}\x{99DD} is the symbol of perl.\n".

You can override this by giving extra arguments; see below.

=head2 Implicit upgrading for byte strings

By default, if strings operating under byte semantics and strings
with Unicode character data are concatenated, the new string will
be created by decoding the byte strings as I<ISO 8859-1 (Latin-1)>.

The B<encoding> pragma changes this to use the specified encoding
instead.  For example:

    use encoding 'utf8';
    my $string = chr(20000); # a Unicode string
    utf8::encode($string);   # now it's a UTF-8 encoded byte string
    # concatenate with another Unicode string
    print length($string . chr(20000));

Will print C<2>, because C<$string> is upgraded as UTF-8.  Without
C<use encoding 'utf8';>, it will print C<4> instead, since C<$string>
is three octets when interpreted as Latin-1.

=head2 Side effects

If the C<encoding> pragma is in scope then the lengths returned are
calculated from the length of C<$/> in Unicode characters, which is not
always the same as the length of C<$/> in the native encoding.

This pragma affects utf8::upgrade, but not utf8::downgrade.

=head1 FEATURES THAT REQUIRE 5.8.1

Some of the features offered by this pragma requires perl 5.8.1.  Most
of these are done by Inaba Hiroto.  Any other features and changes
are good for 5.8.0.

=over

=item "NON-EUC" doublebyte encodings

Because perl needs to parse script before applying this pragma, such
encodings as Shift_JIS and Big-5 that may contain '\' (BACKSLASH;
\x5c) in the second byte fails because the second byte may
accidentally escape the quoting character that follows.  Perl 5.8.1
or later fixes this problem.

=item tr// 

C<tr//> was overlooked by Perl 5 porters when they released perl 5.8.0
See the section below for details.

=item DATA pseudo-filehandle

Another feature that was overlooked was C<DATA>. 

=back

=head1 USAGE

=over 4

=item use encoding [I<ENCNAME>] ;

Sets the script encoding to I<ENCNAME>.  And unless ${^UNICODE} 
exists and non-zero, PerlIO layers of STDIN and STDOUT are set to
":encoding(I<ENCNAME>)".

Note that STDERR WILL NOT be changed.

Also note that non-STD file handles remain unaffected.  Use C<use
open> or C<binmode> to change layers of those.

If no encoding is specified, the environment variable L<PERL_ENCODING>
is consulted.  If no encoding can be found, the error C<Unknown encoding
'I<ENCNAME>'> will be thrown.

=item use encoding I<ENCNAME> [ STDIN =E<gt> I<ENCNAME_IN> ...] ;

You can also individually set encodings of STDIN and STDOUT via the
C<< STDIN => I<ENCNAME> >> form.  In this case, you cannot omit the
first I<ENCNAME>.  C<< STDIN => undef >> turns the IO transcoding
completely off.

When ${^UNICODE} exists and non-zero, these options will completely
ignored.  ${^UNICODE} is a variable introduced in perl 5.8.1.  See
L<perlrun> see L<perlvar/"${^UNICODE}"> and L<perlrun/"-C"> for
details (perl 5.8.1 and later).

=item use encoding I<ENCNAME> Filter=E<gt>1;

This turns the encoding pragma into a source filter.  While the
default approach just decodes interpolated literals (in qq() and
qr()), this will apply a source filter to the entire source code.  See
L</"The Filter Option"> below for details.

=item no encoding;

Unsets the script encoding. The layers of STDIN, STDOUT are
reset to ":raw" (the default unprocessed raw stream of bytes).

=back

=head1 The Filter Option

The magic of C<use encoding> is not applied to the names of
identifiers.  In order to make C<${"\x{4eba}"}++> ($human++, where human
is a single Han ideograph) work, you still need to write your script
in UTF-8 -- or use a source filter.  That's what 'Filter=>1' does.

What does this mean?  Your source code behaves as if it is written in
UTF-8 with 'use utf8' in effect.  So even if your editor only supports
Shift_JIS, for example, you can still try examples in Chapter 15 of
C<Programming Perl, 3rd Ed.>.  For instance, you can use UTF-8
identifiers.

This option is significantly slower and (as of this writing) non-ASCII
identifiers are not very stable WITHOUT this option and with the
source code written in UTF-8.

=head2 Filter-related changes at Encode version 1.87

=over

=item *

The Filter option now sets STDIN and STDOUT like non-filter options.
And C<< STDIN=>I<ENCODING> >> and C<< STDOUT=>I<ENCODING> >> work like
non-filter version.

=item *

C<use utf8> is implicitly declared so you no longer have to C<use
utf8> to C<${"\x{4eba}"}++>.

=back

=head1 CAVEATS

=head2 NOT SCOPED

The pragma is a per script, not a per block lexical.  Only the last
C<use encoding> or C<no encoding> matters, and it affects 
B<the whole script>.  However, the <no encoding> pragma is supported and 
B<use encoding> can appear as many times as you want in a given script. 
The multiple use of this pragma is discouraged.

By the same reason, the use this pragma inside modules is also
discouraged (though not as strongly discouraged as the case above.  
See below).

If you still have to write a module with this pragma, be very careful
of the load order.  See the codes below;

  # called module
  package Module_IN_BAR;
  use encoding "bar";
  # stuff in "bar" encoding here
  1;

  # caller script
  use encoding "foo"
  use Module_IN_BAR;
  # surprise! use encoding "bar" is in effect.

The best way to avoid this oddity is to use this pragma RIGHT AFTER
other modules are loaded.  i.e.

  use Module_IN_BAR;
  use encoding "foo";

=head2 DO NOT MIX MULTIPLE ENCODINGS

Notice that only literals (string or regular expression) having only
legacy code points are affected: if you mix data like this

    \xDF\x{100}

the data is assumed to be in (Latin 1 and) Unicode, not in your native
encoding.  In other words, this will match in "greek":

    "\xDF" =~ /\x{3af}/

but this will not

    "\xDF\x{100}" =~ /\x{3af}\x{100}/

since the C<\xDF> (ISO 8859-7 GREEK SMALL LETTER IOTA WITH TONOS) on
the left will B<not> be upgraded to C<\x{3af}> (Unicode GREEK SMALL
LETTER IOTA WITH TONOS) because of the C<\x{100}> on the left.  You
should not be mixing your legacy data and Unicode in the same string.

This pragma also affects encoding of the 0x80..0xFF code point range:
normally characters in that range are left as eight-bit bytes (unless
they are combined with characters with code points 0x100 or larger,
in which case all characters need to become UTF-8 encoded), but if
the C<encoding> pragma is present, even the 0x80..0xFF range always
gets UTF-8 encoded.

After all, the best thing about this pragma is that you don't have to
resort to \x{....} just to spell your name in a native encoding.
So feel free to put your strings in your encoding in quotes and
regexes.

=head2 tr/// with ranges

The B<encoding> pragma works by decoding string literals in
C<q//,qq//,qr//,qw///, qx//> and so forth.  In perl 5.8.0, this
does not apply to C<tr///>.  Therefore,

  use encoding 'euc-jp';
  #....
  $kana =~ tr/\xA4\xA1-\xA4\xF3/\xA5\xA1-\xA5\xF3/;
  #           -------- -------- -------- --------

Does not work as

  $kana =~ tr/\x{3041}-\x{3093}/\x{30a1}-\x{30f3}/;

=over

=item Legend of characters above

  utf8     euc-jp   charnames::viacode()
  -----------------------------------------
  \x{3041} \xA4\xA1 HIRAGANA LETTER SMALL A
  \x{3093} \xA4\xF3 HIRAGANA LETTER N
  \x{30a1} \xA5\xA1 KATAKANA LETTER SMALL A
  \x{30f3} \xA5\xF3 KATAKANA LETTER N

=back

This counterintuitive behavior has been fixed in perl 5.8.1.

=head3 workaround to tr///;

In perl 5.8.0, you can work around as follows;

  use encoding 'euc-jp';
  #  ....
  eval qq{ \$kana =~ tr/\xA4\xA1-\xA4\xF3/\xA5\xA1-\xA5\xF3/ };

Note the C<tr//> expression is surrounded by C<qq{}>.  The idea behind
is the same as classic idiom that makes C<tr///> 'interpolate'.

   tr/$from/$to/;            # wrong!
   eval qq{ tr/$from/$to/ }; # workaround.

Nevertheless, in case of B<encoding> pragma even C<q//> is affected so
C<tr///> not being decoded was obviously against the will of Perl5
Porters so it has been fixed in Perl 5.8.1 or later.

=head1 EXAMPLE - Greekperl

    use encoding "iso 8859-7";

    # \xDF in ISO 8859-7 (Greek) is \x{3af} in Unicode.

    $a = "\xDF";
    $b = "\x{100}";

    printf "%#x\n", ord($a); # will print 0x3af, not 0xdf

    $c = $a . $b;

    # $c will be "\x{3af}\x{100}", not "\x{df}\x{100}".

    # chr() is affected, and ...

    print "mega\n"  if ord(chr(0xdf)) == 0x3af;

    # ... ord() is affected by the encoding pragma ...

    print "tera\n" if ord(pack("C", 0xdf)) == 0x3af;

    # ... as are eq and cmp ...

    print "peta\n" if "\x{3af}" eq  pack("C", 0xdf);
    print "exa\n"  if "\x{3af}" cmp pack("C", 0xdf) == 0;

    # ... but pack/unpack C are not affected, in case you still
    # want to go back to your native encoding

    print "zetta\n" if unpack("C", (pack("C", 0xdf))) == 0xdf;

=head1 KNOWN PROBLEMS

=over

=item literals in regex that are longer than 127 bytes

For native multibyte encodings (either fixed or variable length),
the current implementation of the regular expressions may introduce
recoding errors for regular expression literals longer than 127 bytes.

=item EBCDIC

The encoding pragma is not supported on EBCDIC platforms.
(Porters who are willing and able to remove this limitation are
welcome.)

=item format

This pragma doesn't work well with format because PerlIO does not
get along very well with it.  When format contains non-ascii
characters it prints funny or gets "wide character warnings".
To understand it, try the code below.

  # Save this one in utf8
  # replace *non-ascii* with a non-ascii string
  my $camel;
  format STDOUT =
  *non-ascii*@>>>>>>>
  $camel
  .
  $camel = "*non-ascii*";
  binmode(STDOUT=>':encoding(utf8)'); # bang!
  write;              # funny 
  print $camel, "\n"; # fine

Without binmode this happens to work but without binmode, print()
fails instead of write().

At any rate, the very use of format is questionable when it comes to
unicode characters since you have to consider such things as character
width (i.e. double-width for ideographs) and directions (i.e. BIDI for
Arabic and Hebrew).

=item Thread safety

C<use encoding ...> is not thread-safe (i.e., do not use in threaded
applications).

=back

=head2 The Logic of :locale

The logic of C<:locale> is as follows:

=over 4

=item 1.

If the platform supports the langinfo(CODESET) interface, the codeset
returned is used as the default encoding for the open pragma.

=item 2.

If 1. didn't work but we are under the locale pragma, the environment
variables LC_ALL and LANG (in that order) are matched for encodings
(the part after C<.>, if any), and if any found, that is used 
as the default encoding for the open pragma.

=item 3.

If 1. and 2. didn't work, the environment variables LC_ALL and LANG
(in that order) are matched for anything looking like UTF-8, and if
any found, C<:utf8> is used as the default encoding for the open
pragma.

=back

If your locale environment variables (LC_ALL, LC_CTYPE, LANG)
contain the strings 'UTF-8' or 'UTF8' (case-insensitive matching),
the default encoding of your STDIN, STDOUT, and STDERR, and of
B<any subsequent file open>, is UTF-8.

=head1 HISTORY

This pragma first appeared in Perl 5.8.0.  For features that require 
5.8.1 and better, see above.

The C<:locale> subpragma was implemented in 2.01, or Perl 5.8.6.

=head1 SEE ALSO

L<perlunicode>, L<Encode>, L<open>, L<Filter::Util::Call>,

Ch. 15 of C<Programming Perl (3rd Edition)>
by Larry Wall, Tom Christiansen, Jon Orwant;
O'Reilly & Associates; ISBN 0-596-00027-8

=cut
