package Memoize::NDBM_File;

=head1 NAME

Memoize::NDBM_File - glue to provide EXISTS for NDBM_File for Storable use

=head1 DESCRIPTION

See L<Memoize>.

=cut

use NDBM_File;
@ISA = qw(NDBM_File);
$VERSION = 0.65;

$Verbose = 0;

sub AUTOLOAD {
  warn "Nonexistent function $AUTOLOAD invoked in Memoize::NDBM_File\n";
}

sub import {
  warn "Importing Memoize::NDBM_File\n" if $Verbose;
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
  warn "Memoize::NDBM_File EXISTS (@_)\n" if $Verbose;
  my $self = shift;
  _backhash($self)  unless exists $keylist{$self};
  my $r = exists $keylist{$self}{$_[0]};
  warn "Memoize::NDBM_File EXISTS (@_) ==> $r\n" if $Verbose;
  $r;
}

sub DEFINED {
  warn "Memoize::NDBM_File DEFINED (@_)\n" if $Verbose;
  my $self = shift;
  _backhash($self)  unless exists $keylist{$self};
  defined $keylist{$self}{$_[0]};
}

sub DESTROY {
  warn "Memoize::NDBM_File DESTROY (@_)\n" if $Verbose;
  my $self = shift;
  delete $keylist{$self};   # So much for reference counting...
  $self->SUPER::DESTROY(@_);
}

# Maybe establish the keylist at TIEHASH time instead?

sub STORE {
  warn "Memoize::NDBM_File STORE (@_)\n" if $VERBOSE;
  my $self = shift;
  $keylist{$self}{$_[0]} = undef;
  $self->SUPER::STORE(@_);
}



# Inherit FETCH and TIEHASH

1;
