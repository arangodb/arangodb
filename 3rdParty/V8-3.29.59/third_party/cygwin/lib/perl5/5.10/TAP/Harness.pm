package TAP::Harness;

use strict;
use Carp;

use File::Spec;
use File::Path;
use IO::Handle;

use TAP::Base;
use TAP::Parser;
use TAP::Parser::Aggregator;
use TAP::Parser::Multiplexer;

use vars qw($VERSION @ISA);

@ISA = qw(TAP::Base);

=head1 NAME

TAP::Harness - Run test scripts with statistics

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

$ENV{HARNESS_ACTIVE}  = 1;
$ENV{HARNESS_VERSION} = $VERSION;

END {

    # For VMS.
    delete $ENV{HARNESS_ACTIVE};
    delete $ENV{HARNESS_VERSION};
}

=head1 DESCRIPTION

This is a simple test harness which allows tests to be run and results
automatically aggregated and output to STDOUT.

=head1 SYNOPSIS

 use TAP::Harness;
 my $harness = TAP::Harness->new( \%args );
 $harness->runtests(@tests);

=cut

my %VALIDATION_FOR;
my @FORMATTER_ARGS;

sub _error {
    my $self = shift;
    return $self->{error} unless @_;
    $self->{error} = shift;
}

BEGIN {

    @FORMATTER_ARGS = qw(
      directives verbosity timer failures errors stdout color
    );

    %VALIDATION_FOR = (
        lib => sub {
            my ( $self, $libs ) = @_;
            $libs = [$libs] unless 'ARRAY' eq ref $libs;

            return [ map {"-I$_"} @$libs ];
        },
        switches        => sub { shift; shift },
        exec            => sub { shift; shift },
        merge           => sub { shift; shift },
        formatter_class => sub { shift; shift },
        formatter       => sub { shift; shift },
        jobs            => sub { shift; shift },
        fork            => sub { shift; shift },
        test_args       => sub { shift; shift },
    );

    for my $method ( sort keys %VALIDATION_FOR ) {
        no strict 'refs';
        if ( $method eq 'lib' || $method eq 'switches' ) {
            *{$method} = sub {
                my $self = shift;
                unless (@_) {
                    $self->{$method} ||= [];
                    return wantarray
                      ? @{ $self->{$method} }
                      : $self->{$method};
                }
                $self->_croak("Too many arguments to method '$method'")
                  if @_ > 1;
                my $args = shift;
                $args = [$args] unless ref $args;
                $self->{$method} = $args;
                return $self;
            };
        }
        else {
            *{$method} = sub {
                my $self = shift;
                return $self->{$method} unless @_;
                $self->{$method} = shift;
            };
        }
    }

    for my $method (@FORMATTER_ARGS) {
        no strict 'refs';
        *{$method} = sub {
            my $self = shift;
            return $self->formatter->$method(@_);
        };
    }
}

##############################################################################

=head1 METHODS

=head2 Class Methods

=head3 C<new>

 my %args = (
    verbosity => 1,
    lib     => [ 'lib', 'blib/lib' ],
 )
 my $harness = TAP::Harness->new( \%args );

The constructor returns a new C<TAP::Harness> object.  It accepts an optional
hashref whose allowed keys are:

=over 4

=item * C<verbosity>

Set the verbosity level:

     1   verbose        Print individual test results to STDOUT.
     0   normal
    -1   quiet          Suppress some test output (mostly failures 
                        while tests are running).
    -2   really quiet   Suppress everything but the tests summary.

=item * C<timer>

Append run time for each test to output. Uses L<Time::HiRes> if available.

=item * C<failures>

Only show test failures (this is a no-op if C<verbose> is selected).

=item * C<lib>

Accepts a scalar value or array ref of scalar values indicating which paths to
allowed libraries should be included if Perl tests are executed.  Naturally,
this only makes sense in the context of tests written in Perl.

=item * C<switches>

Accepts a scalar value or array ref of scalar values indicating which switches
should be included if Perl tests are executed.  Naturally, this only makes
sense in the context of tests written in Perl.

=item * C<test_args>

A reference to an C<@INC> style array of arguments to be passed to each
test program.

=item * C<color>

Attempt to produce color output.

=item * C<exec>

Typically, Perl tests are run through this.  However, anything which spits out
TAP is fine.  You can use this argument to specify the name of the program
(and optional switches) to run your tests with:

  exec => ['/usr/bin/ruby', '-w']
  
=item * C<merge>

If C<merge> is true the harness will create parsers that merge STDOUT
and STDERR together for any processes they start.

