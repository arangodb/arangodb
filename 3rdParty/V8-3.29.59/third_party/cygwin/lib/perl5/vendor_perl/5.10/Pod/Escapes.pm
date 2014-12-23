
require 5;
#                        The documentation is at the end.
# Time-stamp: "2004-05-07 15:31:25 ADT"
package Pod::Escapes;
require Exporter;
@ISA = ('Exporter');
$VERSION = '1.04';
@EXPORT_OK = qw(
  %Code2USASCII
  %Name2character
  %Name2character_number
  %Latin1Code_to_fallback
  %Latin1Char_to_fallback
  e2char
  e2charnum
);
%EXPORT_TAGS = ('ALL' => \@EXPORT_OK);

#==========================================================================

use strict;
use vars qw(
  %Code2USASCII
  %Name2character
  %Name2character_number
  %Latin1Code_to_fallback
  %Latin1Char_to_fallback
  $FAR_CHAR
  $FAR_CHAR_NUMBER
  $NOT_ASCII
);

$FAR_CHAR = "?" unless defined $FAR_CHAR;
$FAR_CHAR_NUMBER = ord($FAR_CHAR) unless defined $FAR_CHAR_NUMBER;

$NOT_ASCII = 'A' ne chr(65) unless defined $NOT_ASCII;

#--------------------------------------------------------------------------
sub e2char {
  my $in = $_[0];
  return undef unless defined $in and length $in;
  
  # Convert to decimal:
  if($in =~ m/^(0[0-7]*)$/s ) {
    $in = oct $in;
  } elsif($in =~ m/^0?x([0-9a-fA-F]+)$/s ) {
    $in = hex $1;
  } # else it's decimal, or named

  if($NOT_ASCII) {
    # We're in bizarro world of not-ASCII!
    # Cope with US-ASCII codes, use fallbacks for Latin-1, or use FAR_CHAR.
    unless($in =~ m/^\d+$/s) {
      # It's a named character reference.  Get its numeric Unicode value.
      $in = $Name2character{$in};
      return undef unless defined $in;  # (if there's no such name)
      $in = ord $in; # (All ents must be one character long.)
        # ...So $in holds the char's US-ASCII numeric value, which we'll
        #  now go get the local equivalent for.
    }

    # It's numeric, whether by origin or by mutation from a known name
    return $Code2USASCII{$in} # so "65" => "A" everywhere
        || $Latin1Code_to_fallback{$in} # Fallback.
        || $FAR_CHAR; # Fall further back
  }
  
  # Normal handling:
  if($in =~ m/^\d+$/s) {
    if($] < 5.007  and  $in > 255) { # can't be trusted with Unicode
      return $FAR_CHAR;
    } else {
      return chr($in);
    }
  } else {
    return $Name2character{$in}; # returns undef if unknown
  }
}

#--------------------------------------------------------------------------
sub e2charnum {
  my $in = $_[0];
  return undef unless defined $in and length $in;
  
  # Convert to decimal:
  if($in =~ m/^(0[0-7]*)$/s ) {
    $in = oct $in;
  } elsif($in =~ m/^0?x([0-9a-fA-F]+)$/s ) {
    $in = hex $1;
  } # else it's decimal, or named

  if($in =~ m/^\d+$/s) {
    return 0 + $in;
  } else {
    return $Name2character_number{$in}; # returns undef if unknown
  }
}

#--------------------------------------------------------------------------

