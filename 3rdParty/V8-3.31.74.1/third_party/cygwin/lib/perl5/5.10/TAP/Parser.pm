package TAP::Parser;

use strict;
use vars qw($VERSION @ISA);

use TAP::Base                 ();
use TAP::Parser::Grammar      ();
use TAP::Parser::Result       ();
use TAP::Parser::Source       ();
use TAP::Parser::Source::Perl ();
use TAP::Parser::Iterator     ();

use Carp qw( confess );

@ISA = qw(TAP::Base);

=head1 NAME

TAP::Parser - Parse L<TAP|Test::Harness::TAP> output

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

my $DEFAULT_TAP_VERSION = 12;
my $MAX_TAP_VERSION     = 13;

$ENV{TAP_VERSION} = $MAX_TAP_VERSION;

END {

    # For VMS.
    delete $ENV{TAP_VERSION};
}

BEGIN {    # making accessors
    foreach my $method (
        qw(
        _stream
        _spool
        _grammar
        exec
        exit
        is_good_plan
        plan
        tests_planned
        tests_run
        wait
        version
        in_todo
        start_time
        end_time
        skip_all
        )
      )
    {
        no strict 'refs';

        # another tiny performance hack
        if ( $method =~ /^_/ ) {
            *$method = sub {
                my $self = shift;
                return $self->{$method} unless @_;

                # Trusted methods
                unless ( ( ref $self ) =~ /^TAP::Parser/ ) {
                    Carp::croak("$method() may not be set externally");
                }

                $self->{$method} = shift;
            };
        }
        else {
            *$method = sub {
                my $self = shift;
                return $self->{$method} unless @_;
                $self->{$method} = shift;
            };
        }
    }
}    # done making accessors

=head1 SYNOPSIS

    use TAP::Parser;

    my $parser = TAP::Parser->new( { source => $source } );

    while ( my $result = $parser->next ) {
        print $result->as_string;
    }

=head1 DESCRIPTION

C<TAP::Parser> is designed to produce a proper parse of TAP output. For
an example of how to run tests through this module, see the simple
harnesses C<examples/>.

There's a wiki dedicated to the Test Anything Protocol:

L<http://testanything.org>

It includes the TAP::Parser Cookbook:

L<http://testanything.org/wiki/index.php/TAP::Parser_Cookbook>

=head1 METHODS

=head2 Class Methods

=head3 C<new>

 my $parser = TAP::Parser->new(\%args);

Returns a new C<TAP::Parser> object.

The arguments should be a hashref with I<one> of the following keys:

=over 4

=item * C<source>

This is the preferred method of passing arguments to the constructor.  To
determine how to handle the source, the following steps are taken.

If the source contains a newline, it's assumed to be a string of raw TAP
output.

If the source is a reference, it's assumed to be something to pass to
the L<TAP::Parser::Iterator::Stream> constructor. This is used
internally and you should not use it.

Otherwise, the parser does a C<-e> check to see if the source exists.  If so,
it attempts to execute the source and read the output as a stream.  This is by
far the preferred method of using the parser.

 foreach my $file ( @test_files ) {
     my $parser = TAP::Parser->new( { source => $file } );
     # do stuff with the parser
 }

=item * C<tap>

The value should be the complete TAP output.

=item * C<exec>

If passed an array reference, will attempt to create the iterator by
passing a L<TAP::Parser::Source> object to
L<TAP::Parser::Iterator::Source>, using the array reference strings as
the command arguments to L<IPC::Open3::open3|IPC::Open3>:

 exec => [ '/usr/bin/ruby', 't/my_test.rb' ]

Note that C<source> and C<exec> are mutually exclusive.

=back

The following keys are optional.

=over 4

=item * C<callback>

If present, each callback corresponding to a given result type will be called
with the result as the argument if the C<run> method is used:

 my %callbacks = (
     test    => \&test_callback,
     plan    => \&plan_callback,
     comment => \&comment_callback,
     bailout => \&bailout_callback,
     unknown => \&unknown_callback,
 );

 my $aggregator = TAP::Parser::Aggregator->new;
 foreach my $file ( @test_files ) {
     my $parser = TAP::Parser->new(
         {
             source    => $file,
             callbacks => \%callbacks,
         }
     );
     $parser->run;
     $aggregator->add( $file, $parser );
 }

=item * C<switches>

If using a Perl file as a source, optional switches may be passed which will
be used when invoking the perl executable.

 my $parser = TAP::Parser->new( {
     source   => $test_file,
     switches => '-Ilib',
 } );

=item * C<test_args>

Used in conjunction with the C<source> option to supply a reference to
an C<@ARGV> style array of arguments to pass to the test program.

=item * C<spool>

If passed a filehandle will write a copy of all parsed TAP to that handle.

