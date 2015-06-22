package Pod::LaTeX;

=head1 NAME

Pod::LaTeX - Convert Pod data to formatted Latex

=head1 SYNOPSIS

  use Pod::LaTeX;
  my $parser = Pod::LaTeX->new ( );

  $parser->parse_from_filehandle;

  $parser->parse_from_file ('file.pod', 'file.tex');

=head1 DESCRIPTION

C<Pod::LaTeX> is a module to convert documentation in the Pod format
into Latex. The L<B<pod2latex>|pod2latex> X<pod2latex> command uses
this module for translation.

C<Pod::LaTeX> is a derived class from L<Pod::Select|Pod::Select>.

=cut


use strict;
require Pod::ParseUtils;
use base qw/ Pod::Select /;

# use Data::Dumper; # for debugging
use Carp;

use vars qw/ $VERSION %HTML_Escapes @LatexSections /;

$VERSION = '0.58';

# Definitions of =headN -> latex mapping
@LatexSections = (qw/
		  chapter
		  section
		  subsection
		  subsubsection
		  paragraph
		  subparagraph
		  /);

# Standard escape sequences converted to Latex.
# The Unicode name of each character is given in the comments.
# Complete LaTeX set added by Peter Acklam.

%HTML_Escapes = (
     'sol'    => '\textfractionsolidus{}',  # xxx - or should it be just '/'
     'verbar' => '|',

     # The stuff below is based on the information available at
     # http://www.w3.org/TR/html401/sgml/entities.html

     # All characters in the range 0xA0-0xFF of the ISO 8859-1 character set.
     # Several of these characters require the `textcomp' LaTeX package.
     'nbsp'   => q|~|,                     # 0xA0 - no-break space = non-breaking space
     'iexcl'  => q|\textexclamdown{}|,     # 0xA1 - inverted exclamation mark
     'cent'   => q|\textcent{}|,           # 0xA2 - cent sign
     'pound'  => q|\textsterling{}|,       # 0xA3 - pound sign
     'curren' => q|\textcurrency{}|,       # 0xA4 - currency sign
     'yen'    => q|\textyen{}|,            # 0xA5 - yen sign = yuan sign
     'brvbar' => q|\textbrokenbar{}|,      # 0xA6 - broken bar = broken vertical bar
     'sect'   => q|\textsection{}|,        # 0xA7 - section sign
     'uml'    => q|\textasciidieresis{}|,  # 0xA8 - diaeresis = spacing diaeresis
     'copy'   => q|\textcopyright{}|,      # 0xA9 - copyright sign
     'ordf'   => q|\textordfeminine{}|,    # 0xAA - feminine ordinal indicator
     'laquo'  => q|\guillemotleft{}|,      # 0xAB - left-pointing double angle quotation mark = left pointing guillemet
     'not'    => q|\textlnot{}|,           # 0xAC - not sign
     'shy'    => q|\-|,                    # 0xAD - soft hyphen = discretionary hyphen
     'reg'    => q|\textregistered{}|,     # 0xAE - registered sign = registered trade mark sign
     'macr'   => q|\textasciimacron{}|,    # 0xAF - macron = spacing macron = overline = APL overbar
     'deg'    => q|\textdegree{}|,         # 0xB0 - degree sign
     'plusmn' => q|\textpm{}|,             # 0xB1 - plus-minus sign = plus-or-minus sign
     'sup2'   => q|\texttwosuperior{}|,    # 0xB2 - superscript two = superscript digit two = squared
     'sup3'   => q|\textthreesuperior{}|,  # 0xB3 - superscript three = superscript digit three = cubed
     'acute'  => q|\textasciiacute{}|,     # 0xB4 - acute accent = spacing acute
     'micro'  => q|\textmu{}|,             # 0xB5 - micro sign
     'para'   => q|\textparagraph{}|,      # 0xB6 - pilcrow sign = paragraph sign
     'middot' => q|\textperiodcentered{}|, # 0xB7 - middle dot = Georgian comma = Greek middle dot
     'cedil'  => q|\c{}|,                  # 0xB8 - cedilla = spacing cedilla
     'sup1'   => q|\textonesuperior{}|,    # 0xB9 - superscript one = superscript digit one
     'ordm'   => q|\textordmasculine{}|,   # 0xBA - masculine ordinal indicator
     'raquo'  => q|\guillemotright{}|,     # 0xBB - right-pointing double angle quotation mark = right pointing guillemet
     'frac14' => q|\textonequarter{}|,     # 0xBC - vulgar fraction one quarter = fraction one quarter
     'frac12' => q|\textonehalf{}|,        # 0xBD - vulgar fraction one half = fraction one half
     'frac34' => q|\textthreequarters{}|,  # 0xBE - vulgar fraction three quarters = fraction three quarters
     'iquest' => q|\textquestiondown{}|,   # 0xBF - inverted question mark = turned question mark
     'Agrave' => q|\`A|,                   # 0xC0 - latin capital letter A with grave = latin capital letter A grave
     'Aacute' => q|\'A|,             # 0xC1 - latin capital letter A with acute
     'Acirc'  => q|\^A|,             # 0xC2 - latin capital letter A with circumflex
     'Atilde' => q|\~A|,             # 0xC3 - latin capital letter A with tilde
     'Auml'   => q|\"A|,             # 0xC4 - latin capital letter A with diaeresis
     'Aring'  => q|\AA{}|,           # 0xC5 - latin capital letter A with ring above = latin capital letter A ring
     'AElig'  => q|\AE{}|,           # 0xC6 - latin capital letter AE = latin capital ligature AE
     'Ccedil' => q|\c{C}|,           # 0xC7 - latin capital letter C with cedilla
     'Egrave' => q|\`E|,             # 0xC8 - latin capital letter E with grave
     'Eacute' => q|\'E|,             # 0xC9 - latin capital letter E with acute
     'Ecirc'  => q|\^E|,             # 0xCA - latin capital letter E with circumflex
     'Euml'   => q|\"E|,             # 0xCB - latin capital letter E with diaeresis
     'Igrave' => q|\`I|,             # 0xCC - latin capital letter I with grave
     'Iacute' => q|\'I|,             # 0xCD - latin capital letter I with acute
     'Icirc'  => q|\^I|,             # 0xCE - latin capital letter I with circumflex
     'Iuml'   => q|\"I|,             # 0xCF - latin capital letter I with diaeresis
     'ETH'    => q|\DH{}|,           # 0xD0 - latin capital letter ETH
     'Ntilde' => q|\~N|,             # 0xD1 - latin capital letter N with tilde
     'Ograve' => q|\`O|,             # 0xD2 - latin capital letter O with grave
     'Oacute' => q|\'O|,             # 0xD3 - latin capital letter O with acute
     'Ocirc'  => q|\^O|,             # 0xD4 - latin capital letter O with circumflex
     'Otilde' => q|\~O|,             # 0xD5 - latin capital letter O with tilde
     'Ouml'   => q|\"O|,             # 0xD6 - latin capital letter O with diaeresis
     'times'  => q|\texttimes{}|,    # 0xD7 - multiplication sign
     'Oslash' => q|\O{}|,            # 0xD8 - latin capital letter O with stroke = latin capital letter O slash
     'Ugrave' => q|\`U|,             # 0xD9 - latin capital letter U with grave
     'Uacute' => q|\'U|,             # 0xDA - latin capital letter U with acute
     'Ucirc'  => q|\^U|,             # 0xDB - latin capital letter U with circumflex
     'Uuml'   => q|\"U|,             # 0xDC - latin capital letter U with diaeresis
     'Yacute' => q|\'Y|,             # 0xDD - latin capital letter Y with acute
     'THORN'  => q|\TH{}|,           # 0xDE - latin capital letter THORN
     'szlig'  => q|\ss{}|,           # 0xDF - latin small letter sharp s = ess-zed
     'agrave' => q|\`a|,             # 0xE0 - latin small letter a with grave = latin small letter a grave
     'aacute' => q|\'a|,             # 0xE1 - latin small letter a with acute
     'acirc'  => q|\^a|,             # 0xE2 - latin small letter a with circumflex
     'atilde' => q|\~a|,             # 0xE3 - latin small letter a with tilde
     'auml'   => q|\"a|,             # 0xE4 - latin small letter a with diaeresis
     'aring'  => q|\aa{}|,           # 0xE5 - latin small letter a with ring above = latin small letter a ring
     'aelig'  => q|\ae{}|,           # 0xE6 - latin small letter ae = latin small ligature ae
     'ccedil' => q|\c{c}|,           # 0xE7 - latin small letter c with cedilla
     'egrave' => q|\`e|,             # 0xE8 - latin small letter e with grave
     'eacute' => q|\'e|,             # 0xE9 - latin small letter e with acute
     'ecirc'  => q|\^e|,             # 0xEA - latin small letter e with circumflex
     'euml'   => q|\"e|,             # 0xEB - latin small letter e with diaeresis
     'igrave' => q|\`i|,             # 0xEC - latin small letter i with grave
     'iacute' => q|\'i|,             # 0xED - latin small letter i with acute
     'icirc'  => q|\^i|,             # 0xEE - latin small letter i with circumflex
     'iuml'   => q|\"i|,             # 0xEF - latin small letter i with diaeresis
     'eth'    => q|\dh{}|,           # 0xF0 - latin small letter eth
     'ntilde' => q|\~n|,             # 0xF1 - latin small letter n with tilde
     'ograve' => q|\`o|,             # 0xF2 - latin small letter o with grave
     'oacute' => q|\'o|,             # 0xF3 - latin small letter o with acute
     'ocirc'  => q|\^o|,             # 0xF4 - latin small letter o with circumflex
     'otilde' => q|\~o|,             # 0xF5 - latin small letter o with tilde
     'ouml'   => q|\"o|,             # 0xF6 - latin small letter o with diaeresis
     'divide' => q|\textdiv{}|,      # 0xF7 - division sign
     'oslash' => q|\o{}|,            # 0xF8 - latin small letter o with stroke, = latin small letter o slash
     'ugrave' => q|\`u|,             # 0xF9 - latin small letter u with grave
     'uacute' => q|\'u|,             # 0xFA - latin small letter u with acute
     'ucirc'  => q|\^u|,             # 0xFB - latin small letter u with circumflex
     'uuml'   => q|\"u|,             # 0xFC - latin small letter u with diaeresis
     'yacute' => q|\'y|,             # 0xFD - latin small letter y with acute
     'thorn'  => q|\th{}|,           # 0xFE - latin small letter thorn
     'yuml'   => q|\"y|,             # 0xFF - latin small letter y with diaeresis

     # Latin Extended-B
     'fnof'   => q|\textflorin{}|,   # latin small f with hook = function = florin

     # Greek
     'Alpha'    => q|$\mathrm{A}$|,      # greek capital letter alpha
     'Beta'     => q|$\mathrm{B}$|,      # greek capital letter beta
     'Gamma'    => q|$\Gamma$|,          # greek capital letter gamma
     'Delta'    => q|$\Delta$|,          # greek capital letter delta
     'Epsilon'  => q|$\mathrm{E}$|,      # greek capital letter epsilon
     'Zeta'     => q|$\mathrm{Z}$|,      # greek capital letter zeta
     'Eta'      => q|$\mathrm{H}$|,      # greek capital letter eta
     'Theta'    => q|$\Theta$|,          # greek capital letter theta
     'Iota'     => q|$\mathrm{I}$|,      # greek capital letter iota
     'Kappa'    => q|$\mathrm{K}$|,      # greek capital letter kappa
     'Lambda'   => q|$\Lambda$|,         # greek capital letter lambda
     'Mu'       => q|$\mathrm{M}$|,      # greek capital letter mu
     'Nu'       => q|$\mathrm{N}$|,      # greek capital letter nu
     'Xi'       => q|$\Xi$|,             # greek capital letter xi
     'Omicron'  => q|$\mathrm{O}$|,      # greek capital letter omicron
     'Pi'       => q|$\Pi$|,             # greek capital letter pi
     'Rho'      => q|$\mathrm{R}$|,      # greek capital letter rho
     'Sigma'    => q|$\Sigma$|,          # greek capital letter sigma
     'Tau'      => q|$\mathrm{T}$|,      # greek capital letter tau
     'Upsilon'  => q|$\Upsilon$|,        # greek capital letter upsilon
     'Phi'      => q|$\Phi$|,            # greek capital letter phi
     'Chi'      => q|$\mathrm{X}$|,      # greek capital letter chi
     'Psi'      => q|$\Psi$|,            # greek capital letter psi
     'Omega'    => q|$\Omega$|,          # greek capital letter omega

     'alpha'    => q|$\alpha$|,          # greek small letter alpha
     'beta'     => q|$\beta$|,           # greek small letter beta
     'gamma'    => q|$\gamma$|,          # greek small letter gamma
     'delta'    => q|$\delta$|,          # greek small letter delta
     'epsilon'  => q|$\epsilon$|,        # greek small letter epsilon
     'zeta'     => q|$\zeta$|,           # greek small letter zeta
     'eta'      => q|$\eta$|,            # greek small letter eta
     'theta'    => q|$\theta$|,          # greek small letter theta
     'iota'     => q|$\iota$|,           # greek small letter iota
     'kappa'    => q|$\kappa$|,          # greek small letter kappa
     'lambda'   => q|$\lambda$|,         # greek small letter lambda
     'mu'       => q|$\mu$|,             # greek small letter mu
     'nu'       => q|$\nu$|,             # greek small letter nu
     'xi'       => q|$\xi$|,             # greek small letter xi
     'omicron'  => q|$o$|,               # greek small letter omicron
     'pi'       => q|$\pi$|,             # greek small letter pi
     'rho'      => q|$\rho$|,            # greek small letter rho
#    'sigmaf'   => q||,                  # greek small letter final sigma
     'sigma'    => q|$\sigma$|,          # greek small letter sigma
     'tau'      => q|$\tau$|,            # greek small letter tau
     'upsilon'  => q|$\upsilon$|,        # greek small letter upsilon
     'phi'      => q|$\phi$|,            # greek small letter phi
     'chi'      => q|$\chi$|,            # greek small letter chi
     'psi'      => q|$\psi$|,            # greek small letter psi
     'omega'    => q|$\omega$|,          # greek small letter omega
#    'thetasym' => q||,                  # greek small letter theta symbol
#    'upsih'    => q||,                  # greek upsilon with hook symbol
#    'piv'      => q||,                  # greek pi symbol

     # General Punctuation
     'bull'     => q|\textbullet{}|,     # bullet = black small circle
     # bullet is NOT the same as bullet operator
     'hellip'   => q|\textellipsis{}|,           # horizontal ellipsis = three dot leader
     'prime'    => q|\textquotesingle{}|,        # prime = minutes = feet
     'Prime'    => q|\textquotedbl{}|,           # double prime = seconds = inches
     'oline'    => q|\textasciimacron{}|,        # overline = spacing overscore
     'frasl'    => q|\textfractionsolidus{}|,    # fraction slash

     # Letterlike Symbols
     'weierp'   => q|$\wp$|,                     # script capital P = power set = Weierstrass p
     'image'    => q|$\Re$|,                     # blackletter capital I = imaginary part
     'real'     => q|$\Im$|,                     # blackletter capital R = real part symbol
     'trade'    => q|\texttrademark{}|,          # trade mark sign
#    'alefsym'  => q||,                          # alef symbol = first transfinite cardinal
     # alef symbol is NOT the same as hebrew letter alef, although the same
     # glyph could be used to depict both characters

     # Arrows
     'larr'     => q|\textleftarrow{}|,          # leftwards arrow
     'uarr'     => q|\textuparrow{}|,            # upwards arrow
     'rarr'     => q|\textrightarrow{}|,         # rightwards arrow
     'darr'     => q|\textdownarrow{}|,          # downwards arrow
     'harr'     => q|$\leftrightarrow$|,         # left right arrow
#    'crarr'    => q||,                          # downwards arrow with corner leftwards = carriage return
     'lArr'     => q|$\Leftarrow$|,              # leftwards double arrow
     # ISO 10646 does not say that lArr is the same as the 'is implied by'
     # arrow but also does not have any other character for that function. So
     # lArr can be used for 'is implied by' as ISOtech suggests
     'uArr'     => q|$\Uparrow$|,                # upwards double arrow
     'rArr'     => q|$\Rightarrow$|,             # rightwards double arrow
     # ISO 10646 does not say this is the 'implies' character but does not
     # have another character with this function so ? rArr can be used for
     # 'implies' as ISOtech suggests
     'dArr'     => q|$\Downarrow$|,              # downwards double arrow
     'hArr'     => q|$\Leftrightarrow$|,         # left right double arrow

     # Mathematical Operators.
     # Some of these require the `amssymb' package.
     'forall'   => q|$\forall$|,                 # for all
     'part'     => q|$\partial$|,                # partial differential
     'exist'    => q|$\exists$|,                 # there exists
     'empty'    => q|$\emptyset$|,               # empty set = null set = diameter
     'nabla'    => q|$\nabla$|,                  # nabla = backward difference
     'isin'     => q|$\in$|,                     # element of
     'notin'    => q|$\notin$|,                  # not an element of
     'ni'       => q|$\ni$|,                     # contains as member
     'prod'     => q|$\prod$|,                   # n-ary product = product sign
     # prod is NOT the same character as 'greek capital letter pi' though the
     # same glyph might be used for both
     'sum'      => q|$\sum$|,                    # n-ary sumation
     # sum is NOT the same character as 'greek capital letter sigma' though
     # the same glyph might be used for both
     'minus'    => q|$-$|,                       # minus sign
     'lowast'   => q|$\ast$|,                    # asterisk operator
     'radic'    => q|$\surd$|,                   # square root = radical sign
     'prop'     => q|$\propto$|,                 # proportional to
     'infin'    => q|$\infty$|,                  # infinity
     'ang'      => q|$\angle$|,                  # angle
     'and'      => q|$\wedge$|,                  # logical and = wedge
     'or'       => q|$\vee$|,                    # logical or = vee
     'cap'      => q|$\cap$|,                    # intersection = cap
     'cup'      => q|$\cup$|,                    # union = cup
     'int'      => q|$\int$|,                    # integral
     'there4'   => q|$\therefore$|,              # therefore
     'sim'      => q|$\sim$|,                    # tilde operator = varies with = similar to
     # tilde operator is NOT the same character as the tilde
     'cong'     => q|$\cong$|,                   # approximately equal to
     'asymp'    => q|$\asymp$|,                  # almost equal to = asymptotic to
     'ne'       => q|$\neq$|,                    # not equal to
     'equiv'    => q|$\equiv$|,                  # identical to
     'le'       => q|$\leq$|,                    # less-than or equal to
     'ge'       => q|$\geq$|,                    # greater-than or equal to
     'sub'      => q|$\subset$|,                 # subset of
     'sup'      => q|$\supset$|,                 # superset of
     # note that nsup, 'not a superset of' is not covered by the Symbol font
     # encoding and is not included.
     'nsub'     => q|$\not\subset$|,             # not a subset of
     'sube'     => q|$\subseteq$|,               # subset of or equal to
     'supe'     => q|$\supseteq$|,               # superset of or equal to
     'oplus'    => q|$\oplus$|,                  # circled plus = direct sum
     'otimes'   => q|$\otimes$|,                 # circled times = vector product
     'perp'     => q|$\perp$|,                   # up tack = orthogonal to = perpendicular
     'sdot'     => q|$\cdot$|,                   # dot operator
     # dot operator is NOT the same character as middle dot

     # Miscellaneous Technical
     'lceil'    => q|$\lceil$|,                  # left ceiling = apl upstile
     'rceil'    => q|$\rceil$|,                  # right ceiling
     'lfloor'   => q|$\lfloor$|,                 # left floor = apl downstile
     'rfloor'   => q|$\rfloor$|,                 # right floor
     'lang'     => q|$\langle$|,                 # left-pointing angle bracket = bra
     # lang is NOT the same character as 'less than' or 'single left-pointing
     # angle quotation mark'
     'rang'     => q|$\rangle$|,                 # right-pointing angle bracket = ket
     # rang is NOT the same character as 'greater than' or 'single
     # right-pointing angle quotation mark'

     # Geometric Shapes
     'loz'      => q|$\lozenge$|,                # lozenge

     # Miscellaneous Symbols
     'spades'   => q|$\spadesuit$|,              # black spade suit
     'clubs'    => q|$\clubsuit$|,               # black club suit = shamrock
     'hearts'   => q|$\heartsuit$|,              # black heart suit = valentine
     'diams'    => q|$\diamondsuit$|,            # black diamond suit

     # C0 Controls and Basic Latin
     'quot'     => q|"|,                         # quotation mark = APL quote ["]
     'amp'      => q|\&|,                        # ampersand
     'lt'       => q|<|,                         # less-than sign
     'gt'       => q|>|,                         # greater-than sign
     'OElig'    => q|\OE{}|,                     # latin capital ligature OE
     'oelig'    => q|\oe{}|,                     # latin small ligature oe
     'Scaron'   => q|\v{S}|,                     # latin capital letter S with caron
     'scaron'   => q|\v{s}|,                     # latin small letter s with caron
     'Yuml'     => q|\"Y|,                       # latin capital letter Y with diaeresis
     'circ'     => q|\textasciicircum{}|,        # modifier letter circumflex accent
     'tilde'    => q|\textasciitilde{}|,         # small tilde
     'ensp'     => q|\phantom{n}|,               # en space
     'emsp'     => q|\hspace{1em}|,              # em space
     'thinsp'   => q|\,|,                        # thin space
     'zwnj'     => q|{}|,                        # zero width non-joiner
