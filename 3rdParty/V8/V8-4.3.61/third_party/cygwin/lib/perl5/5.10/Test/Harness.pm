package Test::Harness;

require 5.00405;

use strict;

use constant IS_WIN32 => ( $^O =~ /^(MS)?Win32$/ );
use constant IS_VMS => ( $^O eq 'VMS' );

use TAP::Harness              ();
use TAP::Parser::Aggregator   ();
use TAP::Parser::Source::Perl ();

use TAP::Parser::Utils qw( split_shell );

use Config;
use Exporter;

# TODO: Emulate at least some of these
use vars qw(
  $VERSION
  @ISA @EXPORT @EXPORT_OK
  $Verbose $Switches $Debug
  $verbose $switches $debug
  $Columns
  $Color
  $Directives
  $Timer
  $Strap
  $has_time_hires
);

# $ML $Last_ML_Print

BEGIN {
    eval q{use Time::HiRes 'time'};
    $has_time_hires = !$@;
}

=head1 NAME

Test::Harness - Run Perl standard test scripts with statistics

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

# Backwards compatibility for exportable variable names.
*verbose  = *Verbose;
*switches = *Switches;
*debug    = *Debug;

$ENV{HARNESS_ACTIVE}  = 1;
$ENV{HARNESS_VERSION} = $VERSION;

END {

    # For VMS.
    delete $ENV{HARNESS_ACTIVE};
    delete $ENV{HARNESS_VERSION};
}

@ISA       = ('Exporter');
@EXPORT    = qw(&runtests);
@EXPORT_OK = qw(&execute_tests $verbose $switches);

$Verbose = $ENV{HARNESS_VERBOSE} || 0;
$Debug   = $ENV{HARNESS_DEBUG}   || 0;
$Switches = '-w';
$Columns = $ENV{HARNESS_COLUMNS} || $ENV{COLUMNS} || 80;
$Columns--;    # Some shells have trouble with a full line of text.
$Timer = $ENV{HARNESS_TIMER} || 0;
$Color = $ENV{HARNESS_COLOR} || 0;

=head1 SYNOPSIS

  use Test::Harness;

  runtests(@test_files);

=head1 DESCRIPTION

Although, for historical reasons, the L<Test::Harness> distribution
takes its name from this module it now exists only to provide
L<TAP::Harness> with an interface that is somewhat backwards compatible
with L<Test::Harness> 2.xx. If you're writing new code consider using
L<TAP::Harness> directly instead.

Emulation is provided for C<runtests> and C<execute_tests> but the
pluggable 'Straps' interface that previous versions of L<Test::Harness>
supported is not reproduced here. Straps is now available as a stand
alone module: L<Test::Harness::Straps>.

See L<TAP::Parser>, L<TAP::Harness> for the main documentation for this
distribution.

=head1 FUNCTIONS

The following functions are available.

=head2 runtests( @test_files )

This runs all the given I<@test_files> and divines whether they passed
or failed based on their output to STDOUT (details above).  It prints
out each individual test which failed along with a summary report and
a how long it all took.

It returns true if everything was ok.  Otherwise it will C<die()> with
one of the messages in the DIAGNOSTICS section.

=cut

sub _has_taint {
    my $test = shift;
    return TAP::Parser::Source::Perl->get_taint(
        TAP::Parser::Source::Perl->shebang($test) );
}

sub _aggregate {
    my ( $harness, $aggregate, @tests ) = @_;

    # Don't propagate to our children
    local $ENV{HARNESS_OPTIONS};

    if (IS_VMS) {

        # Jiggery pokery doesn't appear to work on VMS - so disable it
        # pending investigation.
        _aggregate_tests( $harness, $aggregate, @tests );
    }
    else {
        my $path_sep  = $Config{path_sep};
        my $path_pat  = qr{$path_sep};
        my @extra_inc = _filtered_inc();

        # Supply -I switches in taint mode
        $harness->callback(
            parser_args => sub {
                my ( $args, $test ) = @_;
                if ( _has_taint( $test->[0] ) ) {
                    push @{ $args->{switches} }, map {"-I$_"} _filtered_inc();
                }
            }
        );

        my $previous = $ENV{PERL5LIB};
        local $ENV{PERL5LIB};

        if ($previous) {
            push @extra_inc, split( $path_pat, $previous );
        }

        if (@extra_inc) {
            $ENV{PERL5LIB} = join( $path_sep, @extra_inc );
        }

        _aggregate_tests( $harness, $aggregate, @tests );
    }
}

