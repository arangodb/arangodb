package Memoize::ExpireFile;

=head1 NAME

Memoize::ExpireFile - test for Memoize expiration semantics

=head1 DESCRIPTION

See L<Memoize::Expire>.

=cut

$VERSION = 1.01;
use Carp;

my $Zero = pack("N", 0);

sub TIEHASH {
  my ($package, %args) = @_;
  my $cache = $args{HASH} || {};
  bless {ARGS => \%args, C => $cache} => $package;
}


sub STORE {
#  print "Expiry manager STORE handler\n";
  my ($self, $key, $data) = @_;
  my $cache = $self->{C};
  my $cur_date = pack("N", (stat($key))[9]);
  $cache->{"C$key"} = $data;
  $cache->{"T$key"} = $cur_date;
}

sub FETCH {
  my ($self, $key) = @_;
  $self->{C}{"C$key"};
}

sub EXISTS {
#  print "Expiry manager EXISTS handler\n";
  my ($self, $key) = @_;
  my $cache_date = $self->{C}{"T$key"} || $Zero;
  my $file_date = pack("N", (stat($key))[9]);#
#  if ($self->{ARGS}{CHECK_DATE} && $old_date gt $cur_date) {
#    return $self->{ARGS}{CHECK_DATE}->($key, $old_date, $cur_date);
#  } 
  my $res = $cache_date ge $file_date;
#  print $res ? "... still good\n" : "... expired\n";
  $res;
}

1;