#    'zwj'      => q||,                          # zero width joiner
#    'lrm'      => q||,                          # left-to-right mark
#    'rlm'      => q||,                          # right-to-left mark
     'ndash'    => q|--|,                        # en dash
     'mdash'    => q|---|,                       # em dash
     'lsquo'    => q|\textquoteleft{}|,          # left single quotation mark
     'rsquo'    => q|\textquoteright{}|,         # right single quotation mark
     'sbquo'    => q|\quotesinglbase{}|,         # single low-9 quotation mark
     'ldquo'    => q|\textquotedblleft{}|,       # left double quotation mark
     'rdquo'    => q|\textquotedblright{}|,      # right double quotation mark
     'bdquo'    => q|\quotedblbase{}|,           # double low-9 quotation mark
     'dagger'   => q|\textdagger{}|,             # dagger
     'Dagger'   => q|\textdaggerdbl{}|,          # double dagger
     'permil'   => q|\textperthousand{}|,        # per mille sign
     'lsaquo'   => q|\guilsinglleft{}|,          # single left-pointing angle quotation mark
     'rsaquo'   => q|\guilsinglright{}|,         # single right-pointing angle quotation mark
     'euro'     => q|\texteuro{}|,               # euro sign
);

=head1 OBJECT METHODS

The following methods are provided in this module. Methods inherited
from C<Pod::Select> are not described in the public interface.