=item * C<merge>

If false, STDERR is not captured (though it is 'relayed' to keep it
somewhat synchronized with STDOUT.)

If true, STDERR and STDOUT are the same filehandle.  This may cause
breakage if STDERR contains anything resembling TAP format, but does
allow exact synchronization.

Subtleties of this behavior may be platform-dependent and may change in
the future.

=back

=cut

# new implementation supplied by TAP::Base

##############################################################################

=head2 Instance Methods

=head3 C<next>

  my $parser = TAP::Parser->new( { source => $file } );
  while ( my $result = $parser->next ) {
      print $result->as_string, "\n";
  }

This method returns the results of the parsing, one result at a time.  Note
that it is destructive.  You can't rewind and examine previous results.

If callbacks are used, they will be issued before this call returns.

Each result returned is a subclass of L<TAP::Parser::Result>.  See that
module and related classes for more information on how to use them.

=cut

sub next {
    my $self = shift;
    return ( $self->{_iter} ||= $self->_iter )->();
}

##############################################################################

=head3 C<run>

  $parser->run;

This method merely runs the parser and parses all of the TAP.

=cut

sub run {
    my $self = shift;
    while ( defined( my $result = $self->next ) ) {

        # do nothing
    }
}

{

    # of the following, anything beginning with an underscore is strictly
    # internal and should not be exposed.
    my %initialize = (
        version       => $DEFAULT_TAP_VERSION,
        plan          => '',                    # the test plan (e.g., 1..3)
        tap           => '',                    # the TAP
        tests_run     => 0,                     # actual current test numbers
        results       => [],                    # TAP parser results
        skipped       => [],                    #
        todo          => [],                    #
        passed        => [],                    #
        failed        => [],                    #
        actual_failed => [],                    # how many tests really failed
        actual_passed => [],                    # how many tests really passed
        todo_passed  => [],    # tests which unexpectedly succeed
        parse_errors => [],    # perfect TAP should have none
    );

    # We seem to have this list hanging around all over the place. We could
    # probably get it from somewhere else to avoid the repetition.
    my @legal_callback = qw(
      test
      version
      plan
      comment
      bailout
      unknown
      yaml
      ALL
      ELSE
      EOF
    );

    sub _initialize {
        my ( $self, $arg_for ) = @_;

        # everything here is basically designed to convert any TAP source to a
        # stream.

        # Shallow copy
        my %args = %{ $arg_for || {} };

        $self->SUPER::_initialize( \%args, \@legal_callback );

        my $stream    = delete $args{stream};
        my $tap       = delete $args{tap};
        my $source    = delete $args{source};
        my $exec      = delete $args{exec};
        my $merge     = delete $args{merge};
        my $spool     = delete $args{spool};
        my $switches  = delete $args{switches};
        my @test_args = @{ delete $args{test_args} || [] };

        if ( 1 < grep {defined} $stream, $tap, $source, $exec ) {
            $self->_croak(
                "You may only choose one of 'exec', 'stream', 'tap' or 'source'"
            );
        }

        if ( my @excess = sort keys %args ) {
            $self->_croak("Unknown options: @excess");
        }

        if ($tap) {
            $stream = TAP::Parser::Iterator->new( [ split "\n" => $tap ] );
        }
        elsif ($exec) {
            my $source = TAP::Parser::Source->new;
            $source->source( [ @$exec, @test_args ] );
            $source->merge($merge);    # XXX should just be arguments?
            $stream = $source->get_stream;
        }
        elsif ($source) {
            if ( my $ref = ref $source ) {
                $stream = TAP::Parser::Iterator->new($source);
            }
            elsif ( -e $source ) {

                my $perl = TAP::Parser::Source::Perl->new;

                $perl->switches($switches)
                  if $switches;

                $perl->merge($merge);    # XXX args to new()?

                $perl->source( [ $source, @test_args ] );

                $stream = $perl->get_stream;
            }
            else {
                $self->_croak("Cannot determine source for $source");
            }
        }

        unless ($stream) {
            $self->_croak('PANIC: could not determine stream');
        }

        while ( my ( $k, $v ) = each %initialize ) {
            $self->{$k} = 'ARRAY' eq ref $v ? [] : $v;
        }

        $self->_stream($stream);
        my $grammar = TAP::Parser::Grammar->new($stream);
        $grammar->set_version( $self->version );
        $self->_grammar($grammar);
        $self->_spool($spool);

        $self->start_time( $self->get_time );

        return $self;
    }
}

=head1 INDIVIDUAL RESULTS

If you've read this far in the docs, you've seen this:

    while ( my $result = $parser->next ) {
        print $result->as_string;
    }

Each result returned is a L<TAP::Parser::Result> subclass, referred to as
I<result types>.

=head2 Result types

