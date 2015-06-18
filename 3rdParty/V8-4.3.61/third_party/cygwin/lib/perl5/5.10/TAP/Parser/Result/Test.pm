package TAP::Parser::Result::Test;

use strict;

use vars qw($VERSION @ISA);
use TAP::Parser::Result;
@ISA = 'TAP::Parser::Result';

use vars qw($VERSION);

=head1 NAME

TAP::Parser::Result::Test - Test result token.

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

=head1 DESCRIPTION

This is a subclass of L<TAP::Parser::Result>.  A token of this class will be
returned if a test line is encountered.

 1..1
 ok 1 - woo hooo!

=head1 OVERRIDDEN METHODS

This class is the workhorse of the L<TAP::Parser> system.  Most TAP lines will
be test lines and if C<< $result->is_test >>, then you have a bunch of methods
at your disposal.

=head2 Instance Methods

=cut

##############################################################################

=head3 C<ok>

  my $ok = $result->ok;

Returns the literal text of the C<ok> or C<not ok> status.

=cut

sub ok { shift->{ok} }

##############################################################################

=head3 C<number>

  my $test_number = $result->number;

Returns the number of the test, even if the original TAP output did not supply
that number.

=cut

sub number { shift->{test_num} }

sub _number {
    my ( $self, $number ) = @_;
    $self->{test_num} = $number;
}

##############################################################################

=head3 C<description>

  my $description = $result->description;

Returns the description of the test, if any.  This is the portion after the
test number but before the directive.

=cut

sub description { shift->{description} }

##############################################################################

=head3 C<directive>

  my $directive = $result->directive;

Returns either C<TODO> or C<SKIP> if either directive was present for a test
line.

=cut

sub directive { shift->{directive} }

##############################################################################

=head3 C<explanation>

  my $explanation = $result->explanation;

If a test had either a C<TODO> or C<SKIP> directive, this method will return
the accompanying explantion, if present.

  not ok 17 - 'Pigs can fly' # TODO not enough acid

For the above line, the explanation is I<not enough acid>.

=cut

sub explanation { shift->{explanation} }

##############################################################################

=head3 C<is_ok>

  if ( $result->is_ok ) { ... }

Returns a boolean value indicating whether or not the test passed.  Remember
that for TODO tests, the test always passes.

If the test is unplanned, this method will always return false.  See
C<is_unplanned>.

=cut

sub is_ok {
    my $self = shift;

    return if $self->is_unplanned;

    # TODO directives reverse the sense of a test.
    return $self->has_todo ? 1 : $self->ok !~ /not/;
}

##############################################################################

=head3 C<is_actual_ok>

  if ( $result->is_actual_ok ) { ... }

Returns a boolean value indicating whether or not the test passed, regardless
of its TODO status.

=cut

sub is_actual_ok {
    my $self = shift;
    return $self->{ok} !~ /not/;
}

##############################################################################

=head3 C<actual_passed>

Deprecated.  Please use C<is_actual_ok> instead.

=cut

sub actual_passed {
    warn 'actual_passed() is deprecated.  Please use "is_actual_ok()"';
    goto &is_actual_ok;
}

##############################################################################

=head3 C<todo_passed>

  if ( $test->todo_passed ) {
     # test unexpectedly succeeded
  }

If this is a TODO test and an 'ok' line, this method returns true.
Otherwise, it will always return false (regardless of passing status on
non-todo tests).

This is used to track which tests unexpectedly succeeded.

=cut

sub todo_passed {
    my $self = shift;
    return $self->has_todo && $self->is_actual_ok;
}

##############################################################################

=head3 C<todo_failed>

  # deprecated in favor of 'todo_passed'.  This method was horribly misnamed.

This was a badly misnamed method.  It indicates which TODO tests unexpectedly
succeeded.  Will now issue a warning and call C<todo_passed>.

=cut

sub todo_failed {
    warn 'todo_failed() is deprecated.  Please use "todo_passed()"';
    goto &todo_passed;
}

##############################################################################

=head3 C<has_skip>

  if ( $result->has_skip ) { ... }

Returns a boolean value indicating whether or not this test has a SKIP
directive.

=head3 C<has_todo>

  if ( $result->has_todo ) { ... }

Returns a boolean value indicating whether or not this test has a TODO
directive.

=head3 C<as_string>

  print $result->as_string;

This method prints the test as a string.  It will probably be similar, but
not necessarily identical, to the original test line.  Directives are
capitalized, some whitespace may be trimmed and a test number will be added if
it was not present in the original line.  If you need the original text of the
test line, use the C<raw> method.

=cut

sub as_string {
    my $self   = shift;
    my $string = $self->ok . " " . $self->number;
    if ( my $description = $self->description ) {
        $string .= " $description";
    }
    if ( my $directive = $self->directive ) {
        my $explanation = $self->explanation;
        $string .= " # $directive $explanation";
    }
    return $string;
}

##############################################################################

=head3 C<is_unplanned>

  if ( $test->is_unplanned ) { ... }
  $test->is_unplanned(1);

If a test number is greater than the number of planned tests, this method will
return true.  Unplanned tests will I<always> return false for C<is_ok>,
regardless of whether or not the test C<has_todo>.

Note that if tests have a trailing plan, it is not possible to set this
property for unplanned tests as we do not know it's unplanned until the plan
is reached:

  print <<'END';
  ok 1
  ok 2
  1..1
  END

=cut

sub is_unplanned {
    my $self = shift;
    return ( $self->{unplanned} || '' ) unless @_;
    $self->{unplanned} = !!shift;
    return $self;
}

1;