=over 4

=begin __PRIVATE__

=item C<initialize>

Initialise the object. This method is subclassed from C<Pod::Parser>.
The base class method is invoked. This method defines the default
behaviour of the object unless overridden by supplying arguments to
the constructor. 

Internal settings are defaulted as well as the public instance data.
Internal hash values are accessed directly (rather than through
a method) and start with an underscore.

This method should not be invoked by the user directly.

=end __PRIVATE__

=cut



#   - An array for nested lists

# Arguments have already been read by this point

sub initialize {
  my $self = shift;

  # print Dumper($self);

  # Internals
  $self->{_Lists} = [];             # For nested lists
  $self->{_suppress_all_para}  = 0; # For =begin blocks
  $self->{_dont_modify_any_para}=0; # For =begin blocks
  $self->{_CURRENT_HEAD1}   = '';   # Name of current HEAD1 section

  # Options - only initialise if not already set

  # Cause the '=head1 NAME' field to be treated specially
  # The contents of the NAME paragraph will be converted
  # to a section title. All subsequent =head1 will be converted
  # to =head2 and down. Will not affect =head1's prior to NAME 
  # Assumes:  'Module - purpose' format
  # Also creates a purpose field
  # The name is used for Labeling of the subsequent subsections
  $self->{ReplaceNAMEwithSection} = 0
    unless exists $self->{ReplaceNAMEwithSection};
  $self->{AddPreamble}      = 1    # make full latex document
    unless exists $self->{AddPreamble};
  $self->{StartWithNewPage} = 0    # Start new page for pod section
    unless exists $self->{StartWithNewPage};
  $self->{TableOfContents}  = 0    # Add table of contents
    unless exists $self->{TableOfContents};  # only relevent if AddPreamble=1
   $self->{AddPostamble}     = 1          # Add closing latex code at end
    unless exists $self->{AddPostamble}; #  effectively end{document} and index
  $self->{MakeIndex}        = 1         # Add index (only relevant AddPostamble
    unless exists $self->{MakeIndex};   # and AddPreamble)

  $self->{UniqueLabels}     = 1          # Use label unique for each pod
    unless exists $self->{UniqueLabels}; # either based on the filename
                                         # or supplied

  # Control the level of =head1. default is \section
  # 
  $self->{Head1Level}     = 1   # Offset in latex sections
    unless exists $self->{Head1Level}; # 0 is chapter, 2 is subsection

  # Control at which level numbering of sections is turned off
  # ie subsection becomes subsection*
  # The numbering is relative to the latex sectioning commands
  # and is independent of Pod heading level
  # default is to number \section but not \subsection
  $self->{LevelNoNum} = 2
    unless exists $self->{LevelNoNum};

  # Label to be used as prefix to all internal section names
  # If not defined will attempt to derive it from the filename
  # This can not happen when running parse_from_filehandle though
  # hence the ability to set the label externally
  # The label could then be Pod::Parser_DESCRIPTION or somesuch

  $self->{Label}            = undef # label to be used as prefix
    unless exists $self->{Label};   # to all internal section names

  # These allow the caller to add arbritrary latex code to
  # start and end of document. AddPreamble and AddPostamble are ignored
  # if these are set.
  # Also MakeIndex and TableOfContents are also ignored.
  $self->{UserPreamble}     = undef # User supplied start (AddPreamble =1)
    unless exists $self->{Label};
  $self->{UserPostamble}    = undef # Use supplied end    (AddPostamble=1)
    unless exists $self->{Label};

  # Run base initialize
  $self->SUPER::initialize;

}