%Name2character_number = (
 # General XML/XHTML:
 'lt'   => 60,
 'gt'   => 62,
 'quot' => 34,
 'amp'  => 38,
 'apos' => 39,

 # POD-specific:
 'sol'    => 47,
 'verbar' => 124,

 'lchevron' => 171, # legacy for laquo
 'rchevron' => 187, # legacy for raquo

 # Remember, grave looks like \ (as in virtu\)
 #           acute looks like / (as in re/sume/)
 #           circumflex looks like ^ (as in papier ma^che/)
 #           umlaut/dieresis looks like " (as in nai"ve, Chloe")

 # From the XHTML 1 .ent files:
 'nbsp'     , 160,
 'iexcl'    , 161,
 'cent'     , 162,
 'pound'    , 163,
 'curren'   , 164,
 'yen'      , 165,
 'brvbar'   , 166,
 'sect'     , 167,
 'uml'      , 168,
 'copy'     , 169,
 'ordf'     , 170,
 'laquo'    , 171,
 'not'      , 172,
 'shy'      , 173,
 'reg'      , 174,
 'macr'     , 175,
 'deg'      , 176,
 'plusmn'   , 177,
 'sup2'     , 178,
 'sup3'     , 179,
 'acute'    , 180,
 'micro'    , 181,
 'para'     , 182,
 'middot'   , 183,
 'cedil'    , 184,
 'sup1'     , 185,
 'ordm'     , 186,
 'raquo'    , 187,
 'frac14'   , 188,
 'frac12'   , 189,
 'frac34'   , 190,
 'iquest'   , 191,
 'Agrave'   , 192,
 'Aacute'   , 193,
 'Acirc'    , 194,
 'Atilde'   , 195,
 'Auml'     , 196,
 'Aring'    , 197,
 'AElig'    , 198,
 'Ccedil'   , 199,
 'Egrave'   , 200,
 'Eacute'   , 201,
 'Ecirc'    , 202,
 'Euml'     , 203,
 'Igrave'   , 204,
 'Iacute'   , 205,
 'Icirc'    , 206,
 'Iuml'     , 207,
 'ETH'      , 208,
 'Ntilde'   , 209,
 'Ograve'   , 210,
 'Oacute'   , 211,
 'Ocirc'    , 212,
 'Otilde'   , 213,
 'Ouml'     , 214,
 'times'    , 215,
 'Oslash'   , 216,
 'Ugrave'   , 217,
 'Uacute'   , 218,
 'Ucirc'    , 219,
 'Uuml'     , 220,
 'Yacute'   , 221,
 'THORN'    , 222,
 'szlig'    , 223,
 'agrave'   , 224,
 'aacute'   , 225,
 'acirc'    , 226,
 'atilde'   , 227,
 'auml'     , 228,
 'aring'    , 229,
 'aelig'    , 230,
 'ccedil'   , 231,
 'egrave'   , 232,
 'eacute'   , 233,
 'ecirc'    , 234,
 'euml'     , 235,
 'igrave'   , 236,
 'iacute'   , 237,
 'icirc'    , 238,
 'iuml'     , 239,
 'eth'      , 240,
 'ntilde'   , 241,
 'ograve'   , 242,
 'oacute'   , 243,
 'ocirc'    , 244,
 'otilde'   , 245,
 'ouml'     , 246,
 'divide'   , 247,
 'oslash'   , 248,
 'ugrave'   , 249,
 'uacute'   , 250,
 'ucirc'    , 251,
 'uuml'     , 252,
 'yacute'   , 253,
 'thorn'    , 254,
 'yuml'     , 255,

 'fnof'     , 402,
 'Alpha'    , 913,
 'Beta'     , 914,
 'Gamma'    , 915,
 'Delta'    , 916,
 'Epsilon'  , 917,
 'Zeta'     , 918,
 'Eta'      , 919,
 'Theta'    , 920,
 'Iota'     , 921,
 'Kappa'    , 922,
 'Lambda'   , 923,
 'Mu'       , 924,
 'Nu'       , 925,
 'Xi'       , 926,
 'Omicron'  , 927,
 'Pi'       , 928,
 'Rho'      , 929,
 'Sigma'    , 931,
 'Tau'      , 932,
 'Upsilon'  , 933,
 'Phi'      , 934,
 'Chi'      , 935,
 'Psi'      , 936,
 'Omega'    , 937,
 'alpha'    , 945,
 'beta'     , 946,
 'gamma'    , 947,
 'delta'    , 948,
 'epsilon'  , 949,
 'zeta'     , 950,
 'eta'      , 951,
 'theta'    , 952,
 'iota'     , 953,
 'kappa'    , 954,
 'lambda'   , 955,
 'mu'       , 956,
 'nu'       , 957,
 'xi'       , 958,
 'omicron'  , 959,
 'pi'       , 960,
 'rho'      , 961,
 'sigmaf'   , 962,
 'sigma'    , 963,
 'tau'      , 964,
 'upsilon'  , 965,
 'phi'      , 966,
 'chi'      , 967,
 'psi'      , 968,
 'omega'    , 969,
 'thetasym' , 977,
 'upsih'    , 978,
 'piv'      , 982,
 'bull'     , 8226,
 'hellip'   , 8230,
 'prime'    , 8242,
 'Prime'    , 8243,
 'oline'    , 8254,
 'frasl'    , 8260,
 'weierp'   , 8472,
 'image'    , 8465,
 'real'     , 8476,
 'trade'    , 8482,
 'alefsym'  , 8501,
 'larr'     , 8592,
 'uarr'     , 8593,
 'rarr'     , 8594,
 'darr'     , 8595,
 'harr'     , 8596,
 'crarr'    , 8629,
 'lArr'     , 8656,
 'uArr'     , 8657,
 'rArr'     , 8658,
 'dArr'     , 8659,
 'hArr'     , 8660,
 'forall'   , 8704,
 'part'     , 8706,
 'exist'    , 8707,
 'empty'    , 8709,
 'nabla'    , 8711,
 'isin'     , 8712,
 'notin'    , 8713,
 'ni'       , 8715,
 'prod'     , 8719,
 'sum'      , 8721,
 'minus'    , 8722,
 'lowast'   , 8727,
 'radic'    , 8730,
 'prop'     , 8733,
 'infin'    , 8734,
 'ang'      , 8736,
 'and'      , 8743,
 'or'       , 8744,
 'cap'      , 8745,
 'cup'      , 8746,
 'int'      , 8747,
 'there4'   , 8756,
 'sim'      , 8764,
 'cong'     , 8773,
 'asymp'    , 8776,
 'ne'       , 8800,
 'equiv'    , 8801,
 'le'       , 8804,
 'ge'       , 8805,
 'sub'      , 8834,
 'sup'      , 8835,
 'nsub'     , 8836,
 'sube'     , 8838,
 'supe'     , 8839,
 'oplus'    , 8853,
 'otimes'   , 8855,
 'perp'     , 8869,
 'sdot'     , 8901,
 'lceil'    , 8968,
 'rceil'    , 8969,
 'lfloor'   , 8970,
 'rfloor'   , 8971,
 'lang'     , 9001,
 'rang'     , 9002,
 'loz'      , 9674,
 'spades'   , 9824,
 'clubs'    , 9827,
 'hearts'   , 9829,
 'diams'    , 9830,
 'OElig'    , 338,
 'oelig'    , 339,
 'Scaron'   , 352,
 'scaron'   , 353,
 'Yuml'     , 376,
 'circ'     , 710,
 'tilde'    , 732,
 'ensp'     , 8194,
 'emsp'     , 8195,
 'thinsp'   , 8201,
 'zwnj'     , 8204,
 'zwj'      , 8205,
 'lrm'      , 8206,
 'rlm'      , 8207,
 'ndash'    , 8211,
 'mdash'    , 8212,
 'lsquo'    , 8216,
 'rsquo'    , 8217,
 'sbquo'    , 8218,
 'ldquo'    , 8220,
 'rdquo'    , 8221,
 'bdquo'    , 8222,
 'dagger'   , 8224,
 'Dagger'   , 8225,
 'permil'   , 8240,
 'lsaquo'   , 8249,
 'rsaquo'   , 8250,
 'euro'     , 8364,
);


