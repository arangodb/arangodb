package Params::Check;

use strict;

use Carp                        qw[carp croak];
use Locale::Maketext::Simple    Style => 'gettext';

use Data::Dumper;

BEGIN {
    use Exporter    ();
    use vars        qw[ @ISA $VERSION @EXPORT_OK $VERBOSE $ALLOW_UNKNOWN
                        $STRICT_TYPE $STRIP_LEADING_DASHES $NO_DUPLICATES
                        $PRESERVE_CASE $ONLY_ALLOW_DEFINED $WARNINGS_FATAL
                        $SANITY_CHECK_TEMPLATE $CALLER_DEPTH $_ERROR_STRING
                    ];

    @ISA        =   qw[ Exporter ];
    @EXPORT_OK  =   qw[check allow last_error];

    $VERSION                = '0.26';
    $VERBOSE                = $^W ? 1 : 0;
    $NO_DUPLICATES          = 0;
    $STRIP_LEADING_DASHES   = 0;
    $STRICT_TYPE            = 0;
    $ALLOW_UNKNOWN          = 0;
    $PRESERVE_CASE          = 0;
    $ONLY_ALLOW_DEFINED     = 0;
    $SANITY_CHECK_TEMPLATE  = 1;
    $WARNINGS_FATAL         = 0;
    $CALLER_DEPTH           = 0;
}

my %known_keys = map { $_ => 1 }
                    qw| required allow default strict_type no_override
                        store defined |;

=pod

=head1 NAME

Params::Check - A generic input parsing/checking mechanism.

=head1 SYNOPSIS

    use Params::Check qw[check allow last_error];

    sub fill_personal_info {
        my %hash = @_;
        my $x;

        my $tmpl = {
            firstname   => { required   => 1, defined => 1 },
            lastname    => { required   => 1, store => \$x },
            gender      => { required   => 1,
                             allow      => [qr/M/i, qr/F/i],
                           },
            married     => { allow      => [0,1] },
            age         => { default    => 21,
                             allow      => qr/^\d+$/,
                           },

            phone       => { allow => [ sub { return 1 if /$valid_re/ },
                                        '1-800-PERL' ]
                           },
            id_list     => { default        => [],
                             strict_type    => 1
                           },
            employer    => { default => 'NSA', no_override => 1 },
        };

        ### check() returns a hashref of parsed args on success ###
        my $parsed_args = check( $tmpl, \%hash, $VERBOSE )
                            or die qw[Could not parse arguments!];

        ... other code here ...
    }

    my $ok = allow( $colour, [qw|blue green yellow|] );

    my $error = Params::Check::last_error();


=head1 DESCRIPTION

Params::Check is a generic input parsing/checking mechanism.

It allows you to validate input via a template. The only requirement
is that the arguments must be named.

Params::Check can do the following things for you:

=over 4

=item *

Convert all keys to lowercase

=item *

Check if all required arguments have been provided

=item *

Set arguments that have not been provided to the default

=item *

Weed out arguments that are not supported and warn about them to the
user

=item *

Validate the arguments given by the user based on strings, regexes,
lists or even subroutines

=item *

Enforce type integrity if required

=back

Most of Params::Check's power comes from its template, which we'll
discuss below:

=head1 Template

As you can see in the synopsis, based on your template, the arguments
provided will be validated.

The template can take a different set of rules per key that is used.

The following rules are available:

=over 4

=item default

This is the default value if none was provided by the user.
This is also the type C<strict_type> will look at when checking type
integrity (see below).

=item required

A boolean flag that indicates if this argument was a required
argument. If marked as required and not provided, check() will fail.

=item strict_type

This does a C<ref()> check on the argument provided. The C<ref> of the
argument must be the same as the C<ref> of the default value for this
check to pass.

This is very useful if you insist on taking an array reference as
argument for example.

=item defined

If this template key is true, enforces that if this key is provided by
user input, its value is C<defined>. This just means that the user is
not allowed to pass C<undef> as a value for this key and is equivalent
to:
    allow => sub { defined $_[0] && OTHER TESTS }

=item no_override

This allows you to specify C<constants> in your template. ie, they
keys that are not allowed to be altered by the user. It pretty much
allows you to keep all your C<configurable> data in one place; the
C<Params::Check> template.

=item store

This allows you to pass a reference to a scalar, in which the data
will be stored:

    my $x;
    my $args = check(foo => { default => 1, store => \$x }, $input);

