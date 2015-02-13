package Tie::Scalar;

our $VERSION = '1.00';

=head1 NAME

Tie::Scalar, Tie::StdScalar - base class definitions for tied scalars

=head1 SYNOPSIS

    package NewScalar;
    require Tie::Scalar;

    @ISA = qw(Tie::Scalar);

    sub FETCH { ... }		# Provide a needed method
    sub TIESCALAR { ... }	# Overrides inherited method


    package NewStdScalar;
    require Tie::Scalar;

    @ISA = qw(Tie::StdScalar);

    # All methods provided by default, so define only what needs be overridden
    sub FETCH { ... }


    package main;

    tie $new_scalar, 'NewScalar';
    tie $new_std_scalar, 'NewStdScalar';

=head1 DESCRIPTION

This module provides some skeletal methods for scalar-tying classes. See
L<perltie> for a list of the functions required in tying a scalar to a
package. The basic B<Tie::Scalar> package provides a C<new> method, as well
as methods C<TIESCALAR>, C<FETCH> and C<STORE>. The B<Tie::StdScalar>
package provides all the methods specified in  L<perltie>. It inherits from
B<Tie::Scalar> and causes scalars tied to it to behave exactly like the
built-in scalars, allowing for selective overloading of methods. The C<new>
method is provided as a means of grandfathering, for classes that forget to
provide their own C<TIESCALAR> method.

For developers wishing to write their own tied-scalar classes, the methods
are summarized below. The L<perltie> section not only documents these, but
has sample code as well:

=over 4

=item TIESCALAR classname, LIST

The method invoked by the command C<tie $scalar, classname>. Associates a new
scalar instance with the specified class. C<LIST> would represent additional
arguments (along the lines of L<AnyDBM_File> and compatriots) needed to
complete the association.

=item FETCH this

Retrieve the value of the tied scalar referenced by I<this>.

=item STORE this, value

Store data I<value> in the tied scalar referenced by I<this>.

=item DESTROY this

Free the storage associated with the tied scalar referenced by I<this>.
This is rarely needed, as Perl manages its memory quite well. But the
option exists, should a class wish to perform specific actions upon the
destruction of an instance.

=back

=head1 MORE INFORMATION

The L<perltie> section uses a good example of tying scalars by associating
process IDs with priority.

=cut

use Carp;
use warnings::register;

sub new {
    my $pkg = shift;
    $pkg->TIESCALAR(@_);
}

# "Grandfather" the new, a la Tie::Hash

sub TIESCALAR {
    my $pkg = shift;
	if ($pkg->can('new') and $pkg ne __PACKAGE__) {
	warnings::warnif("WARNING: calling ${pkg}->new since ${pkg}->TIESCALAR is missing");
	$pkg->new(@_);
    }
    else {
	croak "$pkg doesn't define a TIESCALAR method";
    }
}

sub FETCH {
    my $pkg = ref $_[0];
    croak "$pkg doesn't define a FETCH method";
}

sub STORE {
    my $pkg = ref $_[0];
    croak "$pkg doesn't define a STORE method";
}

#
# The Tie::StdScalar package provides scalars that behave exactly like
# Perl's built-in scalars. Good base to inherit from, if you're only going to
# tweak a small bit.
#
package Tie::StdScalar;
@ISA = qw(Tie::Scalar);

sub TIESCALAR {
    my $class = shift;
    my $instance = shift || undef;
    return bless \$instance => $class;
}

sub FETCH {
    return ${$_[0]};
}

sub STORE {
    ${$_[0]} = $_[1];
}

sub DESTROY {
    undef ${$_[0]};
}

1;
