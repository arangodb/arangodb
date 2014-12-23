#
# $Id: CJKConstants.pm,v 2.2 2006/06/03 20:28:48 dankogai Exp $
#

package Encode::CJKConstants;

use strict;
use warnings;
our $RCSID = q$Id: CJKConstants.pm,v 2.2 2006/06/03 20:28:48 dankogai Exp $;
our $VERSION = do { my @r = ( q$Revision: 2.2 $ =~ /\d+/g ); sprintf "%d." . "%02d" x $#r, @r };

use Carp;

require Exporter;
our @ISA         = qw(Exporter);
our @EXPORT      = qw();
our @EXPORT_OK   = qw(%CHARCODE %ESC %RE);
our %EXPORT_TAGS = ( 'all' => [ @EXPORT_OK, @EXPORT ] );

my %_0208 = (
    1978 => '\e\$\@',
    1983 => '\e\$B',
    1990 => '\e&\@\e\$B',
);

our %CHARCODE = (
    UNDEF_EUC     => "\xa2\xae",    # во in EUC
    UNDEF_SJIS    => "\x81\xac",    # во in SJIS
    UNDEF_JIS     => "\xa2\xf7",    # вў -- used in unicode
    UNDEF_UNICODE => "\x20\x20",    # вў -- used in unicode
);

our %ESC = (
    GB_2312   => "\e\$A",
    JIS_0208  => "\e\$B",
    JIS_0212  => "\e\$(D",
    KSC_5601  => "\e\$(C",
    ASC       => "\e\(B",
    KANA      => "\e\(I",
    '2022_KR' => "\e\$)C",
);

our %RE = (
    ASCII     => '[\x00-\x7f]',
    BIN       => '[\x00-\x06\x7f\xff]',
    EUC_0212  => '\x8f[\xa1-\xfe][\xa1-\xfe]',
    EUC_C     => '[\xa1-\xfe][\xa1-\xfe]',
    EUC_KANA  => '\x8e[\xa1-\xdf]',
    JIS_0208  => "$_0208{1978}|$_0208{1983}|$_0208{1990}",
    JIS_0212  => "\e" . '\$\(D',
    ISO_ASC   => "\e" . '\([BJ]',
    JIS_KANA  => "\e" . '\(I',
    '2022_KR' => "\e" . '\$\)C',
    SJIS_C    => '[\x81-\x9f\xe0-\xfc][\x40-\x7e\x80-\xfc]',
    SJIS_KANA => '[\xa1-\xdf]',
    UTF8      => '[\xc0-\xdf][\x80-\xbf]|[\xe0-\xef][\x80-\xbf][\x80-\xbf]'
);

1;

=head1 NAME

Encode::CJKConstants.pm -- Internally used by Encode::??::ISO_2022_*

=cut

