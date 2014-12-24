package URI::WithBase;

use strict;
use vars qw($AUTOLOAD $VERSION);
use URI;

$VERSION = "2.19";

use overload '""' => "as_string", fallback => 1;

sub as_string;  # help overload find it

sub new
{
    my($class, $uri, $base) = @_;
    my $ibase = $base;
    if ($base && ref($base) && UNIVERSAL::isa($base, __PACKAGE__)) {
	$base = $base->abs;
	$ibase = $base->[0];
    }
    bless [URI->new($uri, $ibase), $base], $class;
}

sub new_abs
{
    my $class = shift;
    my $self = $class->new(@_);
    $self->abs;
}

sub _init
{
    my $class = shift;
    my($str, $scheme) = @_;
    bless [URI->new($str, $scheme), undef], $class;
}

sub eq
{
    my($self, $other) = @_;
    $other = $other->[0] if UNIVERSAL::isa($other, __PACKAGE__);
    $self->[0]->eq($other);
}

sub AUTOLOAD
{
    my $self = shift;
    my $method = substr($AUTOLOAD, rindex($AUTOLOAD, '::')+2);
    return if $method eq "DESTROY";
    $self->[0]->$method(@_);
}

sub can {                                  # override UNIVERSAL::can
    my $self = shift;
    $self->SUPER::can(@_) || (
      ref($self)
      ? $self->[0]->can(@_)
      : undef
    )
}

sub base {
    my $self = shift;
    my $base  = $self->[1];

    if (@_) { # set
	my $new_base = shift;
	# ensure absoluteness
	$new_base = $new_base->abs if ref($new_base) && $new_base->isa(__PACKAGE__);
	$self->[1] = $new_base;
    }
    return unless defined wantarray;

    # The base attribute supports 'lazy' conversion from URL strings
    # to URL objects. Strings may be stored but when a string is
    # fetched it will automatically be converted to a URL object.
    # The main benefit is to make it much cheaper to say:
    #   URI::WithBase->new($random_url_string, 'http:')
    if (defined($base) && !ref($base)) {
	$base = ref($self)->new($base);
	$self->[1] = $base unless @_;
    }
    $base;
}

sub clone
{
    my $self = shift;
    my $base = $self->[1];
    $base = $base->clone if ref($base);
    bless [$self->[0]->clone, $base], ref($self);
}

sub abs
{
    my $self = shift;
    my $base = shift || $self->base || return $self->clone;
    $base = $base->as_string if ref($base);
    bless [$self->[0]->abs($base, @_), $base], ref($self);
}

sub rel
{
    my $self = shift;
    my $base = shift || $self->base || return $self->clone;
    $base = $base->as_string if ref($base);
    bless [$self->[0]->rel($base, @_), $base], ref($self);
}

1;

__END__

=head1 NAME

URI::WithBase - URIs which remember their base

=head1 SYNOPSIS

 $u1 = URI::WithBase->new($str, $base);
 $u2 = $u1->abs;

 $base = $u1->base;
 $u1->base( $new_base )

=head1 DESCRIPTION

This module provides the C<URI::WithBase> class.  Objects of this class
are like C<URI> objects, but can keep their base too.  The base
represents the context where this URI was found and can be used to
absolutize or relativize the URI.  All the methods described in L<URI>
are supported for C<URI::WithBase> objects.

The methods provided in addition to or modified from those of C<URI> are:

=over 4

=item $uri = URI::WithBase->new($str, [$base])

The constructor takes an optional base URI as the second argument.
If provided, this argument initializes the base attribute.

=item $uri->base( [$new_base] )

Can be used to get or set the value of the base attribute.
The return value, which is the old value, is a URI object or C<undef>.

=item $uri->abs( [$base_uri] )

The $base_uri argument is now made optional as the object carries its
base with it.  A new object is returned even if $uri is already
absolute (while plain URI objects simply return themselves in
that case).

=item $uri->rel( [$base_uri] )

The $base_uri argument is now made optional as the object carries its
base with it.  A new object is always returned.

=back


=head1 SEE ALSO

L<URI>

=head1 COPYRIGHT

Copyright 1998-2002 Gisle Aas.

=cut