=back

=head2 Data Accessors

The following methods are provided for accessing instance data. These
methods should be used for accessing configuration parameters rather
than assuming the object is a hash.

Default values can be supplied by using these names as keys to a hash
of arguments when using the C<new()> constructor.

=over 4

=item B<AddPreamble>

Logical to control whether a C<latex> preamble is to be written.
If true, a valid C<latex> preamble is written before the pod data is written.
This is similar to:

  \documentclass{article}
  \usepackage[T1]{fontenc}
  \usepackage{textcomp}
  \begin{document}

but will be more complicated if table of contents and indexing are required.
Can be used to set or retrieve the current value.

  $add = $parser->AddPreamble();
  $parser->AddPreamble(1);

If used in conjunction with C<AddPostamble> a full latex document will
be written that could be immediately processed by C<latex>.

For some pod escapes it may be necessary to include the amsmath
package. This is not yet added to the preamble automatically.

=cut

sub AddPreamble {
   my $self = shift;
   if (@_) {
     $self->{AddPreamble} = shift;
   }
   return $self->{AddPreamble};
}

=item B<AddPostamble>

Logical to control whether a standard C<latex> ending is written to the output
file after the document has been processed.
In its simplest form this is simply:

  \end{document}

but can be more complicated if a index is required.
Can be used to set or retrieve the current value.

  $add = $parser->AddPostamble();
  $parser->AddPostamble(1);

If used in conjunction with C<AddPreaamble> a full latex document will
be written that could be immediately processed by C<latex>.

=cut

sub AddPostamble {
   my $self = shift;
   if (@_) {
     $self->{AddPostamble} = shift;
   }
   return $self->{AddPostamble};
}

=item B<Head1Level>

The C<latex> sectioning level that should be used to correspond to
a pod C<=head1> directive. This can be used, for example, to turn
a C<=head1> into a C<latex> C<subsection>. This should hold a number
corresponding to the required position in an array containing the
following elements:

 [0] chapter
 [1] section
 [2] subsection
 [3] subsubsection
 [4] paragraph
 [5] subparagraph

Can be used to set or retrieve the current value:

  $parser->Head1Level(2);
  $sect = $parser->Head1Level;

Setting this number too high can result in sections that may not be reproducible
in the expected way. For example, setting this to 4 would imply that C<=head3>
do not have a corresponding C<latex> section (C<=head1> would correspond to
a C<paragraph>).

A check is made to ensure that the supplied value is an integer in the
range 0 to 5.

Default is for a value of 1 (i.e. a C<section>).

=cut

sub Head1Level {
   my $self = shift;
   if (@_) {
     my $arg = shift;
     if ($arg =~ /^\d$/ && $arg <= $#LatexSections) {
       $self->{Head1Level} = $arg;
     } else {
       carp "Head1Level supplied ($arg) must be integer in range 0 to ".$#LatexSections . "- Ignoring\n";
     }
   }
   return $self->{Head1Level};
}

=item B<Label>

This is the label that is prefixed to all C<latex> label and index
entries to make them unique. In general, pods have similarly titled
sections (NAME, DESCRIPTION etc) and a C<latex> label will be multiply
defined if more than one pod document is to be included in a single
C<latex> file. To overcome this, this label is prefixed to a label
whenever a label is required (joined with an underscore) or to an
index entry (joined by an exclamation mark which is the normal index
separator). For example, C<\label{text}> becomes C<\label{Label_text}>.

Can be used to set or retrieve the current value:

  $label = $parser->Label;
  $parser->Label($label);

This label is only used if C<UniqueLabels> is true.
Its value is set automatically from the C<NAME> field
if C<ReplaceNAMEwithSection> is true. If this is not the case
it must be set manually before starting the parse.

Default value is C<undef>.

=cut

sub Label {
   my $self = shift;
   if (@_) {
     $self->{Label} = shift;
   }
   return $self->{Label};
}

