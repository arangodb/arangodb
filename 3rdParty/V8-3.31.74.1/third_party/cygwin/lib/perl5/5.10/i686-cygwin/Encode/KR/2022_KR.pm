package Encode::KR::2022_KR;
use strict;
use warnings;
our $VERSION = do { my @r = ( q$Revision: 2.2 $ =~ /\d+/g ); sprintf "%d." . "%02d" x $#r, @r };

use Encode qw(:fallbacks);

use base qw(Encode::Encoding);
__PACKAGE__->Define('iso-2022-kr');

sub needs_lines { 1 }

sub perlio_ok {
    return 0;    # for the time being
}

sub decode {
    my ( $obj, $str, $chk ) = @_;
    my $res     = $str;
    my $residue = iso_euc( \$res );

    # This is for PerlIO
    $_[1] = $residue if $chk;
    return Encode::decode( 'euc-kr', $res, FB_PERLQQ );
}

sub encode {
    my ( $obj, $utf8, $chk ) = @_;

    # empty the input string in the stack so perlio is ok
    $_[1] = '' if $chk;
    my $octet = Encode::encode( 'euc-kr', $utf8, FB_PERLQQ );
    euc_iso( \$octet );
    return $octet;
}

use Encode::CJKConstants qw(:all);

# ISO<->EUC

sub iso_euc {
    my $r_str = shift;
    $$r_str =~ s/$RE{'2022_KR'}//gox;    # remove the designator
    $$r_str =~ s{                      # replace characters in GL
     \x0e                              # between SO(\x0e) and SI(\x0f)
     ([^\x0f]*)                        # with characters in GR
     \x0f
        }
    {
                        my $out= $1;
      $out =~ tr/\x21-\x7e/\xa1-\xfe/;
      $out;
    }geox;
    my ($residue) = ( $$r_str =~ s/(\e.*)$//so );
    return $residue;
}

sub euc_iso {
    no warnings qw(uninitialized);
    my $r_str = shift;
    substr( $$r_str, 0, 0 ) =
      $ESC{'2022_KR'};    # put the designator at the beg.
    $$r_str =~
      s{                         # move KS X 1001 characters in GR to GL
        ($RE{EUC_C}+)                     # and enclose them with SO and SI
        }{
            my $str = $1;
            $str =~ tr/\xA1-\xFE/\x21-\x7E/;
            "\x0e" . $str . "\x0f";
        }geox;
    $$r_str;
}

1;
__END__

=head1 NAME

Encode::KR::2022_KR -- internally used by Encode::KR

=cut
