package Encode::CN::HZ;

use strict;
use warnings;
use utf8 ();

use vars qw($VERSION);
$VERSION = do { my @r = ( q$Revision: 2.5 $ =~ /\d+/g ); sprintf "%d." . "%02d" x $#r, @r };

use Encode qw(:fallbacks);

use base qw(Encode::Encoding);
__PACKAGE__->Define('hz');

# HZ is a combination of ASCII and escaped GB, so we implement it
# with the GB2312(raw) encoding here. Cf. RFCs 1842 & 1843.

# not ported for EBCDIC.  Which should be used, "~" or "\x7E"?

sub needs_lines { 1 }

sub decode ($$;$) {
    my ( $obj, $str, $chk ) = @_;

    my $GB  = Encode::find_encoding('gb2312-raw');
    my $ret = '';
    my $in_ascii = 1;    # default mode is ASCII.

    while ( length $str ) {
        if ($in_ascii) {    # ASCII mode
            if ( $str =~ s/^([\x00-\x7D\x7F]+)// ) {    # no '~' => ASCII
                $ret .= $1;

                # EBCDIC should need ascii2native, but not ported.
            }
            elsif ( $str =~ s/^\x7E\x7E// ) {           # escaped tilde
                $ret .= '~';
            }
            elsif ( $str =~ s/^\x7E\cJ// ) {    # '\cJ' == LF in ASCII
                1;                              # no-op
            }
            elsif ( $str =~ s/^\x7E\x7B// ) {    # '~{'
                $in_ascii = 0;                   # to GB
            }
            else {    # encounters an invalid escape, \x80 or greater
                last;
            }
        }
        else {        # GB mode; the byte ranges are as in RFC 1843.
            no warnings 'uninitialized';
            if ( $str =~ s/^((?:[\x21-\x77][\x21-\x7E])+)// ) {
                $ret .= $GB->decode( $1, $chk );
            }
            elsif ( $str =~ s/^\x7E\x7D// ) {    # '~}'
                $in_ascii = 1;
            }
            else {                               # invalid
                last;
            }
        }
    }
    $_[1] = '' if $chk;    # needs_lines guarantees no partial character
    return $ret;
}

sub cat_decode {
    my ( $obj, undef, $src, $pos, $trm, $chk ) = @_;
    my ( $rdst, $rsrc, $rpos ) = \@_[ 1 .. 3 ];

    my $GB  = Encode::find_encoding('gb2312-raw');
    my $ret = '';
    my $in_ascii = 1;      # default mode is ASCII.

    my $ini_pos = pos($$rsrc);

    substr( $src, 0, $pos ) = '';

    my $ini_len = bytes::length($src);

    # $trm is the first of the pair '~~', then 2nd tilde is to be removed.
    # XXX: Is better C<$src =~ s/^\x7E// or die if ...>?
    $src =~ s/^\x7E// if $trm eq "\x7E";

    while ( length $src ) {
        my $now;
        if ($in_ascii) {    # ASCII mode
            if ( $src =~ s/^([\x00-\x7D\x7F])// ) {    # no '~' => ASCII
                $now = $1;
            }
            elsif ( $src =~ s/^\x7E\x7E// ) {          # escaped tilde
                $now = '~';
            }
            elsif ( $src =~ s/^\x7E\cJ// ) {    # '\cJ' == LF in ASCII
                next;
            }
            elsif ( $src =~ s/^\x7E\x7B// ) {    # '~{'
                $in_ascii = 0;                   # to GB
                next;
            }
            else {    # encounters an invalid escape, \x80 or greater
                last;
            }
        }
        else {        # GB mode; the byte ranges are as in RFC 1843.
            if ( $src =~ s/^((?:[\x21-\x77][\x21-\x7F])+)// ) {
                $now = $GB->decode( $1, $chk );
            }
            elsif ( $src =~ s/^\x7E\x7D// ) {    # '~}'
                $in_ascii = 1;
                next;
            }
            else {                               # invalid
                last;
            }
        }

        next if !defined $now;

        $ret .= $now;

        if ( $now eq $trm ) {
            $$rdst .= $ret;
            $$rpos = $ini_pos + $pos + $ini_len - bytes::length($src);
            pos($$rsrc) = $ini_pos;
            return 1;
        }
    }

    $$rdst .= $ret;
    $$rpos = $ini_pos + $pos + $ini_len - bytes::length($src);
    pos($$rsrc) = $ini_pos;
    return '';    # terminator not found
}

sub encode($$;$) {
    my ( $obj, $str, $chk ) = @_;

    my $GB  = Encode::find_encoding('gb2312-raw');
    my $ret = '';
    my $in_ascii = 1;    # default mode is ASCII.

    no warnings 'utf8';  # $str may be malformed UTF8 at the end of a chunk.

    while ( length $str ) {
        if ( $str =~ s/^([[:ascii:]]+)// ) {
            my $tmp = $1;
            $tmp =~ s/~/~~/g;    # escapes tildes
            if ( !$in_ascii ) {
                $ret .= "\x7E\x7D";    # '~}'
                $in_ascii = 1;
            }
            $ret .= pack 'a*', $tmp;    # remove UTF8 flag.
        }
        elsif ( $str =~ s/(.)// ) {
            my $s = $1;
            my $tmp = $GB->encode( $s, $chk );
            last if !defined $tmp;
            if ( length $tmp == 2 ) {    # maybe a valid GB char (XXX)
                if ($in_ascii) {
                    $ret .= "\x7E\x7B";    # '~{'
                    $in_ascii = 0;
                }
                $ret .= $tmp;
            }
            elsif ( length $tmp ) {        # maybe FALLBACK in ASCII (XXX)
                if ( !$in_ascii ) {
                    $ret .= "\x7E\x7D";    # '~}'
                    $in_ascii = 1;
                }
                $ret .= $tmp;
            }
        }
        else {    # if $str is malformed UTF8 *and* if length $str != 0.
            last;
        }
    }
    $_[1] = $str if $chk;

    # The state at the end of the chunk is discarded, even if in GB mode.
    # That results in the combination of GB-OUT and GB-IN, i.e. "~}~{".
    # Parhaps it is harmless, but further investigations may be required...

    if ( !$in_ascii ) {
        $ret .= "\x7E\x7D";    # '~}'
        $in_ascii = 1;
    }
    utf8::encode($ret); # https://rt.cpan.org/Ticket/Display.html?id=35120
    return $ret;
}

1;
__END__

=head1 NAME

Encode::CN::HZ -- internally used by Encode::CN

=cut