=item B<LevelNoNum>

Control the point at which C<latex> section numbering is turned off.
For example, this can be used to make sure that C<latex> sections
are numbered but subsections are not.

Can be used to set or retrieve the current value:

  $lev = $parser->LevelNoNum;
  $parser->LevelNoNum(2);

The argument must be an integer between 0 and 5 and is the same as the
number described in C<Head1Level> method description. The number has
nothing to do with the pod heading number, only the C<latex> sectioning.

Default is 2. (i.e. C<latex> subsections are written as C<subsection*>
but sections are numbered).

=cut

sub LevelNoNum {
   my $self = shift;
   if (@_) {
     $self->{LevelNoNum} = shift;
   }
   return $self->{LevelNoNum};
}

=item B<MakeIndex>

Controls whether C<latex> commands for creating an index are to be inserted
into the preamble and postamble

  $makeindex = $parser->MakeIndex;
  $parser->MakeIndex(0);

Irrelevant if both C<AddPreamble> and C<AddPostamble> are false (or equivalently,
C<UserPreamble> and C<UserPostamble> are set).

Default is for an index to be created.

=cut

sub MakeIndex {
   my $self = shift;
   if (@_) {
     $self->{MakeIndex} = shift;
   }
   return $self->{MakeIndex};
}

=item B<ReplaceNAMEwithSection>

This controls whether the C<NAME> section in the pod is to be translated
literally or converted to a slightly modified output where the section
name is the pod name rather than "NAME".

If true, the pod segment

  =head1 NAME

  pod::name - purpose

  =head1 SYNOPSIS

is converted to the C<latex>

  \section{pod::name\label{pod_name}\index{pod::name}}

  Purpose

  \subsection*{SYNOPSIS\label{pod_name_SYNOPSIS}%
               \index{pod::name!SYNOPSIS}}

(dependent on the value of C<Head1Level> and C<LevelNoNum>). Note that
subsequent C<head1> directives translate to subsections rather than
sections and that the labels and index now include the pod name (dependent
on the value of C<UniqueLabels>).

The C<Label> is set from the pod name regardless of any current value
of C<Label>.

  $mod = $parser->ReplaceNAMEwithSection;
  $parser->ReplaceNAMEwithSection(0);

Default is to translate the pod literally.

=cut

sub ReplaceNAMEwithSection {
   my $self = shift;
   if (@_) {
     $self->{ReplaceNAMEwithSection} = shift;
   }
   return $self->{ReplaceNAMEwithSection};
}

=item B<StartWithNewPage>

If true, each pod translation will begin with a C<latex>
C<\clearpage>.

  $parser->StartWithNewPage(1);
  $newpage = $parser->StartWithNewPage;

Default is false.

=cut

sub StartWithNewPage {
   my $self = shift;
   if (@_) {
     $self->{StartWithNewPage} = shift;
   }
   return $self->{StartWithNewPage};
}

=item B<TableOfContents>

If true, a table of contents will be created.
Irrelevant if C<AddPreamble> is false or C<UserPreamble>
is set.

  $toc = $parser->TableOfContents;
  $parser->TableOfContents(1);

Default is false.

=cut

sub TableOfContents {
   my $self = shift;
   if (@_) {
     $self->{TableOfContents} = shift;
   }
   return $self->{TableOfContents};
}

=item B<UniqueLabels>

If true, the translator will attempt to make sure that
each C<latex> label or index entry will be uniquely identified
by prefixing the contents of C<Label>. This allows
multiple documents to be combined without clashing 
common labels such as C<DESCRIPTION> and C<SYNOPSIS>

  $parser->UniqueLabels(1);
  $unq = $parser->UniqueLabels;

Default is true.

=cut

sub UniqueLabels {
   my $self = shift;
   if (@_) {
     $self->{UniqueLabels} = shift;
   }
   return $self->{UniqueLabels};
}

=item B<UserPreamble>

User supplied C<latex> preamble. Added before the pod translation
data. 

If set, the contents will be prepended to the output file before the translated 
data regardless of the value of C<AddPreamble>.
C<MakeIndex> and C<TableOfContents> will also be ignored.

=cut

sub UserPreamble {
   my $self = shift;
   if (@_) {
     $self->{UserPreamble} = shift;
   }
   return $self->{UserPreamble};
}

=item B<UserPostamble>

User supplied C<latex> postamble. Added after the pod translation
data. 

If set, the contents will be prepended to the output file after the translated 
data regardless of the value of C<AddPostamble>.
C<MakeIndex> will also be ignored.

=cut

sub UserPostamble {
   my $self = shift;
   if (@_) {
     $self->{UserPostamble} = shift;
   }
   return $self->{UserPostamble};
}

=begin __PRIVATE__

=item B<Lists>

Contains details of the currently active lists.
  The array contains C<Pod::List> objects. A new C<Pod::List>
object is created each time a list is encountered and it is
pushed onto this stack. When the list context ends, it 
is popped from the stack. The array will be empty if no
lists are active.

Returns array of list information in list context
Returns array ref in scalar context

=cut



sub lists {
  my $self = shift;
  return @{ $self->{_Lists} } if wantarray();
  return $self->{_Lists};
}

=end __PRIVATE__

=back

=begin __PRIVATE__

=head2 Subclassed methods

The following methods override methods provided in the C<Pod::Select>
base class. See C<Pod::Parser> and C<Pod::Select> for more information
on what these methods require.

=over 4

=cut

######### END ACCESSORS ###################

# Opening pod

=item B<begin_pod>

Writes the C<latex> preamble if requested. Only writes something
if AddPreamble is true. Writes a standard header unless a UserPreamble
is defined.

=cut

sub begin_pod {
  my $self = shift;

  # Get the pod identification
  # This should really come from the '=head1 NAME' paragraph

  my $infile = $self->input_file;
  my $class = ref($self);
  my $date = gmtime(time);

  # Comment message to say where this came from
  my $comment = << "__TEX_COMMENT__";
%%  Latex generated from POD in document $infile
%%  Using the perl module $class
%%  Converted on $date
__TEX_COMMENT__

  # Write the preamble
  # If the caller has supplied one then we just use that

  my $preamble = '';

  if ($self->AddPreamble) {

    if (defined $self->UserPreamble) {

      $preamble = $self->UserPreamble;

      # Add the description of where this came from
      $preamble .=  "\n$comment\n%%  Preamble supplied by user.\n\n";

    } else {

      # Write our own preamble

      # Code to initialise index making
      # Use an array so that we can prepend comment if required
      my @makeidx = (
		     '\usepackage{makeidx}',
		     '\makeindex',
		    );

      unless ($self->MakeIndex) {
	foreach (@makeidx) {
	  $_ = '%% ' . $_;
	}
      }
      my $makeindex = join("\n",@makeidx) . "\n";

      # Table of contents
      my $tableofcontents = '\tableofcontents';

      $tableofcontents = '%% ' . $tableofcontents
	unless $self->TableOfContents;

      # Roll our own
      $preamble = << "__TEX_HEADER__";
\\documentclass{article}
\\usepackage[T1]{fontenc}
\\usepackage{textcomp}

$comment

$makeindex

\\begin{document}

$tableofcontents

__TEX_HEADER__

    }
  }

  # Write the header (blank if none)
  $self->_output($preamble);

  # Start on new page if requested
  $self->_output("\\clearpage\n") if $self->StartWithNewPage;

}


=item B<end_pod>

Write the closing C<latex> code. Only writes something if AddPostamble
is true. Writes a standard header unless a UserPostamble is defined.

=cut

