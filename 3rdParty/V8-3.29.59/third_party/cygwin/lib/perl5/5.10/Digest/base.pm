package Digest::base;

use strict;
use vars qw($VERSION);
$VERSION = "1.00";

# subclass is supposed to implement at least these
sub new;
sub clone;
sub add;
sub digest;

sub reset {
    my $self = shift;
    $self->new(@_);  # ugly
}

sub addfile {
    my ($self, $handle) = @_;

    my $n;
    my $buf = "";

    while (($n = read($handle, $buf, 4*1024))) {
        $self->add($buf);
    }
    unless (defined $n) {
	require Carp;
	Carp::croak("Read failed: $!");
    }

    $self;
}

sub add_bits {
    my $self = shift;
    my $bits;
    my $nbits;
    if (@_ == 1) {
	my $arg = shift;
	$bits = pack("B*", $arg);
	$nbits = length($arg);
    }
    else {
	($bits, $nbits) = @_;
    }
    if (($nbits % 8) != 0) {
	require Carp;
	Carp::croak("Number of bits must be multiple of 8 for this algorithm");
    }
    return $self->add(substr($bits, 0, $nbits/8));
}

sub hexdigest {
    my $self = shift;
    return unpack("H*", $self->digest(@_));
}

sub b64digest {
    my $self = shift;
    require MIME::Base64;
    my $b64 = MIME::Base64::encode($self->digest(@_), "");
    $b64 =~ s/=+$//;
    return $b64;
}

1;

__END__

=head1 NAME

Digest::base - Digest base class

=head1 SYNOPSIS

  package Digest::Foo;
  use base 'Digest::base';

=head1 DESCRIPTION

The C<Digest::base> class provide implementations of the methods
C<addfile> and C<add_bits> in terms of C<add>, and of the methods
C<hexdigest> and C<b64digest> in terms of C<digest>.

Digest implementations might want to inherit from this class to get
this implementations of the alternative I<add> and I<digest> methods.
A minimal subclass needs to implement the following methods by itself:

    new
    clone
    add
    digest

The arguments and expected behaviour of these methods are described in
L<Digest>.

=head1 SEE ALSO

L<Digest>
