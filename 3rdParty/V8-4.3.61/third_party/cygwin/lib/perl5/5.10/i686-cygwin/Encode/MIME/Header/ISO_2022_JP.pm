package Encode::MIME::Header::ISO_2022_JP;

use strict;
use warnings;

use base qw(Encode::MIME::Header);

$Encode::Encoding{'MIME-Header-ISO_2022_JP'} =
  bless { encode => 'B', bpl => 76, Name => 'MIME-Header-ISO_2022_JP' } =>
  __PACKAGE__;

use constant HEAD => '=?ISO-2022-JP?B?';
use constant TAIL => '?=';

use Encode::CJKConstants qw(%RE);

our $VERSION = do { my @r = ( q$Revision: 1.3 $ =~ /\d+/g ); sprintf "%d." . "%02d" x $#r, @r };

# I owe the below codes totally to
#   Jcode by Dan Kogai & http://www.din.or.jp/~ohzaki/perl.htm#JP_Base64

sub encode {
    my $self = shift;
    my $str  = shift;

    utf8::encode($str) if ( Encode::is_utf8($str) );
    Encode::from_to( $str, 'utf8', 'euc-jp' );

    my ($trailing_crlf) = ( $str =~ /(\n|\r|\x0d\x0a)$/o );

    $str = _mime_unstructured_header( $str, $self->{bpl} );

    not $trailing_crlf and $str =~ s/(\n|\r|\x0d\x0a)$//o;

    return $str;
}

sub _mime_unstructured_header {
    my ( $oldheader, $bpl ) = @_;
    my $crlf = $oldheader =~ /\n$/;
    my ( $header, @words, @wordstmp, $i ) = ('');

    $oldheader =~ s/\s+$//;

    @wordstmp = split /\s+/, $oldheader;

    for ( $i = 0 ; $i < $#wordstmp ; $i++ ) {
        if (    $wordstmp[$i] !~ /^[\x21-\x7E]+$/
            and $wordstmp[ $i + 1 ] !~ /^[\x21-\x7E]+$/ )
        {
            $wordstmp[ $i + 1 ] = "$wordstmp[$i] $wordstmp[$i + 1]";
        }
        else {
            push( @words, $wordstmp[$i] );
        }
    }

    push( @words, $wordstmp[-1] );

    for my $word (@words) {
        if ( $word =~ /^[\x21-\x7E]+$/ ) {
            $header =~ /(?:.*\n)*(.*)/;
            if ( length($1) + length($word) > $bpl ) {
                $header .= "\n $word";
            }
            else {
                $header .= $word;
            }
        }
        else {
            $header = _add_encoded_word( $word, $header, $bpl );
        }

        $header =~ /(?:.*\n)*(.*)/;

        if ( length($1) == $bpl ) {
            $header .= "\n ";
        }
        else {
            $header .= ' ';
        }
    }

    $header =~ s/\n? $//mg;

    $crlf ? "$header\n" : $header;
}

sub _add_encoded_word {
    my ( $str, $line, $bpl ) = @_;
    my $result = '';

    while ( length($str) ) {
        my $target = $str;
        $str = '';

        if (
            length($line) + 22 +
            ( $target =~ /^(?:$RE{EUC_0212}|$RE{EUC_C})/o ) * 8 > $bpl )
        {
            $line =~ s/[ \t\n\r]*$/\n/;
            $result .= $line;
            $line = ' ';
        }

        while (1) {
            my $iso_2022_jp = $target;
            Encode::from_to( $iso_2022_jp, 'euc-jp', 'iso-2022-jp' );

            my $encoded =
              HEAD . MIME::Base64::encode_base64( $iso_2022_jp, '' ) . TAIL;

            if ( length($encoded) + length($line) > $bpl ) {
                $target =~
                  s/($RE{EUC_0212}|$RE{EUC_KANA}|$RE{EUC_C}|$RE{ASCII})$//o;
                $str = $1 . $str;
            }
            else {
                $line .= $encoded;
                last;
            }
        }

    }

    $result . $line;
}

1;
__END__

