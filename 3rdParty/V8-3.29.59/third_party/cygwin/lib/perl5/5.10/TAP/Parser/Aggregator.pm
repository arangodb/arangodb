package TAP::Parser::Aggregator;

use strict;
use Benchmark;
use vars qw($VERSION);

=head1 NAME

TAP::Parser::Aggregator - Aggregate TAP::Parser results

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

=head1 SYNOPSIS

    use TAP::Parser::Aggregator;

    my $aggregate = TAP::Parser::Aggregator->new;
    $aggregate->add( 't/00-load.t', $load_parser );
    $aggregate->add( 't/10-lex.t',  $lex_parser  );

    my $summary = <<'END_SUMMARY';
    Passed:  %s
    Failed:  %s
    Unexpectedly succeeded: %s
    END_SUMMARY
    printf $summary,
           scalar $aggregate->passed,
           scalar $aggregate->failed,
           scalar $aggregate->todo_passed;

=head1 DESCRIPTION

C<TAP::Parser::Aggregator> collects parser objects and allows
reporting/querying their aggregate results.

=head1 METHODS

=head2 Class Methods

=head3 C<new>

 my $aggregate = TAP::Parser::Aggregator->new;

Returns a new C<TAP::Parser::Aggregator> object.

=cut

my %SUMMARY_METHOD_FOR;

BEGIN {    # install summary methods
    %SUMMARY_METHOD_FOR = map { $_ => $_ } qw(
      failed
      parse_errors
      passed
      skipped
      todo
      todo_passed
      total
      wait
      exit
    );
    $SUMMARY_METHOD_FOR{total} = 'tests_run';

    foreach my $method ( keys %SUMMARY_METHOD_FOR ) {
        next if 'total' eq $method;
        no strict 'refs';
        *$method = sub {
            my $self = shift;
            return wantarray
              ? @{ $self->{"descriptions_for_$method"} }
              : $self->{$method};
        };
    }
}    # end install summary methods

sub new {
    my ($class) = @_;
    my $self = bless {}, $class;
    $self->_initialize;
    return $self;
}

sub _initialize {
    my ($self) = @_;
    $self->{parser_for}  = {};
    $self->{parse_order} = [];
    foreach my $summary ( keys %SUMMARY_METHOD_FOR ) {
        $self->{$summary} = 0;
        next if 'total' eq $summary;
        $self->{"descriptions_for_$summary"} = [];
    }
    return $self;
}

##############################################################################

=head2 Instance Methods

=head3 C<add>

  $aggregate->add( $description => $parser );

The C<$description> is usually a test file name (but only by
convention.)  It is used as a unique identifier (see e.g.
L<"parsers">.)  Reusing a description is a fatal error.

The C<$parser> is a L<TAP::Parser|TAP::Parser> object.

=cut

sub add {
    my ( $self, $description, $parser ) = @_;
    if ( exists $self->{parser_for}{$description} ) {
        $self->_croak( "You already have a parser for ($description)."
              . " Perhaps you have run the same test twice." );
    }
    push @{ $self->{parse_order} } => $description;
    $self->{parser_for}{$description} = $parser;

    while ( my ( $summary, $method ) = each %SUMMARY_METHOD_FOR ) {
        if ( my $count = $parser->$method() ) {
            $self->{$summary} += $count;
            push @{ $self->{"descriptions_for_$summary"} } => $description;
        }
    }

    return $self;
}

##############################################################################

=head3 C<parsers>

  my $count   = $aggregate->parsers;
  my @parsers = $aggregate->parsers;
  my @parsers = $aggregate->parsers(@descriptions);

In scalar context without arguments, this method returns the number of parsers
aggregated.  In list context without arguments, returns the parsers in the
order they were added.

If C<@descriptions> is given, these correspond to the keys used in each
call to the add() method.  Returns an array of the requested parsers (in
the requested order) in list context or an array reference in scalar
context.

Requesting an unknown identifier is a fatal error.

=cut

sub parsers {
    my $self = shift;
    return $self->_get_parsers(@_) if @_;
    my $descriptions = $self->{parse_order};
    my @parsers      = @{ $self->{parser_for} }{@$descriptions};

    # Note:  Because of the way context works, we must assign the parsers to
    # the @parsers array or else this method does not work as documented.
    return @parsers;
}

sub _get_parsers {
    my ( $self, @descriptions ) = @_;
    my @parsers;
    foreach my $description (@descriptions) {
        $self->_croak("A parser for ($description) could not be found")
          unless exists $self->{parser_for}{$description};
        push @parsers => $self->{parser_for}{$description};
    }
    return wantarray ? @parsers : \@parsers;
}

