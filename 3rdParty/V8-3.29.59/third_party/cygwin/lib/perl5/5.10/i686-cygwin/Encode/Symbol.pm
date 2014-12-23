package Encode::Symbol;
use strict;
use warnings;
use Encode;
our $VERSION = do { my @r = ( q$Revision: 2.2 $ =~ /\d+/g ); sprintf "%d." . "%02d" x $#r, @r };

use XSLoader;
XSLoader::load( __PACKAGE__, $VERSION );

1;
__END__

=head1 NAME

Encode::Symbol - Symbol Encodings

=head1 SYNOPSIS

    use Encode qw/encode decode/; 
    $symbol  = encode("symbol", $utf8); # loads Encode::Symbol implicitly
    $utf8 = decode("", $symbol);        # ditto

=head1 ABSTRACT

This module implements symbol and dingbats encodings.  Encodings
supported are as follows.   

  Canonical   Alias		Description
  --------------------------------------------------------------------
  symbol
  dingbats
  AdobeZDingbat
  AdobeSymbol
  MacDingbats

=head1 DESCRIPTION

To find out how to use this module in detail, see L<Encode>.

=head1 SEE ALSO

L<Encode>

=cut
