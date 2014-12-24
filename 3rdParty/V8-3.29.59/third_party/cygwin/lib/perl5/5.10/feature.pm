package feature;

our $VERSION = '1.13';

# (feature name) => (internal name, used in %^H)
my %feature = (
    switch => 'feature_switch',
    say    => "feature_say",
    state  => "feature_state",
);

# NB. the latest bundle must be loaded by the -E switch (see toke.c)

my %feature_bundle = (
    "5.10" => [qw(switch say state)],
### "5.11" => [qw(switch say state)],
);

# special case
$feature_bundle{"5.9.5"} = $feature_bundle{"5.10"};

# TODO:
# - think about versioned features (use feature switch => 2)

=head1 NAME

feature - Perl pragma to enable new syntactic features

=head1 SYNOPSIS

    use feature qw(switch say);
    given ($foo) {
	when (1)	  { say "\$foo == 1" }
	when ([2,3])	  { say "\$foo == 2 || \$foo == 3" }
	when (/^a[bc]d$/) { say "\$foo eq 'abd' || \$foo eq 'acd'" }
	when ($_ > 100)   { say "\$foo > 100" }
	default		  { say "None of the above" }
    }

    use feature ':5.10'; # loads all features available in perl 5.10

=head1 DESCRIPTION

It is usually impossible to add new syntax to Perl without breaking
some existing programs. This pragma provides a way to minimize that
risk. New syntactic constructs can be enabled by C<use feature 'foo'>,
and will be parsed only when the appropriate feature pragma is in
scope.

=head2 Lexical effect

Like other pragmas (C<use strict>, for example), features have a lexical
effect. C<use feature qw(foo)> will only make the feature "foo" available
from that point to the end of the enclosing block.

    {
        use feature 'say';
        say "say is available here";
    }
    print "But not here.\n";

=head2 C<no feature>

Features can also be turned off by using C<no feature "foo">. This too
has lexical effect.

    use feature 'say';
    say "say is available here";
    {
        no feature 'say';
        print "But not here.\n";
    }
    say "Yet it is here.";

C<no feature> with no features specified will turn off all features.

=head2 The 'switch' feature

C<use feature 'switch'> tells the compiler to enable the Perl 6
given/when construct.

See L<perlsyn/"Switch statements"> for details.

=head2 The 'say' feature

C<use feature 'say'> tells the compiler to enable the Perl 6
C<say> function.

See L<perlfunc/say> for details.

=head2 the 'state' feature

C<use feature 'state'> tells the compiler to enable C<state>
variables.

See L<perlsub/"Persistent Private Variables"> for details.

=head1 FEATURE BUNDLES

It's possible to load a whole slew of features in one go, using
a I<feature bundle>. The name of a feature bundle is prefixed with
a colon, to distinguish it from an actual feature. At present, the
only feature bundle is C<use feature ":5.10"> which is equivalent
to C<use feature qw(switch say state)>.

Specifying sub-versions such as the C<0> in C<5.10.0> in feature bundles has
no effect: feature bundles are guaranteed to be the same for all sub-versions.

=head1 IMPLICIT LOADING

There are two ways to load the C<feature> pragma implicitly :

=over 4

=item *

By using the C<-E> switch on the command-line instead of C<-e>. It enables
all available features in the main compilation unit (that is, the one-liner.)

=item *

By requiring explicitly a minimal Perl version number for your program, with
the C<use VERSION> construct, and when the version is higher than or equal to
5.10.0. That is,

    use 5.10.0;

will do an implicit

    use feature ':5.10';

and so on. Note how the trailing sub-version is automatically stripped from the
version.

But to avoid portability warnings (see L<perlfunc/use>), you may prefer:

    use 5.010;

with the same effect.

=back

=cut

sub import {
    my $class = shift;
    if (@_ == 0) {
	croak("No features specified");
    }
    while (@_) {
	my $name = shift(@_);
	if (substr($name, 0, 1) eq ":") {
	    my $v = substr($name, 1);
	    if (!exists $feature_bundle{$v}) {
		$v =~ s/^([0-9]+)\.([0-9]+).[0-9]+$/$1.$2/;
		if (!exists $feature_bundle{$v}) {
		    unknown_feature_bundle(substr($name, 1));
		}
	    }
	    unshift @_, @{$feature_bundle{$v}};
	    next;
	}
	if (!exists $feature{$name}) {
	    unknown_feature($name);
	}
	$^H{$feature{$name}} = 1;
    }
}

sub unimport {
    my $class = shift;

    # A bare C<no feature> should disable *all* features
    if (!@_) {
	delete @^H{ values(%feature) };
	return;
    }

    while (@_) {
	my $name = shift;
	if (substr($name, 0, 1) eq ":") {
	    my $v = substr($name, 1);
	    if (!exists $feature_bundle{$v}) {
		$v =~ s/^([0-9]+)\.([0-9]+).[0-9]+$/$1.$2/;
		if (!exists $feature_bundle{$v}) {
		    unknown_feature_bundle(substr($name, 1));
		}
	    }
	    unshift @_, @{$feature_bundle{$v}};
	    next;
	}
	if (!exists($feature{$name})) {
	    unknown_feature($name);
	}
	else {
	    delete $^H{$feature{$name}};
	}
    }
}

sub unknown_feature {
    my $feature = shift;
    croak(sprintf('Feature "%s" is not supported by Perl %vd',
	    $feature, $^V));
}

sub unknown_feature_bundle {
    my $feature = shift;
    croak(sprintf('Feature bundle "%s" is not supported by Perl %vd',
	    $feature, $^V));
}

sub croak {
    require Carp;
    Carp::croak(@_);
}

1;
