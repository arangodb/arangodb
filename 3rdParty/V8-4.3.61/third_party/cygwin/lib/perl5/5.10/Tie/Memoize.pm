use strict;
package Tie::Memoize;
use Tie::Hash;
our @ISA = 'Tie::ExtraHash';
our $VERSION = '1.1';

our $exists_token = \undef;

sub croak {require Carp; goto &Carp::croak}

# Format: [0: STORAGE, 1: EXISTS-CACHE, 2: FETCH_function;
#	   3: EXISTS_function, 4: DATA, 5: EXISTS_different ]

sub FETCH {
  my ($h,$key) = ($_[0][0], $_[1]);
  my $res = $h->{$key};
  return $res if defined $res;	# Shortcut if accessible
  return $res if exists $h->{$key}; # Accessible, but undef
  my $cache = $_[0][1]{$key};
  return if defined $cache and not $cache; # Known to not exist
  my @res = $_[0][2]->($key, $_[0][4]);	# Autoload
  $_[0][1]{$key} = 0, return unless @res; # Cache non-existence
  delete $_[0][1]{$key};	# Clear existence cache, not needed any more
  $_[0][0]{$key} = $res[0];	# Store data and return
}

sub EXISTS   {
  my ($a,$key) = (shift, shift);
  return 1 if exists $a->[0]{$key}; # Have data
  my $cache = $a->[1]{$key};
  return $cache if defined $cache; # Existence cache
  my @res = $a->[3]($key,$a->[4]);
  $a->[1]{$key} = 0, return unless @res; # Cache non-existence
  # Now we know it exists
  return ($a->[1]{$key} = 1) if $a->[5]; # Only existence reported
  # Now know the value
  $a->[0]{$key} = $res[0];    # Store data
  return 1
}

sub TIEHASH  {
  croak 'syntax: tie %hash, \'Tie::AutoLoad\', \&fetch_subr' if @_ < 2;
  croak 'syntax: tie %hash, \'Tie::AutoLoad\', \&fetch_subr, $data, \&exists_subr, \%data_cache, \%existence_cache' if @_ > 6;
  push @_, undef if @_ < 3;	# Data
  push @_, $_[1] if @_ < 4;	# exists
  push @_, {} while @_ < 6;	# initial value and caches
  bless [ @_[4,5,1,3,2], $_[1] ne $_[3]], $_[0]
}

1;

=head1 NAME

Tie::Memoize - add data to hash when needed

=head1 SYNOPSIS

  require Tie::Memoize;
  tie %hash, 'Tie::Memoize',
      \&fetch,			# The rest is optional
      $DATA, \&exists,
      {%ini_value}, {%ini_existence};

=head1 DESCRIPTION

This package allows a tied hash to autoload its values on the first access,
and to use the cached value on the following accesses.

Only read-accesses (via fetching the value or C<exists>) result in calls to
the functions; the modify-accesses are performed as on a normal hash.

The required arguments during C<tie> are the hash, the package, and
the reference to the C<FETCH>ing function.  The optional arguments are
an arbitrary scalar $data, the reference to the C<EXISTS> function,
and initial values of the hash and of the existence cache.

Both the C<FETCH>ing function and the C<EXISTS> functions have the
same signature: the arguments are C<$key, $data>; $data is the same
value as given as argument during tie()ing.  Both functions should
return an empty list if the value does not exist.  If C<EXISTS>
function is different from the C<FETCH>ing function, it should return
a TRUE value on success.  The C<FETCH>ing function should return the
intended value if the key is valid.

=head1 Inheriting from B<Tie::Memoize>

The structure of the tied() data is an array reference with elements

  0:  cache of known values
  1:  cache of known existence of keys
  2:  FETCH  function
  3:  EXISTS function
  4:  $data

The rest is for internal usage of this package.  In particular, if
TIEHASH is overwritten, it should call SUPER::TIEHASH.

=head1 EXAMPLE

  sub slurp {
    my ($key, $dir) = shift;
    open my $h, '<', "$dir/$key" or return;
    local $/; <$h>			# slurp it all
  }
  sub exists { my ($key, $dir) = shift; return -f "$dir/$key" }

  tie %hash, 'Tie::Memoize', \&slurp, $directory, \&exists,
      { fake_file1 => $content1, fake_file2 => $content2 },
      { pretend_does_not_exists => 0, known_to_exist => 1 };

This example treats the slightly modified contents of $directory as a
hash.  The modifications are that the keys F<fake_file1> and
F<fake_file2> fetch values $content1 and $content2, and
F<pretend_does_not_exists> will never be accessed.  Additionally, the
existence of F<known_to_exist> is never checked (so if it does not
exists when its content is needed, the user of %hash may be confused).

=head1 BUGS

FIRSTKEY and NEXTKEY methods go through the keys which were already read,
not all the possible keys of the hash.

=head1 AUTHOR

Ilya Zakharevich L<mailto:perl-module-hash-memoize@ilyaz.org>.

=cut

