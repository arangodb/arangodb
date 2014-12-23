package Unicode::Normalize;

BEGIN {
    unless ("A" eq pack('U', 0x41)) {
	die "Unicode::Normalize cannot stringify a Unicode code point\n";
    }
}

use 5.006;
use strict;
use warnings;
use Carp;

no warnings 'utf8';

our $VERSION = '1.02';
our $PACKAGE = __PACKAGE__;

require Exporter;
require DynaLoader;

our @ISA = qw(Exporter DynaLoader);
our @EXPORT = qw( NFC NFD NFKC NFKD );
our @EXPORT_OK = qw(
    normalize decompose reorder compose
    checkNFD checkNFKD checkNFC checkNFKC check
    getCanon getCompat getComposite getCombinClass
    isExclusion isSingleton isNonStDecomp isComp2nd isComp_Ex
    isNFD_NO isNFC_NO isNFC_MAYBE isNFKD_NO isNFKC_NO isNFKC_MAYBE
    FCD checkFCD FCC checkFCC composeContiguous
    splitOnLastStarter
);
our %EXPORT_TAGS = (
    all       => [ @EXPORT, @EXPORT_OK ],
    normalize => [ @EXPORT, qw/normalize decompose reorder compose/ ],
    check     => [ qw/checkNFD checkNFKD checkNFC checkNFKC check/ ],
    fast      => [ qw/FCD checkFCD FCC checkFCC composeContiguous/ ],
);

######

bootstrap Unicode::Normalize $VERSION;

######

##
## utilites for tests
##

sub pack_U {
    return pack('U*', @_);
}

sub unpack_U {
    return unpack('U*', shift(@_).pack('U*'));
}


##
## normalization forms
##

sub FCD ($) {
    my $str = shift;
    return checkFCD($str) ? $str : NFD($str);
}

our %formNorm = (
    NFC  => \&NFC,	C  => \&NFC,
    NFD  => \&NFD,	D  => \&NFD,
    NFKC => \&NFKC,	KC => \&NFKC,
    NFKD => \&NFKD,	KD => \&NFKD,
    FCD  => \&FCD,	FCC => \&FCC,
);

sub normalize($$)
{
    my $form = shift;
    my $str = shift;
    if (exists $formNorm{$form}) {
	return $formNorm{$form}->($str);
    }
    croak($PACKAGE."::normalize: invalid form name: $form");
}


##
## quick check
##

our %formCheck = (
    NFC  => \&checkNFC, 	C  => \&checkNFC,
    NFD  => \&checkNFD, 	D  => \&checkNFD,
    NFKC => \&checkNFKC,	KC => \&checkNFKC,
    NFKD => \&checkNFKD,	KD => \&checkNFKD,
    FCD  => \&checkFCD, 	FCC => \&checkFCC,
);

sub check($$)
{
    my $form = shift;
    my $str = shift;
    if (exists $formCheck{$form}) {
	return $formCheck{$form}->($str);
    }
    croak($PACKAGE."::check: invalid form name: $form");
}

1;
__END__

=head1 NAME

Unicode::Normalize - Unicode Normalization Forms

=head1 SYNOPSIS

(1) using function names exported by default:

  use Unicode::Normalize;

  $NFD_string  = NFD($string);  # Normalization Form D
  $NFC_string  = NFC($string);  # Normalization Form C
  $NFKD_string = NFKD($string); # Normalization Form KD
  $NFKC_string = NFKC($string); # Normalization Form KC

(2) using function names exported on request:

  use Unicode::Normalize 'normalize';

  $NFD_string  = normalize('D',  $string);  # Normalization Form D
  $NFC_string  = normalize('C',  $string);  # Normalization Form C
  $NFKD_string = normalize('KD', $string);  # Normalization Form KD
  $NFKC_string = normalize('KC', $string);  # Normalization Form KC

=head1 DESCRIPTION

Parameters:

C<$string> is used as a string under character semantics (see F<perlunicode>).

C<$code_point> should be an unsigned integer representing a Unicode code point.