=head3 C<descriptions>

Get an array of descriptions in the order in which they were added to the aggregator.

=cut

sub descriptions { @{ shift->{parse_order} || [] } }

=head3 C<start>

Call C<start> immediately before adding any results to the aggregator.
Among other times it records the start time for the test run.

=cut

sub start {
    my $self = shift;
    $self->{start_time} = Benchmark->new;
}

=head3 C<stop>

Call C<stop> immediately after adding all test results to the aggregator.

=cut

sub stop {
    my $self = shift;
    $self->{end_time} = Benchmark->new;
}

=head3 C<elapsed>

Elapsed returns a L<Benchmark> object that represents the running time
of the aggregated tests. In order for C<elapsed> to be valid you must
call C<start> before running the tests and C<stop> immediately
afterwards.

=cut

sub elapsed {
    my $self = shift;

    require Carp;
    Carp::croak
      q{Can't call elapsed without first calling start and then stop}
      unless defined $self->{start_time} && defined $self->{end_time};
    return timediff( $self->{end_time}, $self->{start_time} );
}

=head3 C<elapsed_timestr>

Returns a formatted string representing the runtime returned by
C<elapsed()>.  This lets the caller not worry about Benchmark.

=cut

sub elapsed_timestr {
    my $self = shift;

    my $elapsed = $self->elapsed;

    return timestr($elapsed);
}

=head3 C<all_passed>

Return true if all the tests passed and no parse errors were detected.

=cut

sub all_passed {
    my $self = shift;
    return
         $self->total
      && $self->total == $self->passed
      && !$self->has_errors;
}

=head3 C<get_status>

Get a single word describing the status of the aggregated tests.
Depending on the outcome of the tests returns 'PASS', 'FAIL' or
'NOTESTS'. This token is understood by L<CPAN::Reporter>.

=cut

sub get_status {
    my $self = shift;

    my $total  = $self->total;
    my $passed = $self->passed;

    return
        ( $self->has_errors || $total != $passed ) ? 'FAIL'
      : $total ? 'PASS'
      :          'NOTESTS';
}

##############################################################################

=head2 Summary methods

Each of the following methods will return the total number of corresponding
tests if called in scalar context.  If called in list context, returns the
descriptions of the parsers which contain the corresponding tests (see C<add>
for an explanation of description.

=over 4

=item * failed

=item * parse_errors

=item * passed

=item * skipped

=item * todo

=item * todo_passed

=item * wait

=item * exit

=back

For example, to find out how many tests unexpectedly succeeded (TODO tests
which passed when they shouldn't):

 my $count        = $aggregate->todo_passed;
 my @descriptions = $aggregate->todo_passed;

Note that C<wait> and C<exit> are the totals of the wait and exit
statuses of each of the tests. These values are totalled only to provide
a true value if any of them are non-zero.

=cut

##############################################################################

=head3 C<total>

  my $tests_run = $aggregate->total;

Returns the total number of tests run.

=cut

sub total { shift->{total} }

##############################################################################

=head3 C<has_problems>

  if ( $parser->has_problems ) {
      ...
  }

Identical to C<has_errors>, but also returns true if any TODO tests
unexpectedly succeeded.  This is more akin to "warnings".

=cut

sub has_problems {
    my $self = shift;
    return $self->todo_passed
      || $self->has_errors;
}

##############################################################################

=head3 C<has_errors>

  if ( $parser->has_errors ) {
      ...
  }

Returns true if I<any> of the parsers failed.  This includes:

=over 4

=item * Failed tests

=item * Parse erros

=item * Bad exit or wait status

=back

=cut

sub has_errors {
    my $self = shift;
    return
         $self->failed
      || $self->parse_errors
      || $self->exit
      || $self->wait;
}

##############################################################################

=head3 C<todo_failed>

  # deprecated in favor of 'todo_passed'.  This method was horribly misnamed.

This was a badly misnamed method.  It indicates which TODO tests unexpectedly
succeeded.  Will now issue a warning and call C<todo_passed>.

=cut

sub todo_failed {
    warn
      '"todo_failed" is deprecated.  Please use "todo_passed".  See the docs.';
    goto &todo_passed;
}

sub _croak {
    my $proto = shift;
    require Carp;
    Carp::croak(@_);
}

=head1 See Also

L<TAP::Parser>

L<TAP::Harness>

=cut

1;