=item * C<formatter_class>

The name of the class to use to format output. The default is
L<TAP::Formatter::Console>.

=item * C<formatter>

If set C<formatter> must be an object that is capable of formatting the
TAP output. See L<TAP::Formatter::Console> for an example.

=item * C<errors>

If parse errors are found in the TAP output, a note of this will be made
in the summary report.  To see all of the parse errors, set this argument to
true:

  errors => 1

=item * C<directives>

If set to a true value, only test results with directives will be displayed.
This overrides other settings such as C<verbose> or C<failures>.

=item * C<stdout>

A filehandle for catching standard output.

=back

Any keys for which the value is C<undef> will be ignored.

=cut

# new supplied by TAP::Base

{
    my @legal_callback = qw(
      parser_args
      made_parser
      before_runtests
      after_runtests
      after_test
    );

    sub _initialize {
        my ( $self, $arg_for ) = @_;
        $arg_for ||= {};

        $self->SUPER::_initialize( $arg_for, \@legal_callback );
        my %arg_for = %$arg_for;    # force a shallow copy

        for my $name ( sort keys %VALIDATION_FOR ) {
            my $property = delete $arg_for{$name};
            if ( defined $property ) {
                my $validate = $VALIDATION_FOR{$name};

                my $value = $self->$validate($property);
                if ( $self->_error ) {
                    $self->_croak;
                }
                $self->$name($value);
            }
        }

        $self->jobs(1) unless defined $self->jobs;

        unless ( $self->formatter ) {

            $self->formatter_class( my $class = $self->formatter_class
                  || 'TAP::Formatter::Console' );

            croak "Bad module name $class"
              unless $class =~ /^ \w+ (?: :: \w+ ) *$/x;

            eval "require $class";
            $self->_croak("Can't load $class") if $@;

            # This is a little bodge to preserve legacy behaviour. It's
            # pretty horrible that we know which args are destined for
            # the formatter.
            my %formatter_args = ( jobs => $self->jobs );
            for my $name (@FORMATTER_ARGS) {
                if ( defined( my $property = delete $arg_for{$name} ) ) {
                    $formatter_args{$name} = $property;
                }
            }

            $self->formatter( $class->new( \%formatter_args ) );
        }

        if ( my @props = sort keys %arg_for ) {
            $self->_croak("Unknown arguments to TAP::Harness::new (@props)");
        }

        return $self;
    }
}

##############################################################################

=head2 Instance Methods

=head3 C<runtests>

    $harness->runtests(@tests);

Accepts and array of C<@tests> to be run.  This should generally be the names
of test files, but this is not required.  Each element in C<@tests> will be
passed to C<TAP::Parser::new()> as a C<source>.  See L<TAP::Parser> for more
information.

It is possible to provide aliases that will be displayed in place of the
test name by supplying the test as a reference to an array containing
C<< [ $test, $alias ] >>:

    $harness->runtests( [ 't/foo.t', 'Foo Once' ],
                        [ 't/foo.t', 'Foo Twice' ] );

Normally it is an error to attempt to run the same test twice. Aliases
allow you to overcome this limitation by giving each run of the test a
unique name.

Tests will be run in the order found.

If the environment variable C<PERL_TEST_HARNESS_DUMP_TAP> is defined it
should name a directory into which a copy of the raw TAP for each test
will be written. TAP is written to files named for each test.
Subdirectories will be created as needed.

Returns a L<TAP::Parser::Aggregator> containing the test results.

=cut

sub runtests {
    my ( $self, @tests ) = @_;

    my $aggregate = TAP::Parser::Aggregator->new;

    $self->_make_callback( 'before_runtests', $aggregate );
    $aggregate->start;
    $self->aggregate_tests( $aggregate, @tests );
    $aggregate->stop;
    $self->formatter->summary($aggregate);
    $self->_make_callback( 'after_runtests', $aggregate );

    return $aggregate;
}

sub _after_test {
    my ( $self, $aggregate, $test, $parser ) = @_;

    $self->_make_callback( 'after_test', $test, $parser );
    $aggregate->add( $test->[1], $parser );
}

