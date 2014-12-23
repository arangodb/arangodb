package TAP::Parser::Result::Comment;

use strict;

use vars qw($VERSION @ISA);
use TAP::Parser::Result;
@ISA = 'TAP::Parser::Result';

=head1 NAME

TAP::Parser::Result::Comment - Comment result token.

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

=head1 DESCRIPTION

This is a subclass of L<TAP::Parser::Result>.  A token of this class will be
returned if a comment line is encountered.

 1..1
 ok 1 - woo hooo!
 # this is a comment

=head1 OVERRIDDEN METHODS

Mainly listed here to shut up the pitiful screams of the pod coverage tests.
They keep me awake at night.

=over 4

=item * C<as_string>

Note that this method merely returns the comment preceded by a '# '.

=back

=cut

##############################################################################

=head2 Instance Methods

=head3 C<comment> 

  if ( $result->is_comment ) {
      my $comment = $result->comment;
      print "I have something to say:  $comment";
  }

=cut

sub comment   { shift->{comment} }
sub as_string { shift->{raw} }

1;
