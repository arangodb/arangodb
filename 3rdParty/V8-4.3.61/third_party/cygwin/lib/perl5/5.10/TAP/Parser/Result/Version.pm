package TAP::Parser::Result::Version;

use strict;

use vars qw($VERSION @ISA);
use TAP::Parser::Result;
@ISA = 'TAP::Parser::Result';

=head1 NAME

TAP::Parser::Result::Version - TAP syntax version token.

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

=head1 DESCRIPTION

This is a subclass of L<TAP::Parser::Result>.  A token of this class will be
returned if a version line is encountered.

 TAP version 13
 ok 1
 not ok 2

The first version of TAP to include an explicit version number is 13.

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

=head3 C<version> 

  if ( $result->is_version ) {
     print $result->version;
  }

This is merely a synonym for C<as_string>.

=cut

sub version { shift->{version} }

1;