Basically, you fetch individual results from the TAP.  The six types, with
examples of each, are as follows:

=over 4

=item * Version

 TAP version 12

=item * Plan

 1..42

=item * Pragma

 pragma +strict

=item * Test

 ok 3 - We should start with some foobar!

=item * Comment

 # Hope we don't use up the foobar.

=item * Bailout

 Bail out!  We ran out of foobar!

=item * Unknown

 ... yo, this ain't TAP! ...

=back

Each result fetched is a result object of a different type.  There are common
methods to each result object and different types may have methods unique to
their type.  Sometimes a type method may be overridden in a subclass, but its
use is guaranteed to be identical.

=head2 Common type methods

=head3 C<type>

Returns the type of result, such as C<comment> or C<test>.

=head3 C<as_string>

Prints a string representation of the token.  This might not be the exact
output, however.  Tests will have test numbers added if not present, TODO and
SKIP directives will be capitalized and, in general, things will be cleaned
up.  If you need the original text for the token, see the C<raw> method.

=head3  C<raw>

Returns the original line of text which was parsed.

=head3 C<is_plan>

Indicates whether or not this is the test plan line.

=head3 C<is_test>

Indicates whether or not this is a test line.

=head3 C<is_comment>

Indicates whether or not this is a comment. Comments will generally only
appear in the TAP stream if STDERR is merged to STDOUT. See the
C<merge> option.

=head3 C<is_bailout>

Indicates whether or not this is bailout line.

=head3 C<is_yaml>

Indicates whether or not the current item is a YAML block.

=head3 C<is_unknown>

Indicates whether or not the current line could be parsed.

=head3 C<is_ok>

  if ( $result->is_ok ) { ... }

Reports whether or not a given result has passed.  Anything which is B<not> a
test result returns true.  This is merely provided as a convenient shortcut
which allows you to do this:

 my $parser = TAP::Parser->new( { source => $source } );
 while ( my $result = $parser->next ) {
     # only print failing results
     print $result->as_string unless $result->is_ok;
 }

=head2 C<plan> methods

 if ( $result->is_plan ) { ... }

If the above evaluates as true, the following methods will be available on the
C<$result> object.

=head3 C<plan>

  if ( $result->is_plan ) {
     print $result->plan;
  }

This is merely a synonym for C<as_string>.

=head3 C<directive>

 my $directive = $result->directive;

If a SKIP directive is included with the plan, this method will return it.

 1..0 # SKIP: why bother?

=head3 C<explanation>

 my $explanation = $result->explanation;

If a SKIP directive was included with the plan, this method will return the
explanation, if any.

=head2 C<pragma> methods

 if ( $result->is_pragma ) { ... }

If the above evaluates as true, the following methods will be available on the
C<$result> object.

=head3 C<pragmas>

Returns a list of pragmas each of which is a + or - followed by the
pragma name.
 
=head2 C<commment> methods

 if ( $result->is_comment ) { ... }

If the above evaluates as true, the following methods will be available on the
C<$result> object.

=head3 C<comment>

  if ( $result->is_comment ) {
      my $comment = $result->comment;
      print "I have something to say:  $comment";
  }

=head2 C<bailout> methods

 if ( $result->is_bailout ) { ... }

If the above evaluates as true, the following methods will be available on the
C<$result> object.

=head3 C<explanation>

  if ( $result->is_bailout ) {
      my $explanation = $result->explanation;
      print "We bailed out because ($explanation)";
  }

If, and only if, a token is a bailout token, you can get an "explanation" via
this method.  The explanation is the text after the mystical "Bail out!" words
which appear in the tap output.

=head2 C<unknown> methods

 if ( $result->is_unknown ) { ... }

There are no unique methods for unknown results.

=head2 C<test> methods

 if ( $result->is_test ) { ... }

If the above evaluates as true, the following methods will be available on the
C<$result> object.

=head3 C<ok>

  my $ok = $result->ok;

Returns the literal text of the C<ok> or C<not ok> status.

=head3 C<number>

  my $test_number = $result->number;

Returns the number of the test, even if the original TAP output did not supply
that number.

=head3 C<description>

  my $description = $result->description;

Returns the description of the test, if any.  This is the portion after the
test number but before the directive.

=head3 C<directive>

  my $directive = $result->directive;

Returns either C<TODO> or C<SKIP> if either directive was present for a test
line.

=head3 C<explanation>

  my $explanation = $result->explanation;

If a test had either a C<TODO> or C<SKIP> directive, this method will return
the accompanying explantion, if present.

  not ok 17 - 'Pigs can fly' # TODO not enough acid

For the above line, the explanation is I<not enough acid>.

=head3 C<is_ok>

  if ( $result->is_ok ) { ... }

