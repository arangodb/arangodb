package Encode::Guess;
use strict;
use warnings;
use Encode qw(:fallbacks find_encoding);
our $VERSION = do { my @r = ( q$Revision: 2.2 $ =~ /\d+/g ); sprintf "%d." . "%02d" x $#r, @r };

my $Canon = 'Guess';
sub DEBUG () { 0 }
our %DEF_SUSPECTS = map { $_ => find_encoding($_) } qw(ascii utf8);
$Encode::Encoding{$Canon} = bless {
    Name     => $Canon,
    Suspects => {%DEF_SUSPECTS},
} => __PACKAGE__;

use base qw(Encode::Encoding);
sub needs_lines { 1 }
sub perlio_ok   { 0 }

our @EXPORT         = qw(guess_encoding);
our $NoUTFAutoGuess = 0;
our $UTF8_BOM       = pack( "C3", 0xef, 0xbb, 0xbf );

sub import {    # Exporter not used so we do it on our own
    my $callpkg = caller;
    for my $item (@EXPORT) {
        no strict 'refs';
        *{"$callpkg\::$item"} = \&{"$item"};
    }
    set_suspects(@_);
}

sub set_suspects {
    my $class = shift;
    my $self = ref($class) ? $class : $Encode::Encoding{$Canon};
    $self->{Suspects} = {%DEF_SUSPECTS};
    $self->add_suspects(@_);
}

sub add_suspects {
    my $class = shift;
    my $self = ref($class) ? $class : $Encode::Encoding{$Canon};
    for my $c (@_) {
        my $e = find_encoding($c) or die "Unknown encoding: $c";
        $self->{Suspects}{ $e->name } = $e;
        DEBUG and warn "Added: ", $e->name;
    }
}

sub decode($$;$) {
    my ( $obj, $octet, $chk ) = @_;
    my $guessed = guess( $obj, $octet );
    unless ( ref($guessed) ) {
        require Carp;
        Carp::croak($guessed);
    }
    my $utf8 = $guessed->decode( $octet, $chk );
    $_[1] = $octet if $chk;
    return $utf8;
}

sub guess_encoding {
    guess( $Encode::Encoding{$Canon}, @_ );
}

sub guess {
    my $class = shift;
    my $obj   = ref($class) ? $class : $Encode::Encoding{$Canon};
    my $octet = shift;

    # sanity check
    return unless defined $octet and length $octet;

    # cheat 0: utf8 flag;
    if ( Encode::is_utf8($octet) ) {
        return find_encoding('utf8') unless $NoUTFAutoGuess;
        Encode::_utf8_off($octet);
    }

    # cheat 1: BOM
    use Encode::Unicode;
    unless ($NoUTFAutoGuess) {
        my $BOM = pack( 'C3', unpack( "C3", $octet ) );
        return find_encoding('utf8')
          if ( defined $BOM and $BOM eq $UTF8_BOM );
        $BOM = unpack( 'N', $octet );
        return find_encoding('UTF-32')
          if ( defined $BOM and ( $BOM == 0xFeFF or $BOM == 0xFFFe0000 ) );
        $BOM = unpack( 'n', $octet );
        return find_encoding('UTF-16')
          if ( defined $BOM and ( $BOM == 0xFeFF or $BOM == 0xFFFe ) );
        if ( $octet =~ /\x00/o )
        {    # if \x00 found, we assume UTF-(16|32)(BE|LE)
            my $utf;
            my ( $be, $le ) = ( 0, 0 );
            if ( $octet =~ /\x00\x00/o ) {    # UTF-32(BE|LE) assumed
                $utf = "UTF-32";
                for my $char ( unpack( 'N*', $octet ) ) {
                    $char & 0x0000ffff and $be++;
                    $char & 0xffff0000 and $le++;
                }
            }
            else {                            # UTF-16(BE|LE) assumed
                $utf = "UTF-16";
                for my $char ( unpack( 'n*', $octet ) ) {
                    $char & 0x00ff and $be++;
                    $char & 0xff00 and $le++;
                }
            }
            DEBUG and warn "$utf, be == $be, le == $le";
            $be == $le
              and return
              "Encodings ambiguous between $utf BE and LE ($be, $le)";
            $utf .= ( $be > $le ) ? 'BE' : 'LE';
            return find_encoding($utf);
        }
    }
    my %try = %{ $obj->{Suspects} };
    for my $c (@_) {
        my $e = find_encoding($c) or die "Unknown encoding: $c";
        $try{ $e->name } = $e;
        DEBUG and warn "Added: ", $e->name;
    }
    my $nline = 1;
    for my $line ( split /\r\n?|\n/, $octet ) {

        # cheat 2 -- \e in the string
        if ( $line =~ /\e/o ) {
            my @keys = keys %try;
            delete @try{qw/utf8 ascii/};
            for my $k (@keys) {
                ref( $try{$k} ) eq 'Encode::XS' and delete $try{$k};
            }
        }
        my %ok = %try;

        # warn join(",", keys %try);
        for my $k ( keys %try ) {
            my $scratch = $line;
            $try{$k}->decode( $scratch, FB_QUIET );
            if ( $scratch eq '' ) {
                DEBUG and warn sprintf( "%4d:%-24s ok\n", $nline, $k );
            }
            else {
                use bytes ();
                DEBUG
                  and warn sprintf( "%4d:%-24s not ok; %d bytes left\n",
                    $nline, $k, bytes::length($scratch) );
                delete $ok{$k};
            }
        }
        %ok or return "No appropriate encodings found!";
        if ( scalar( keys(%ok) ) == 1 ) {
            my ($retval) = values(%ok);
            return $retval;
        }
        %try = %ok;
        $nline++;
    }
    $try{ascii}
      or return "Encodings too ambiguous: ", join( " or ", keys %try );
    return $try{ascii};
}

1;
__END__

=head1 NAME

Encode::Guess -- Guesses encoding from data

=head1 SYNOPSIS

  # if you are sure $data won't contain anything bogus

  use Encode;
  use Encode::Guess qw/euc-jp shiftjis 7bit-jis/;
  my $utf8 = decode("Guess", $data);
  my $data = encode("Guess", $utf8);   # this doesn't work!

  # more elaborate way
  use Encode::Guess;
  my $enc = guess_encoding($data, qw/euc-jp shiftjis 7bit-jis/);
  ref($enc) or die "Can't guess: $enc"; # trap error this way
  $utf8 = $enc->decode($data);
  # or
  $utf8 = decode($enc->name, $data)

=head1 ABSTRACT

Encode::Guess enables you to guess in what encoding a given data is
encoded, or at least tries to.  

=head1 DESCRIPTION

By default, it checks only ascii, utf8 and UTF-16/32 with BOM.

  use Encode::Guess; # ascii/utf8/BOMed UTF

To use it more practically, you have to give the names of encodings to
check (I<suspects> as follows).  The name of suspects can either be
canonical names or aliases.

CAVEAT: Unlike UTF-(16|32), BOM in utf8 is NOT AUTOMATICALLY STRIPPED.

 # tries all major Japanese Encodings as well
  use Encode::Guess qw/euc-jp shiftjis 7bit-jis/;

If the C<$Encode::Guess::NoUTFAutoGuess> variable is set to a true
value, no heuristics will be applied to UTF8/16/32, and the result
will be limited to the suspects and C<ascii>.

=over 4

=item Encode::Guess->set_suspects

You can also change the internal suspects list via C<set_suspects>
method. 

  use Encode::Guess;
  Encode::Guess->set_suspects(qw/euc-jp shiftjis 7bit-jis/);

=item Encode::Guess->add_suspects

Or you can use C<add_suspects> method.  The difference is that
C<set_suspects> flushes the current suspects list while
C<add_suspects> adds.

  use Encode::Guess;
  Encode::Guess->add_suspects(qw/euc-jp shiftjis 7bit-jis/);
  # now the suspects are euc-jp,shiftjis,7bit-jis, AND
  # euc-kr,euc-cn, and big5-eten
  Encode::Guess->add_suspects(qw/euc-kr euc-cn big5-eten/);

=item Encode::decode("Guess" ...)

When you are content with suspects list, you can now

  my $utf8 = Encode::decode("Guess", $data);

=item Encode::Guess->guess($data)

But it will croak if:

=over

=item *

Two or more suspects remain

=item *

No suspects left

=back

So you should instead try this;

  my $decoder = Encode::Guess->guess($data);

On success, $decoder is an object that is documented in
L<Encode::Encoding>.  So you can now do this;

  my $utf8 = $decoder->decode($data);

On failure, $decoder now contains an error message so the whole thing
would be as follows;

  my $decoder = Encode::Guess->guess($data);
  die $decoder unless ref($decoder);
  my $utf8 = $decoder->decode($data);

=item guess_encoding($data, [, I<list of suspects>])

You can also try C<guess_encoding> function which is exported by
default.  It takes $data to check and it also takes the list of
suspects by option.  The optional suspect list is I<not reflected> to
the internal suspects list.

  my $decoder = guess_encoding($data, qw/euc-jp euc-kr euc-cn/);
  die $decoder unless ref($decoder);
  my $utf8 = $decoder->decode($data);
  # check only ascii and utf8
  my $decoder = guess_encoding($data);

=back

=head1 CAVEATS

=over 4

=item *

Because of the algorithm used, ISO-8859 series and other single-byte
encodings do not work well unless either one of ISO-8859 is the only
one suspect (besides ascii and utf8).

  use Encode::Guess;
  # perhaps ok
  my $decoder = guess_encoding($data, 'latin1');
  # definitely NOT ok
  my $decoder = guess_encoding($data, qw/latin1 greek/);

The reason is that Encode::Guess guesses encoding by trial and error.
It first splits $data into lines and tries to decode the line for each
suspect.  It keeps it going until all but one encoding is eliminated
out of suspects list.  ISO-8859 series is just too successful for most
cases (because it fills almost all code points in \x00-\xff).

=item *

Do not mix national standard encodings and the corresponding vendor
encodings.

  # a very bad idea
  my $decoder
     = guess_encoding($data, qw/shiftjis MacJapanese cp932/);

The reason is that vendor encoding is usually a superset of national
standard so it becomes too ambiguous for most cases.

=item *

On the other hand, mixing various national standard encodings
automagically works unless $data is too short to allow for guessing.

 # This is ok if $data is long enough
 my $decoder =  
  guess_encoding($data, qw/euc-cn
                           euc-jp shiftjis 7bit-jis
                           euc-kr
                           big5-eten/);

=item *

DO NOT PUT TOO MANY SUSPECTS!  Don't you try something like this!

  my $decoder = guess_encoding($data, 
                               Encode->encodings(":all"));

=back

It is, after all, just a guess.  You should alway be explicit when it
comes to encodings.  But there are some, especially Japanese,
environment that guess-coding is a must.  Use this module with care. 

=head1 TO DO

Encode::Guess does not work on EBCDIC platforms.

=head1 SEE ALSO

L<Encode>, L<Encode::Encoding>

=cut

