package Memoize::Storable;

=head1 NAME

Memoize::Storable - store Memoized data in Storable database

=head1 DESCRIPTION

See L<Memoize>.

=cut

use Storable ();
$VERSION = 0.65;
$Verbose = 0;

sub TIEHASH {
  require Carp if $Verbose;
  my $package = shift;
  my $filename = shift;
  my $truehash = (-e $filename) ? Storable::retrieve($filename) : {};
  my %options;
  print STDERR "Memoize::Storable::TIEHASH($filename, @_)\n" if $Verbose;
  @options{@_} = ();
  my $self = 
    {FILENAME => $filename, 
     H => $truehash, 
     OPTIONS => \%options
    };
  bless $self => $package;
}

sub STORE {
  require Carp if $Verbose;
  my $self = shift;
  print STDERR "Memoize::Storable::STORE(@_)\n" if $Verbose;
  $self->{H}{$_[0]} = $_[1];
}

sub FETCH {
  require Carp if $Verbose;
  my $self = shift;
  print STDERR "Memoize::Storable::FETCH(@_)\n" if $Verbose;
  $self->{H}{$_[0]};
}

sub EXISTS {
  require Carp if $Verbose;
  my $self = shift;
  print STDERR "Memoize::Storable::EXISTS(@_)\n" if $Verbose;
  exists $self->{H}{$_[0]};
}

sub DESTROY {
  require Carp if $Verbose;
  my $self= shift;
  print STDERR "Memoize::Storable::DESTROY(@_)\n" if $Verbose;
  if ($self->{OPTIONS}{'nstore'}) {
    Storable::nstore($self->{H}, $self->{FILENAME});
  } else {
    Storable::store($self->{H}, $self->{FILENAME});
  }
}

sub FIRSTKEY {
  'Fake hash from Memoize::Storable';
}

sub NEXTKEY {
  undef;
}
1;