Note: Between XSUB and pure Perl, there is an incompatibility
about the interpretation of C<$code_point> as a decimal number.
XSUB converts C<$code_point> to an unsigned integer, but pure Perl does not.
Do not use a floating point nor a negative sign in C<$code_point>.

=head2 Normalization Forms

=over 4

=item C<$NFD_string = NFD($string)>

It returns the Normalization Form D (formed by canonical decomposition).

=item C<$NFC_string = NFC($string)>

It returns the Normalization Form C (formed by canonical decomposition
followed by canonical composition).

=item C<$NFKD_string = NFKD($string)>

It returns the Normalization Form KD (formed by compatibility decomposition).

=item C<$NFKC_string = NFKC($string)>

It returns the Normalization Form KC (formed by compatibility decomposition
followed by B<canonical> composition).

=item C<$FCD_string = FCD($string)>

If the given string is in FCD ("Fast C or D" form; cf. UTN #5),
it returns the string without modification; otherwise it returns an FCD string.

Note: FCD is not always unique, then plural forms may be equivalent
each other. C<FCD()> will return one of these equivalent forms.

=item C<$FCC_string = FCC($string)>

It returns the FCC form ("Fast C Contiguous"; cf. UTN #5).

Note: FCC is unique, as well as four normalization forms (NF*).

=item C<$normalized_string = normalize($form_name, $string)>

It returns the normalization form of C<$form_name>.

As C<$form_name>, one of the following names must be given.

  'C'  or 'NFC'  for Normalization Form C  (UAX #15)
  'D'  or 'NFD'  for Normalization Form D  (UAX #15)
  'KC' or 'NFKC' for Normalization Form KC (UAX #15)
  'KD' or 'NFKD' for Normalization Form KD (UAX #15)

  'FCD'          for "Fast C or D" Form  (UTN #5)
  'FCC'          for "Fast C Contiguous" (UTN #5)

=back

=head2 Decomposition and Composition

=over 4

=item C<$decomposed_string = decompose($string [, $useCompatMapping])>

It returns the concatenation of the decomposition of each character
in the string.

If the second parameter (a boolean) is omitted or false,
the decomposition is canonical decomposition;
if the second parameter (a boolean) is true,
the decomposition is compatibility decomposition.

The string returned is not always in NFD/NFKD. Reordering may be required.

    $NFD_string  = reorder(decompose($string));       # eq. to NFD()
    $NFKD_string = reorder(decompose($string, TRUE)); # eq. to NFKD()

=item C<$reordered_string = reorder($string)>

It returns the result of reordering the combining characters
according to Canonical Ordering Behavior.

For example, when you have a list of NFD/NFKD strings,
you can get the concatenated NFD/NFKD string from them, by saying

    $concat_NFD  = reorder(join '', @NFD_strings);
    $concat_NFKD = reorder(join '', @NFKD_strings);

=item C<$composed_string = compose($string)>

It returns the result of canonical composition
without applying any decomposition.

For example, when you have a NFD/NFKD string,
you can get its NFC/NFKC string, by saying

    $NFC_string  = compose($NFD_string);
    $NFKC_string = compose($NFKD_string);

=back

=head2 Quick Check

(see Annex 8, UAX #15; and F<DerivedNormalizationProps.txt>)

The following functions check whether the string is in that normalization form.

The result returned will be one of the following:

    YES     The string is in that normalization form.
    NO      The string is not in that normalization form.
    MAYBE   Dubious. Maybe yes, maybe no.

=over 4

=item C<$result = checkNFD($string)>

It returns true (C<1>) if C<YES>; false (C<empty string>) if C<NO>.

=item C<$result = checkNFC($string)>

It returns true (C<1>) if C<YES>; false (C<empty string>) if C<NO>;
C<undef> if C<MAYBE>.

=item C<$result = checkNFKD($string)>

It returns true (C<1>) if C<YES>; false (C<empty string>) if C<NO>.

=item C<$result = checkNFKC($string)>

It returns true (C<1>) if C<YES>; false (C<empty string>) if C<NO>;
C<undef> if C<MAYBE>.

=item C<$result = checkFCD($string)>

It returns true (C<1>) if C<YES>; false (C<empty string>) if C<NO>.

=item C<$result = checkFCC($string)>

It returns true (C<1>) if C<YES>; false (C<empty string>) if C<NO>;
C<undef> if C<MAYBE>.

Note: If a string is not in FCD, it must not be in FCC.
So C<checkFCC($not_FCD_string)> should return C<NO>.

=item C<$result = check($form_name, $string)>

It returns true (C<1>) if C<YES>; false (C<empty string>) if C<NO>;
C<undef> if C<MAYBE>.

As C<$form_name>, one of the following names must be given.

  'C'  or 'NFC'  for Normalization Form C  (UAX #15)
  'D'  or 'NFD'  for Normalization Form D  (UAX #15)
  'KC' or 'NFKC' for Normalization Form KC (UAX #15)
  'KD' or 'NFKD' for Normalization Form KD (UAX #15)

  'FCD'          for "Fast C or D" Form  (UTN #5)
  'FCC'          for "Fast C Contiguous" (UTN #5)

=back

B<Note>

In the cases of NFD, NFKD, and FCD, the answer must be
either C<YES> or C<NO>. The answer C<MAYBE> may be returned
in the cases of NFC, NFKC, and FCC.

A C<MAYBE> string should contain at least one combining character
or the like. For example, C<COMBINING ACUTE ACCENT> has
the MAYBE_NFC/MAYBE_NFKC property.

Both C<checkNFC("A\N{COMBINING ACUTE ACCENT}")>
and C<checkNFC("B\N{COMBINING ACUTE ACCENT}")> will return C<MAYBE>.
C<"A\N{COMBINING ACUTE ACCENT}"> is not in NFC
(its NFC is C<"\N{LATIN CAPITAL LETTER A WITH ACUTE}">),
while C<"B\N{COMBINING ACUTE ACCENT}"> is in NFC.

If you want to check exactly, compare the string with its NFC/NFKC/FCC.

    if ($string eq NFC($string)) {
	# $string is exactly normalized in NFC;
    } else {
	# $string is not normalized in NFC;
    }

    if ($string eq NFKC($string)) {
	# $string is exactly normalized in NFKC;
    } else {
	# $string is not normalized in NFKC;
    }

=head2 Character Data

These functions are interface of character data used internally.
If you want only to get Unicode normalization forms, you don't need
call them yourself.

=over 4

=item C<$canonical_decomposition = getCanon($code_point)>

If the character is canonically decomposable (including Hangul Syllables),
it returns the (full) canonical decomposition as a string.
Otherwise it returns C<undef>.

B<Note:> According to the Unicode standard, the canonical decomposition
of the character that is not canonically decomposable is same as
the character itself.

=item C<$compatibility_decomposition = getCompat($code_point)>

If the character is compatibility decomposable (including Hangul Syllables),
it returns the (full) compatibility decomposition as a string.
Otherwise it returns C<undef>.

B<Note:> According to the Unicode standard, the compatibility decomposition
of the character that is not compatibility decomposable is same as
the character itself.

=item C<$code_point_composite = getComposite($code_point_here, $code_point_next)>

If two characters here and next (as code points) are composable
(including Hangul Jamo/Syllables and Composition Exclusions),
it returns the code point of the composite.

If they are not composable, it returns C<undef>.

=item C<$combining_class = getCombinClass($code_point)>

It returns the combining class (as an integer) of the character.

=item C<$may_be_composed_with_prev_char = isComp2nd($code_point)>

It returns a boolean whether the character of the specified codepoint
may be composed with the previous one in a certain composition
(including Hangul Compositions, but excluding
Composition Exclusions and Non-Starter Decompositions).

=item C<$is_exclusion = isExclusion($code_point)>

It returns a boolean whether the code point is a composition exclusion.

=item C<$is_singleton = isSingleton($code_point)>

It returns a boolean whether the code point is a singleton

=item C<$is_non_starter_decomposition = isNonStDecomp($code_point)>

It returns a boolean whether the code point has Non-Starter Decomposition.

=item C<$is_Full_Composition_Exclusion = isComp_Ex($code_point)>

It returns a boolean of the derived property Comp_Ex
(Full_Composition_Exclusion). This property is generated from
Composition Exclusions + Singletons + Non-Starter Decompositions.

=item C<$NFD_is_NO = isNFD_NO($code_point)>

It returns a boolean of the derived property NFD_NO
(NFD_Quick_Check=No).

=item C<$NFC_is_NO = isNFC_NO($code_point)>

It returns a boolean of the derived property NFC_NO
(NFC_Quick_Check=No).

=item C<$NFC_is_MAYBE = isNFC_MAYBE($code_point)>

It returns a boolean of the derived property NFC_MAYBE
(NFC_Quick_Check=Maybe).

=item C<$NFKD_is_NO = isNFKD_NO($code_point)>

It returns a boolean of the derived property NFKD_NO
(NFKD_Quick_Check=No).

=item C<$NFKC_is_NO = isNFKC_NO($code_point)>

It returns a boolean of the derived property NFKC_NO
(NFKC_Quick_Check=No).

=item C<$NFKC_is_MAYBE = isNFKC_MAYBE($code_point)>

It returns a boolean of the derived property NFKC_MAYBE
(NFKC_Quick_Check=Maybe).

=back

=head1 EXPORT

C<NFC>, C<NFD>, C<NFKC>, C<NFKD>: by default.

C<normalize> and other some functions: on request.

=head1 CAVEATS

=over 4

=item Perl's version vs. Unicode version

Since this module refers to perl core's Unicode database in the directory
F</lib/unicore> (or formerly F</lib/unicode>), the Unicode version of
normalization implemented by this module depends on your perl's version.

    perl's version     implemented Unicode version
       5.6.1              3.0.1
       5.7.2              3.1.0
       5.7.3              3.1.1 (normalization is same as 3.1.0)
       5.8.0              3.2.0
     5.8.1-5.8.3          4.0.0
     5.8.4-5.8.6          4.0.1 (normalization is same as 4.0.0)
     5.8.7-5.8.8          4.1.0

=item Correction of decomposition mapping

In older Unicode versions, a small number of characters (all of which are
CJK compatibility ideographs as far as they have been found) may have
an erroneous decomposition mapping (see F<NormalizationCorrections.txt>).
Anyhow, this module will neither refer to F<NormalizationCorrections.txt>
nor provide any specific version of normalization. Therefore this module
running on an older perl with an older Unicode database may use
the erroneous decomposition mapping blindly conforming to the Unicode database.

=item Revised definition of canonical composition

In Unicode 4.1.0, the definition D2 of canonical composition (which
affects NFC and NFKC) has been changed (see Public Review Issue #29
and recent UAX #15). This module has used the newer definition
since the version 0.07 (Oct 31, 2001).
This module will not support the normalization according to the older
definition, even if the Unicode version implemented by perl is
lower than 4.1.0.

=back

=head1 AUTHOR

SADAHIRO Tomoyuki <SADAHIRO@cpan.org>

Copyright(C) 2001-2007, SADAHIRO Tomoyuki. Japan. All rights reserved.

This module is free software; you can redistribute it
and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

=over 4

=item http://www.unicode.org/reports/tr15/

Unicode Normalization Forms - UAX #15

=item http://www.unicode.org/Public/UNIDATA/CompositionExclusions.txt

Composition Exclusion Table

=item http://www.unicode.org/Public/UNIDATA/DerivedNormalizationProps.txt

Derived Normalization Properties

=item http://www.unicode.org/Public/UNIDATA/NormalizationCorrections.txt

Normalization Corrections

=item http://www.unicode.org/review/pr-29.html

Public Review Issue #29: Normalization Issue

=item http://www.unicode.org/notes/tn5/

Canonical Equivalence in Applications - UTN #5

=back

=cut
