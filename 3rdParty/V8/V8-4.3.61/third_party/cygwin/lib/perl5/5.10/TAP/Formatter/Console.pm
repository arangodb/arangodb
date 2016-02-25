package TAP::Formatter::Console;

use strict;
use TAP::Base ();
use POSIX qw(strftime);

use vars qw($VERSION @ISA);

@ISA = qw(TAP::Base);

my $MAX_ERRORS = 5;
my %VALIDATION_FOR;

BEGIN {
    %VALIDATION_FOR = (
        directives => sub { shift; shift },
        verbosity  => sub { shift; shift },
        timer      => sub { shift; shift },
        failures   => sub { shift; shift },
        errors     => sub { shift; shift },
        color      => sub { shift; shift },
        jobs       => sub { shift; shift },
        stdout     => sub {
            my ( $self, $ref ) = @_;
            $self->_croak("option 'stdout' needs a filehandle")
              unless ( ref $ref || '' ) eq 'GLOB'
              or eval { $ref->can('print') };
            return $ref;
        },
    );

    my @getter_setters = qw(
      _longest
      _tests_without_extensions
      _printed_summary_header
      _colorizer
    );

    for my $method ( @getter_setters, keys %VALIDATION_FOR ) {
        no strict 'refs';
        *$method = sub {
            my $self = shift;
            return $self->{$method} unless @_;
            $self->{$method} = shift;
        };
    }
}

=head1 NAME

TAP::Formatter::Console - Harness output delegate for default console output

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

=head1 DESCRIPTION

This provides console orientated output formatting for TAP::Harness.

=head1 SYNOPSIS

 use TAP::Formatter::Console;
 my $harness = TAP::Formatter::Console->new( \%args );

=cut

sub _initialize {
    my ( $self, $arg_for ) = @_;
    $arg_for ||= {};

    $self->SUPER::_initialize($arg_for);
    my %arg_for = %$arg_for;    # force a shallow copy

    $self->verbosity(0);

    for my $name ( keys %VALIDATION_FOR ) {
        my $property = delete $arg_for{$name};
        if ( defined $property ) {
            my $validate = $VALIDATION_FOR{$name};
            $self->$name( $self->$validate($property) );
        }
    }

    if ( my @props = keys %arg_for ) {
        $self->_croak(
            "Unknown arguments to " . __PACKAGE__ . "::new (@props)" );
    }

    $self->stdout( \*STDOUT ) unless $self->stdout;

    if ( $self->color ) {
        require TAP::Formatter::Color;
        $self->_colorizer( TAP::Formatter::Color->new );
    }

    return $self;
}

sub verbose      { shift->verbosity >= 1 }
sub quiet        { shift->verbosity <= -1 }
sub really_quiet { shift->verbosity <= -2 }
sub silent       { shift->verbosity <= -3 }

=head1 METHODS

=head2 Class Methods

=head3 C<new>

 my %args = (
    verbose => 1,
 )
 my $harness = TAP::Formatter::Console->new( \%args );

The constructor returns a new C<TAP::Formatter::Console> object. If
a L<TAP::Harness> is created with no C<formatter> a
C<TAP::Formatter::Console> is automatically created. If any of the
following options were given to TAP::Harness->new they well be passed to
this constructor which accepts an optional hashref whose allowed keys are:

=over 4

=item * C<verbosity>

Set the verbosity level.

=item * C<verbose>

Printing individual test results to STDOUT.

=item * C<timer>

Append run time for each test to output. Uses L<Time::HiRes> if available.

=item * C<failures>

Only show test failures (this is a no-op if C<verbose> is selected).

=item * C<quiet>

Suppressing some test output (mostly failures while tests are running).

=item * C<really_quiet>

Suppressing everything but the tests summary.

=item * C<silent>

Suppressing all output.

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

=item * C<color>

If defined specifies whether color output is desired. If C<color> is not
defined it will default to color output if color support is available on
the current platform and output is not being redirected.