sub _aggregate_forked {
    my ( $self, $aggregate, @tests ) = @_;

    eval { require Parallel::Iterator };

    croak "Parallel::Iterator required for --fork option ($@)"
      if $@;

    my $iter = Parallel::Iterator::iterate(
        { workers => $self->jobs || 0 },
        sub {
            my ( $id, $test ) = @_;

            my ( $parser, $session ) = $self->make_parser($test);

            while ( defined( my $result = $parser->next ) ) {
                exit 1 if $result->is_bailout;
            }

            $self->finish_parser( $parser, $session );

            # Can't serialise coderefs...
            delete $parser->{_iter};
            delete $parser->{_stream};
            delete $parser->{_grammar};
            return $parser;
        },
        \@tests
    );

    while ( my ( $id, $parser ) = $iter->() ) {
        $self->_after_test( $aggregate, $tests[$id], $parser );
    }

    return;
}

sub _aggregate_parallel {
    my ( $self, $aggregate, @tests ) = @_;

    my $jobs = $self->jobs;
    my $mux  = TAP::Parser::Multiplexer->new;

    RESULT: {

        # Keep multiplexer topped up
        while ( @tests && $mux->parsers < $jobs ) {
            my $test = shift @tests;
            my ( $parser, $session ) = $self->make_parser($test);
            $mux->add( $parser, [ $session, $test ] );
        }

        if ( my ( $parser, $stash, $result ) = $mux->next ) {
            my ( $session, $test ) = @$stash;
            if ( defined $result ) {
                $session->result($result);
                exit 1 if $result->is_bailout;
            }
            else {

                # End of parser. Automatically removed from the mux.
                $self->finish_parser( $parser, $session );
                $self->_after_test( $aggregate, $test, $parser );
            }
            redo RESULT;
        }
    }

    return;
}

sub _aggregate_single {
    my ( $self, $aggregate, @tests ) = @_;

    for my $test (@tests) {
        my ( $parser, $session ) = $self->make_parser($test);

        while ( defined( my $result = $parser->next ) ) {
            $session->result($result);
            if ( $result->is_bailout ) {

                # Keep reading until input is exhausted in the hope
                # of allowing any pending diagnostics to show up.
                1 while $parser->next;
                exit 1;
            }
        }

        $self->finish_parser( $parser, $session );
        $self->_after_test( $aggregate, $test, $parser );
    }

    return;
}

=head3 C<aggregate_tests>

  $harness->aggregate_tests( $aggregate, @tests );

Run the named tests and display a summary of result. Tests will be run
in the order found. 

Test results will be added to the supplied L<TAP::Parser::Aggregator>.
C<aggregate_tests> may be called multiple times to run several sets of
tests. Multiple C<Test::Harness> instances may be used to pass results
to a single aggregator so that different parts of a complex test suite
may be run using different C<TAP::Harness> settings. This is useful, for
example, in the case where some tests should run in parallel but others
are unsuitable for parallel execution.

    my $formatter = TAP::Formatter::Console->new;
    my $ser_harness = TAP::Harness->new( { formatter => $formatter } );
    my $par_harness = TAP::Harness->new( { formatter => $formatter,
                                           jobs => 9 } );
    my $aggregator = TAP::Parser::Aggregator->new;
    
    $aggregator->start();
    $ser_harness->aggregate_tests( $aggregator, @ser_tests );
    $par_harness->aggregate_tests( $aggregator, @par_tests );
    $aggregator->stop();
    $formatter->summary( $aggregator );

Note that for simpler testing requirements it will often be possible to
replace the above code with a single call to C<runtests>.

Each elements of the @tests array is either

=over

=item * the file name of a test script to run

=item * a reference to a [ file name, display name ]

=back

When you supply a separate display name it becomes possible to run a
test more than once; the display name is effectively the alias by which
the test is known inside the harness. The harness doesn't care if it
runs the same script more than once when each invocation uses a
different name.

=cut

sub aggregate_tests {
    my ( $self, $aggregate, @tests ) = @_;

    my $jobs = $self->jobs;

    my @expanded = map { 'ARRAY' eq ref $_ ? $_ : [ $_, $_ ] } @tests;

    # #12458
    local $ENV{HARNESS_IS_VERBOSE} = 1
      if $self->formatter->verbosity > 0;

    # Formatter gets only names
    $self->formatter->prepare( map { $_->[1] } @expanded );

    if ( $self->jobs > 1 ) {
        if ( $self->fork ) {
            $self->_aggregate_forked( $aggregate, @expanded );
        }
        else {
            $self->_aggregate_parallel( $aggregate, @expanded );
        }
    }
    else {
        $self->_aggregate_single( $aggregate, @expanded );
    }

    return;
}

=head3 C<jobs>

Returns the number of concurrent test runs the harness is handling. For the default
harness this value is always 1. A parallel harness such as L<TAP::Harness::Parallel>
will override this to return the number of jobs it is handling.

