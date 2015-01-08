
require 5;
## This module is to be use()'d only by Pod::Simple::Transcode

package Pod::Simple::TranscodeDumb;
use strict;
use vars qw($VERSION %Supported);
$VERSION = '2.02';
# This module basically pretends it knows how to transcode, except
#  only for null-transcodings!  We use this when Encode isn't
#  available.

%Supported = (
  'ascii'       => 1,
  'ascii-ctrl'  => 1,
  'iso-8859-1'  => 1,
  'null'        => 1,
  'latin1'      => 1,
  'latin-1'     => 1,
  %Supported,
);

sub is_dumb  {1}
sub is_smart {0}

sub all_encodings {
  return sort keys %Supported;
}

sub encoding_is_available {
  return exists $Supported{lc $_[1]};
}

sub encmodver {
  return __PACKAGE__ . " v" .($VERSION || '?');
}

sub make_transcoder {
  my($e) = $_[1];
  die "WHAT ENCODING!?!?" unless $e;
  my $x;
  return sub {;
    #foreach $x (@_) {
    #  if(Pod::Simple::ASCII and !Pod::Simple::UNICODE and $] > 5.005) {
    #    # We're in horrible gimp territory, so we need to knock out
    #    # all the highbit things
    #    $x =
    #      pack 'C*',
    #      map {; ($_ < 128) ? $_ : 0x7e }
    #      unpack "C*",
    #      $x
    #    ;
    #  }
    #}
    #
    #return;
  };
}


1;


