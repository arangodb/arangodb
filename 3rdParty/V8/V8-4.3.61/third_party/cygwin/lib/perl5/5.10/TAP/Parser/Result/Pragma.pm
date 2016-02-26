package TAP::Parser::Result::Pragma;

use strict;

use vars qw($VERSION @ISA);
use TAP::Parser::Result;
@ISA = 'TAP::Parser::Result';

=head1 NAME

TAP::Parser::Result::Pragma - TAP pragma token.

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

=head1 DESCRIPTION

This is a subclass of L<TAP::Parser::Result>.  A token of this class will be
returned if a pragma is encountered.

 TAP version 13
 pragma +strict, -foo

Pragmas are only supported from TAP version 13 onwards.

=head1 OVERRIDDEN METHODS

Mainly listed here to shut up the pitiful screams of the pod coverage tests.
They keep me awake at night.

=over 4

=item * C<as_string>

=item * C<raw>

=back

=cut

##############################################################################

=head2 Instance Methods

=head3 C<pragmas> 

if ( $result->is_pragma ) {
    @pragmas = $result->pragmas;
}

=cut

sub pragmas {
    my @pragmas = @{ shift->{pragmas} };
    return wantarray ? @pragmas : \@pragmas;
}

1;