=item * C<jobs>

The number of concurrent jobs this formatter will handle.

=back

Any keys for which the value is C<undef> will be ignored.

=cut

# new supplied by TAP::Base

=head3 C<prepare>

Called by Test::Harness before any test output is generated. 

=cut

sub prepare {
    my ( $self, @tests ) = @_;

    my $longest = 0;

    my $tests_without_extensions = 0;
    foreach my $test (@tests) {
        $longest = length $test if length $test > $longest;
        if ( $test !~ /\.\w+$/ ) {

            # TODO: Coverage?
            $tests_without_extensions = 1;
        }
    }

    $self->_tests_without_extensions($tests_without_extensions);
    $self->_longest($longest);
}

sub _format_now { strftime "[%H:%M:%S]", localtime }

sub _format_name {
    my ( $self, $test ) = @_;
    my $name  = $test;
    my $extra = 0;
    unless ( $self->_tests_without_extensions ) {
        $name =~ s/(\.\w+)$//;    # strip the .t or .pm
        $extra = length $1;
    }
    my $periods = '.' x ( $self->_longest + $extra + 4 - length $test );

    if ( $self->timer ) {
        my $stamp = $self->_format_now();
        return "$stamp $name$periods";
    }
    else {
        return "$name$periods";
    }

}

=head3 C<open_test>

Called to create a new test session. A test session looks like this:

    my $session = $formatter->open_test( $test, $parser );
    while ( defined( my $result = $parser->next ) ) {
        $session->result($result);
        exit 1 if $result->is_bailout;
    }
    $session->close_test;

=cut

sub open_test {
    my ( $self, $test, $parser ) = @_;

    my $class
      = $self->jobs > 1
      ? 'TAP::Formatter::Console::ParallelSession'
      : 'TAP::Formatter::Console::Session';

    eval "require $class";
    $self->_croak($@) if $@;

    my $session = $class->new(
        {   name      => $test,
            formatter => $self,
            parser    => $parser
        }
    );

    $session->header;

    return $session;
}

=head3 C<summary>

  $harness->summary( $aggregate );

C<summary> prints the summary report after all tests are run.  The argument is
an aggregate.

=cut

sub summary {
    my ( $self, $aggregate ) = @_;

    return if $self->silent;

    my @t     = $aggregate->descriptions;
    my $tests = \@t;

    my $runtime = $aggregate->elapsed_timestr;

    my $total  = $aggregate->total;
    my $passed = $aggregate->passed;

    if ( $self->timer ) {
        $self->_output( $self->_format_now(), "\n" );
    }

    # TODO: Check this condition still works when all subtests pass but
    # the exit status is nonzero

    if ( $aggregate->all_passed ) {
        $self->_output("All tests successful.\n");
    }

    # ~TODO option where $aggregate->skipped generates reports
    if ( $total != $passed or $aggregate->has_problems ) {
        $self->_output("\nTest Summary Report");
        $self->_output("\n-------------------\n");
        foreach my $test (@$tests) {
            $self->_printed_summary_header(0);
            my ($parser) = $aggregate->parsers($test);
            $self->_output_summary_failure(
                'failed',
                [ '  Failed test:  ', '  Failed tests:  ' ],
                $test, $parser
            );
            $self->_output_summary_failure(
                'todo_passed',
                "  TODO passed:   ", $test, $parser
            );

            # ~TODO this cannot be the default
            #$self->_output_summary_failure( 'skipped', "  Tests skipped: " );

            if ( my $exit = $parser->exit ) {
                $self->_summary_test_header( $test, $parser );
                $self->_failure_output("  Non-zero exit status: $exit\n");
            }

            if ( my @errors = $parser->parse_errors ) {
                my $explain;
                if ( @errors > $MAX_ERRORS && !$self->errors ) {
                    $explain
                      = "Displayed the first $MAX_ERRORS of "
                      . scalar(@errors)
                      . " TAP syntax errors.\n"
                      . "Re-run prove with the -p option to see them all.\n";
                    splice @errors, $MAX_ERRORS;
                }
                $self->_summary_test_header( $test, $parser );
                $self->_failure_output(
                    sprintf "  Parse errors: %s\n",
                    shift @errors
                );
                foreach my $error (@errors) {
                    my $spaces = ' ' x 16;
                    $self->_failure_output("$spaces$error\n");
                }
                $self->_failure_output($explain) if $explain;
            }
        }
    }
    my $files = @$tests;
    $self->_output("Files=$files, Tests=$total, $runtime\n");
    my $status = $aggregate->get_status;
    $self->_output("Result: $status\n");
}