sub _aggregate_tests {
    my ( $harness, $aggregate, @tests ) = @_;
    $aggregate->start();
    $harness->aggregate_tests( $aggregate, @tests );
    $aggregate->stop();

}

sub runtests {
    my @tests = @_;

    # shield against -l
    local ( $\, $, );

    my $harness   = _new_harness();
    my $aggregate = TAP::Parser::Aggregator->new();

    _aggregate( $harness, $aggregate, @tests );

    $harness->formatter->summary($aggregate);

    my $total  = $aggregate->total;
    my $passed = $aggregate->passed;
    my $failed = $aggregate->failed;

    my @parsers = $aggregate->parsers;

    my $num_bad = 0;
    for my $parser (@parsers) {
        $num_bad++ if $parser->has_problems;
    }

    die(sprintf(
            "Failed %d/%d test programs. %d/%d subtests failed.\n",
            $num_bad, scalar @parsers, $failed, $total
        )
    ) if $num_bad;

    return $total && $total == $passed;
}

sub _canon {
    my @list   = sort { $a <=> $b } @_;
    my @ranges = ();
    my $count  = scalar @list;
    my $pos    = 0;

    while ( $pos < $count ) {
        my $end = $pos + 1;
        $end++ while $end < $count && $list[$end] <= $list[ $end - 1 ] + 1;
        push @ranges, ( $end == $pos + 1 )
          ? $list[$pos]
          : join( '-', $list[$pos], $list[ $end - 1 ] );
        $pos = $end;
    }

    return join( ' ', @ranges );
}

sub _new_harness {
    my $sub_args = shift || {};

    my ( @lib, @switches );
    for my $opt (
        split_shell( $Switches, $ENV{HARNESS_PERL_SWITCHES} ) )
    {
        if ( $opt =~ /^ -I (.*) $ /x ) {
            push @lib, $1;
        }
        else {
            push @switches, $opt;
        }
    }

    # Do things the old way on VMS...
    push @lib, _filtered_inc() if IS_VMS;

    # If $Verbose isn't numeric default to 1. This helps core.
    my $verbosity = ( $Verbose ? ( $Verbose !~ /\d/ ) ? 1 : $Verbose : 0 );

    my $args = {
        timer      => $Timer,
        directives => $Directives,
        lib        => \@lib,
        switches   => \@switches,
        color      => $Color,
        verbosity  => $verbosity,
    };

    $args->{stdout} = $sub_args->{out}
      if exists $sub_args->{out};

    if ( defined( my $env_opt = $ENV{HARNESS_OPTIONS} ) ) {
        for my $opt ( split /:/, $env_opt ) {
            if ( $opt =~ /^j(\d*)$/ ) {
                $args->{jobs} = $1 || 9;
            }
            elsif ( $opt eq 'f' ) {
                $args->{fork} = 1;
            }
            elsif ( $opt eq 'c' ) {
                $args->{color} = 1;
            }
            else {
                die "Unknown HARNESS_OPTIONS item: $opt\n";
            }
        }
    }

    return TAP::Harness->new($args);
}

# Get the parts of @INC which are changed from the stock list AND
# preserve reordering of stock directories.
sub _filtered_inc {
    my @inc = grep { !ref } @INC;    #28567

    if (IS_VMS) {

        # VMS has a 255-byte limit on the length of %ENV entries, so
        # toss the ones that involve perl_root, the install location
        @inc = grep !/perl_root/i, @inc;

    }
    elsif (IS_WIN32) {

        # Lose any trailing backslashes in the Win32 paths
        s/[\\\/+]$// foreach @inc;
    }

    my @default_inc = _default_inc();

    my @new_inc;
    my %seen;
    for my $dir (@inc) {
        next if $seen{$dir}++;

        if ( $dir eq ( $default_inc[0] || '' ) ) {
            shift @default_inc;
        }
        else {
            push @new_inc, $dir;
        }

        shift @default_inc while @default_inc and $seen{ $default_inc[0] };
    }

    return @new_inc;
}

