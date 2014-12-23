package Tie::RefHash;

use vars qw/$VERSION/;

$VERSION = "1.38";

use 5.005;

=head1 NAME

Tie::RefHash - use references as hash keys

=head1 SYNOPSIS

    require 5.004;
    use Tie::RefHash;
    tie HASHVARIABLE, 'Tie::RefHash', LIST;
    tie HASHVARIABLE, 'Tie::RefHash::Nestable', LIST;

    untie HASHVARIABLE;

=head1 DESCRIPTION

This module provides the ability to use references as hash keys if you
first C<tie> the hash variable to this module.  Normally, only the
keys of the tied hash itself are preserved as references; to use
references as keys in hashes-of-hashes, use Tie::RefHash::Nestable,
included as part of Tie::RefHash.

It is implemented using the standard perl TIEHASH interface.  Please
see the C<tie> entry in perlfunc(1) and perltie(1) for more information.

The Nestable version works by looking for hash references being stored
and converting them to tied hashes so that they too can have
references as keys.  This will happen without warning whenever you
store a reference to one of your own hashes in the tied hash.

=head1 EXAMPLE

    use Tie::RefHash;
    tie %h, 'Tie::RefHash';
    $a = [];
    $b = {};
    $c = \*main;
    $d = \"gunk";
    $e = sub { 'foo' };
    %h = ($a => 1, $b => 2, $c => 3, $d => 4, $e => 5);
    $a->[0] = 'foo';
    $b->{foo} = 'bar';
    for (keys %h) {
       print ref($_), "\n";
    }

    tie %h, 'Tie::RefHash::Nestable';
    $h{$a}->{$b} = 1;
    for (keys %h, keys %{$h{$a}}) {
       print ref($_), "\n";
    }

=head1 THREAD SUPPORT

L<Tie::RefHash> fully supports threading using the C<CLONE> method.

=head1 STORABLE SUPPORT

L<Storable> hooks are provided for semantically correct serialization and
cloning of tied refhashes.

=head1 RELIC SUPPORT

This version of Tie::RefHash seems to no longer work with 5.004. This has not
been throughly investigated. Patches welcome ;-)

=head1 MAINTAINER

Yuval Kogman E<lt>nothingmuch@woobling.orgE<gt>

=head1 AUTHOR

Gurusamy Sarathy        gsar@activestate.com

'Nestable' by Ed Avis   ed@membled.com

=head1 SEE ALSO

perl(1), perlfunc(1), perltie(1)

=cut

use Tie::Hash;
use vars '@ISA';
@ISA = qw(Tie::Hash);
use strict;
use Carp qw/croak/;

BEGIN {
  local $@;
  # determine whether we need to take care of threads
  use Config ();
  my $usethreads = $Config::Config{usethreads}; # && exists $INC{"threads.pm"}
  *_HAS_THREADS = $usethreads ? sub () { 1 } : sub () { 0 };
  *_HAS_SCALAR_UTIL = eval { require Scalar::Util; 1 } ? sub () { 1 } : sub () { 0 };
  *_HAS_WEAKEN = defined(&Scalar::Util::weaken) ? sub () { 1 } : sub () { 0 };
}

BEGIN {
  # create a refaddr function

  local $@;

  if ( _HAS_SCALAR_UTIL ) {
    Scalar::Util->import("refaddr");
  } else {
    require overload;

    *refaddr = sub {
      if ( overload::StrVal($_[0]) =~ /\( 0x ([a-zA-Z0-9]+) \)$/x) {
          return $1;
      } else {
        die "couldn't parse StrVal: " . overload::StrVal($_[0]);
      }
    };
  }
}

my (@thread_object_registry, $count); # used by the CLONE method to rehash the keys after their refaddr changed