Returns a boolean value indicating whether or not the test passed.  Remember
that for TODO tests, the test always passes.

B<Note:>  this was formerly C<passed>.  The latter method is deprecated and
will issue a warning.

=head3 C<is_actual_ok>

  if ( $result->is_actual_ok ) { ... }

Returns a boolean value indicating whether or not the test passed, regardless
of its TODO status.

B<Note:>  this was formerly C<actual_passed>.  The latter method is deprecated
and will issue a warning.

=head3 C<is_unplanned>

  if ( $test->is_unplanned ) { ... }

If a test number is greater than the number of planned tests, this method will
return true.  Unplanned tests will I<always> return false for C<is_ok>,
regardless of whether or not the test C<has_todo> (see
L<TAP::Parser::Result::Test> for more information about this).

=head3 C<has_skip>

  if ( $result->has_skip ) { ... }

Returns a boolean value indicating whether or not this test had a SKIP
directive.

=head3 C<has_todo>

  if ( $result->has_todo ) { ... }

Returns a boolean value indicating whether or not this test had a TODO
directive.

Note that TODO tests I<always> pass.  If you need to know whether or not
they really passed, check the C<is_actual_ok> method.

=head3 C<in_todo>

  if ( $parser->in_todo ) { ... }

True while the most recent result was a TODO. Becomes true before the
TODO result is returned and stays true until just before the next non-
TODO test is returned.

=head1 TOTAL RESULTS

After parsing the TAP, there are many methods available to let you dig through
the results and determine what is meaningful to you.

=head2 Individual Results

These results refer to individual tests which are run.

=head3 C<passed>

 my @passed = $parser->passed; # the test numbers which passed
 my $passed = $parser->passed; # the number of tests which passed

This method lets you know which (or how many) tests passed.  If a test failed
but had a TODO directive, it will be counted as a passed test.

=cut

sub passed { @{ shift->{passed} } }

=head3 C<failed>

 my @failed = $parser->failed; # the test numbers which failed
 my $failed = $parser->failed; # the number of tests which failed

This method lets you know which (or how many) tests failed.  If a test passed
but had a TODO directive, it will B<NOT> be counted as a failed test.

=cut

sub failed { @{ shift->{failed} } }

=head3 C<actual_passed>

 # the test numbers which actually passed
 my @actual_passed = $parser->actual_passed;

 # the number of tests which actually passed
 my $actual_passed = $parser->actual_passed;

This method lets you know which (or how many) tests actually passed,
regardless of whether or not a TODO directive was found.

=cut

sub actual_passed { @{ shift->{actual_passed} } }
*actual_ok = \&actual_passed;

=head3 C<actual_ok>

This method is a synonym for C<actual_passed>.

=head3 C<actual_failed>

 # the test numbers which actually failed
 my @actual_failed = $parser->actual_failed;

 # the number of tests which actually failed
 my $actual_failed = $parser->actual_failed;

This method lets you know which (or how many) tests actually failed,
regardless of whether or not a TODO directive was found.

=cut

sub actual_failed { @{ shift->{actual_failed} } }

##############################################################################

=head3 C<todo>

 my @todo = $parser->todo; # the test numbers with todo directives
 my $todo = $parser->todo; # the number of tests with todo directives

This method lets you know which (or how many) tests had TODO directives.

=cut

sub todo { @{ shift->{todo} } }

=head3 C<todo_passed>

 # the test numbers which unexpectedly succeeded
 my @todo_passed = $parser->todo_passed;

 # the number of tests which unexpectedly succeeded
 my $todo_passed = $parser->todo_passed;

This method lets you know which (or how many) tests actually passed but were
declared as "TODO" tests.

=cut

sub todo_passed { @{ shift->{todo_passed} } }

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

=head3 C<skipped>

 my @skipped = $parser->skipped; # the test numbers with SKIP directives
 my $skipped = $parser->skipped; # the number of tests with SKIP directives

This method lets you know which (or how many) tests had SKIP directives.

=cut

sub skipped { @{ shift->{skipped} } }

=head2 Pragmas

=head3 C<pragma>

Get or set a pragma. To get the state of a pragma:

  if ( $p->pragma('strict') ) {
      # be strict
  }

To set the state of a pragma:

  $p->pragma('strict', 1); # enable strict mode

=cut

sub pragma {
    my ( $self, $pragma ) = splice @_, 0, 2;

    return $self->{pragma}->{$pragma} unless @_;

    if ( my $state = shift ) {
        $self->{pragma}->{$pragma} = 1;
    }
    else {
        delete $self->{pragma}->{$pragma};
    }

    return;
}

=head3 C<pragmas>

Get a list of all the currently enabled pragmas:

  my @pragmas_enabled = $p->pragmas;

=cut

sub pragmas { sort keys %{ shift->{pragma} || {} } }