{

    # Cache this to avoid repeatedly shelling out to Perl.
    my @inc;

    sub _default_inc {
        return @inc if @inc;
        my $perl = $ENV{HARNESS_PERL} || $^X;
        chomp( @inc = `$perl -le "print join qq[\\n], \@INC"` );
        return @inc;
    }
}

sub _check_sequence {
    my @list = @_;
    my $prev;
    while ( my $next = shift @list ) {
        return if defined $prev && $next <= $prev;
        $prev = $next;
    }

    return 1;
}

sub execute_tests {
    my %args = @_;

    my $harness   = _new_harness( \%args );
    my $aggregate = TAP::Parser::Aggregator->new();

    my %tot = (
        bonus       => 0,
        max         => 0,
        ok          => 0,
        bad         => 0,
        good        => 0,
        files       => 0,
        tests       => 0,
        sub_skipped => 0,
        todo        => 0,
        skipped     => 0,
        bench       => undef,
    );

    # Install a callback so we get to see any plans the
    # harness executes.
    $harness->callback(
        made_parser => sub {
            my $parser = shift;
            $parser->callback(
                plan => sub {
                    my $plan = shift;
                    if ( $plan->directive eq 'SKIP' ) {
                        $tot{skipped}++;
                    }
                }
            );
        }
    );

    _aggregate( $harness, $aggregate, @{ $args{tests} } );

    $tot{bench} = $aggregate->elapsed;
    my @tests = $aggregate->descriptions;

    # TODO: Work out the circumstances under which the files
    # and tests totals can differ.
    $tot{files} = $tot{tests} = scalar @tests;

    my %failedtests = ();
    my %todo_passed = ();

    for my $test (@tests) {
        my ($parser) = $aggregate->parsers($test);

        my @failed = $parser->failed;

        my $wstat         = $parser->wait;
        my $estat         = $parser->exit;
        my $planned       = $parser->tests_planned;
        my @errors        = $parser->parse_errors;
        my $passed        = $parser->passed;
        my $actual_passed = $parser->actual_passed;

        my $ok_seq = _check_sequence( $parser->actual_passed );

        # Duplicate exit, wait status semantics of old version
        $estat ||= '' unless $wstat;
        $wstat ||= '';

        $tot{max} += ( $planned || 0 );
        $tot{bonus} += $parser->todo_passed;
        $tot{ok} += $passed > $actual_passed ? $passed : $actual_passed;
        $tot{sub_skipped} += $parser->skipped;
        $tot{todo}        += $parser->todo;

        if ( @failed || $estat || @errors ) {
            $tot{bad}++;

            my $huh_planned = $planned ? undef : '??';
            my $huh_errors  = $ok_seq  ? undef : '??';

            $failedtests{$test} = {
                'canon' => $huh_planned
                  || $huh_errors
                  || _canon(@failed)
                  || '??',
                'estat'  => $estat,
                'failed' => $huh_planned
                  || $huh_errors
                  || scalar @failed,
                'max' => $huh_planned || $planned,
                'name'  => $test,
                'wstat' => $wstat
            };
        }
        else {
            $tot{good}++;
        }

        my @todo = $parser->todo_passed;
        if (@todo) {
            $todo_passed{$test} = {
                'canon'  => _canon(@todo),
                'estat'  => $estat,
                'failed' => scalar @todo,
                'max'    => scalar $parser->todo,
                'name'   => $test,
                'wstat'  => $wstat
            };
        }
    }

    return ( \%tot, \%failedtests, \%todo_passed );
}

=head2 execute_tests( tests => \@test_files, out => \*FH )

Runs all the given C<@test_files> (just like C<runtests()>) but
doesn't generate the final report.  During testing, progress
information will be written to the currently selected output
filehandle (usually C<STDOUT>), or to the filehandle given by the
C<out> parameter.  The I<out> is optional.

