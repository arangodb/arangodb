#
# $Id: UTF7.pm,v 2.4 2006/06/03 20:28:48 dankogai Exp $
#
package Encode::Unicode::UTF7;
use strict;
use warnings;
no warnings 'redefine';
use base qw(Encode::Encoding);
__PACKAGE__->Define('UTF-7');
our $VERSION = do { my @r = ( q$Revision: 2.4 $ =~ /\d+/g ); sprintf "%d." . "%02d" x $#r, @r };
use MIME::Base64;
use Encode;

#
# Algorithms taken from Unicode::String by Gisle Aas
#

our $OPTIONAL_DIRECT_CHARS = 1;
my $specials = quotemeta "\'(),-./:?";
$OPTIONAL_DIRECT_CHARS
  and $specials .= quotemeta "!\"#$%&*;<=>@[]^_`{|}";

# \s will not work because it matches U+3000 DEOGRAPHIC SPACE
# We use qr/[\n\r\t\ ] instead
my $re_asis    = qr/(?:[\n\r\t\ A-Za-z0-9$specials])/;
my $re_encoded = qr/(?:[^\n\r\t\ A-Za-z0-9$specials])/;
my $e_utf16    = find_encoding("UTF-16BE");

sub needs_lines { 1 }

sub encode($$;$) {
    my ( $obj, $str, $chk ) = @_;
    my $len = length($str);
    pos($str) = 0;
    my $bytes = '';
    while ( pos($str) < $len ) {
        if ( $str =~ /\G($re_asis+)/ogc ) {
            $bytes .= $1;
        }
        elsif ( $str =~ /\G($re_encoded+)/ogsc ) {
            if ( $1 eq "+" ) {
                $bytes .= "+-";
            }
            else {
                my $s = $1;
                my $base64 = encode_base64( $e_utf16->encode($s), '' );
                $base64 =~ s/=+$//;
                $bytes .= "+$base64-";
            }
        }
        else {
            die "This should not happen! (pos=" . pos($str) . ")";
        }
    }
    $_[1] = '' if $chk;
    return $bytes;
}

sub decode($$;$) {
    my ( $obj, $bytes, $chk ) = @_;
    my $len = length($bytes);
    my $str = "";
    no warnings 'uninitialized';
    while ( pos($bytes) < $len ) {
        if ( $bytes =~ /\G([^+]+)/ogc ) {
            $str .= $1;
        }
        elsif ( $bytes =~ /\G\+-/ogc ) {
            $str .= "+";
        }
        elsif ( $bytes =~ /\G\+([A-Za-z0-9+\/]+)-?/ogsc ) {
            my $base64 = $1;
            my $pad    = length($base64) % 4;
            $base64 .= "=" x ( 4 - $pad ) if $pad;
            $str .= $e_utf16->decode( decode_base64($base64) );
        }
        elsif ( $bytes =~ /\G\+/ogc ) {
            $^W and warn "Bad UTF7 data escape";
            $str .= "+";
        }
        else {
            die "This should not happen " . pos($bytes);
        }
    }
    $_[1] = '' if $chk;
    return $str;
}
1;
__END__

=head1 NAME

Encode::Unicode::UTF7 -- UTF-7 encoding

=head1 SYNOPSIS

    use Encode qw/encode decode/; 
    $utf7 = encode("UTF-7", $utf8);
    $utf8 = decode("UTF-7", $ucs2);

=head1 ABSTRACT

This module implements UTF-7 encoding documented in RFC 2152.  UTF-7,
as its name suggests, is a 7-bit re-encoded version of UTF-16BE.  It
is designed to be MTA-safe and expected to be a standard way to
exchange Unicoded mails via mails.  But with the advent of UTF-8 and
8-bit compliant MTAs, UTF-7 is hardly ever used.

UTF-7 was not supported by Encode until version 1.95 because of that.
But Unicode::String, a module by Gisle Aas which adds Unicode supports
to non-utf8-savvy perl did support UTF-7, the UTF-7 support was added
so Encode can supersede Unicode::String 100%.

=head1 In Practice

When you want to encode Unicode for mails and web pages, however, do
not use UTF-7 unless you are sure your recipients and readers can
handle it.  Very few MUAs and WWW Browsers support these days (only
Mozilla seems to support one).  For general cases, use UTF-8 for
message body and MIME-Header for header instead.

=head1 SEE ALSO

L<Encode>, L<Encode::Unicode>, L<Unicode::String>

RFC 2781 L<http://www.ietf.org/rfc/rfc2152.txt>

=cut