# Fill out %Name2character...
{
  %Name2character = ();
  my($name, $number);
  while( ($name, $number) = each %Name2character_number) {
    if($] < 5.007  and  $number > 255) {
      $Name2character{$name} = $FAR_CHAR;
      # substitute for Unicode characters, for perls
      #  that can't reliable handle them
    } else {
      $Name2character{$name} = chr $number;
      # normal case
    }
  }
  # So they resolve 'right' even in EBCDIC-land
  $Name2character{'lt'  }   = '<';
  $Name2character{'gt'  }   = '>';
  $Name2character{'quot'}   = '"';
  $Name2character{'amp' }   = '&';
  $Name2character{'apos'}   = "'";
  $Name2character{'sol' }   = '/';
  $Name2character{'verbar'} = '|';
}

#--------------------------------------------------------------------------

%Code2USASCII = (
# mostly generated by
#  perl -e "printf qq{  \x25 3s, '\x25s',\n}, $_, chr($_) foreach (32 .. 126)"
   32, ' ',
   33, '!',
   34, '"',
   35, '#',
   36, '$',
   37, '%',
   38, '&',
   39, "'", #!
   40, '(',
   41, ')',
   42, '*',
   43, '+',
   44, ',',
   45, '-',
   46, '.',
   47, '/',
   48, '0',
   49, '1',
   50, '2',
   51, '3',
   52, '4',
   53, '5',
   54, '6',
   55, '7',
   56, '8',
   57, '9',
   58, ':',
   59, ';',
   60, '<',
   61, '=',
   62, '>',
   63, '?',
   64, '@',
   65, 'A',
   66, 'B',
   67, 'C',
   68, 'D',
   69, 'E',
   70, 'F',
   71, 'G',
   72, 'H',
   73, 'I',
   74, 'J',
   75, 'K',
   76, 'L',
   77, 'M',
   78, 'N',
   79, 'O',
   80, 'P',
   81, 'Q',
   82, 'R',
   83, 'S',
   84, 'T',
   85, 'U',
   86, 'V',
   87, 'W',
   88, 'X',
   89, 'Y',
   90, 'Z',
   91, '[',
   92, "\\", #!
   93, ']',
   94, '^',
   95, '_',
   96, '`',
   97, 'a',
   98, 'b',
   99, 'c',
  100, 'd',
  101, 'e',
  102, 'f',
  103, 'g',
  104, 'h',
  105, 'i',
  106, 'j',
  107, 'k',
  108, 'l',
  109, 'm',
  110, 'n',
  111, 'o',
  112, 'p',
  113, 'q',
  114, 'r',
  115, 's',
  116, 't',
  117, 'u',
  118, 'v',
  119, 'w',
  120, 'x',
  121, 'y',
  122, 'z',
  123, '{',
  124, '|',
  125, '}',
  126, '~',
);

