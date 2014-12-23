package NEXT;
$VERSION = '0.61';
use Carp;
use strict;
use overload ();

sub NEXT::ELSEWHERE::ancestors
{
	my @inlist = shift;
	my @outlist = ();
	while (my $next = shift @inlist) {
		push @outlist, $next;
		no strict 'refs';
		unshift @inlist, @{"$outlist[-1]::ISA"};
	}
	return @outlist;
}

sub NEXT::ELSEWHERE::ordered_ancestors
{
	my @inlist = shift;
	my @outlist = ();
	while (my $next = shift @inlist) {
		push @outlist, $next;
		no strict 'refs';
		push @inlist, @{"$outlist[-1]::ISA"};
	}
	return sort { $a->isa($b) ? -1
	            : $b->isa($a) ? +1
	            :                0 } @outlist;
}

sub NEXT::ELSEWHERE::buildAUTOLOAD
{
    my $autoload_name = caller() . '::AUTOLOAD';

    no strict 'refs';
    *{$autoload_name} = sub {
        my ($self) = @_;
        my $depth = 1;
        until (((caller($depth))[3]||q{}) !~ /^\(eval\)$/) { $depth++ }
        my $caller = (caller($depth))[3];
        my $wanted = $NEXT::AUTOLOAD || $autoload_name;
        undef $NEXT::AUTOLOAD;
        my ($caller_class, $caller_method) = $caller =~ m{(.*)::(.*)}g;
        my ($wanted_class, $wanted_method) = $wanted =~ m{(.*)::(.*)}g;
        croak "Can't call $wanted from $caller"
            unless $caller_method eq $wanted_method;

        my $key = ref $self && overload::Overloaded($self)
            ? overload::StrVal($self) : $self;

        local ($NEXT::NEXT{$key,$wanted_method}, $NEXT::SEEN) =
            ($NEXT::NEXT{$key,$wanted_method}, $NEXT::SEEN);

        unless ($NEXT::NEXT{$key,$wanted_method}) {
            my @forebears =
                NEXT::ELSEWHERE::ancestors ref $self || $self,
                            $wanted_class;
            while (@forebears) {
                last if shift @forebears eq $caller_class
            }
            no strict 'refs';
            @{$NEXT::NEXT{$key,$wanted_method}} = 
                map { *{"${_}::$caller_method"}{CODE}||() } @forebears
                    unless $wanted_method eq 'AUTOLOAD';
            @{$NEXT::NEXT{$key,$wanted_method}} = 
                map { (*{"${_}::AUTOLOAD"}{CODE}) ? "${_}::AUTOLOAD" : ()} @forebears
                    unless @{$NEXT::NEXT{$key,$wanted_method}||[]};
            $NEXT::SEEN->{$key,*{$caller}{CODE}}++;
        }
        my $call_method = shift @{$NEXT::NEXT{$key,$wanted_method}};
        while ($wanted_class =~ /^NEXT\b.*\b(UNSEEN|DISTINCT)\b/
            && defined $call_method
            && $NEXT::SEEN->{$key,$call_method}++) {
            $call_method = shift @{$NEXT::NEXT{$key,$wanted_method}};
        }
        unless (defined $call_method) {
            return unless $wanted_class =~ /^NEXT:.*:ACTUAL/;
            (local $Carp::CarpLevel)++;
            croak qq(Can't locate object method "$wanted_method" ),
                qq(via package "$caller_class");
        };
        return $self->$call_method(@_[1..$#_]) if ref $call_method eq 'CODE';
        no strict 'refs';
        ($wanted_method=${$caller_class."::AUTOLOAD"}) =~ s/.*:://
            if $wanted_method eq 'AUTOLOAD';
        $$call_method = $caller_class."::NEXT::".$wanted_method;
        return $call_method->(@_);
    };
}

no strict 'vars';
package NEXT;                                  NEXT::ELSEWHERE::buildAUTOLOAD();
package NEXT::UNSEEN;		@ISA = 'NEXT';     NEXT::ELSEWHERE::buildAUTOLOAD();
package NEXT::DISTINCT;		@ISA = 'NEXT';     NEXT::ELSEWHERE::buildAUTOLOAD();
package NEXT::ACTUAL;		@ISA = 'NEXT';     NEXT::ELSEWHERE::buildAUTOLOAD();
package NEXT::ACTUAL::UNSEEN;	@ISA = 'NEXT'; NEXT::ELSEWHERE::buildAUTOLOAD();
package NEXT::ACTUAL::DISTINCT;	@ISA = 'NEXT'; NEXT::ELSEWHERE::buildAUTOLOAD();
package NEXT::UNSEEN::ACTUAL;	@ISA = 'NEXT'; NEXT::ELSEWHERE::buildAUTOLOAD();
package NEXT::DISTINCT::ACTUAL;	@ISA = 'NEXT'; NEXT::ELSEWHERE::buildAUTOLOAD();

package EVERY;

sub EVERY::ELSEWHERE::buildAUTOLOAD {
    my $autoload_name = caller() . '::AUTOLOAD';

    no strict 'refs';
    *{$autoload_name} = sub {
        my ($self) = @_;
        my $depth = 1;
        until (((caller($depth))[3]||q{}) !~ /^\(eval\)$/) { $depth++ }
        my $caller = (caller($depth))[3];
        my $wanted = $EVERY::AUTOLOAD || $autoload_name;
        undef $EVERY::AUTOLOAD;
        my ($wanted_class, $wanted_method) = $wanted =~ m{(.*)::(.*)}g;

        my $key = ref($self) && overload::Overloaded($self)
            ? overload::StrVal($self) : $self;

        local $NEXT::ALREADY_IN_EVERY{$key,$wanted_method} =
            $NEXT::ALREADY_IN_EVERY{$key,$wanted_method};

        return if $NEXT::ALREADY_IN_EVERY{$key,$wanted_method}++;

        my @forebears = NEXT::ELSEWHERE::ordered_ancestors ref $self || $self,
                                        $wanted_class;
        @forebears = reverse @forebears if $wanted_class =~ /\bLAST\b/;
        no strict 'refs';
        my %seen;
        my @every = map { my $sub = "${_}::$wanted_method";
                    !*{$sub}{CODE} || $seen{$sub}++ ? () : $sub
                    } @forebears
                    unless $wanted_method eq 'AUTOLOAD';

        my $want = wantarray;
        if (@every) {
            if ($want) {
                return map {($_, [$self->$_(@_[1..$#_])])} @every;
            }
            elsif (defined $want) {
                return { map {($_, scalar($self->$_(@_[1..$#_])))}
                        @every
                    };
            }
            else {
                $self->$_(@_[1..$#_]) for @every;
                return;
            }
        }

        @every = map { my $sub = "${_}::AUTOLOAD";
                !*{$sub}{CODE} || $seen{$sub}++ ? () : "${_}::AUTOLOAD"
                } @forebears;
        if ($want) {
            return map { $$_ = ref($self)."::EVERY::".$wanted_method;
                    ($_, [$self->$_(@_[1..$#_])]);
                } @every;
        }
        elsif (defined $want) {
            return { map { $$_ = ref($self)."::EVERY::".$wanted_method;
                    ($_, scalar($self->$_(@_[1..$#_])))
                    } @every
                };
        }
        else {
            for (@every) {
                $$_ = ref($self)."::EVERY::".$wanted_method;
                $self->$_(@_[1..$#_]);
            }
            return;
        }
    };
}

package EVERY::LAST;   @ISA = 'EVERY';   EVERY::ELSEWHERE::buildAUTOLOAD();
package EVERY;         @ISA = 'NEXT';    EVERY::ELSEWHERE::buildAUTOLOAD();

1;

__END__

=head1 NAME

NEXT.pm - Provide a pseudo-class NEXT (et al) that allows method redispatch


=head1 SYNOPSIS

    use NEXT;

    package A;
    sub A::method   { print "$_[0]: A method\n";   $_[0]->NEXT::method() }
    sub A::DESTROY  { print "$_[0]: A dtor\n";     $_[0]->NEXT::DESTROY() }

    package B;
    use base qw( A );
    sub B::AUTOLOAD { print "$_[0]: B AUTOLOAD\n"; $_[0]->NEXT::AUTOLOAD() }
    sub B::DESTROY  { print "$_[0]: B dtor\n";     $_[0]->NEXT::DESTROY() }

    package C;
    sub C::method   { print "$_[0]: C method\n";   $_[0]->NEXT::method() }
    sub C::AUTOLOAD { print "$_[0]: C AUTOLOAD\n"; $_[0]->NEXT::AUTOLOAD() }
    sub C::DESTROY  { print "$_[0]: C dtor\n";     $_[0]->NEXT::DESTROY() }

    package D;
    use base qw( B C );
    sub D::method   { print "$_[0]: D method\n";   $_[0]->NEXT::method() }
    sub D::AUTOLOAD { print "$_[0]: D AUTOLOAD\n"; $_[0]->NEXT::AUTOLOAD() }
    sub D::DESTROY  { print "$_[0]: D dtor\n";     $_[0]->NEXT::DESTROY() }

    package main;

    my $obj = bless {}, "D";

    $obj->method();		# Calls D::method, A::method, C::method
    $obj->missing_method(); # Calls D::AUTOLOAD, B::AUTOLOAD, C::AUTOLOAD

    # Clean-up calls D::DESTROY, B::DESTROY, A::DESTROY, C::DESTROY



=head1 DESCRIPTION

NEXT.pm adds a pseudoclass named C<NEXT> to any program
that uses it. If a method C<m> calls C<$self-E<gt>NEXT::m()>, the call to
C<m> is redispatched as if the calling method had not originally been found.

In other words, a call to C<$self-E<gt>NEXT::m()> resumes the depth-first,
left-to-right search of C<$self>'s class hierarchy that resulted in the
original call to C<m>.

Note that this is not the same thing as C<$self-E<gt>SUPER::m()>, which
begins a new dispatch that is restricted to searching the ancestors
of the current class. C<$self-E<gt>NEXT::m()> can backtrack
past the current class -- to look for a suitable method in other
ancestors of C<$self> -- whereas C<$self-E<gt>SUPER::m()> cannot.

A typical use would be in the destructors of a class hierarchy,
as illustrated in the synopsis above. Each class in the hierarchy
has a DESTROY method that performs some class-specific action
and then redispatches the call up the hierarchy. As a result,
when an object of class D is destroyed, the destructors of I<all>
its parent classes are called (in depth-first, left-to-right order).

Another typical use of redispatch would be in C<AUTOLOAD>'ed methods.
If such a method determined that it was not able to handle a
particular call, it might choose to redispatch that call, in the
hope that some other C<AUTOLOAD> (above it, or to its left) might
do better.

By default, if a redispatch attempt fails to find another method
elsewhere in the objects class hierarchy, it quietly gives up and does
nothing (but see L<"Enforcing redispatch">). This gracious acquiescence
is also unlike the (generally annoying) behaviour of C<SUPER>, which
throws an exception if it cannot redispatch.

Note that it is a fatal error for any method (including C<AUTOLOAD>)
to attempt to redispatch any method that does not have the
same name. For example:

        sub D::oops { print "oops!\n"; $_[0]->NEXT::other_method() }


=head2 Enforcing redispatch

It is possible to make C<NEXT> redispatch more demandingly (i.e. like
C<SUPER> does), so that the redispatch throws an exception if it cannot
find a "next" method to call.

To do this, simple invoke the redispatch as:

	$self->NEXT::ACTUAL::method();

rather than:

	$self->NEXT::method();

The C<ACTUAL> tells C<NEXT> that there must actually be a next method to call,
or it should throw an exception.

C<NEXT::ACTUAL> is most commonly used in C<AUTOLOAD> methods, as a means to
decline an C<AUTOLOAD> request, but preserve the normal exception-on-failure 
semantics:

	sub AUTOLOAD {
		if ($AUTOLOAD =~ /foo|bar/) {
			# handle here
		}
		else {  # try elsewhere
			shift()->NEXT::ACTUAL::AUTOLOAD(@_);
		}
	}

By using C<NEXT::ACTUAL>, if there is no other C<AUTOLOAD> to handle the
method call, an exception will be thrown (as usually happens in the absence of
a suitable C<AUTOLOAD>).


=head2 Avoiding repetitions

If C<NEXT> redispatching is used in the methods of a "diamond" class hierarchy:

	#     A   B
	#    / \ /
	#   C   D
	#    \ /
	#     E

	use NEXT;

	package A;                 
	sub foo { print "called A::foo\n"; shift->NEXT::foo() }

	package B;                 
	sub foo { print "called B::foo\n"; shift->NEXT::foo() }

	package C; @ISA = qw( A );
	sub foo { print "called C::foo\n"; shift->NEXT::foo() }

	package D; @ISA = qw(A B);
	sub foo { print "called D::foo\n"; shift->NEXT::foo() }

	package E; @ISA = qw(C D);
	sub foo { print "called E::foo\n"; shift->NEXT::foo() }

	E->foo();

then derived classes may (re-)inherit base-class methods through two or
more distinct paths (e.g. in the way C<E> inherits C<A::foo> twice --
through C<C> and C<D>). In such cases, a sequence of C<NEXT> redispatches
will invoke the multiply inherited method as many times as it is
inherited. For example, the above code prints:

        called E::foo
        called C::foo
        called A::foo
        called D::foo
        called A::foo
        called B::foo

(i.e. C<A::foo> is called twice).

In some cases this I<may> be the desired effect within a diamond hierarchy,
but in others (e.g. for destructors) it may be more appropriate to 
call each method only once during a sequence of redispatches.

To cover such cases, you can redispatch methods via:

        $self->NEXT::DISTINCT::method();

rather than:

        $self->NEXT::method();

This causes the redispatcher to only visit each distinct C<method> method
once. That is, to skip any classes in the hierarchy that it has
already visited during redispatch. So, for example, if the
previous example were rewritten:

        package A;                 
        sub foo { print "called A::foo\n"; shift->NEXT::DISTINCT::foo() }

        package B;                 
        sub foo { print "called B::foo\n"; shift->NEXT::DISTINCT::foo() }

        package C; @ISA = qw( A );
        sub foo { print "called C::foo\n"; shift->NEXT::DISTINCT::foo() }

        package D; @ISA = qw(A B);
        sub foo { print "called D::foo\n"; shift->NEXT::DISTINCT::foo() }

        package E; @ISA = qw(C D);
        sub foo { print "called E::foo\n"; shift->NEXT::DISTINCT::foo() }

        E->foo();

then it would print:
        
        called E::foo
        called C::foo
        called A::foo
        called D::foo
        called B::foo

and omit the second call to C<A::foo> (since it would not be distinct
from the first call to C<A::foo>).

Note that you can also use:

        $self->NEXT::DISTINCT::ACTUAL::method();

or:

        $self->NEXT::ACTUAL::DISTINCT::method();

to get both unique invocation I<and> exception-on-failure.

Note that, for historical compatibility, you can also use
C<NEXT::UNSEEN> instead of C<NEXT::DISTINCT>.


=head2 Invoking all versions of a method with a single call

Yet another pseudo-class that NEXT.pm provides is C<EVERY>.
Its behaviour is considerably simpler than that of the C<NEXT> family.
A call to:

	$obj->EVERY::foo();

calls I<every> method named C<foo> that the object in C<$obj> has inherited.
That is:

	use NEXT;

	package A; @ISA = qw(B D X);
	sub foo { print "A::foo " }

	package B; @ISA = qw(D X);
	sub foo { print "B::foo " }

	package X; @ISA = qw(D);
	sub foo { print "X::foo " }

	package D;
	sub foo { print "D::foo " }

	package main;

	my $obj = bless {}, 'A';
	$obj->EVERY::foo();        # prints" A::foo B::foo X::foo D::foo

Prefixing a method call with C<EVERY::> causes every method in the
object's hierarchy with that name to be invoked. As the above example
illustrates, they are not called in Perl's usual "left-most-depth-first"
order. Instead, they are called "breadth-first-dependency-wise".

That means that the inheritance tree of the object is traversed breadth-first
and the resulting order of classes is used as the sequence in which methods
are called. However, that sequence is modified by imposing a rule that the
appropriate method of a derived class must be called before the same method of
any ancestral class. That's why, in the above example, C<X::foo> is called
before C<D::foo>, even though C<D> comes before C<X> in C<@B::ISA>.

In general, there's no need to worry about the order of calls. They will be
left-to-right, breadth-first, most-derived-first. This works perfectly for
most inherited methods (including destructors), but is inappropriate for
some kinds of methods (such as constructors, cloners, debuggers, and
initializers) where it's more appropriate that the least-derived methods be
called first (as more-derived methods may rely on the behaviour of their
"ancestors"). In that case, instead of using the C<EVERY> pseudo-class:

	$obj->EVERY::foo();        # prints" A::foo B::foo X::foo D::foo      

you can use the C<EVERY::LAST> pseudo-class:

	$obj->EVERY::LAST::foo();  # prints" D::foo X::foo B::foo A::foo      

which reverses the order of method call.

Whichever version is used, the actual methods are called in the same
context (list, scalar, or void) as the original call via C<EVERY>, and return:

=over

=item *

A hash of array references in list context. Each entry of the hash has the
fully qualified method name as its key and a reference to an array containing
the method's list-context return values as its value.

=item *

A reference to a hash of scalar values in scalar context. Each entry of the hash has the
fully qualified method name as its key and the method's scalar-context return values as its value.

=item *

Nothing in void context (obviously).

=back

=head2 Using C<EVERY> methods

The typical way to use an C<EVERY> call is to wrap it in another base
method, that all classes inherit. For example, to ensure that every
destructor an object inherits is actually called (as opposed to just the
left-most-depth-first-est one):

        package Base;
        sub DESTROY { $_[0]->EVERY::Destroy }

        package Derived1; 
        use base 'Base';
        sub Destroy {...}

        package Derived2; 
        use base 'Base', 'Derived1';
        sub Destroy {...}

et cetera. Every derived class than needs its own clean-up
behaviour simply adds its own C<Destroy> method (I<not> a C<DESTROY> method),
which the call to C<EVERY::LAST::Destroy> in the inherited destructor
then correctly picks up.

Likewise, to create a class hierarchy in which every initializer inherited by
a new object is invoked:

        package Base;
        sub new {
		my ($class, %args) = @_;
		my $obj = bless {}, $class;
		$obj->EVERY::LAST::Init(\%args);
	}

        package Derived1; 
        use base 'Base';
        sub Init {
		my ($argsref) = @_;
		...
	}

        package Derived2; 
        use base 'Base', 'Derived1';
        sub Init {
		my ($argsref) = @_;
		...
	}

et cetera. Every derived class than needs some additional initialization
behaviour simply adds its own C<Init> method (I<not> a C<new> method),
which the call to C<EVERY::LAST::Init> in the inherited constructor
then correctly picks up.


=head1 AUTHOR

Damian Conway (damian@conway.org)

=head1 BUGS AND IRRITATIONS

Because it's a module, not an integral part of the interpreter, NEXT.pm
has to guess where the surrounding call was found in the method
look-up sequence. In the presence of diamond inheritance patterns
it occasionally guesses wrong.

It's also too slow (despite caching).

Comment, suggestions, and patches welcome.

=head1 COPYRIGHT

 Copyright (c) 2000-2001, Damian Conway. All Rights Reserved.
 This module is free software. It may be used, redistributed
    and/or modified under the same terms as Perl itself.
