package Memoize::SDBM_File;

=head1 NAME

Memoize::SDBM_File - glue to provide EXISTS for SDBM_File for Storable use

=head1 DESCRIPTION

See L<Memoize>.

=cut

use SDBM_File;
@ISA = qw(SDBM_File);
$VERSION = 0.65;

$Verbose = 0;

sub AUTOLOAD {
  warn "Nonexistent function $AUTOLOAD invoked in Memoize::SDBM_File\n";
}

sub import {
  warn "Importing Memoize::SDBM_File\n" if $Verbose;
}


my %keylist;

# This is so ridiculous...
sub _backhash {
  my $self = shift;
  my %fakehash;
  my $k; 
  for ($k = $self->FIRSTKEY(); defined $k; $k = $self->NEXTKEY($k)) {
    $fakehash{$k} = undef;
  }
  $keylist{$self} = \%fakehash;
}

sub EXISTS {
  warn "Memoize::SDBM_File EXISTS (@_)\n" if $Verbose;
  my $self = shift;
  _backhash($self)  unless exists $keylist{$self};
  my $r = exists $keylist{$self}{$_[0]};
  warn "Memoize::SDBM_File EXISTS (@_) ==> $r\n" if $Verbose;
  $r;
}

sub DEFINED {
  warn "Memoize::SDBM_File DEFINED (@_)\n" if $Verbose;
  my $self = shift;
  _backhash($self)  unless exists $keylist{$self};
  defined $keylist{$self}{$_[0]};
}

sub DESTROY {
  warn "Memoize::SDBM_File DESTROY (@_)\n" if $Verbose;
  my $self = shift;
  delete $keylist{$self};   # So much for reference counting...
  $self->SUPER::DESTROY(@_);
}

# Maybe establish the keylist at TIEHASH time instead?

sub STORE {
  warn "Memoize::SDBM_File STORE (@_)\n" if $VERBOSE;
  my $self = shift;
  $keylist{$self}{$_[0]} = undef;
  $self->SUPER::STORE(@_);
}

# Inherit FETCH and TIEHASH

1;
