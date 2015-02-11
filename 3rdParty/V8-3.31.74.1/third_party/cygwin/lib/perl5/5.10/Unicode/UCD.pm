package Unicode::UCD;

use strict;
use warnings;

our $VERSION = '0.25';

use Storable qw(dclone);

require Exporter;

our @ISA = qw(Exporter);

our @EXPORT_OK = qw(charinfo
		    charblock charscript
		    charblocks charscripts
		    charinrange
		    general_categories bidi_types
		    compexcl
		    casefold casespec
		    namedseq);

use Carp;

=head1 NAME

Unicode::UCD - Unicode character database

=head1 SYNOPSIS

    use Unicode::UCD 'charinfo';
    my $charinfo   = charinfo($codepoint);

    use Unicode::UCD 'charblock';
    my $charblock  = charblock($codepoint);

    use Unicode::UCD 'charscript';
    my $charscript = charscript($codepoint);

    use Unicode::UCD 'charblocks';
    my $charblocks = charblocks();

    use Unicode::UCD 'charscripts';
    my $charscripts = charscripts();

    use Unicode::UCD qw(charscript charinrange);
    my $range = charscript($script);
    print "looks like $script\n" if charinrange($range, $codepoint);

    use Unicode::UCD qw(general_categories bidi_types);
    my $categories = general_categories();
    my $types = bidi_types();

    use Unicode::UCD 'compexcl';
    my $compexcl = compexcl($codepoint);

    use Unicode::UCD 'namedseq';
    my $namedseq = namedseq($named_sequence_name);

    my $unicode_version = Unicode::UCD::UnicodeVersion();

=head1 DESCRIPTION

The Unicode::UCD module offers a simple interface to the Unicode
Character Database.

=cut

my $UNICODEFH;
my $BLOCKSFH;
my $SCRIPTSFH;
my $VERSIONFH;
my $COMPEXCLFH;
my $CASEFOLDFH;
my $CASESPECFH;
my $NAMEDSEQFH;

sub openunicode {
    my ($rfh, @path) = @_;
    my $f;
    unless (defined $$rfh) {
	for my $d (@INC) {
	    use File::Spec;
	    $f = File::Spec->catfile($d, "unicore", @path);
	    last if open($$rfh, $f);
	    undef $f;
	}
	croak __PACKAGE__, ": failed to find ",
              File::Spec->catfile(@path), " in @INC"
	    unless defined $f;
    }
    return $f;
}

=head2 charinfo

    use Unicode::UCD 'charinfo';

    my $charinfo = charinfo(0x41);

charinfo() returns a reference to a hash that has the following fields
as defined by the Unicode standard:

    key

    code             code point with at least four hexdigits
    name             name of the character IN UPPER CASE
    category         general category of the character
    combining        classes used in the Canonical Ordering Algorithm
    bidi             bidirectional type
    decomposition    character decomposition mapping
    decimal          if decimal digit this is the integer numeric value
    digit            if digit this is the numeric value
    numeric          if numeric is the integer or rational numeric value
    mirrored         if mirrored in bidirectional text
    unicode10        Unicode 1.0 name if existed and different
    comment          ISO 10646 comment field
    upper            uppercase equivalent mapping
    lower            lowercase equivalent mapping
    title            titlecase equivalent mapping

    block            block the character belongs to (used in \p{In...})
    script           script the character belongs to

If no match is found, a reference to an empty hash is returned.

The C<block> property is the same as returned by charinfo().  It is
not defined in the Unicode Character Database proper (Chapter 4 of the
Unicode 3.0 Standard, aka TUS3) but instead in an auxiliary database
(Chapter 14 of TUS3).  Similarly for the C<script> property.

Note that you cannot do (de)composition and casing based solely on the
above C<decomposition> and C<lower>, C<upper>, C<title>, properties,
you will need also the compexcl(), casefold(), and casespec() functions.

=cut

# NB: This function is duplicated in charnames.pm
sub _getcode {
    my $arg = shift;

    if ($arg =~ /^[1-9]\d*$/) {
	return $arg;
    } elsif ($arg =~ /^(?:[Uu]\+|0[xX])?([[:xdigit:]]+)$/) {
	return hex($1);
    }

    return;
}