sub TIEHASH {
  my $c = shift;
  my $s = [];
  bless $s, $c;
  while (@_) {
    $s->STORE(shift, shift);
  }

  if (_HAS_THREADS ) {

    if ( _HAS_WEAKEN ) {
      # remember the object so that we can rekey it on CLONE
      push @thread_object_registry, $s;
      # but make this a weak reference, so that there are no leaks
      Scalar::Util::weaken( $thread_object_registry[-1] );

      if ( ++$count > 1000 ) {
        # this ensures we don't fill up with a huge array dead weakrefs
        @thread_object_registry = grep { defined } @thread_object_registry;
        $count = 0;
      }
    } else {
      $count++; # used in the warning
    }
  }

  return $s;
}

my $storable_format_version = join("/", __PACKAGE__, "0.01");

sub STORABLE_freeze {
  my ( $self, $is_cloning ) = @_;
  my ( $refs, $reg ) = @$self;
  return ( $storable_format_version, [ values %$refs ], $reg );
}

sub STORABLE_thaw {
  my ( $self, $is_cloning, $version, $refs, $reg ) = @_;
  croak "incompatible versions of Tie::RefHash between freeze and thaw"
    unless $version eq $storable_format_version;

  @$self = ( {}, $reg );
  $self->_reindex_keys( $refs );
}

sub CLONE {
  my $pkg = shift;

  if ( $count and not _HAS_WEAKEN ) {
    warn "Tie::RefHash is not threadsafe without Scalar::Util::weaken";
  }

  # when the thread has been cloned all the objects need to be updated.
  # dead weakrefs are undefined, so we filter them out
  @thread_object_registry = grep { defined && do { $_->_reindex_keys; 1 } } @thread_object_registry;
  $count = 0; # we just cleaned up
}

sub _reindex_keys {
  my ( $self, $extra_keys ) = @_;
  # rehash all the ref keys based on their new StrVal
  %{ $self->[0] } = map { refaddr($_->[0]) => $_ } (values(%{ $self->[0] }), @{ $extra_keys || [] });
}

sub FETCH {
  my($s, $k) = @_;
  if (ref $k) {
      my $kstr = refaddr($k);
      if (defined $s->[0]{$kstr}) {
        $s->[0]{$kstr}[1];
      }
      else {
        undef;
      }
  }
  else {
      $s->[1]{$k};
  }
}

sub STORE {
  my($s, $k, $v) = @_;
  if (ref $k) {
    $s->[0]{refaddr($k)} = [$k, $v];
  }
  else {
    $s->[1]{$k} = $v;
  }
  $v;
}

sub DELETE {
  my($s, $k) = @_;
  (ref $k)
    ? (delete($s->[0]{refaddr($k)}) || [])->[1]
    : delete($s->[1]{$k});
}

sub EXISTS {
  my($s, $k) = @_;
  (ref $k) ? exists($s->[0]{refaddr($k)}) : exists($s->[1]{$k});
}

sub FIRSTKEY {
  my $s = shift;
  keys %{$s->[0]};  # reset iterator
  keys %{$s->[1]};  # reset iterator
  $s->[2] = 0;      # flag for iteration, see NEXTKEY
  $s->NEXTKEY;
}

sub NEXTKEY {
  my $s = shift;
  my ($k, $v);
  if (!$s->[2]) {
    if (($k, $v) = each %{$s->[0]}) {
      return $v->[0];
    }
    else {
      $s->[2] = 1;
    }
  }
  return each %{$s->[1]};
}

sub CLEAR {
  my $s = shift;
  $s->[2] = 0;
  %{$s->[0]} = ();
  %{$s->[1]} = ();
}

package Tie::RefHash::Nestable;
use vars '@ISA';
@ISA = 'Tie::RefHash';

sub STORE {
  my($s, $k, $v) = @_;
  if (ref($v) eq 'HASH' and not tied %$v) {
      my @elems = %$v;
      tie %$v, ref($s), @elems;
  }
  $s->SUPER::STORE($k, $v);
}

1;