=head2 Summary Results

These results are "meta" information about the total results of an individual
test program.

=head3 C<plan>

 my $plan = $parser->plan;

Returns the test plan, if found.

=head3 C<good_plan>

Deprecated.  Use C<is_good_plan> instead.

=cut

sub good_plan {
    warn 'good_plan() is deprecated.  Please use "is_good_plan()"';
    goto &is_good_plan;
}

##############################################################################

=head3 C<is_good_plan>

  if ( $parser->is_good_plan ) { ... }

Returns a boolean value indicating whether or not the number of tests planned
matches the number of tests run.

B<Note:>  this was formerly C<good_plan>.  The latter method is deprecated and
will issue a warning.

And since we're on that subject ...

=head3 C<tests_planned>

  print $parser->tests_planned;

Returns the number of tests planned, according to the plan.  For example, a
plan of '1..17' will mean that 17 tests were planned.

=head3 C<tests_run>

  print $parser->tests_run;

Returns the number of tests which actually were run.  Hopefully this will
match the number of C<< $parser->tests_planned >>.

=head3 C<skip_all>

Returns a true value (actually the reason for skipping) if all tests
were skipped.

=head3 C<start_time>

Returns the time when the Parser was created.

=head3 C<end_time>

Returns the time when the end of TAP input was seen.

=head3 C<has_problems>

  if ( $parser->has_problems ) {
      ...
  }

This is a 'catch-all' method which returns true if any tests have currently
failed, any TODO tests unexpectedly succeeded, or any parse errors occurred.

=cut

sub has_problems {
    my $self = shift;
    return
         $self->failed
      || $self->parse_errors
      || $self->wait
      || $self->exit;
}

=head3 C<version>

  $parser->version;

Once the parser is done, this will return the version number for the
parsed TAP. Version numbers were introduced with TAP version 13 so if no
version number is found version 12 is assumed.

=head3 C<exit>

  $parser->exit;

Once the parser is done, this will return the exit status.  If the parser ran
an executable, it returns the exit status of the executable.

=head3 C<wait>

  $parser->wait;

Once the parser is done, this will return the wait status.  If the parser ran
an executable, it returns the wait status of the executable.  Otherwise, this
mererely returns the C<exit> status.

=head3 C<parse_errors>

 my @errors = $parser->parse_errors; # the parser errors
 my $errors = $parser->parse_errors; # the number of parser_errors

Fortunately, all TAP output is perfect.  In the event that it is not, this
method will return parser errors.  Note that a junk line which the parser does
not recognize is C<not> an error.  This allows this parser to handle future
versions of TAP.  The following are all TAP errors reported by the parser:

=over 4

=item * Misplaced plan

The plan (for example, '1..5'), must only come at the beginning or end of the
TAP output.

=item * No plan

Gotta have a plan!

=item * More than one plan

 1..3
 ok 1 - input file opened
 not ok 2 - first line of the input valid # todo some data
 ok 3 read the rest of the file
 1..3

Right.  Very funny.  Don't do that.

=item * Test numbers out of sequence

 1..3
 ok 1 - input file opened
 not ok 2 - first line of the input valid # todo some data
 ok 2 read the rest of the file

That last test line above should have the number '3' instead of '2'.

Note that it's perfectly acceptable for some lines to have test numbers and
others to not have them.  However, when a test number is found, it must be in
sequence.  The following is also an error:

 1..3
 ok 1 - input file opened
 not ok - first line of the input valid # todo some data
 ok 2 read the rest of the file

But this is not:

 1..3
 ok  - input file opened
 not ok - first line of the input valid # todo some data
 ok 3 read the rest of the file

=back

=cut

sub parse_errors { @{ shift->{parse_errors} } }

sub _add_error {
    my ( $self, $error ) = @_;
    push @{ $self->{parse_errors} } => $error;
    return $self;
}