# Lingua::KO::Hangul::Util not part of the standard distribution
# but it will be used if available.

eval { require Lingua::KO::Hangul::Util };
my $hasHangulUtil = ! $@;
if ($hasHangulUtil) {
    Lingua::KO::Hangul::Util->import();
}

sub hangul_decomp { # internal: called from charinfo
    if ($hasHangulUtil) {
	my @tmp = decomposeHangul(shift);
	return sprintf("%04X %04X",      @tmp) if @tmp == 2;
	return sprintf("%04X %04X %04X", @tmp) if @tmp == 3;
    }
    return;
}

sub hangul_charname { # internal: called from charinfo
    return sprintf("HANGUL SYLLABLE-%04X", shift);
}

sub han_charname { # internal: called from charinfo
    return sprintf("CJK UNIFIED IDEOGRAPH-%04X", shift);
}

my @CharinfoRanges = (
# block name
# [ first, last, coderef to name, coderef to decompose ],
# CJK Ideographs Extension A
  [ 0x3400,   0x4DB5,   \&han_charname,   undef  ],
# CJK Ideographs
  [ 0x4E00,   0x9FA5,   \&han_charname,   undef  ],
# Hangul Syllables
  [ 0xAC00,   0xD7A3,   $hasHangulUtil ? \&getHangulName : \&hangul_charname,  \&hangul_decomp ],
# Non-Private Use High Surrogates
  [ 0xD800,   0xDB7F,   undef,   undef  ],
# Private Use High Surrogates
  [ 0xDB80,   0xDBFF,   undef,   undef  ],
# Low Surrogates
  [ 0xDC00,   0xDFFF,   undef,   undef  ],
# The Private Use Area
  [ 0xE000,   0xF8FF,   undef,   undef  ],
# CJK Ideographs Extension B
  [ 0x20000,  0x2A6D6,  \&han_charname,   undef  ],
# Plane 15 Private Use Area
  [ 0xF0000,  0xFFFFD,  undef,   undef  ],
# Plane 16 Private Use Area
  [ 0x100000, 0x10FFFD, undef,   undef  ],
);

sub charinfo {
    my $arg  = shift;
    my $code = _getcode($arg);
    croak __PACKAGE__, "::charinfo: unknown code '$arg'"
	unless defined $code;
    my $hexk = sprintf("%06X", $code);
    my($rcode,$rname,$rdec);
    foreach my $range (@CharinfoRanges){
      if ($range->[0] <= $code && $code <= $range->[1]) {
        $rcode = $hexk;
	$rcode =~ s/^0+//;
	$rcode =  sprintf("%04X", hex($rcode));
        $rname = $range->[2] ? $range->[2]->($code) : '';
        $rdec  = $range->[3] ? $range->[3]->($code) : '';
        $hexk  = sprintf("%06X", $range->[0]); # replace by the first
        last;
      }
    }
    openunicode(\$UNICODEFH, "UnicodeData.txt");
    if (defined $UNICODEFH) {
	use Search::Dict 1.02;
	if (look($UNICODEFH, "$hexk;", { xfrm => sub { $_[0] =~ /^([^;]+);(.+)/; sprintf "%06X;$2", hex($1) } } ) >= 0) {
	    my $line = <$UNICODEFH>;
	    return unless defined $line;
	    chomp $line;
	    my %prop;
	    @prop{qw(
		     code name category
		     combining bidi decomposition
		     decimal digit numeric
		     mirrored unicode10 comment
		     upper lower title
		    )} = split(/;/, $line, -1);
	    $hexk =~ s/^0+//;
	    $hexk =  sprintf("%04X", hex($hexk));
	    if ($prop{code} eq $hexk) {
		$prop{block}  = charblock($code);
		$prop{script} = charscript($code);
		if(defined $rname){
                    $prop{code} = $rcode;
                    $prop{name} = $rname;
                    $prop{decomposition} = $rdec;
                }
		return \%prop;
	    }
	}
    }
    return;
}