sub end_pod {
  my $self = shift;

  # End string
  my $end = '';

  # Use the user version of the postamble if defined
  if ($self->AddPostamble) {

    if (defined $self->UserPostamble) {
      $end = $self->UserPostamble;

    } else {

      # Check for index
      my $makeindex = '\printindex';

      $makeindex = '%% '. $makeindex  unless $self->MakeIndex;

      $end = "$makeindex\n\n\\end{document}\n";
    }
  }

  $self->_output($end);

}

=item B<command>

Process basic pod commands.

=cut

sub command {
  my $self = shift;
  my ($command, $paragraph, $line_num, $parobj) = @_;

  # return if we dont care
  return if $command eq 'pod';

  # Store a copy of the raw text in case we are in a =for
  # block and need to preserve the existing latex
  my $rawpara = $paragraph;

  # Do the latex escapes
  $paragraph = $self->_replace_special_chars($paragraph);

  # Interpolate pod sequences in paragraph
  $paragraph = $self->interpolate($paragraph, $line_num);
  $paragraph =~ s/\s+$//;

  # Replace characters that can only be done after 
  # interpolation of interior sequences
  $paragraph = $self->_replace_special_chars_late($paragraph);

  # Now run the command
  if ($command eq 'over') {

    $self->begin_list($paragraph, $line_num);

  } elsif ($command eq 'item') {

    $self->add_item($paragraph, $line_num);

  } elsif ($command eq 'back') {

    $self->end_list($line_num);

  } elsif ($command eq 'head1') {

    # Store the name of the section
    $self->{_CURRENT_HEAD1} = $paragraph;

    # Print it
    $self->head(1, $paragraph, $parobj);

  } elsif ($command eq 'head2') {

    $self->head(2, $paragraph, $parobj);

  } elsif ($command eq 'head3') {

    $self->head(3, $paragraph, $parobj);

  } elsif ($command eq 'head4') {

    $self->head(4, $paragraph, $parobj);

  } elsif ($command eq 'head5') {

    $self->head(5, $paragraph, $parobj);

  } elsif ($command eq 'head6') {

    $self->head(6, $paragraph, $parobj);

  } elsif ($command eq 'begin') {

    # pass through if latex
    if ($paragraph =~ /^latex/i) {
      # Make sure that subsequent paragraphs are not modfied before printing
      $self->{_dont_modify_any_para} = 1;

    } else {
      # Suppress all subsequent paragraphs unless 
      # it is explcitly intended for latex
      $self->{_suppress_all_para} = 1;
    }

  } elsif ($command eq 'for') {

    # =for latex
    #   some latex

    # With =for we will get the text for the full paragraph
    # as well as the format name.
    # We do not get an additional paragraph later on. The next
    # paragraph is not governed by the =for

    # The first line contains the format and the rest is the
    # raw code.
    my ($format, $chunk) = split(/\n/, $rawpara, 2);

    # If we have got some latex code print it out immediately
    # unmodified. Else do nothing.
    if ($format =~ /^latex/i) {
      # Make sure that next paragraph is not modfied before printing
      $self->_output( $chunk );

    }

  } elsif ($command eq 'end') {

    # Reset suppression
    $self->{_suppress_all_para} = 0;
    $self->{_dont_modify_any_para} = 0;

  } elsif ($command eq 'pod') {

    # Do nothing

  } else {
    carp "Command $command not recognised at line $line_num\n";
  }

}

=item B<verbatim>

Verbatim text

=cut

sub verbatim {
  my $self = shift;
  my ($paragraph, $line_num, $parobj) = @_;

  # Expand paragraph unless in =begin block
  if ($self->{_dont_modify_any_para}) {
    # Just print as is
    $self->_output($paragraph);

  } else {

    return if $paragraph =~ /^\s+$/;

    # Clean trailing space
    $paragraph =~ s/\s+$//;

    # Clean tabs. Routine taken from Tabs.pm
    # by David Muir Sharnoff muir@idiom.com,
    # slightly modified by hsmyers@sdragons.com 10/22/01
    my @l = split("\n",$paragraph);
    foreach (@l) {
      1 while s/(^|\n)([^\t\n]*)(\t+)/
	$1. $2 . (" " x 
		  (8 * length($3)
		   - (length($2) % 8)))
	  /sex;
    }
    $paragraph = join("\n",@l);
    # End of change.



    $self->_output('\begin{verbatim}' . "\n$paragraph\n". '\end{verbatim}'."\n");
  }
}

=item B<textblock>

Plain text paragraph.

=cut

sub textblock {
  my $self = shift;
  my ($paragraph, $line_num, $parobj) = @_;

  # print Dumper($self);

  # Expand paragraph unless in =begin block
  if ($self->{_dont_modify_any_para}) {
    # Just print as is
    $self->_output($paragraph);

    return;
  }


  # Escape latex special characters
  $paragraph = $self->_replace_special_chars($paragraph);

  # Interpolate interior sequences
  my $expansion = $self->interpolate($paragraph, $line_num);
  $expansion =~ s/\s+$//;

  # Escape special characters that can not be done earlier
  $expansion = $self->_replace_special_chars_late($expansion);

  # If we are replacing 'head1 NAME' with a section
  # we need to look in the paragraph and rewrite things
  # Need to make sure this is called only on the first paragraph
  # following 'head1 NAME' and not on subsequent paragraphs that may be
  # present.
  if ($self->{_CURRENT_HEAD1} =~ /^NAME/i && $self->ReplaceNAMEwithSection()) {

    # Strip white space from start and end
    $paragraph =~ s/^\s+//;
    $paragraph =~ s/\s$//;

    # Split the string into 2 parts
    my ($name, $purpose) = split(/\s+-\s+/, $expansion,2);

    # Now prevent this from triggering until a new head1 NAME is set
    $self->{_CURRENT_HEAD1} = '_NAME';

    # Might want to clear the Label() before doing this (CHECK)

    # Print the heading
    $self->head(1, $name, $parobj);

    # Set the labeling in case we want unique names later
    $self->Label( $self->_create_label( $name, 1 ) );

    # Raise the Head1Level by one so that subsequent =head1 appear
    # as subsections of the main name section unless we are already
    # at maximum [Head1Level() could check this itself - CHECK]
    $self->Head1Level( $self->Head1Level() + 1)
      unless $self->Head1Level == $#LatexSections;

    # Now write out the new latex paragraph
    $purpose = ucfirst($purpose);
    $self->_output("\n\n$purpose\n\n");

  } else {
    # Just write the output
    $self->_output("\n\n$expansion\n\n");
  }

}

=item B<interior_sequence>

Interior sequence expansion

=cut