sub _make_state_table {
    my $self = shift;
    my %states;
    my %planned_todo = ();

    # These transitions are defaults for all states
    my %state_globals = (
        comment => {},
        bailout => {},
        yaml    => {},
        version => {
            act => sub {
                $self->_add_error(
                    'If TAP version is present it must be the first line of output'
                );
            },
        },
        unknown => {
            act => sub {
                my $unk = shift;
                if ( $self->pragma('strict') ) {
                    $self->_add_error(
                        'Unknown TAP token: "' . $unk->raw . '"' );
                }
            },
        },
        pragma => {
            act => sub {
                my ($pragma) = @_;
                for my $pr ( $pragma->pragmas ) {
                    if ( $pr =~ /^ ([-+])(\w+) $/x ) {
                        $self->pragma( $2, $1 eq '+' );
                    }
                }
            },
        },
    );

    # Provides default elements for transitions
    my %state_defaults = (
        plan => {
            act => sub {
                my ($plan) = @_;
                $self->tests_planned( $plan->tests_planned );
                $self->plan( $plan->plan );
                if ( $plan->has_skip ) {
                    $self->skip_all( $plan->explanation
                          || '(no reason given)' );
                }

                $planned_todo{$_}++ for @{ $plan->todo_list };
            },
        },
        test => {
            act => sub {
                my ($test) = @_;

                my ( $number, $tests_run )
                  = ( $test->number, ++$self->{tests_run} );

                # Fake TODO state
                if ( defined $number && delete $planned_todo{$number} ) {
                    $test->set_directive('TODO');
                }

                my $has_todo = $test->has_todo;

                $self->in_todo($has_todo);
                if ( defined( my $tests_planned = $self->tests_planned ) ) {
                    if ( $tests_run > $tests_planned ) {
                        $test->is_unplanned(1);
                    }
                }

                if ($number) {
                    if ( $number != $tests_run ) {
                        my $count = $tests_run;
                        $self->_add_error( "Tests out of sequence.  Found "
                              . "($number) but expected ($count)" );
                    }
                }
                else {
                    $test->_number( $number = $tests_run );
                }

                push @{ $self->{todo} } => $number if $has_todo;
                push @{ $self->{todo_passed} } => $number
                  if $test->todo_passed;
                push @{ $self->{skipped} } => $number
                  if $test->has_skip;

                push @{ $self->{ $test->is_ok ? 'passed' : 'failed' } } =>
                  $number;
                push @{
                    $self->{
                        $test->is_actual_ok
                        ? 'actual_passed'
                        : 'actual_failed'
                      }
                  } => $number;
            },
        },
        yaml => { act => sub { }, },
    );

    # Each state contains a hash the keys of which match a token type. For
    # each token
    # type there may be:
    #   act      A coderef to run
    #   goto     The new state to move to. Stay in this state if
    #            missing
    #   continue Goto the new state and run the new state for the
    #            current token
    %states = (
        INIT => {
            version => {
                act => sub {
                    my ($version) = @_;
                    my $ver_num = $version->version;
                    if ( $ver_num <= $DEFAULT_TAP_VERSION ) {
                        my $ver_min = $DEFAULT_TAP_VERSION + 1;
                        $self->_add_error(
                                "Explicit TAP version must be at least "
                              . "$ver_min. Got version $ver_num" );
                        $ver_num = $DEFAULT_TAP_VERSION;
                    }
                    if ( $ver_num > $MAX_TAP_VERSION ) {
                        $self->_add_error(
                                "TAP specified version $ver_num but "
                              . "we don't know about versions later "
                              . "than $MAX_TAP_VERSION" );
                        $ver_num = $MAX_TAP_VERSION;
                    }
                    $self->version($ver_num);
                    $self->_grammar->set_version($ver_num);
                },
                goto => 'PLAN'
            },
            plan => { goto => 'PLANNED' },
            test => { goto => 'UNPLANNED' },
        },
        PLAN => {
            plan => { goto => 'PLANNED' },
            test => { goto => 'UNPLANNED' },
        },
        PLANNED => {
            test => { goto => 'PLANNED_AFTER_TEST' },
            plan => {
                act => sub {
                    my ($version) = @_;
                    $self->_add_error(
                        'More than one plan found in TAP output');
                },
            },
        },
        PLANNED_AFTER_TEST => {
            test => { goto => 'PLANNED_AFTER_TEST' },
            plan => { act  => sub { }, continue => 'PLANNED' },
            yaml => { goto => 'PLANNED' },
        },
        GOT_PLAN => {
            test => {
                act => sub {
                    my ($plan) = @_;
                    my $line = $self->plan;
                    $self->_add_error(
                            "Plan ($line) must be at the beginning "
                          . "or end of the TAP output" );
                    $self->is_good_plan(0);
                },
                continue => 'PLANNED'
            },
            plan => { continue => 'PLANNED' },
        },
        UNPLANNED => {
            test => { goto => 'UNPLANNED_AFTER_TEST' },
            plan => { goto => 'GOT_PLAN' },
        },
        UNPLANNED_AFTER_TEST => {
            test => { act  => sub { }, continue => 'UNPLANNED' },
            plan => { act  => sub { }, continue => 'UNPLANNED' },
            yaml => { goto => 'PLANNED' },
        },
    );

    # Apply globals and defaults to state table
    for my $name ( keys %states ) {

        # Merge with globals
        my $st = { %state_globals, %{ $states{$name} } };

        # Add defaults
        for my $next ( sort keys %{$st} ) {
            if ( my $default = $state_defaults{$next} ) {
                for my $def ( sort keys %{$default} ) {
                    $st->{$next}->{$def} ||= $default->{$def};
                }
            }
        }

        # Stuff back in table
        $states{$name} = $st;
    }

    return \%states;
}