Returns a list of two values, C<$total> and C<$failed>, describing the
results.  C<$total> is a hash ref summary of all the tests run.  Its
keys and values are this:

    bonus           Number of individual todo tests unexpectedly passed
    max             Number of individual tests ran
    ok              Number of individual tests passed
    sub_skipped     Number of individual tests skipped
    todo            Number of individual todo tests

    files           Number of test files ran
    good            Number of test files passed
    bad             Number of test files failed
    tests           Number of test files originally given
    skipped         Number of test files skipped

If C<< $total->{bad} == 0 >> and C<< $total->{max} > 0 >>, you've
got a successful test.

C<$failed> is a hash ref of all the test scripts that failed.  Each key
is the name of a test script, each value is another hash representing
how that script failed.  Its keys are these:

    name        Name of the test which failed
    estat       Script's exit value
    wstat       Script's wait status
    max         Number of individual tests
    failed      Number which failed
    canon       List of tests which failed (as string).

C<$failed> should be empty if everything passed.

=cut

1;
__END__

=head1 EXPORT

C<&runtests> is exported by C<Test::Harness> by default.

C<&execute_tests>, C<$verbose>, C<$switches> and C<$debug> are
exported upon request.

=head1 ENVIRONMENT VARIABLES THAT TAP::HARNESS::COMPATIBLE SETS

C<Test::Harness> sets these before executing the individual tests.

=over 4

=item C<HARNESS_ACTIVE>

This is set to a true value.  It allows the tests to determine if they
are being executed through the harness or by any other means.

=item C<HARNESS_VERSION>

This is the version of C<Test::Harness>.

=back

=head1 ENVIRONMENT VARIABLES THAT AFFECT TEST::HARNESS

=over 4

=item C<HARNESS_TIMER>

Setting this to true will make the harness display the number of
milliseconds each test took.  You can also use F<prove>'s C<--timer>
switch.

=item C<HARNESS_VERBOSE>

If true, C<Test::Harness> will output the verbose results of running
its tests.  Setting C<$Test::Harness::verbose> will override this,
or you can use the C<-v> switch in the F<prove> utility.

=item C<HARNESS_OPTIONS>

Provide additional options to the harness. Currently supported options are:

=over

=item C<< j<n> >>

Run <n> (default 9) parallel jobs.

=item C<< f >>

Use forked parallelism.

=back

Multiple options may be separated by colons:

    HARNESS_OPTIONS=j9:f make test

=back

=head1 Taint Mode

Normally when a Perl program is run in taint mode the contents of the
C<PERL5LIB> environment variable do not appear in C<@INC>.

Because C<PERL5LIB> is often used during testing to add build
directories to C<@INC> C<Test::Harness> (actually
L<TAP::Parser::Source::Perl>) passes the names of any directories found
in C<PERL5LIB> as -I switches. The net effect of this is that
C<PERL5LIB> is honoured even in taint mode.

=head1 SEE ALSO

L<TAP::Harness>

=head1 BUGS

Please report any bugs or feature requests to
C<bug-test-harness at rt.cpan.org>, or through the web interface at
L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Test-Harness>.  I will be 
notified, and then you'll automatically be notified of progress on your bug 
as I make changes.

=head1 AUTHORS

Andy Armstrong  C<< <andy@hexten.net> >>

L<Test::Harness> 2.64 (maintained by Andy Lester and on which this
module is based) has this attribution:

    Either Tim Bunce or Andreas Koenig, we don't know. What we know for
    sure is, that it was inspired by Larry Wall's F<TEST> script that came
    with perl distributions for ages. Numerous anonymous contributors
    exist.  Andreas Koenig held the torch for many years, and then
    Michael G Schwern.

=head1 LICENCE AND COPYRIGHT

Copyright (c) 2007-2008, Andy Armstrong C<< <andy@hexten.net> >>. All rights reserved.

This module is free software; you can redistribute it and/or
modify it under the same terms as Perl itself. See L<perlartistic>.

