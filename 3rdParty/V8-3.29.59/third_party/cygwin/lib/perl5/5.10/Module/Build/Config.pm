package Module::Build::Config;

use strict;
use vars qw($VERSION);
$VERSION = '0.2808_01';
$VERSION = eval $VERSION;
use Config;

sub new {
  my ($pack, %args) = @_;
  return bless {
		stack => {},
		values => $args{values} || {},
	       }, $pack;
}

sub get {
  my ($self, $key) = @_;
  return $self->{values}{$key} if ref($self) && exists $self->{values}{$key};
  return $Config{$key};
}

sub set {
  my ($self, $key, $val) = @_;
  $self->{values}{$key} = $val;
}

sub push {
  my ($self, $key, $val) = @_;
  push @{$self->{stack}{$key}}, $self->{values}{$key}
    if exists $self->{values}{$key};
  $self->{values}{$key} = $val;
}

sub pop {
  my ($self, $key) = @_;

  my $val = delete $self->{values}{$key};
  if ( exists $self->{stack}{$key} ) {
    $self->{values}{$key} = pop @{$self->{stack}{$key}};
    delete $self->{stack}{$key} unless @{$self->{stack}{$key}};
  }

  return $val;
}

sub values_set {
  my $self = shift;
  return undef unless ref($self);
  return $self->{values};
}

sub all_config {
  my $self = shift;
  my $v = ref($self) ? $self->{values} : {};
  return {%Config, %$v};
}

1;