=head3 C<get_select_handles>

Get an a list of file handles which can be passed to C<select> to
determine the readiness of this parser.

=cut

sub get_select_handles { shift->_stream->get_select_handles }

sub _iter {
    my $self        = shift;
    my $stream      = $self->_stream;
    my $spool       = $self->_spool;
    my $grammar     = $self->_grammar;
    my $state       = 'INIT';
    my $state_table = $self->_make_state_table;

    # Make next_state closure
    my $next_state = sub {
        my $token = shift;
        my $type  = $token->type;
        TRANS: {
            my $state_spec = $state_table->{$state}
              or die "Illegal state: $state";

            if ( my $next = $state_spec->{$type} ) {
                if ( my $act = $next->{act} ) {
                    $act->($token);
                }
                if ( my $cont = $next->{continue} ) {
                    $state = $cont;
                    redo TRANS;
                }
                elsif ( my $goto = $next->{goto} ) {
                    $state = $goto;
                }
            }
            else {
                confess("Unhandled token type: $type\n");
            }
        }
        return $token;
    };

    # Handle end of stream - which means either pop a block or finish
    my $end_handler = sub {
        $self->exit( $stream->exit );
        $self->wait( $stream->wait );
        $self->_finish;
        return;
    };

    # Finally make the closure that we return. For performance reasons
    # there are two versions of the returned function: one that handles
    # callbacks and one that does not.
    if ( $self->_has_callbacks ) {
        return sub {
            my $result = eval { $grammar->tokenize };
            $self->_add_error($@) if $@;

            if ( defined $result ) {
                $result = $next_state->($result);

                if ( my $code = $self->_callback_for( $result->type ) ) {
                    $_->($result) for @{$code};
                }
                else {
                    $self->_make_callback( 'ELSE', $result );
                }

                $self->_make_callback( 'ALL', $result );

                # Echo TAP to spool file
                print {$spool} $result->raw, "\n" if $spool;
            }
            else {
                $result = $end_handler->();
                $self->_make_callback( 'EOF', $result )
                  unless defined $result;
            }

            return $result;
        };
    }    # _has_callbacks
    else {
        return sub {
            my $result = eval { $grammar->tokenize };
            $self->_add_error($@) if $@;

            if ( defined $result ) {
                $result = $next_state->($result);

                # Echo TAP to spool file
                print {$spool} $result->raw, "\n" if $spool;
            }
            else {
                $result = $end_handler->();
            }

            return $result;
        };
    }    # no callbacks
}

sub _finish {
    my $self = shift;

    $self->end_time( $self->get_time );

    # sanity checks
    if ( !$self->plan ) {
        $self->_add_error('No plan found in TAP output');
    }
    else {
        $self->is_good_plan(1) unless defined $self->is_good_plan;
    }
    if ( $self->tests_run != ( $self->tests_planned || 0 ) ) {
        $self->is_good_plan(0);
        if ( defined( my $planned = $self->tests_planned ) ) {
            my $ran = $self->tests_run;
            $self->_add_error(
                "Bad plan.  You planned $planned tests but ran $ran.");
        }
    }
    if ( $self->tests_run != ( $self->passed + $self->failed ) ) {

        # this should never happen
        my $actual = $self->tests_run;
        my $passed = $self->passed;
        my $failed = $self->failed;
        $self->_croak( "Panic: planned test count ($actual) did not equal "
              . "sum of passed ($passed) and failed ($failed) tests!" );
    }

    $self->is_good_plan(0) unless defined $self->is_good_plan;
    return $self;
}

=head3 C<delete_spool>

Delete and return the spool.

  my $fh = $parser->delete_spool;

=cut

sub delete_spool {
    my $self = shift;

    return delete $self->{_spool};
}

##############################################################################

=head1 CALLBACKS

As mentioned earlier, a "callback" key may be added to the
C<TAP::Parser> constructor. If present, each callback corresponding to a
given result type will be called with the result as the argument if the
C<run> method is used. The callback is expected to be a subroutine
reference (or anonymous subroutine) which is invoked with the parser
result as its argument.

 my %callbacks = (
     test    => \&test_callback,
     plan    => \&plan_callback,
     comment => \&comment_callback,
     bailout => \&bailout_callback,
     unknown => \&unknown_callback,
 );

 my $aggregator = TAP::Parser::Aggregator->new;
 foreach my $file ( @test_files ) {
     my $parser = TAP::Parser->new(
         {
             source    => $file,
             callbacks => \%callbacks,
         }
     );
     $parser->run;
     $aggregator->add( $file, $parser );
 }