sub _output_summary_failure {
    my ( $self, $method, $name, $test, $parser ) = @_;

    # ugly hack.  Must rethink this :(
    my $output = $method eq 'failed' ? '_failure_output' : '_output';

    if ( my @r = $parser->$method() ) {
        $self->_summary_test_header( $test, $parser );
        my ( $singular, $plural )
          = 'ARRAY' eq ref $name ? @$name : ( $name, $name );
        $self->$output( @r == 1 ? $singular : $plural );
        my @results = $self->_balanced_range( 40, @r );
        $self->$output( sprintf "%s\n" => shift @results );
        my $spaces = ' ' x 16;
        while (@results) {
            $self->$output( sprintf "$spaces%s\n" => shift @results );
        }
    }
}

sub _summary_test_header {
    my ( $self, $test, $parser ) = @_;
    return if $self->_printed_summary_header;
    my $spaces = ' ' x ( $self->_longest - length $test );
    $spaces = ' ' unless $spaces;
    my $output = $self->_get_output_method($parser);
    $self->$output(
        sprintf "$test$spaces(Wstat: %d Tests: %d Failed: %d)\n",
        $parser->wait, $parser->tests_run, scalar $parser->failed
    );
    $self->_printed_summary_header(1);
}

sub _output {
    my $self = shift;

    print { $self->stdout } @_;
}

# Use _colorizer delegate to set output color. NOP if we have no delegate
sub _set_colors {
    my ( $self, @colors ) = @_;
    if ( my $colorizer = $self->_colorizer ) {
        my $output_func = $self->{_output_func} ||= sub {
            $self->_output(@_);
        };
        $colorizer->set_color( $output_func, $_ ) for @colors;
    }
}

sub _failure_output {
    my $self = shift;
    $self->_set_colors('red');
    my $out = join '', @_;
    my $has_newline = chomp $out;
    $self->_output($out);
    $self->_set_colors('reset');
    $self->_output($/)
      if $has_newline;
}

sub _balanced_range {
    my ( $self, $limit, @range ) = @_;
    @range = $self->_range(@range);
    my $line = "";
    my @lines;
    my $curr = 0;
    while (@range) {
        if ( $curr < $limit ) {
            my $range = ( shift @range ) . ", ";
            $line .= $range;
            $curr += length $range;
        }
        elsif (@range) {
            $line =~ s/, $//;
            push @lines => $line;
            $line = '';
            $curr = 0;
        }
    }
    if ($line) {
        $line =~ s/, $//;
        push @lines => $line;
    }
    return @lines;
}

sub _range {
    my ( $self, @numbers ) = @_;

    # shouldn't be needed, but subclasses might call this
    @numbers = sort { $a <=> $b } @numbers;
    my ( $min, @range );

    foreach my $i ( 0 .. $#numbers ) {
        my $num  = $numbers[$i];
        my $next = $numbers[ $i + 1 ];
        if ( defined $next && $next == $num + 1 ) {
            if ( !defined $min ) {
                $min = $num;
            }
        }
        elsif ( defined $min ) {
            push @range => "$min-$num";
            undef $min;
        }
        else {
            push @range => $num;
        }
    }
    return @range;
}

sub _get_output_method {
    my ( $self, $parser ) = @_;
    return $parser->has_problems ? '_failure_output' : '_output';
}

1;
