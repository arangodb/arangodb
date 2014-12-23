package TAP::Parser::Result::Plan;

use strict;

use vars qw($VERSION @ISA);
use TAP::Parser::Result;
@ISA = 'TAP::Parser::Result';

=head1 NAME

TAP::Parser::Result::Plan - Plan result token.

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

=head1 DESCRIPTION

This is a subclass of L<TAP::Parser::Result>.  A token of this class will be
returned if a plan line is encountered.

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

=head3 C<plan> 

  if ( $result->is_plan ) {
     print $result->plan;
  }

This is merely a synonym for C<as_string>.

=cut

sub plan { '1..' . shift->{tests_planned} }

##############################################################################

=head3 C<tests_planned>

  my $planned = $result->tests_planned;

Returns the number of tests planned.  For example, a plan of C<1..17> will
cause this method to return '17'.

=cut

sub tests_planned { shift->{tests_planned} }

##############################################################################

=head3 C<directive>

 my $directive = $plan->directive; 

If a SKIP directive is included with the plan, this method will return it.

 1..0 # SKIP: why bother?

=cut

sub directive { shift->{directive} }

##############################################################################

=head3 C<has_skip>

  if ( $result->has_skip ) { ... }

Returns a boolean value indicating whether or not this test has a SKIP
directive.

=head3 C<explanation>

 my $explanation = $plan->explanation;

If a SKIP directive was included with the plan, this method will return the
explanation, if any.

=cut

sub explanation { shift->{explanation} }

=head3 C<todo_list>

  my $todo = $result->todo_list;
  for ( @$todo ) {
      ...
  }

=cut

sub todo_list { shift->{todo_list} }

1;