Callbacks may also be added like this:

 $parser->callback( test => \&test_callback );
 $parser->callback( plan => \&plan_callback );

The following keys allowed for callbacks. These keys are case-sensitive.

=over 4

=item * C<test>

Invoked if C<< $result->is_test >> returns true.

=item * C<version>

Invoked if C<< $result->is_version >> returns true.

=item * C<plan>

Invoked if C<< $result->is_plan >> returns true.

=item * C<comment>

Invoked if C<< $result->is_comment >> returns true.

=item * C<bailout>

Invoked if C<< $result->is_unknown >> returns true.

=item * C<yaml>

Invoked if C<< $result->is_yaml >> returns true.

=item * C<unknown>

Invoked if C<< $result->is_unknown >> returns true.

=item * C<ELSE>

If a result does not have a callback defined for it, this callback will
be invoked. Thus, if all of the previous result types are specified as
callbacks, this callback will I<never> be invoked.

=item * C<ALL>

This callback will always be invoked and this will happen for each
result after one of the above callbacks is invoked.  For example, if
L<Term::ANSIColor> is loaded, you could use the following to color your
test output:

 my %callbacks = (
     test => sub {
         my $test = shift;
         if ( $test->is_ok && not $test->directive ) {
             # normal passing test
             print color 'green';
         }
         elsif ( !$test->is_ok ) {    # even if it's TODO
             print color 'white on_red';
         }
         elsif ( $test->has_skip ) {
             print color 'white on_blue';

         }
         elsif ( $test->has_todo ) {
             print color 'white';
         }
     },
     ELSE => sub {
         # plan, comment, and so on (anything which isn't a test line)
         print color 'black on_white';
     },
     ALL => sub {
         # now print them
         print shift->as_string;
         print color 'reset';
         print "\n";
     },
 );

=item * C<EOF>

Invoked when there are no more lines to be parsed. Since there is no
accompanying L<TAP::Parser::Result> object the C<TAP::Parser> object is
passed instead.

=back

=head1 TAP GRAMMAR

If you're looking for an EBNF grammar, see L<TAP::Parser::Grammar>.

=head1 BACKWARDS COMPATABILITY

The Perl-QA list attempted to ensure backwards compatability with
L<Test::Harness>.  However, there are some minor differences.

=head2 Differences

=over 4

=item * TODO plans

A little-known feature of L<Test::Harness> is that it supported TODO
lists in the plan:

 1..2 todo 2
 ok 1 - We have liftoff
 not ok 2 - Anti-gravity device activated

Under L<Test::Harness>, test number 2 would I<pass> because it was
listed as a TODO test on the plan line. However, we are not aware of
anyone actually using this feature and hard-coding test numbers is
discouraged because it's very easy to add a test and break the test
number sequence. This makes test suites very fragile. Instead, the
following should be used:

 1..2
 ok 1 - We have liftoff
 not ok 2 - Anti-gravity device activated # TODO

=item * 'Missing' tests

It rarely happens, but sometimes a harness might encounter
'missing tests:

 ok 1
 ok 2
 ok 15
 ok 16
 ok 17

L<Test::Harness> would report tests 3-14 as having failed. For the
C<TAP::Parser>, these tests are not considered failed because they've
never run. They're reported as parse failures (tests out of sequence).

=back

=head1 ACKNOWLEDGEMENTS

All of the following have helped. Bug reports, patches, (im)moral
support, or just words of encouragement have all been forthcoming.

=over 4

=item * Michael Schwern

=item * Andy Lester

=item * chromatic

=item * GEOFFR

=item * Shlomi Fish

=item * Torsten Schoenfeld

=item * Jerry Gay

=item * Aristotle

=item * Adam Kennedy

=item * Yves Orton

=item * Adrian Howard

=item * Sean & Lil

=item * Andreas J. Koenig

=item * Florian Ragwitz

=item * Corion

=item * Mark Stosberg

=item * Matt Kraai

=back

=head1 AUTHORS

Curtis "Ovid" Poe <ovid@cpan.org>

Andy Armstong <andy@hexten.net>

Eric Wilhelm @ <ewilhelm at cpan dot org>

Michael Peters <mpeters at plusthree dot com>

Leif Eriksen <leif dot eriksen at bigpond dot com>

=head1 BUGS

Please report any bugs or feature requests to
C<bug-tapx-parser@rt.cpan.org>, or through the web interface at
L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=TAP-Parser>.
We will be notified, and then you'll automatically be notified of
progress on your bug as we make changes.

Obviously, bugs which include patches are best. If you prefer, you can
patch against bleed by via anonymous checkout of the latest version:

 svn checkout http://svn.hexten.net/tapx

=head1 COPYRIGHT & LICENSE

Copyright 2006-2008 Curtis "Ovid" Poe, all rights reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=cut

1;
