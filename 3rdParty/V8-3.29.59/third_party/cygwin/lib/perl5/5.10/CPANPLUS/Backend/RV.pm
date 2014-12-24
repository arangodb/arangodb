package CPANPLUS::Backend::RV;

use strict;
use vars qw[$STRUCT];


use CPANPLUS::Error;
use CPANPLUS::Internals::Constants;

use IPC::Cmd                    qw[can_run run];
use Params::Check               qw[check];

use base 'Object::Accessor';

local $Params::Check::VERBOSE = 1;


=pod

=head1 NAME

CPANPLUS::Backend::RV

=head1 SYNOPSIS

    ### create a CPANPLUS::Backend::RV object
    $backend_rv     = CPANPLUS::Backend::RV->new(
                                ok          => $boolean,
                                args        => $args,
                                rv          => $return_value
                                function    => $calling_function );

    ### if you have a CPANPLUS::Backend::RV object
    $passed_args    = $backend_rv->args;    # args passed to function
    $ok             = $backend_rv->ok;      # boolean indication overall
                                            # result of the call
    $function       = $backend_rv->fucntion # name of the calling
                                            # function
    $rv             = $backend_rv->rv       # the actual return value
                                            # of the calling function

=head1 DESCRIPTION

This module provides return value objects for multi-module
calls to CPANPLUS::Backend. In boolean context, it returns the status
of the overall result (ie, the same as the C<ok> method would).

=head1 METHODS

=head2 new( ok => BOOL, args => DATA, rv => DATA, [function => $method_name] )

Creates a new CPANPLUS::Backend::RV object from the data provided.
This method should only be called by CPANPLUS::Backend functions.
The accessors may be used by users inspecting an RV object.

All the argument names can be used as accessors later to retrieve the
data.

Arguments:

=over 4

=item ok

Boolean indicating overall success

=item args

The arguments provided to the function that returned this rv object.
Useful to inspect later to see what was actually passed to the function
in case of an error.

=item rv

An arbitrary data structure that has the detailed return values of each
of your multi-module calls.

=item function

The name of the function that created this rv object.
Can be explicitly passed. If not, C<new()> will try to deduce the name
from C<caller()> information.

=back

=cut

sub new {
    my $class   = shift;
    my %hash    = @_;

    my $tmpl = {
        ok          => { required => 1, allow => BOOLEANS },
        args        => { required => 1 },
        rv          => { required => 1 },
        function    => { default => CALLING_FUNCTION->() },
    };

    my $args    = check( $tmpl, \%hash ) or return;
    my $self    = bless {}, $class;

#    $self->mk_accessors( qw[ok args function rv] );
    $self->mk_accessors( keys %$tmpl );

    ### set the values passed in the struct ###
    while( my($key,$val) = each %$args ) {
        $self->$key( $val );
    }

    return $self;
}

sub _ok { return shift->ok }
#sub _stringify  { Carp::carp( "stringifying!" ); overload::StrVal( shift ) }

### make it easier to check if($rv) { foo() }
### this allows people to not have to explicitly say
### if( $rv->ok ) { foo() }
### XXX add an explicit stringify, so it doesn't fall back to "bool"? :(
use overload bool       => \&_ok, 
#             '""'       => \&_stringify,
             fallback   => 1;

=pod

=head1 BUG REPORTS

Please report bugs or other issues to E<lt>bug-cpanplus@rt.cpan.org<gt>.

=head1 AUTHOR

This module by Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 COPYRIGHT

The CPAN++ interface (of which this module is a part of) is copyright (c) 
2001 - 2007, Jos Boumans E<lt>kane@cpan.orgE<gt>. All rights reserved.

This library is free software; you may redistribute and/or modify it 
under the same terms as Perl itself.

=cut

1;