This is basically shorthand for saying:

    my $args = check( { foo => { default => 1 }, $input );
    my $x    = $args->{foo};

You can alter the global variable $Params::Check::NO_DUPLICATES to
control whether the C<store>'d key will still be present in your
result set. See the L<Global Variables> section below.

=item allow

A set of criteria used to validate a particular piece of data if it
has to adhere to particular rules.

See the C<allow()> function for details.

=back

=head1 Functions

=head2 check( \%tmpl, \%args, [$verbose] );

This function is not exported by default, so you'll have to ask for it
via:

    use Params::Check qw[check];

or use its fully qualified name instead.

C<check> takes a list of arguments, as follows:

=over 4

=item Template

This is a hashreference which contains a template as explained in the
C<SYNOPSIS> and C<Template> section.

=item Arguments

This is a reference to a hash of named arguments which need checking.

=item Verbose

A boolean to indicate whether C<check> should be verbose and warn
about what went wrong in a check or not.

You can enable this program wide by setting the package variable
C<$Params::Check::VERBOSE> to a true value. For details, see the
section on C<Global Variables> below.

=back

C<check> will return when it fails, or a hashref with lowercase
keys of parsed arguments when it succeeds.

So a typical call to check would look like this:

    my $parsed = check( \%template, \%arguments, $VERBOSE )
                    or warn q[Arguments could not be parsed!];

A lot of the behaviour of C<check()> can be altered by setting
package variables. See the section on C<Global Variables> for details
on this.

=cut

sub check {
    my ($utmpl, $href, $verbose) = @_;

    ### did we get the arguments we need? ###
    return if !$utmpl or !$href;

    ### sensible defaults ###
    $verbose ||= $VERBOSE || 0;

    ### clear the current error string ###
    _clear_error();

    ### XXX what type of template is it? ###
    ### { key => { } } ?
    #if (ref $args eq 'HASH') {
    #    1;
    #}

    ### clean up the template ###
    my $args = _clean_up_args( $href ) or return;

    ### sanity check + defaults + required keys set? ###
    my $defs = _sanity_check_and_defaults( $utmpl, $args, $verbose )
                    or return;

    ### deref only once ###
    my %utmpl   = %$utmpl;
    my %args    = %$args;
    my %defs    = %$defs;

    ### flag to see if anything went wrong ###
    my $wrong; 
    
    ### flag to see if we warned for anything, needed for warnings_fatal
    my $warned;

    for my $key (keys %args) {

        ### you gave us this key, but it's not in the template ###
        unless( $utmpl{$key} ) {

            ### but we'll allow it anyway ###
            if( $ALLOW_UNKNOWN ) {
                $defs{$key} = $args{$key};

            ### warn about the error ###
            } else {
                _store_error(
                    loc("Key '%1' is not a valid key for %2 provided by %3",
                        $key, _who_was_it(), _who_was_it(1)), $verbose);
                $warned ||= 1;
            }
            next;
        }

        ### check if you're even allowed to override this key ###
        if( $utmpl{$key}->{'no_override'} ) {
            _store_error(
                loc(q[You are not allowed to override key '%1'].
                    q[for %2 from %3], $key, _who_was_it(), _who_was_it(1)),
                $verbose
            );
            $warned ||= 1;
            next;
        }

        ### copy of this keys template instructions, to save derefs ###
        my %tmpl = %{$utmpl{$key}};

        ### check if you were supposed to provide defined() values ###
        if( ($tmpl{'defined'} || $ONLY_ALLOW_DEFINED) and
            not defined $args{$key}
        ) {
            _store_error(loc(q|Key '%1' must be defined when passed|, $key),
                $verbose );
            $wrong ||= 1;
            next;
        }

        ### check if they should be of a strict type, and if it is ###
        if( ($tmpl{'strict_type'} || $STRICT_TYPE) and
            (ref $args{$key} ne ref $tmpl{'default'})
        ) {
            _store_error(loc(q|Key '%1' needs to be of type '%2'|,
                        $key, ref $tmpl{'default'} || 'SCALAR'), $verbose );
            $wrong ||= 1;
            next;
        }

        ### check if we have an allow handler, to validate against ###
        ### allow() will report its own errors ###
        if( exists $tmpl{'allow'} and not do {
                local $_ERROR_STRING;
                allow( $args{$key}, $tmpl{'allow'} )
            }         
        ) {
            ### stringify the value in the error report -- we don't want dumps
            ### of objects, but we do want to see *roughly* what we passed
            _store_error(loc(q|Key '%1' (%2) is of invalid type for '%3' |.
                             q|provided by %4|,
                            $key, "$args{$key}", _who_was_it(),
                            _who_was_it(1)), $verbose);
            $wrong ||= 1;
            next;
        }

        ### we got here, then all must be OK ###
        $defs{$key} = $args{$key};

    }

    ### croak with the collected errors if there were errors and 
    ### we have the fatal flag toggled.
    croak(__PACKAGE__->last_error) if ($wrong || $warned) && $WARNINGS_FATAL;

    ### done with our loop... if $wrong is set, somethign went wrong
    ### and the user is already informed, just return...
    return if $wrong;

    ### check if we need to store any of the keys ###
    ### can't do it before, because something may go wrong later,
    ### leaving the user with a few set variables
    for my $key (keys %defs) {
        if( my $ref = $utmpl{$key}->{'store'} ) {
            $$ref = $NO_DUPLICATES ? delete $defs{$key} : $defs{$key};
        }
    }

    return \%defs;
}

=head2 allow( $test_me, \@criteria );

The function that handles the C<allow> key in the template is also
available for independent use.

The function takes as first argument a key to test against, and
as second argument any form of criteria that are also allowed by
the C<allow> key in the template.

You can use the following types of values for allow:

=over 4

=item string

The provided argument MUST be equal to the string for the validation
to pass.

=item regexp

The provided argument MUST match the regular expression for the
validation to pass.

=item subroutine

The provided subroutine MUST return true in order for the validation
to pass and the argument accepted.

(This is particularly useful for more complicated data).

=item array ref

The provided argument MUST equal one of the elements of the array
ref for the validation to pass. An array ref can hold all the above
values.

=back

It returns true if the key matched the criteria, or false otherwise.

=cut

sub allow {
    ### use $_[0] and $_[1] since this is hot code... ###
    #my ($val, $ref) = @_;

    ### it's a regexp ###
    if( ref $_[1] eq 'Regexp' ) {
        local $^W;  # silence warnings if $val is undef #
        return if $_[0] !~ /$_[1]/;

    ### it's a sub ###
    } elsif ( ref $_[1] eq 'CODE' ) {
        return unless $_[1]->( $_[0] );

    ### it's an array ###
    } elsif ( ref $_[1] eq 'ARRAY' ) {

        ### loop over the elements, see if one of them says the
        ### value is OK
        ### also, short-cicruit when possible
        for ( @{$_[1]} ) {
            return 1 if allow( $_[0], $_ );
        }
        
        return;

    ### fall back to a simple, but safe 'eq' ###
    } else {
        return unless _safe_eq( $_[0], $_[1] );
    }

    ### we got here, no failures ###
    return 1;
}

### helper functions ###

### clean up the template ###
sub _clean_up_args {
    ### don't even bother to loop, if there's nothing to clean up ###
    return $_[0] if $PRESERVE_CASE and !$STRIP_LEADING_DASHES;

    my %args = %{$_[0]};

    ### keys are note aliased ###
    for my $key (keys %args) {
        my $org = $key;
        $key = lc $key unless $PRESERVE_CASE;
        $key =~ s/^-// if $STRIP_LEADING_DASHES;
        $args{$key} = delete $args{$org} if $key ne $org;
    }

    ### return references so we always return 'true', even on empty
    ### arguments
    return \%args;
}

sub _sanity_check_and_defaults {
    my %utmpl   = %{$_[0]};
    my %args    = %{$_[1]};
    my $verbose = $_[2];

    my %defs; my $fail;
    for my $key (keys %utmpl) {

        ### check if required keys are provided
        ### keys are now lower cased, unless preserve case was enabled
        ### at which point, the utmpl keys must match, but that's the users
        ### problem.
        if( $utmpl{$key}->{'required'} and not exists $args{$key} ) {
            _store_error(
                loc(q|Required option '%1' is not provided for %2 by %3|,
                    $key, _who_was_it(1), _who_was_it(2)), $verbose );

            ### mark the error ###
            $fail++;
            next;
        }

        ### next, set the default, make sure the key exists in %defs ###
        $defs{$key} = $utmpl{$key}->{'default'}
                        if exists $utmpl{$key}->{'default'};

        if( $SANITY_CHECK_TEMPLATE ) {
            ### last, check if they provided any weird template keys
            ### -- do this last so we don't always execute this code.
            ### just a small optimization.
            map {   _store_error(
                        loc(q|Template type '%1' not supported [at key '%2']|,
                        $_, $key), 1, 1 );
            } grep {
                not $known_keys{$_}
            } keys %{$utmpl{$key}};
        
            ### make sure you passed a ref, otherwise, complain about it!
            if ( exists $utmpl{$key}->{'store'} ) {
                _store_error( loc(
                    q|Store variable for '%1' is not a reference!|, $key
                ), 1, 1 ) unless ref $utmpl{$key}->{'store'};
            }
        }
    }

    ### errors found ###
    return if $fail;

    ### return references so we always return 'true', even on empty
    ### defaults
    return \%defs;
}

sub _safe_eq {
    ### only do a straight 'eq' if they're both defined ###
    return defined($_[0]) && defined($_[1])
                ? $_[0] eq $_[1]
                : defined($_[0]) eq defined($_[1]);
}

sub _who_was_it {
    my $level = $_[0] || 0;

    return (caller(2 + $CALLER_DEPTH + $level))[3] || 'ANON'
}

=head2 last_error()

Returns a string containing all warnings and errors reported during
the last time C<check> was called.

This is useful if you want to report then some other way than
C<carp>'ing when the verbose flag is on.

It is exported upon request.

=cut

{   $_ERROR_STRING = '';

    sub _store_error {
        my($err, $verbose, $offset) = @_[0..2];
        $verbose ||= 0;
        $offset  ||= 0;
        my $level   = 1 + $offset;

        local $Carp::CarpLevel = $level;

        carp $err if $verbose;

        $_ERROR_STRING .= $err . "\n";
    }

    sub _clear_error {
        $_ERROR_STRING = '';
    }

    sub last_error { $_ERROR_STRING }
}

1;

=head1 Global Variables

The behaviour of Params::Check can be altered by changing the
following global variables:

=head2 $Params::Check::VERBOSE

This controls whether Params::Check will issue warnings and
explanations as to why certain things may have failed.
If you set it to 0, Params::Check will not output any warnings.

The default is 1 when L<warnings> are enabled, 0 otherwise;

=head2 $Params::Check::STRICT_TYPE

This works like the C<strict_type> option you can pass to C<check>,
which will turn on C<strict_type> globally for all calls to C<check>.

The default is 0;

=head2 $Params::Check::ALLOW_UNKNOWN

If you set this flag, unknown options will still be present in the
return value, rather than filtered out. This is useful if your
subroutine is only interested in a few arguments, and wants to pass
the rest on blindly to perhaps another subroutine.

The default is 0;

=head2 $Params::Check::STRIP_LEADING_DASHES

If you set this flag, all keys passed in the following manner:

    function( -key => 'val' );

will have their leading dashes stripped.

=head2 $Params::Check::NO_DUPLICATES

If set to true, all keys in the template that are marked as to be
stored in a scalar, will also be removed from the result set.

Default is false, meaning that when you use C<store> as a template
key, C<check> will put it both in the scalar you supplied, as well as
in the hashref it returns.

=head2 $Params::Check::PRESERVE_CASE

If set to true, L<Params::Check> will no longer convert all keys from
the user input to lowercase, but instead expect them to be in the
case the template provided. This is useful when you want to use
similar keys with different casing in your templates.

Understand that this removes the case-insensitivy feature of this
module.

Default is 0;

=head2 $Params::Check::ONLY_ALLOW_DEFINED

If set to true, L<Params::Check> will require all values passed to be
C<defined>. If you wish to enable this on a 'per key' basis, use the
template option C<defined> instead.

Default is 0;

=head2 $Params::Check::SANITY_CHECK_TEMPLATE

If set to true, L<Params::Check> will sanity check templates, validating
for errors and unknown keys. Although very useful for debugging, this
can be somewhat slow in hot-code and large loops.

To disable this check, set this variable to C<false>.

Default is 1;

=head2 $Params::Check::WARNINGS_FATAL

If set to true, L<Params::Check> will C<croak> when an error during 
template validation occurs, rather than return C<false>.

Default is 0;

=head2 $Params::Check::CALLER_DEPTH

This global modifies the argument given to C<caller()> by
C<Params::Check::check()> and is useful if you have a custom wrapper
function around C<Params::Check::check()>. The value must be an
integer, indicating the number of wrapper functions inserted between
the real function call and C<Params::Check::check()>.

Example wrapper function, using a custom stacktrace:

    sub check {
        my ($template, $args_in) = @_;

        local $Params::Check::WARNINGS_FATAL = 1;
        local $Params::Check::CALLER_DEPTH = $Params::Check::CALLER_DEPTH + 1;
        my $args_out = Params::Check::check($template, $args_in);

        my_stacktrace(Params::Check::last_error) unless $args_out;

        return $args_out;
    }

Default is 0;

=head1 AUTHOR

This module by
Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 Acknowledgements

Thanks to Richard Soderberg for his performance improvements.

=head1 COPYRIGHT

This module is
copyright (c) 2003,2004 Jos Boumans E<lt>kane@cpan.orgE<gt>.
All rights reserved.

This library is free software;
you may redistribute and/or modify it under the same
terms as Perl itself.

=cut

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