sub interior_sequence {
  my $self = shift;

  my ($seq_command, $seq_argument, $pod_seq) = @_;

  if ($seq_command eq 'B') {
    return "\\textbf{$seq_argument}";

  } elsif ($seq_command eq 'I') {
    return "\\textit{$seq_argument}";

  } elsif ($seq_command eq 'E') {

    # If it is simply a number
    if ($seq_argument =~ /^\d+$/) {
      return chr($seq_argument);
    # Look up escape in hash table
    } elsif (exists $HTML_Escapes{$seq_argument}) {
      return $HTML_Escapes{$seq_argument};

    } else {
      my ($file, $line) = $pod_seq->file_line();
      warn "Escape sequence $seq_argument not recognised at line $line of file $file\n";
      return;
    }

  } elsif ($seq_command eq 'Z') {

    # Zero width space
    return '{}';

  } elsif ($seq_command eq 'C') {
    return "\\texttt{$seq_argument}";

  } elsif ($seq_command eq 'F') {
    return "\\emph{$seq_argument}";

  } elsif ($seq_command eq 'S') {
    # non breakable spaces
    my $nbsp = '~';

    $seq_argument =~ s/\s/$nbsp/g;
    return $seq_argument;

  } elsif ($seq_command eq 'L') {
    my $link = new Pod::Hyperlink($seq_argument);

    # undef on failure
    unless (defined $link) {
      carp $@;
      return;
    }

    # Handle internal links differently
    my $type = $link->type;
    my $page = $link->page;

    if ($type eq 'section' && $page eq '') {
      # Use internal latex reference 
      my $node = $link->node;

      # Convert to a label
      $node = $self->_create_label($node);

      return "\\S\\ref{$node}";

    } else {
      # Use default markup for external references
      # (although Starlink would use \xlabel)
      my $markup = $link->markup;
      my ($file, $line) = $pod_seq->file_line();

      return $self->interpolate($link->markup, $line);
    }



  } elsif ($seq_command eq 'P') {
    # Special markup for Pod::Hyperlink
    # Replace :: with / - but not sure if I want to do this
    # any more.
    my $link = $seq_argument;
    $link =~ s|::|/|g;

    my $ref = "\\emph{$seq_argument}";
    return $ref;

  } elsif ($seq_command eq 'Q') {
    # Special markup for Pod::Hyperlink
    return "\\textsf{$seq_argument}";

  } elsif ($seq_command eq 'X') {
    # Index entries

    # use \index command
    # I will let '!' go through for now
    # not sure how sub categories are handled in X<>
    my $index = $self->_create_index($seq_argument);
    return "\\index{$index}\n";

  } else {
    carp "Unknown sequence $seq_command<$seq_argument>";
  }

}

=back

=head2 List Methods

Methods used to handle lists.

=over 4

=item B<begin_list>

Called when a new list is found (via the C<over> directive).
Creates a new C<Pod::List> object and stores it on the 
list stack.

  $parser->begin_list($indent, $line_num);

=cut

sub begin_list {
  my $self = shift;
  my $indent = shift;
  my $line_num = shift;

  # Indicate that a list should be started for the next item
  # need to do this to work out the type of list
  push ( @{$self->lists}, new Pod::List(-indent => $indent, 
					-start => $line_num,
					-file => $self->input_file,
				       )	 
       );

}

=item B<end_list>

Called when the end of a list is found (the C<back> directive).
Pops the C<Pod::List> object off the stack of lists and writes
the C<latex> code required to close a list.

  $parser->end_list($line_num);

=cut

sub end_list {
  my $self = shift;
  my $line_num = shift;

  unless (defined $self->lists->[-1]) {
    my $file = $self->input_file;
    warn "No list is active at line $line_num (file=$file). Missing =over?\n";
    return;
  }

  # What to write depends on list type
  my $type = $self->lists->[-1]->type;

  # Dont write anything if the list type is not set
  # iomplying that a list was created but no entries were
  # placed in it (eg because of a =begin/=end combination)
  $self->_output("\\end{$type}\n")
    if (defined $type && length($type) > 0);
  
  # Clear list
  pop(@{ $self->lists});

}

=item B<add_item>

Add items to the list. The first time an item is encountered 
(determined from the state of the current C<Pod::List> object)
the type of list is determined (ordered, unnumbered or description)
and the relevant latex code issued.

  $parser->add_item($paragraph, $line_num);

=cut

sub add_item {
  my $self = shift;
  my $paragraph = shift;
  my $line_num = shift;

  unless (defined $self->lists->[-1]) {
    my $file = $self->input_file;
    warn "List has already ended by line $line_num of file $file. Missing =over?\n";
    # Replace special chars
#    $paragraph = $self->_replace_special_chars($paragraph);
    $self->_output("$paragraph\n\n");
    return;
  }

  # If paragraphs printing is turned off via =begin/=end or whatver
  # simply return immediately
  return if $self->{_suppress_all_para};

  # Check to see whether we are starting a new lists
  if (scalar($self->lists->[-1]->item) == 0) {

    # Examine the paragraph to determine what type of list
    # we have
    $paragraph =~ s/\s+$//;
    $paragraph =~ s/^\s+//;

    my $type;
    if (substr($paragraph, 0,1) eq '*') {
      $type = 'itemize';
    } elsif ($paragraph =~ /^\d/) {
      $type = 'enumerate';
    } else {
      $type = 'description';
    }
    $self->lists->[-1]->type($type);

    $self->_output("\\begin{$type}\n");

  }

  my $type = $self->lists->[-1]->type;

  if ($type eq 'description') {
    # Handle long items - long items do not wrap
    # If the string is longer than 40 characters we split
    # it into a real item header and some bold text.
    my $maxlen = 40;
    my ($hunk1, $hunk2) = $self->_split_delimited( $paragraph, $maxlen );

    # Print the first hunk
    $self->_output("\n\\item[{$hunk1}] ");

    # and the second hunk if it is defined
    if ($hunk2) {
      $self->_output("\\textbf{$hunk2}");
    } else {
      # Not there so make sure we have a new line
      $self->_output("\\mbox{}");
    }

  } else {
    # If the item was '* Something' or '\d+ something' we still need to write
    # out the something. Also allow 1) and 1.
    my $extra_info = $paragraph;
    $extra_info =~ s/^(\*|\d+[\.\)]?)\s*//;
    $self->_output("\n\\item $extra_info");
  }

  # Store the item name in the object. Required so that 
  # we can tell if the list is new or not
  $self->lists->[-1]->item($paragraph);

}

=back

=head2 Methods for headings

=over 4

=item B<head>

Print a heading of the required level.

  $parser->head($level, $paragraph, $parobj);

The first argument is the pod heading level. The second argument
is the contents of the heading. The 3rd argument is a Pod::Paragraph
object so that the line number can be extracted.

=cut

sub head {
  my $self = shift;
  my $num = shift;
  my $paragraph = shift;
  my $parobj = shift;

  # If we are replace 'head1 NAME' with a section
  # we return immediately if we get it
  return 
    if ($self->{_CURRENT_HEAD1} =~ /^NAME/i && $self->ReplaceNAMEwithSection());

  # Create a label
  my $label = $self->_create_label($paragraph);

  # Create an index entry
  my $index = $self->_create_index($paragraph);

  # Work out position in the above array taking into account
  # that =head1 is equivalent to $self->Head1Level

  my $level = $self->Head1Level() - 1 + $num;

  # Warn if heading to large
  if ($num > $#LatexSections) {
    my $line = $parobj->file_line;
    my $file = $self->input_file;
    warn "Heading level too large ($level) for LaTeX at line $line of file $file\n";
    $level = $#LatexSections;
  }

  # Check to see whether section should be unnumbered
  my $star = ($level >= $self->LevelNoNum ? '*' : '');

  # Section
  $self->_output("\\" .$LatexSections[$level] .$star ."{$paragraph\\label{".$label ."}\\index{".$index."}}\n");

}


=back

=end __PRIVATE__

=begin __PRIVATE__

=head2 Internal methods

Internal routines are described in this section. They do not form part of the
public interface. All private methods start with an underscore.

=over 4

=item B<_output>

Output text to the output filehandle. This method must be always be called
to output parsed text.

   $parser->_output($text);

Does not write anything if a =begin is active that should be
ignored.

=cut

sub _output { 
  my $self = shift;
  my $text = shift;

  print { $self->output_handle } $text
    unless $self->{_suppress_all_para};

}


=item B<_replace_special_chars>

Subroutine to replace characters that are special in C<latex>
with the escaped forms

  $escaped = $parser->_replace_special_chars($paragraph);

Need to call this routine before interior_sequences are munged but not
if verbatim. It must be called before interpolation of interior
sequences so that curly brackets and special latex characters inserted
during interpolation are not themselves escaped. This means that < and
> can not be modified here since the text still contains interior
sequences.