sub _search { # Binary search in a [[lo,hi,prop],[...],...] table.
    my ($table, $lo, $hi, $code) = @_;

    return if $lo > $hi;

    my $mid = int(($lo+$hi) / 2);

    if ($table->[$mid]->[0] < $code) {
	if ($table->[$mid]->[1] >= $code) {
	    return $table->[$mid]->[2];
	} else {
	    _search($table, $mid + 1, $hi, $code);
	}
    } elsif ($table->[$mid]->[0] > $code) {
	_search($table, $lo, $mid - 1, $code);
    } else {
	return $table->[$mid]->[2];
    }
}

sub charinrange {
    my ($range, $arg) = @_;
    my $code = _getcode($arg);
    croak __PACKAGE__, "::charinrange: unknown code '$arg'"
	unless defined $code;
    _search($range, 0, $#$range, $code);
}

=head2 charblock

    use Unicode::UCD 'charblock';

    my $charblock = charblock(0x41);
    my $charblock = charblock(1234);
    my $charblock = charblock("0x263a");
    my $charblock = charblock("U+263a");

    my $range     = charblock('Armenian');

With a B<code point argument> charblock() returns the I<block> the character
belongs to, e.g.  C<Basic Latin>.  Note that not all the character
positions within all blocks are defined.

See also L</Blocks versus Scripts>.

If supplied with an argument that can't be a code point, charblock() tries
to do the opposite and interpret the argument as a character block. The
return value is a I<range>: an anonymous list of lists that contain
I<start-of-range>, I<end-of-range> code point pairs. You can test whether
a code point is in a range using the L</charinrange> function. If the
argument is not a known character block, C<undef> is returned.

=cut

my @BLOCKS;
my %BLOCKS;

sub _charblocks {
    unless (@BLOCKS) {
	if (openunicode(\$BLOCKSFH, "Blocks.txt")) {
	    local $_;
	    while (<$BLOCKSFH>) {
		if (/^([0-9A-F]+)\.\.([0-9A-F]+);\s+(.+)/) {
		    my ($lo, $hi) = (hex($1), hex($2));
		    my $subrange = [ $lo, $hi, $3 ];
		    push @BLOCKS, $subrange;
		    push @{$BLOCKS{$3}}, $subrange;
		}
	    }
	    close($BLOCKSFH);
	}
    }
}

sub charblock {
    my $arg = shift;

    _charblocks() unless @BLOCKS;

    my $code = _getcode($arg);

    if (defined $code) {
	_search(\@BLOCKS, 0, $#BLOCKS, $code);
    } else {
	if (exists $BLOCKS{$arg}) {
	    return dclone $BLOCKS{$arg};
	} else {
	    return;
	}
    }
}

=head2 charscript

    use Unicode::UCD 'charscript';

    my $charscript = charscript(0x41);
    my $charscript = charscript(1234);
    my $charscript = charscript("U+263a");

    my $range      = charscript('Thai');

With a B<code point argument> charscript() returns the I<script> the
character belongs to, e.g.  C<Latin>, C<Greek>, C<Han>.

See also L</Blocks versus Scripts>.

If supplied with an argument that can't be a code point, charscript() tries
to do the opposite and interpret the argument as a character script. The
return value is a I<range>: an anonymous list of lists that contain
I<start-of-range>, I<end-of-range> code point pairs. You can test whether a
code point is in a range using the L</charinrange> function. If the
argument is not a known character script, C<undef> is returned.

=cut

my @SCRIPTS;
my %SCRIPTS;

sub _charscripts {
    unless (@SCRIPTS) {
	if (openunicode(\$SCRIPTSFH, "Scripts.txt")) {
	    local $_;
	    while (<$SCRIPTSFH>) {
		if (/^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s+;\s+(\w+)/) {
		    my ($lo, $hi) = (hex($1), $2 ? hex($2) : hex($1));
		    my $script = lc($3);
		    $script =~ s/\b(\w)/uc($1)/ge;
		    my $subrange = [ $lo, $hi, $script ];
		    push @SCRIPTS, $subrange;
		    push @{$SCRIPTS{$script}}, $subrange;
		}
	    }
	    close($SCRIPTSFH);
	    @SCRIPTS = sort { $a->[0] <=> $b->[0] } @SCRIPTS;
	}
    }
}

sub charscript {
    my $arg = shift;

    _charscripts() unless @SCRIPTS;

    my $code = _getcode($arg);

    if (defined $code) {
	_search(\@SCRIPTS, 0, $#SCRIPTS, $code);
    } else {
	if (exists $SCRIPTS{$arg}) {
	    return dclone $SCRIPTS{$arg};
	} else {
	    return;
	}
    }
}

=head2 charblocks

    use Unicode::UCD 'charblocks';

    my $charblocks = charblocks();

charblocks() returns a reference to a hash with the known block names
as the keys, and the code point ranges (see L</charblock>) as the values.

See also L</Blocks versus Scripts>.

=cut

sub charblocks {
    _charblocks() unless %BLOCKS;
    return dclone \%BLOCKS;
}

=head2 charscripts

    use Unicode::UCD 'charscripts';

    my $charscripts = charscripts();

charscripts() returns a reference to a hash with the known script
names as the keys, and the code point ranges (see L</charscript>) as
the values.

See also L</Blocks versus Scripts>.

=cut

sub charscripts {
    _charscripts() unless %SCRIPTS;
    return dclone \%SCRIPTS;
}

=head2 Blocks versus Scripts

The difference between a block and a script is that scripts are closer
to the linguistic notion of a set of characters required to present
languages, while block is more of an artifact of the Unicode character
numbering and separation into blocks of (mostly) 256 characters.

For example the Latin B<script> is spread over several B<blocks>, such
as C<Basic Latin>, C<Latin 1 Supplement>, C<Latin Extended-A>, and
C<Latin Extended-B>.  On the other hand, the Latin script does not
contain all the characters of the C<Basic Latin> block (also known as
the ASCII): it includes only the letters, and not, for example, the digits
or the punctuation.

For blocks see http://www.unicode.org/Public/UNIDATA/Blocks.txt

For scripts see UTR #24: http://www.unicode.org/unicode/reports/tr24/

=head2 Matching Scripts and Blocks

Scripts are matched with the regular-expression construct
C<\p{...}> (e.g. C<\p{Tibetan}> matches characters of the Tibetan script),
while C<\p{In...}> is used for blocks (e.g. C<\p{InTibetan}> matches
any of the 256 code points in the Tibetan block).

=head2 Code Point Arguments

A I<code point argument> is either a decimal or a hexadecimal scalar
designating a Unicode character, or C<U+> followed by hexadecimals
designating a Unicode character.  In other words, if you want a code
point to be interpreted as a hexadecimal number, you must prefix it
with either C<0x> or C<U+>, because a string like e.g. C<123> will
be interpreted as a decimal code point.  Also note that Unicode is
B<not> limited to 16 bits (the number of Unicode characters is
open-ended, in theory unlimited): you may have more than 4 hexdigits.

=head2 charinrange

In addition to using the C<\p{In...}> and C<\P{In...}> constructs, you
can also test whether a code point is in the I<range> as returned by
L</charblock> and L</charscript> or as the values of the hash returned
by L</charblocks> and L</charscripts> by using charinrange():

    use Unicode::UCD qw(charscript charinrange);

    $range = charscript('Hiragana');
    print "looks like hiragana\n" if charinrange($range, $codepoint);

=cut

my %GENERAL_CATEGORIES =
 (
    'L'  =>         'Letter',
    'LC' =>         'CasedLetter',
    'Lu' =>         'UppercaseLetter',
    'Ll' =>         'LowercaseLetter',
    'Lt' =>         'TitlecaseLetter',
    'Lm' =>         'ModifierLetter',
    'Lo' =>         'OtherLetter',
    'M'  =>         'Mark',
    'Mn' =>         'NonspacingMark',
    'Mc' =>         'SpacingMark',
    'Me' =>         'EnclosingMark',
    'N'  =>         'Number',
    'Nd' =>         'DecimalNumber',
    'Nl' =>         'LetterNumber',
    'No' =>         'OtherNumber',
    'P'  =>         'Punctuation',
    'Pc' =>         'ConnectorPunctuation',
    'Pd' =>         'DashPunctuation',
    'Ps' =>         'OpenPunctuation',
    'Pe' =>         'ClosePunctuation',
    'Pi' =>         'InitialPunctuation',
    'Pf' =>         'FinalPunctuation',
    'Po' =>         'OtherPunctuation',
    'S'  =>         'Symbol',
    'Sm' =>         'MathSymbol',
    'Sc' =>         'CurrencySymbol',
    'Sk' =>         'ModifierSymbol',
    'So' =>         'OtherSymbol',
    'Z'  =>         'Separator',
    'Zs' =>         'SpaceSeparator',
    'Zl' =>         'LineSeparator',
    'Zp' =>         'ParagraphSeparator',
    'C'  =>         'Other',
    'Cc' =>         'Control',
    'Cf' =>         'Format',
    'Cs' =>         'Surrogate',
    'Co' =>         'PrivateUse',
    'Cn' =>         'Unassigned',
 );

sub general_categories {
    return dclone \%GENERAL_CATEGORIES;
}

=head2 general_categories

    use Unicode::UCD 'general_categories';

    my $categories = general_categories();

The general_categories() returns a reference to a hash which has short
general category names (such as C<Lu>, C<Nd>, C<Zs>, C<S>) as keys and long
names (such as C<UppercaseLetter>, C<DecimalNumber>, C<SpaceSeparator>,
C<Symbol>) as values.  The hash is reversible in case you need to go
from the long names to the short names.  The general category is the
one returned from charinfo() under the C<category> key.

=cut

my %BIDI_TYPES =
 (
   'L'   => 'Left-to-Right',
   'LRE' => 'Left-to-Right Embedding',
   'LRO' => 'Left-to-Right Override',
   'R'   => 'Right-to-Left',
   'AL'  => 'Right-to-Left Arabic',
   'RLE' => 'Right-to-Left Embedding',
   'RLO' => 'Right-to-Left Override',
   'PDF' => 'Pop Directional Format',
   'EN'  => 'European Number',
   'ES'  => 'European Number Separator',
   'ET'  => 'European Number Terminator',
   'AN'  => 'Arabic Number',
   'CS'  => 'Common Number Separator',
   'NSM' => 'Non-Spacing Mark',
   'BN'  => 'Boundary Neutral',
   'B'   => 'Paragraph Separator',
   'S'   => 'Segment Separator',
   'WS'  => 'Whitespace',
   'ON'  => 'Other Neutrals',
 ); 

sub bidi_types {
    return dclone \%BIDI_TYPES;
}

=head2 bidi_types

    use Unicode::UCD 'bidi_types';

    my $categories = bidi_types();

The bidi_types() returns a reference to a hash which has the short
bidi (bidirectional) type names (such as C<L>, C<R>) as keys and long
names (such as C<Left-to-Right>, C<Right-to-Left>) as values.  The
hash is reversible in case you need to go from the long names to the
short names.  The bidi type is the one returned from charinfo()
under the C<bidi> key.  For the exact meaning of the various bidi classes
the Unicode TR9 is recommended reading:
http://www.unicode.org/reports/tr9/tr9-17.html
(as of Unicode 5.0.0)

=cut

=head2 compexcl

    use Unicode::UCD 'compexcl';

    my $compexcl = compexcl("09dc");

The compexcl() returns the composition exclusion (that is, if the
character should not be produced during a precomposition) of the 
character specified by a B<code point argument>.

If there is a composition exclusion for the character, true is
returned.  Otherwise, false is returned.

=cut

my %COMPEXCL;

sub _compexcl {
    unless (%COMPEXCL) {
	if (openunicode(\$COMPEXCLFH, "CompositionExclusions.txt")) {
	    local $_;
	    while (<$COMPEXCLFH>) {
		if (/^([0-9A-F]+)\s+\#\s+/) {
		    my $code = hex($1);
		    $COMPEXCL{$code} = undef;
		}
	    }
	    close($COMPEXCLFH);
	}
    }
}

sub compexcl {
    my $arg  = shift;
    my $code = _getcode($arg);
    croak __PACKAGE__, "::compexcl: unknown code '$arg'"
	unless defined $code;

    _compexcl() unless %COMPEXCL;

    return exists $COMPEXCL{$code};
}

=head2 casefold

    use Unicode::UCD 'casefold';

    my $casefold = casefold("00DF");

The casefold() returns the locale-independent case folding of the
character specified by a B<code point argument>.

If there is a case folding for that character, a reference to a hash
with the following fields is returned:

    key

    code             code point with at least four hexdigits
    status           "C", "F", "S", or "I"
    mapping          one or more codes separated by spaces

The meaning of the I<status> is as follows:

   C                 common case folding, common mappings shared
                     by both simple and full mappings
   F                 full case folding, mappings that cause strings
                     to grow in length. Multiple characters are separated
                     by spaces
   S                 simple case folding, mappings to single characters
                     where different from F
   I                 special case for dotted uppercase I and
                     dotless lowercase i
                     - If this mapping is included, the result is
                       case-insensitive, but dotless and dotted I's
                       are not distinguished
                     - If this mapping is excluded, the result is not
                       fully case-insensitive, but dotless and dotted
                       I's are distinguished

If there is no case folding for that character, C<undef> is returned.

For more information about case mappings see
http://www.unicode.org/unicode/reports/tr21/

=cut

my %CASEFOLD;

sub _casefold {
    unless (%CASEFOLD) {
	if (openunicode(\$CASEFOLDFH, "CaseFolding.txt")) {
	    local $_;
	    while (<$CASEFOLDFH>) {
		if (/^([0-9A-F]+); ([CFSI]); ([0-9A-F]+(?: [0-9A-F]+)*);/) {
		    my $code = hex($1);
		    $CASEFOLD{$code} = { code    => $1,
					 status  => $2,
					 mapping => $3 };
		}
	    }
	    close($CASEFOLDFH);
	}
    }
}

sub casefold {
    my $arg  = shift;
    my $code = _getcode($arg);
    croak __PACKAGE__, "::casefold: unknown code '$arg'"
	unless defined $code;

    _casefold() unless %CASEFOLD;

    return $CASEFOLD{$code};
}

=head2 casespec

    use Unicode::UCD 'casespec';

    my $casespec = casespec("FB00");

The casespec() returns the potentially locale-dependent case mapping
of the character specified by a B<code point argument>.  The mapping
may change the length of the string (which the basic Unicode case
mappings as returned by charinfo() never do).

If there is a case folding for that character, a reference to a hash
with the following fields is returned:

    key

    code             code point with at least four hexdigits
    lower            lowercase
    title            titlecase
    upper            uppercase
    condition        condition list (may be undef)

The C<condition> is optional.  Where present, it consists of one or
more I<locales> or I<contexts>, separated by spaces (other than as
used to separate elements, spaces are to be ignored).  A condition
list overrides the normal behavior if all of the listed conditions are
true.  Case distinctions in the condition list are not significant.
Conditions preceded by "NON_" represent the negation of the condition.

Note that when there are multiple case folding definitions for a
single code point because of different locales, the value returned by
casespec() is a hash reference which has the locales as the keys and
hash references as described above as the values.

A I<locale> is defined as a 2-letter ISO 3166 country code, possibly
followed by a "_" and a 2-letter ISO language code (possibly followed
by a "_" and a variant code).  You can find the lists of those codes,
see L<Locale::Country> and L<Locale::Language>.

A I<context> is one of the following choices:

    FINAL            The letter is not followed by a letter of
                     general category L (e.g. Ll, Lt, Lu, Lm, or Lo)
    MODERN           The mapping is only used for modern text
    AFTER_i          The last base character was "i" (U+0069)

For more information about case mappings see
http://www.unicode.org/unicode/reports/tr21/

=cut

my %CASESPEC;

sub _casespec {
    unless (%CASESPEC) {
	if (openunicode(\$CASESPECFH, "SpecialCasing.txt")) {
	    local $_;
	    while (<$CASESPECFH>) {
		if (/^([0-9A-F]+); ([0-9A-F]+(?: [0-9A-F]+)*)?; ([0-9A-F]+(?: [0-9A-F]+)*)?; ([0-9A-F]+(?: [0-9A-F]+)*)?; (\w+(?: \w+)*)?/) {
		    my ($hexcode, $lower, $title, $upper, $condition) =
			($1, $2, $3, $4, $5);
		    my $code = hex($hexcode);
		    if (exists $CASESPEC{$code}) {
			if (exists $CASESPEC{$code}->{code}) {
			    my ($oldlower,
				$oldtitle,
				$oldupper,
				$oldcondition) =
				    @{$CASESPEC{$code}}{qw(lower
							   title
							   upper
							   condition)};
			    if (defined $oldcondition) {
				my ($oldlocale) =
				($oldcondition =~ /^([a-z][a-z](?:_\S+)?)/);
				delete $CASESPEC{$code};
				$CASESPEC{$code}->{$oldlocale} =
				{ code      => $hexcode,
				  lower     => $oldlower,
				  title     => $oldtitle,
				  upper     => $oldupper,
				  condition => $oldcondition };
			    }
			}
			my ($locale) =
			    ($condition =~ /^([a-z][a-z](?:_\S+)?)/);
			$CASESPEC{$code}->{$locale} =
			{ code      => $hexcode,
			  lower     => $lower,
			  title     => $title,
			  upper     => $upper,
			  condition => $condition };
		    } else {
			$CASESPEC{$code} =
			{ code      => $hexcode,
			  lower     => $lower,
			  title     => $title,
			  upper     => $upper,
			  condition => $condition };
		    }
		}
	    }
	    close($CASESPECFH);
	}
    }
}

sub casespec {
    my $arg  = shift;
    my $code = _getcode($arg);
    croak __PACKAGE__, "::casespec: unknown code '$arg'"
	unless defined $code;

    _casespec() unless %CASESPEC;

    return ref $CASESPEC{$code} ? dclone $CASESPEC{$code} : $CASESPEC{$code};
}

=head2 namedseq()

    use Unicode::UCD 'namedseq';

    my $namedseq = namedseq("KATAKANA LETTER AINU P");
    my @namedseq = namedseq("KATAKANA LETTER AINU P");
    my %namedseq = namedseq();

If used with a single argument in a scalar context, returns the string
consisting of the code points of the named sequence, or C<undef> if no
named sequence by that name exists.  If used with a single argument in
a list context, returns list of the code points.  If used with no
arguments in a list context, returns a hash with the names of the
named sequences as the keys and the named sequences as strings as
the values.  Otherwise, returns C<undef> or empty list depending
on the context.

(New from Unicode 4.1.0)

=cut

my %NAMEDSEQ;

sub _namedseq {
    unless (%NAMEDSEQ) {
	if (openunicode(\$NAMEDSEQFH, "NamedSequences.txt")) {
	    local $_;
	    while (<$NAMEDSEQFH>) {
		if (/^(.+)\s*;\s*([0-9A-F]+(?: [0-9A-F]+)*)$/) {
		    my ($n, $s) = ($1, $2);
		    my @s = map { chr(hex($_)) } split(' ', $s);
		    $NAMEDSEQ{$n} = join("", @s);
		}
	    }
	    close($NAMEDSEQFH);
	}
    }
}

sub namedseq {
    _namedseq() unless %NAMEDSEQ;
    my $wantarray = wantarray();
    if (defined $wantarray) {
	if ($wantarray) {
	    if (@_ == 0) {
		return %NAMEDSEQ;
	    } elsif (@_ == 1) {
		my $s = $NAMEDSEQ{ $_[0] };
		return defined $s ? map { ord($_) } split('', $s) : ();
	    }
	} elsif (@_ == 1) {
	    return $NAMEDSEQ{ $_[0] };
	}
    }
    return;
}

=head2 Unicode::UCD::UnicodeVersion

Unicode::UCD::UnicodeVersion() returns the version of the Unicode
Character Database, in other words, the version of the Unicode
standard the database implements.  The version is a string
of numbers delimited by dots (C<'.'>).

=cut

my $UNICODEVERSION;

sub UnicodeVersion {
    unless (defined $UNICODEVERSION) {
	openunicode(\$VERSIONFH, "version");
	chomp($UNICODEVERSION = <$VERSIONFH>);
	close($VERSIONFH);
	croak __PACKAGE__, "::VERSION: strange version '$UNICODEVERSION'"
	    unless $UNICODEVERSION =~ /^\d+(?:\.\d+)+$/;
    }
    return $UNICODEVERSION;
}

=head2 Implementation Note

The first use of charinfo() opens a read-only filehandle to the Unicode
Character Database (the database is included in the Perl distribution).
The filehandle is then kept open for further queries.  In other words,
if you are wondering where one of your filehandles went, that's where.

=head1 BUGS

Does not yet support EBCDIC platforms.

=head1 AUTHOR

Jarkko Hietaniemi

=cut

1;
