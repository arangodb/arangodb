package attributes;

our $VERSION = 0.09;

@EXPORT_OK = qw(get reftype);
@EXPORT = ();
%EXPORT_TAGS = (ALL => [@EXPORT, @EXPORT_OK]);

use strict;

sub croak {
    require Carp;
    goto &Carp::croak;
}

sub carp {
    require Carp;
    goto &Carp::carp;
}

## forward declaration(s) rather than wrapping the bootstrap call in BEGIN{}
#sub reftype ($) ;
#sub _fetch_attrs ($) ;
#sub _guess_stash ($) ;
#sub _modify_attrs ;
#
# The extra trips through newATTRSUB in the interpreter wipe out any savings
# from avoiding the BEGIN block.  Just do the bootstrap now.
BEGIN { bootstrap attributes }

sub import {
    @_ > 2 && ref $_[2] or do {
	require Exporter;
	goto &Exporter::import;
    };
    my (undef,$home_stash,$svref,@attrs) = @_;

    my $svtype = uc reftype($svref);
    my $pkgmeth;
    $pkgmeth = UNIVERSAL::can($home_stash, "MODIFY_${svtype}_ATTRIBUTES")
	if defined $home_stash && $home_stash ne '';
    my @badattrs;
    if ($pkgmeth) {
	my @pkgattrs = _modify_attrs($svref, @attrs);
	@badattrs = $pkgmeth->($home_stash, $svref, @pkgattrs);
	if (!@badattrs && @pkgattrs) {
            require warnings;
	    return unless warnings::enabled('reserved');
	    @pkgattrs = grep { m/\A[[:lower:]]+(?:\z|\()/ } @pkgattrs;
	    if (@pkgattrs) {
		for my $attr (@pkgattrs) {
		    $attr =~ s/\(.+\z//s;
		}
		my $s = ((@pkgattrs == 1) ? '' : 's');
		carp "$svtype package attribute$s " .
		    "may clash with future reserved word$s: " .
		    join(' : ' , @pkgattrs);
	    }
	}
    }
    else {
	@badattrs = _modify_attrs($svref, @attrs);
    }
    if (@badattrs) {
	croak "Invalid $svtype attribute" .
	    (( @badattrs == 1 ) ? '' : 's') .
	    ": " .
	    join(' : ', @badattrs);
    }
}

sub get ($) {
    @_ == 1  && ref $_[0] or
	croak 'Usage: '.__PACKAGE__.'::get $ref';
    my $svref = shift;
    my $svtype = uc reftype $svref;
    my $stash = _guess_stash $svref;
    $stash = caller unless defined $stash;
    my $pkgmeth;
    $pkgmeth = UNIVERSAL::can($stash, "FETCH_${svtype}_ATTRIBUTES")
	if defined $stash && $stash ne '';
    return $pkgmeth ?
		(_fetch_attrs($svref), $pkgmeth->($stash, $svref)) :
		(_fetch_attrs($svref))
	;
}

sub require_version { goto &UNIVERSAL::VERSION }

1;
__END__
#The POD goes here

=head1 NAME

attributes - get/set subroutine or variable attributes

=head1 SYNOPSIS

  sub foo : method ;
  my ($x,@y,%z) : Bent = 1;
  my $s = sub : method { ... };

  use attributes ();	# optional, to get subroutine declarations
  my @attrlist = attributes::get(\&foo);

  use attributes 'get'; # import the attributes::get subroutine
  my @attrlist = get \&foo;

=head1 DESCRIPTION

Subroutine declarations and definitions may optionally have attribute lists
associated with them.  (Variable C<my> declarations also may, but see the
warning below.)  Perl handles these declarations by passing some information
about the call site and the thing being declared along with the attribute
list to this module.  In particular, the first example above is equivalent to
the following:

    use attributes __PACKAGE__, \&foo, 'method';

The second example in the synopsis does something equivalent to this:

    use attributes ();
    my ($x,@y,%z);
    attributes::->import(__PACKAGE__, \$x, 'Bent');
    attributes::->import(__PACKAGE__, \@y, 'Bent');
    attributes::->import(__PACKAGE__, \%z, 'Bent');
    ($x,@y,%z) = 1;

Yes, that's a lot of expansion.

B<WARNING>: attribute declarations for variables are still evolving.
The semantics and interfaces of such declarations could change in
future versions.  They are present for purposes of experimentation
with what the semantics ought to be.  Do not rely on the current
implementation of this feature.

There are only a few attributes currently handled by Perl itself (or
directly by this module, depending on how you look at it.)  However,
package-specific attributes are allowed by an extension mechanism.
(See L<"Package-specific Attribute Handling"> below.)

The setting of subroutine attributes happens at compile time.
Variable attributes in C<our> declarations are also applied at compile time.
However, C<my> variables get their attributes applied at run-time.
This means that you have to I<reach> the run-time component of the C<my>
before those attributes will get applied.  For example:

    my $x : Bent = 42 if 0;

will neither assign 42 to $x I<nor> will it apply the C<Bent> attribute
to the variable.

An attempt to set an unrecognized attribute is a fatal error.  (The
error is trappable, but it still stops the compilation within that
C<eval>.)  Setting an attribute with a name that's all lowercase
letters that's not a built-in attribute (such as "foo") will result in
a warning with B<-w> or C<use warnings 'reserved'>.

=head2 What C<import> does

In the description it is mentioned that

  sub foo : method;

is equivalent to

  use attributes __PACKAGE__, \&foo, 'method';

As you might know this calls the C<import> function of C<attributes> at compile 
time with these parameters: 'attributes', the caller's package name, the reference 
to the code and 'method'.

  attributes->import( __PACKAGE__, \&foo, 'method' );

So you want to know what C<import> actually does?

First of all C<import> gets the type of the third parameter ('CODE' in this case).
C<attributes.pm> checks if there is a subroutine called C<< MODIFY_<reftype>_ATTRIBUTES >>
in the caller's namespace (here: 'main'). In this case a subroutine C<MODIFY_CODE_ATTRIBUTES> is
required. Then this method is called to check if you have used a "bad attribute".
The subroutine call in this example would look like

  MODIFY_CODE_ATTRIBUTES( 'main', \&foo, 'method' );

C<< MODIFY_<reftype>_ATTRIBUTES >> has to return a list of all "bad attributes".
If there are any bad attributes C<import> croaks.

(See L<"Package-specific Attribute Handling"> below.)

=head2 Built-in Attributes

The following are the built-in attributes for subroutines:

=over 4

=item locked

B<5.005 threads only!  The use of the "locked" attribute currently
only makes sense if you are using the deprecated "Perl 5.005 threads"
implementation of threads.>

Setting this attribute is only meaningful when the subroutine or
method is to be called by multiple threads.  When set on a method
subroutine (i.e., one marked with the B<method> attribute below),
Perl ensures that any invocation of it implicitly locks its first
argument before execution.  When set on a non-method subroutine,
Perl ensures that a lock is taken on the subroutine itself before
execution.  The semantics of the lock are exactly those of one
explicitly taken with the C<lock> operator immediately after the
subroutine is entered.

=item method

Indicates that the referenced subroutine is a method.
This has a meaning when taken together with the B<locked> attribute,
as described there.  It also means that a subroutine so marked
will not trigger the "Ambiguous call resolved as CORE::%s" warning.

=item lvalue

Indicates that the referenced subroutine is a valid lvalue and can
be assigned to. The subroutine must return a modifiable value such
as a scalar variable, as described in L<perlsub>.

=back

For global variables there is C<unique> attribute: see L<perlfunc/our>.

=head2 Available Subroutines

The following subroutines are available for general use once this module
has been loaded:

=over 4

=item get

This routine expects a single parameter--a reference to a
subroutine or variable.  It returns a list of attributes, which may be
empty.  If passed invalid arguments, it uses die() (via L<Carp::croak|Carp>)
to raise a fatal exception.  If it can find an appropriate package name
for a class method lookup, it will include the results from a
C<FETCH_I<type>_ATTRIBUTES> call in its return list, as described in
L<"Package-specific Attribute Handling"> below.
Otherwise, only L<built-in attributes|"Built-in Attributes"> will be returned.

=item reftype

This routine expects a single parameter--a reference to a subroutine or
variable.  It returns the built-in type of the referenced variable,
ignoring any package into which it might have been blessed.
This can be useful for determining the I<type> value which forms part of
the method names described in L<"Package-specific Attribute Handling"> below.

=back

Note that these routines are I<not> exported by default.

=head2 Package-specific Attribute Handling

B<WARNING>: the mechanisms described here are still experimental.  Do not
rely on the current implementation.  In particular, there is no provision
for applying package attributes to 'cloned' copies of subroutines used as
closures.  (See L<perlref/"Making References"> for information on closures.)
Package-specific attribute handling may change incompatibly in a future
release.

When an attribute list is present in a declaration, a check is made to see
whether an attribute 'modify' handler is present in the appropriate package
(or its @ISA inheritance tree).  Similarly, when C<attributes::get> is
called on a valid reference, a check is made for an appropriate attribute
'fetch' handler.  See L<"EXAMPLES"> to see how the "appropriate package"
determination works.

The handler names are based on the underlying type of the variable being
declared or of the reference passed.  Because these attributes are
associated with subroutine or variable declarations, this deliberately
ignores any possibility of being blessed into some package.  Thus, a
subroutine declaration uses "CODE" as its I<type>, and even a blessed
hash reference uses "HASH" as its I<type>.

The class methods invoked for modifying and fetching are these:

=over 4

=item FETCH_I<type>_ATTRIBUTES

This method is called with two arguments:  the relevant package name,
and a reference to a variable or subroutine for which package-defined
attributes are desired.  The expected return value is a list of
associated attributes.  This list may be empty.

=item MODIFY_I<type>_ATTRIBUTES

This method is called with two fixed arguments, followed by the list of
attributes from the relevant declaration.  The two fixed arguments are
the relevant package name and a reference to the declared subroutine or
variable.  The expected return value is a list of attributes which were
not recognized by this handler.  Note that this allows for a derived class
to delegate a call to its base class, and then only examine the attributes
which the base class didn't already handle for it.

The call to this method is currently made I<during> the processing of the
declaration.  In particular, this means that a subroutine reference will
probably be for an undefined subroutine, even if this declaration is
actually part of the definition.

=back

Calling C<attributes::get()> from within the scope of a null package
declaration C<package ;> for an unblessed variable reference will
not provide any starting package name for the 'fetch' method lookup.
Thus, this circumstance will not result in a method call for package-defined
attributes.  A named subroutine knows to which symbol table entry it belongs
(or originally belonged), and it will use the corresponding package.
An anonymous subroutine knows the package name into which it was compiled
(unless it was also compiled with a null package declaration), and so it
will use that package name.

=head2 Syntax of Attribute Lists

An attribute list is a sequence of attribute specifications, separated by
whitespace or a colon (with optional whitespace).
Each attribute specification is a simple
name, optionally followed by a parenthesised parameter list.
If such a parameter list is present, it is scanned past as for the rules
for the C<q()> operator.  (See L<perlop/"Quote and Quote-like Operators">.)
The parameter list is passed as it was found, however, and not as per C<q()>.

Some examples of syntactically valid attribute lists:

    switch(10,foo(7,3))  :  expensive
    Ugly('\(") :Bad
    _5x5
    locked method

Some examples of syntactically invalid attribute lists (with annotation):

    switch(10,foo()		# ()-string not balanced
    Ugly('(')			# ()-string not balanced
    5x5				# "5x5" not a valid identifier
    Y2::north			# "Y2::north" not a simple identifier
    foo + bar			# "+" neither a colon nor whitespace

=head1 EXPORTS

=head2 Default exports

None.

=head2 Available exports

The routines C<get> and C<reftype> are exportable.

=head2 Export tags defined

The C<:ALL> tag will get all of the above exports.

=head1 EXAMPLES

Here are some samples of syntactically valid declarations, with annotation
as to how they resolve internally into C<use attributes> invocations by
perl.  These examples are primarily useful to see how the "appropriate
package" is found for the possible method lookups for package-defined
attributes.

=over 4

=item 1.

Code:

    package Canine;
    package Dog;
    my Canine $spot : Watchful ;

Effect:

    use attributes ();
    attributes::->import(Canine => \$spot, "Watchful");

=item 2.

Code:

    package Felis;
    my $cat : Nervous;

Effect:

    use attributes ();
    attributes::->import(Felis => \$cat, "Nervous");

=item 3.

Code:

    package X;
    sub foo : locked ;

Effect:

    use attributes X => \&foo, "locked";

=item 4.

Code:

    package X;
    sub Y::x : locked { 1 }

Effect:

    use attributes Y => \&Y::x, "locked";

=item 5.

Code:

    package X;
    sub foo { 1 }

    package Y;
    BEGIN { *bar = \&X::foo; }

    package Z;
    sub Y::bar : locked ;

Effect:

    use attributes X => \&X::foo, "locked";

=back

This last example is purely for purposes of completeness.  You should not
be trying to mess with the attributes of something in a package that's
not your own.

=head1 MORE EXAMPLES

=over 4

=item 1.

    sub MODIFY_CODE_ATTRIBUTES {
       my ($class,$code,@attrs) = @_;

       my $allowed = 'MyAttribute';
       my @bad = grep { $_ ne $allowed } @attrs;

       return @bad;
    }

    sub foo : MyAttribute {
       print "foo\n";
    }

This example runs. At compile time C<MODIFY_CODE_ATTRIBUTES> is called. In that
subroutine, we check if any attribute is disallowed and we return a list of
these "bad attributes".

As we return an empty list, everything is fine.

=item 2.

  sub MODIFY_CODE_ATTRIBUTES {
     my ($class,$code,@attrs) = @_;

     my $allowed = 'MyAttribute';
     my @bad = grep{ $_ ne $allowed }@attrs;

     return @bad;
  }

  sub foo : MyAttribute Test {
     print "foo\n";
  }

This example is aborted at compile time as we use the attribute "Test" which
isn't allowed. C<MODIFY_CODE_ATTRIBUTES> returns a list that contains a single
element ('Test').

=back

=head1 SEE ALSO

L<perlsub/"Private Variables via my()"> and
L<perlsub/"Subroutine Attributes"> for details on the basic declarations;
L<attrs> for the obsolescent form of subroutine attribute specification
which this module replaces;
L<perlfunc/use> for details on the normal invocation mechanism.

=cut

