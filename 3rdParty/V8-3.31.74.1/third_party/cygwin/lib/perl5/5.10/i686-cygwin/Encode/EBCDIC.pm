package Encode::EBCDIC;
use strict;
use warnings;
use Encode;
our $VERSION = do { my @r = ( q$Revision: 2.2 $ =~ /\d+/g ); sprintf "%d." . "%02d" x $#r, @r };

use XSLoader;
XSLoader::load( __PACKAGE__, $VERSION );

1;
__END__

=head1 NAME

Encode::EBCDIC - EBCDIC Encodings

=head1 SYNOPSIS

    use Encode qw/encode decode/; 
    $posix_bc  = encode("posix-bc", $utf8); # loads Encode::EBCDIC implicitly
    $utf8 = decode("", $posix_bc);          # ditto

=head1 ABSTRACT

This module implements various EBCDIC-Based encodings.  Encodings
supported are as follows.   

  Canonical   Alias		Description
  --------------------------------------------------------------------
  cp37  
  cp500  
  cp875  
  cp1026  
  cp1047  
  posix-bc

=head1 DESCRIPTION

To find how to use this module in detail, see L<Encode>.

=head1 SEE ALSO

L<Encode>, L<perlebcdic>

=cut