Special characters and the C<latex> equivalents are:

  }     \}
  {     \{
  _     \_
  $     \$
  %     \%
  &     \&
  \     $\backslash$
  ^     \^{}
  ~     \~{}
  #     \#

=cut

sub _replace_special_chars {
  my $self = shift;
  my $paragraph = shift;

  # Replace a \ with $\backslash$
  # This is made more complicated because the dollars will be escaped
  # by the subsequent replacement. Easiest to add \backslash 
  # now and then add the dollars
  $paragraph =~ s/\\/\\backslash/g;

  # Must be done after escape of \ since this command adds latex escapes
  # Replace characters that can be escaped
  $paragraph =~ s/([\$\#&%_{}])/\\$1/g;

  # Replace ^ characters with \^{} so that $^F works okay
  $paragraph =~ s/(\^)/\\$1\{\}/g;

  # Replace tilde (~) with \texttt{\~{}}
  $paragraph =~ s/~/\\texttt\{\\~\{\}\}/g;

  # Now add the dollars around each \backslash
  $paragraph =~ s/(\\backslash)/\$$1\$/g;
  return $paragraph;
}

=item B<_replace_special_chars_late>

Replace special characters that can not be replaced before interior
sequence interpolation. See C<_replace_special_chars> for a routine
to replace special characters prior to interpolation of interior
sequences.

Does the following transformation:

  <   $<$
  >   $>$
  |   $|$


=cut

sub _replace_special_chars_late {
  my $self = shift;
  my $paragraph = shift;

  # < and >
  $paragraph =~ s/(<|>)/\$$1\$/g;

  # Replace | with $|$
  $paragraph =~ s'\|'$|$'g;


  return $paragraph;
}


=item B<_create_label>

Return a string that can be used as an internal reference
in a C<latex> document (i.e. accepted by the C<\label> command)

 $label = $parser->_create_label($string)

If UniqueLabels is true returns a label prefixed by Label()
This can be suppressed with an optional second argument.

 $label = $parser->_create_label($string, $suppress);

If a second argument is supplied (of any value including undef)
the Label() is never prefixed. This means that this routine can
be called to create a Label() without prefixing a previous setting.

=cut

sub _create_label {
  my $self = shift;
  my $paragraph = shift;
  my $suppress = (@_ ? 1 : 0 );

  # Remove latex commands
  $paragraph = $self->_clean_latex_commands($paragraph);

  # Remove non alphanumerics from the label and replace with underscores
  # want to protect '-' though so use negated character classes 
  $paragraph =~ s/[^-:\w]/_/g;

  # Multiple underscores will look unsightly so remove repeats
  # This will also have the advantage of tidying up the end and
  # start of string
  $paragraph =~ s/_+/_/g;

  # If required need to make sure that the label is unique
  # since it is possible to have multiple pods in a single
  # document
  if (!$suppress && $self->UniqueLabels() && defined $self->Label) {
    $paragraph = $self->Label() .'_'. $paragraph;
  }

  return $paragraph;
}


=item B<_create_index>

Similar to C<_create_label> except an index entry is created.
If C<UniqueLabels> is true, the index entry is prefixed by 
the current C<Label> and an exclamation mark.

  $ind = $parser->_create_index($paragraph);

An exclamation mark is used by C<makeindex> to generate 
sub-entries in an index.

=cut

sub _create_index {
  my $self = shift;
  my $paragraph = shift;
  my $suppress = (@_ ? 1 : 0 );

  # Remove latex commands
  $paragraph = $self->_clean_latex_commands($paragraph);

  # If required need to make sure that the index entry is unique
  # since it is possible to have multiple pods in a single
  # document
  if (!$suppress && $self->UniqueLabels() && defined $self->Label) {
    $paragraph = $self->Label() .'!'. $paragraph;
  }

  # Need to replace _ with space
  $paragraph =~ s/_/ /g;

  return $paragraph;

}

=item B<_clean_latex_commands>

Removes latex commands from text. The latex command is assumed to be of the
form C<\command{ text }>. "C<text>" is retained

  $clean = $parser->_clean_latex_commands($text);

=cut

sub _clean_latex_commands {
  my $self = shift;
  my $paragraph = shift;

  # Remove latex commands of the form \text{ }
  # and replace with the contents of the { }
  # need to make this non-greedy so that it can handle
  #  "\text{a} and \text2{b}"
  # without converting it to
  #  "a} and \text2{b"
  # This match will still get into trouble if \} is present 
  # This is not vital since the subsequent replacement of non-alphanumeric
  # characters will tidy it up anyway
  $paragraph =~ s/\\\w+{(.*?)}/$1/g;

  return $paragraph
}

=item B<_split_delimited>

Split the supplied string into two parts at approximately the
specified word boundary. Special care is made to make sure that it
does not split in the middle of some curly brackets.

e.g. "this text is \textbf{very bold}" would not be split into
"this text is \textbf{very" and " bold".

  ($hunk1, $hunk2) = $self->_split_delimited( $para, $length);

The length indicates the maximum length of hunk1.

=cut

# initially Supplied by hsmyers@sdragons.com
# 10/25/01, utility to split \hbox
# busting lines. Reformatted by TimJ to match module style.
sub _split_delimited {
  my $self = shift;
  my $input = shift;
  my $limit = shift;

  # Return immediately if already small
  return ($input, '') if length($input) < $limit;

  my @output;
  my $s = '';
  my $t = '';
  my $depth = 0;
  my $token;

  $input =~ s/\n/ /gm;
  $input .= ' ';
  foreach ( split ( //, $input ) ) {
    $token .= $_;
    if (/\{/) {
      $depth++;
    } elsif ( /}/ ) {
      $depth--;
    } elsif ( / / and $depth == 0) {
      push @output, $token if ( $token and $token ne ' ' );
      $token = '';
    }
  }

  foreach  (@output) {
    if (length($s) < $limit) {
      $s .= $_;
    } else {
      $t .= $_;
    }
  }

  # Tidy up
  $s =~ s/\s+$//;
  $t =~ s/\s+$//;
  return ($s,$t);
}

=back

=end __PRIVATE__

=head1 NOTES

Compatible with C<latex2e> only. Can not be used with C<latex> v2.09
or earlier.

A subclass of C<Pod::Select> so that specific pod sections can be
converted to C<latex> by using the C<select> method.

Some HTML escapes are missing and many have not been tested.

=head1 SEE ALSO

L<Pod::Parser>, L<Pod::Select>, L<pod2latex>

=head1 AUTHORS

Tim Jenness E<lt>tjenness@cpan.orgE<gt>

Bug fixes and improvements have been received from: Simon Cozens
E<lt>simon@cozens.netE<gt>, Mark A. Hershberger
E<lt>mah@everybody.orgE<gt>, Marcel Grunauer
E<lt>marcel@codewerk.comE<gt>, Hugh S Myers
E<lt>hsmyers@sdragons.comE<gt>, Peter J Acklam
E<lt>jacklam@math.uio.noE<gt>, Sudhi Herle E<lt>sudhi@herle.netE<gt>,
Ariel Scolnicov E<lt>ariels@compugen.co.ilE<gt>,
Adriano Rodrigues Ferreira E<lt>ferreira@triang.com.brE<gt> and
R. de Vries E<lt>r.de.vries@dutchspace.nlE<gt>.


=head1 COPYRIGHT

Copyright (C) 2000-2004 Tim Jenness. All Rights Reserved.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=begin __PRIVATE__

=head1 REVISION

$Id: LaTeX.pm,v 1.19 2004/12/30 01:40:44 timj Exp $

=end __PRIVATE__

=cut

1;