=head3 C<fork>

If true the harness will attempt to fork and run the parser for each
test in a separate process. Currently this option requires
L<Parallel::Iterator> to be installed.

=cut

##############################################################################

=head1 SUBCLASSING

C<TAP::Harness> is designed to be (mostly) easy to subclass.  If you don't
like how a particular feature functions, just override the desired methods.

=head2 Methods

TODO: This is out of date

The following methods are ones you may wish to override if you want to
subclass C<TAP::Harness>.

=head3 C<summary>

  $harness->summary( \%args );

C<summary> prints the summary report after all tests are run.  The argument is
a hashref with the following keys:

=over 4

=item * C<start>

This is created with C<< Benchmark->new >> and it the time the tests started.
You can print a useful summary time, if desired, with:

  $self->output(timestr( timediff( Benchmark->new, $start_time ), 'nop' ));

=item * C<tests>

This is an array reference of all test names.  To get the L<TAP::Parser>
object for individual tests:

 my $aggregate = $args->{aggregate};
 my $tests     = $args->{tests};

 for my $name ( @$tests ) {
     my ($parser) = $aggregate->parsers($test);
     ... do something with $parser
 }

This is a bit clunky and will be cleaned up in a later release.

=back

=cut

sub _get_parser_args {
    my ( $self, $test ) = @_;
    my $test_prog = $test->[0];
    my %args      = ();
    my @switches;
    @switches = $self->lib if $self->lib;
    push @switches => $self->switches if $self->switches;
    $args{switches} = \@switches;
    $args{spool}    = $self->_open_spool($test_prog);
    $args{merge}    = $self->merge;
    $args{exec}     = $self->exec;

    if ( my $exec = $self->exec ) {
        $args{exec} = [ @$exec, $test_prog ];
    }
    else {
        $args{source} = $test_prog;
    }

    if ( defined( my $test_args = $self->test_args ) ) {
        $args{test_args} = $test_args;
    }

    return \%args;
}

=head3 C<make_parser>

Make a new parser and display formatter session. Typically used and/or
overridden in subclasses.

    my ( $parser, $session ) = $harness->make_parser;


=cut

sub make_parser {
    my ( $self, $test ) = @_;

    my $args = $self->_get_parser_args($test);
    $self->_make_callback( 'parser_args', $args, $test );
    my $parser = TAP::Parser->new($args);

    $self->_make_callback( 'made_parser', $parser, $test );
    my $session = $self->formatter->open_test( $test->[1], $parser );

    return ( $parser, $session );
}

=head3 C<finish_parser>

Terminate use of a parser. Typically used and/or overridden in
subclasses. The parser isn't destroyed as a result of this.

=cut

sub finish_parser {
    my ( $self, $parser, $session ) = @_;

    $session->close_test;
    $self->_close_spool($parser);

    return $parser;
}

sub _open_spool {
    my $self = shift;
    my $test = shift;

    if ( my $spool_dir = $ENV{PERL_TEST_HARNESS_DUMP_TAP} ) {

        my $spool = File::Spec->catfile( $spool_dir, $test );

        # Make the directory
        my ( $vol, $dir, undef ) = File::Spec->splitpath($spool);
        my $path = File::Spec->catpath( $vol, $dir, '' );
        eval { mkpath($path) };
        $self->_croak($@) if $@;

        my $spool_handle = IO::Handle->new;
        open( $spool_handle, ">$spool" )
          or $self->_croak(" Can't write $spool ( $! ) ");

        return $spool_handle;
    }

    return;
}

sub _close_spool {
    my $self = shift;
    my ($parser) = @_;

    if ( my $spool_handle = $parser->delete_spool ) {
        close($spool_handle)
          or $self->_croak(" Error closing TAP spool file( $! ) \n ");
    }

    return;
}

sub _croak {
    my ( $self, $message ) = @_;
    unless ($message) {
        $message = $self->_error;
    }
    $self->SUPER::_croak($message);

    return;
}

=head1 REPLACING

If you like the C<prove> utility and L<TAP::Parser> but you want your
own harness, all you need to do is write one and provide C<new> and
C<runtests> methods. Then you can use the C<prove> utility like so:

 prove --harness My::Test::Harness

Note that while C<prove> accepts a list of tests (or things to be
tested), C<new> has a fairly rich set of arguments. You'll probably want
to read over this code carefully to see how all of them are being used.

=head1 SEE ALSO

L<Test::Harness>

=cut

1;

# vim:ts=4:sw=4:et:sta