#--------------------------------------------------------------------------

%Latin1Code_to_fallback = ();
@Latin1Code_to_fallback{0xA0 .. 0xFF} = (
# Copied from Text/Unidecode/x00.pm:

' ', qq{!}, qq{C/}, 'PS', qq{\$?}, qq{Y=}, qq{|}, 'SS', qq{"}, qq{(c)}, 'a', qq{<<}, qq{!}, "", qq{(r)}, qq{-},
'deg', qq{+-}, '2', '3', qq{'}, 'u', 'P', qq{*}, qq{,}, '1', 'o', qq{>>}, qq{1/4}, qq{1/2}, qq{3/4}, qq{?},
'A', 'A', 'A', 'A', 'A', 'A', 'AE', 'C', 'E', 'E', 'E', 'E', 'I', 'I', 'I', 'I',
'D', 'N', 'O', 'O', 'O', 'O', 'O', 'x', 'O', 'U', 'U', 'U', 'U', 'U', 'Th', 'ss',
'a', 'a', 'a', 'a', 'a', 'a', 'ae', 'c', 'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',
'd', 'n', 'o', 'o', 'o', 'o', 'o', qq{/}, 'o', 'u', 'u', 'u', 'u', 'y', 'th', 'y',

);

{
  # Now stuff %Latin1Char_to_fallback:
  %Latin1Char_to_fallback = ();
  my($k,$v);
  while( ($k,$v) = each %Latin1Code_to_fallback) {
    $Latin1Char_to_fallback{chr $k} = $v;
    #print chr($k), ' => ', $v, "\n";
  }
}

#--------------------------------------------------------------------------
1;
__END__

=head1 NAME

Pod::Escapes -- for resolving Pod EE<lt>...E<gt> sequences

=head1 SYNOPSIS

  use Pod::Escapes qw(e2char);
  ...la la la, parsing POD, la la la...
  $text = e2char($e_node->label);
  unless(defined $text) {
    print "Unknown E sequence \"", $e_node->label, "\"!";
  }
  ...else print/interpolate $text...

=head1 DESCRIPTION

This module provides things that are useful in decoding
Pod EE<lt>...E<gt> sequences.  Presumably, it should be used
only by Pod parsers and/or formatters.

By default, Pod::Escapes exports none of its symbols.  But
you can request any of them to be exported.
Either request them individually, as with
C<use Pod::Escapes qw(symbolname symbolname2...);>,
or you can do C<use Pod::Escapes qw(:ALL);> to get all
exportable symbols.

=head1 GOODIES

=over

=item e2char($e_content)

Given a name or number that could appear in a
C<EE<lt>name_or_numE<gt>> sequence, this returns the string that
it stands for.  For example, C<e2char('sol')>, C<e2char('47')>,
C<e2char('0x2F')>, and C<e2char('057')> all return "/",
because C<EE<lt>solE<gt>>, C<EE<lt>47E<gt>>, C<EE<lt>0x2fE<gt>>,
and C<EE<lt>057E<gt>>, all mean "/".  If
the name has no known value (as with a name of "qacute") or is
syntactally invalid (as with a name of "1/4"), this returns undef.

=item e2charnum($e_content)

Given a name or number that could appear in a
C<EE<lt>name_or_numE<gt>> sequence, this returns the number of
the Unicode character that this stands for.  For example,
C<e2char('sol')>, C<e2char('47')>,
C<e2char('0x2F')>, and C<e2char('057')> all return 47,
because C<EE<lt>solE<gt>>, C<EE<lt>47E<gt>>, C<EE<lt>0x2fE<gt>>,
and C<EE<lt>057E<gt>>, all mean "/", whose Unicode number is 47.  If
the name has no known value (as with a name of "qacute") or is
syntactally invalid (as with a name of "1/4"), this returns undef.

=item $Name2character{I<name>}

Maps from names (as in C<EE<lt>I<name>E<gt>>) like "eacute" or "sol"
to the string that each stands for.  Note that this does not
include numerics (like "64" or "x981c").  Under old Perl versions
(before 5.7) you get a "?" in place of characters whose Unicode
value is over 255.

=item $Name2character_number{I<name>}

Maps from names (as in C<EE<lt>I<name>E<gt>>) like "eacute" or "sol"
to the Unicode value that each stands for.  For example,
C<$Name2character_number{'eacute'}> is 201, and
C<$Name2character_number{'eacute'}> is 8364.  You get the correct
Unicode value, regardless of the version of Perl you're using --
which differs from C<%Name2character>'s behavior under pre-5.7 Perls.

Note that this hash does not
include numerics (like "64" or "x981c").

=item $Latin1Code_to_fallback{I<integer>}

For numbers in the range 160 (0x00A0) to 255 (0x00FF), this maps
from the character code for a Latin-1 character (like 233 for
lowercase e-acute) to the US-ASCII character that best aproximates
it (like "e").  You may find this useful if you are rendering
POD in a format that you think deals well only with US-ASCII
characters.

=item $Latin1Char_to_fallback{I<character>}

Just as above, but maps from characters (like "\xE9", 
lowercase e-acute) to characters (like "e").

=item $Code2USASCII{I<integer>}

This maps from US-ASCII codes (like 32) to the corresponding
character (like space, for 32).  Only characters 32 to 126 are
defined.  This is meant for use by C<e2char($x)> when it senses
that it's running on a non-ASCII platform (where chr(32) doesn't
get you a space -- but $Code2USASCII{32} will).  It's
documented here just in case you might find it useful.

=back

=head1 CAVEATS

On Perl versions before 5.7, Unicode characters with a value
over 255 (like lambda or emdash) can't be conveyed.  This
module does work under such early Perl versions, but in the
place of each such character, you get a "?".  Latin-1
characters (characters 160-255) are unaffected.

Under EBCDIC platforms, C<e2char($n)> may not always be the
same as C<chr(e2charnum($n))>, and ditto for
C<$Name2character{$name}> and
C<chr($Name2character_number{$name})>.

=head1 SEE ALSO

L<perlpod|perlpod>

L<perlpodspec|perlpodspec>

L<Text::Unidecode|Text::Unidecode>

=head1 COPYRIGHT AND DISCLAIMERS

Copyright (c) 2001-2004 Sean M. Burke.  All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

This program is distributed in the hope that it will be useful, but
without any warranty; without even the implied warranty of
merchantability or fitness for a particular purpose.

Portions of the data tables in this module are derived from the
entity declarations in the W3C XHTML specification.

Currently (October 2001), that's these three:

 http://www.w3.org/TR/xhtml1/DTD/xhtml-lat1.ent
 http://www.w3.org/TR/xhtml1/DTD/xhtml-special.ent
 http://www.w3.org/TR/xhtml1/DTD/xhtml-symbol.ent

=head1 AUTHOR

Sean M. Burke C<sburke@cpan.org>

=cut

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# What I used for reading the XHTML .ent files:

use strict;
my(@norms, @good, @bad);
my $dir = 'c:/sgml/docbook/';
my %escapes;
foreach my $file (qw(
  xhtml-symbol.ent
  xhtml-lat1.ent
  xhtml-special.ent
)) {
  open(IN, "<$dir$file") or die "can't read-open $dir$file: $!";
  print "Reading $file...\n";
  while(<IN>) {
    if(m/<!ENTITY\s+(\S+)\s+"&#([^;]+);">/) {
      my($name, $value) = ($1,$2);
      next if $name eq 'quot' or $name eq 'apos' or $name eq 'gt';
    
      $value = hex $1 if $value =~ m/^x([a-fA-F0-9]+)$/s;
      print "ILLEGAL VALUE $value" unless $value =~ m/^\d+$/s;
      if($value > 255) {
        push @good , sprintf "   %-10s , chr(%s),\n", "'$name'", $value;
        push @bad  , sprintf "   %-10s , \$bad,\n", "'$name'", $value;
      } else {
        push @norms, sprintf " %-10s , chr(%s),\n", "'$name'", $value;
      }
    } elsif(m/<!ENT/) {
      print "# Skipping $_";
    }
  
  }
  close(IN);
}

print @norms;
print "\n ( \$] .= 5.006001 ? (\n";
print @good;
print " ) : (\n";
print @bad;
print " )\n);\n";

__END__
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


