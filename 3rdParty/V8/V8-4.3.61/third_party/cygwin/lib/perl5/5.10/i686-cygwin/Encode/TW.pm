package Encode::TW;
BEGIN {
    if ( ord("A") == 193 ) {
        die "Encode::TW not supported on EBCDIC\n";
    }
}
use strict;
use warnings;
use Encode;
our $VERSION = do { my @r = ( q$Revision: 2.2 $ =~ /\d+/g ); sprintf "%d." . "%02d" x $#r, @r };
use XSLoader;
XSLoader::load( __PACKAGE__, $VERSION );

1;
__END__

=head1 NAME

Encode::TW - Taiwan-based Chinese Encodings

=head1 SYNOPSIS

    use Encode qw/encode decode/; 
    $big5 = encode("big5", $utf8); # loads Encode::TW implicitly
    $utf8 = decode("big5", $big5); # ditto

=head1 DESCRIPTION

This module implements tradition Chinese charset encodings as used
in Taiwan and Hong Kong.
Encodings supported are as follows.

  Canonical   Alias		Description
  --------------------------------------------------------------------
  big5-eten   /\bbig-?5$/i	Big5 encoding (with ETen extensions)
          /\bbig5-?et(en)?$/i
          /\btca-?big5$/i
  big5-hkscs  /\bbig5-?hk(scs)?$/i
              /\bhk(scs)?-?big5$/i
                                Big5 + Cantonese characters in Hong Kong
  MacChineseTrad		Big5 + Apple Vendor Mappings
  cp950		                Code Page 950 
                                = Big5 + Microsoft vendor mappings
  --------------------------------------------------------------------

To find out how to use this module in detail, see L<Encode>.

=head1 NOTES

Due to size concerns, C<EUC-TW> (Extended Unix Character), C<CCCII>
(Chinese Character Code for Information Interchange), C<BIG5PLUS>
(CMEX's Big5+) and C<BIG5EXT> (CMEX's Big5e) are distributed separately
on CPAN, under the name L<Encode::HanExtra>. That module also contains
extra China-based encodings.

=head1 BUGS

Since the original C<big5> encoding (1984) is not supported anywhere
(glibc and DOS-based systems uses C<big5> to mean C<big5-eten>; Microsoft
uses C<big5> to mean C<cp950>), a conscious decision was made to alias
C<big5> to C<big5-eten>, which is the de facto superset of the original
big5.

The C<CNS11643> encoding files are not complete. For common C<CNS11643>
manipulation, please use C<EUC-TW> in L<Encode::HanExtra>, which contains
planes 1-7.

The ASCII region (0x00-0x7f) is preserved for all encodings, even
though this conflicts with mappings by the Unicode Consortium.  See

L<http://www.debian.or.jp/~kubota/unicode-symbols.html.en>

to find out why it is implemented that way.

=head1 SEE ALSO

L<Encode>

=cut
