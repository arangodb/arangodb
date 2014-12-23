package TAP::Parser::Result::YAML;

use strict;

use vars qw($VERSION @ISA);
use TAP::Parser::Result;
@ISA = 'TAP::Parser::Result';

=head1 NAME

TAP::Parser::Result::YAML - YAML result token.

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

=head1 DESCRIPTION

This is a subclass of L<TAP::Parser::Result>.  A token of this class will be
returned if a YAML block is encountered.

 1..1
 ok 1 - woo hooo!

C<1..1> is the plan.  Gotta have a plan.

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

=head3 C<data> 

  if ( $result->is_yaml ) {
     print $result->data;
  }

Return the parsed YAML data for this result

=cut

sub data { shift->{data} }

1;
