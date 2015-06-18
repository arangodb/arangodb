package CPAN::Reporter;
use strict;
use vars qw/$VERSION/;
$VERSION = '1.13'; 
$VERSION = eval $VERSION;

use Config;
use CPAN ();
use CPAN::Version ();
use File::Basename qw/basename/;
use File::Find ();
use File::HomeDir ();
use File::Path qw/mkpath rmtree/;
use File::Spec ();
use IO::File ();
use Probe::Perl ();
use Tee qw/tee/;
use Test::Reporter ();
use CPAN::Reporter::Config ();
use CPAN::Reporter::History ();
use CPAN::Reporter::PrereqCheck ();

use constant MAX_OUTPUT_LENGTH => 50_000;

#--------------------------------------------------------------------------#
# public API
#--------------------------------------------------------------------------#

sub configure {
    goto &CPAN::Reporter::Config::_configure; 
}

sub grade_make { 
    my @args = @_;
    my $result = _init_result( 'make', @args ) or return; 
    _compute_make_grade($result);
    if ( $result->{grade} eq 'discard' ) {
        $CPAN::Frontend->myprint( 
            "\nCPAN::Reporter: test results were not valid, $result->{grade_msg}.\n\n",
            $result->{prereq_pm}, "\n",
            "Test report will not be sent"
        );
        CPAN::Reporter::History::_record_history( $result ) 
            if not CPAN::Reporter::History::_is_duplicate( $result );
    }
    else {
        _print_grade_msg($result->{make_cmd}, $result);
        if ( $result->{grade} ne 'pass' ) { _dispatch_report( $result ) }
    }
    return $result->{success};
}

sub grade_PL {
    my @args = @_;
    my $result = _init_result( 'PL', @args ) or return; 
    _compute_PL_grade($result);
    if ( $result->{grade} eq 'discard' ) {
        $CPAN::Frontend->myprint( 
            "\nCPAN::Reporter: test results were not valid, $result->{grade_msg}.\n\n",
            $result->{prereq_pm}, "\n",
            "Test report will not be sent"
        );
        CPAN::Reporter::History::_record_history( $result ) 
            if not CPAN::Reporter::History::_is_duplicate( $result );
    }
    else {
        _print_grade_msg($result->{PL_file} , $result);
        if ( $result->{grade} ne 'pass' ) { _dispatch_report( $result ) }
    }
    return $result->{success};
}

sub grade_test {
    my @args = @_;
    my $result = _init_result( 'test', @args ) or return; 
    _compute_test_grade($result);
    if ( $result->{grade} eq 'discard' ) {
        $CPAN::Frontend->myprint( 
            "\nCPAN::Reporter: test results were not valid, $result->{grade_msg}.\n\n",
            $result->{prereq_pm}, "\n",
            "Test report will not be sent"
        );
        CPAN::Reporter::History::_record_history( $result ) 
            if not CPAN::Reporter::History::_is_duplicate( $result );
    }
    else {
        _print_grade_msg( "Test", $result );
        _dispatch_report( $result );
    }
    return $result->{success};
}

sub record_command {
    my ($command, $timeout) = @_;

    # XXX refactor this! 
    # Get configuration options
    if ( -r CPAN::Reporter::Config::_get_config_file() ) {
        my $config_obj = CPAN::Reporter::Config::_open_config_file();
        my $config;
        $config = CPAN::Reporter::Config::_get_config_options( $config_obj ) 
            if $config_obj;

        $timeout ||= $config->{command_timeout}; # might still be undef
    }

    my ($cmd, $redirect) = _split_redirect($command);

    my $temp_out = _temp_filename( 'CPAN-Reporter-TO-' );

    # Teeing a command loses its exit value so we must wrap the command 
    # and print the exit code so we can read it off of output
    my $wrap_code;
    if ( $timeout ) {
        $wrap_code = $^O eq 'MSWin32'
                   ? _timeout_wrapper_win32($cmd, $timeout)
                   : _timeout_wrapper($cmd, $timeout);
    }
    # if no timeout or timeout wrap code wasn't available
    if ( ! $wrap_code ) {
        my $safecmd = quotemeta($cmd);
        $wrap_code = << "HERE";
my \$rc = system("$safecmd");
my \$ec = \$rc == -1 ? -1 : \$?;
print "($safecmd exited with \$ec)\\n";
HERE
    }

    # write code to a tempfile for execution
    my $wrapper_name = _temp_filename( 'CPAN-Reporter-CW-' );
    my $wrapper_fh = IO::File->new( $wrapper_name, 'w' )
        or die "Could not create a wrapper for $cmd\: $!";

    $wrapper_fh->print( $wrap_code );
    $wrapper_fh->close;
    
    # tee the command wrapper
    my $tee_input = Probe::Perl->find_perl_interpreter() .  " $wrapper_name";
    $tee_input .= " $redirect" if defined $redirect;
    tee($tee_input, { stderr => 1 }, $temp_out);
        
    # read back the output
    my $output_fh = IO::File->new($temp_out, "r");
    if ( !$output_fh ) {
        $CPAN::Frontend->mywarn( 
            "CPAN::Reporter: couldn't read command results for '$cmd'\n" 
        );
        return;
    }
    my @cmd_output = <$output_fh>;
    $output_fh->close;

    # cleanup
    unlink $wrapper_name, $temp_out;

    if ( ! @cmd_output ) {
        $CPAN::Frontend->mywarn( 
            "CPAN::Reporter: didn't capture command results for '$cmd'\n"
        );
        return;
    }

    # extract the exit value
    my $exit_value;
    if ( $cmd_output[-1] =~ m{exited with} ) {
        ($exit_value) = $cmd_output[-1] =~ m{exited with ([-0-9]+)};
        pop @cmd_output;
    }
    
    # bail out on some errors
    if ( ! defined $exit_value ) {
        $CPAN::Frontend->mywarn( 
            "CPAN::Reporter: couldn't determine exit value for '$cmd'\n"
        );
        return;
    }
    elsif ( $exit_value == -1 ) {
        $CPAN::Frontend->mywarn( 
            "CPAN::Reporter: couldn't execute '$cmd'\n"
        );
        return;
    }

    return \@cmd_output, $exit_value;
}

sub test {
    my ($dist, $system_command) = @_;
    my ($output, $exit_value) = record_command( $system_command );
    return grade_test( $dist, $system_command, $output, $exit_value );
}

#--------------------------------------------------------------------------#
# private functions
#--------------------------------------------------------------------------#

#--------------------------------------------------------------------------#
# _compute_make_grade
#--------------------------------------------------------------------------#

sub _compute_make_grade {
    my $result = shift;
    my ($grade,$msg);
    if ( $result->{exit_value} ) {
        $result->{grade} = "fail";
        $result->{grade_msg} = "Stopped with an error"
    }
    else {
        $result->{grade} = "pass";
        $result->{grade_msg} = "No errors"
    }

    _downgrade_known_causes( $result );
    
    $result->{success} =  $result->{grade} eq 'pass'
                       || $result->{grade} eq 'unknown';
    return;
}

#--------------------------------------------------------------------------#
# _compute_PL_grade
#--------------------------------------------------------------------------#

sub _compute_PL_grade {
    my $result = shift;
    my ($grade,$msg);
    if ( $result->{exit_value} ) {
        $result->{grade} = "fail";
        $result->{grade_msg} = "Stopped with an error"
    }
    else {
        $result->{grade} = "pass";
        $result->{grade_msg} = "No errors"
    }

    _downgrade_known_causes( $result );
    
    $result->{success} =  $result->{grade} eq 'pass'
                       || $result->{grade} eq 'unknown';
    return;
}

#--------------------------------------------------------------------------#
# _compute_test_grade
#
# Don't shortcut to unknown unless _has_tests because a custom
# Makefile.PL or Build.PL might define tests in a non-standard way
# 
# With test.pl and 'make test', any t/*.t might pass Test::Harness, but
# test.pl might still fail, or there might only be test.pl,
# so use exit code directly
#
# Likewise, if we have recursive Makefile.PL, then we don't trust the
# reverse-order parsing and should just take the exit code directly 
#
# Otherwise, parse in reverse order for Test::Harness output or a couple
# other significant strings and stop after the first match.  Going in 
# reverse and stopping is done to (hopefully) avoid picking up spurious 
# results from any test output.  But then we have to check for 
# unsupported OS strings in case those were printed but were not fatal.
#--------------------------------------------------------------------------#

sub _compute_test_grade {
    my $result = shift;
    my ($grade,$msg);
    my $output = $result->{output};

    # In some cases, get a result straight from the exit code 
    if ( $result->{is_make} && ( -f "test.pl" || _has_recursive_make() ) ) {
        if ( $result->{exit_value} ) {
            $grade = "fail";
            $msg = "'make test' error detected";
        }
        else {
            $grade = "pass";
            $msg = "'make test' no errors";
        }
    }
    # Otherwise, get a result from Test::Harness output
    else {
        # figure out the right harness parser
        _expand_result( $result ); 
        my $harness_version = $result->{toolchain}{'Test::Harness'}{have};
        my $harness_parser = CPAN::Version->vgt($harness_version, '2.99_01')
                    ? \&_parse_tap_harness
                    : \&_parse_test_harness;
        # parse lines in reverse
        for my $i ( reverse 0 .. $#{$output} ) {
            if ( $output->[$i] =~ m{No support for OS|OS unsupported}ims ) { # from any *.t file
                $grade = 'na';
                $msg = 'This platform is not supported';
            }
            elsif ( $output->[$i] =~ m{^.?No tests defined}ms ) { # from M::B
                $grade = 'unknown';
                $msg = 'No tests provided';
            }
            else {
                ($grade, $msg) = $harness_parser->( $output->[$i] );
            }
            last if $grade;
        }
        # fallback on exit value if no recognizable Test::Harness output
        if ( ! $grade ) {
            $grade = $result->{exit_value} ? "fail" : "pass";
            $msg = ( $result->{is_make} ? "'make test' " : "'Build test' " )
                 . ( $result->{exit_value} ? "error detected" : "no errors");
        }
    }

    $result->{grade} = $grade;
    $result->{grade_msg} = $msg;

    _downgrade_known_causes( $result );

    $result->{success} =  $result->{grade} eq 'pass'
                       || $result->{grade} eq 'unknown';
    return;
}

#--------------------------------------------------------------------------#
# _dispatch_report
#
# Set up Test::Reporter and prompt user for CC, edit, send
#--------------------------------------------------------------------------#

sub _dispatch_report {
    my $result = shift;
    my $phase = $result->{phase};

    $CPAN::Frontend->myprint(
        "CPAN::Reporter: preparing a CPAN Testers report for $result->{dist_name}\n"
    );

    # Get configuration options
    my $config_obj = CPAN::Reporter::Config::_open_config_file();
    my $config;
    $config = CPAN::Reporter::Config::_get_config_options( $config_obj ) 
        if $config_obj;
    if ( ! $config->{email_from} ) {
        $CPAN::Frontend->mywarn( << "EMAIL_REQUIRED");
        
CPAN::Reporter: required 'email_from' option missing an email address, so
test report will not be sent. See documentation for configuration details.

EMAIL_REQUIRED
        return;
    }
        
    # Abort if the distribution name is not formatted according to 
    # CPAN Testers requirements: Dist-Name-version.suffix
    # Regex from CPAN-Testers should extract name, separator, version
    # and extension
    my @format_checks = $result->{dist_basename} =~ 
        m{(.+)([\-\_])(v?\d.*)(\.(?:tar\.(?:gz|bz2)|tgz|zip))$}i;
    ;
    if ( ! grep { length } @format_checks ) {
        $CPAN::Frontend->mywarn( << "END_BAD_DISTNAME");
        
CPAN::Reporter: the distribution name '$result->{dist_basename}' does not 
appear to be packaged according to CPAN tester guidelines. Perhaps it is 
not a normal CPAN distribution.

Test report will not be sent.

END_BAD_DISTNAME

        return;
    }

    # Gather 'expensive' data for the report
    _expand_result( $result);

    # Skip if distribution name matches the send_skipfile
    if ( $config->{send_skipfile} && -r $config->{send_skipfile} ) {
        my $send_skipfile = IO::File->new( $config->{send_skipfile}, "r" );
        my $dist_id = $result->{dist}->pretty_id;
        while ( my $pattern = <$send_skipfile> ) {
            chomp($pattern);
            # ignore comments
            next if substr($pattern,0,1) eq '#';
            # if it doesn't match, continue with next pattern
            next if $dist_id !~ /$pattern/i;
            # if it matches, warn and return
            $CPAN::Frontend->myprint( << "END_SKIP_DIST" );
CPAN::Reporter: '$dist_id' matched against the send_skipfile.  

Test report will not be sent.

END_SKIP_DIST

            return;
        }
    }

    # Setup the test report
    my $tr = Test::Reporter->new;
    $tr->grade( $result->{grade} );
    $tr->distribution( $result->{dist_name}  );

    # Skip if duplicate and not sending duplicates
    my $is_duplicate = CPAN::Reporter::History::_is_duplicate( $result );
    if ( $is_duplicate ) {
        if ( _prompt( $config, "send_duplicates", $tr->grade) =~ /^n/ ) {
            $CPAN::Frontend->myprint(<< "DUPLICATE_REPORT");

CPAN::Reporter: this appears to be a duplicate report for the $phase phase:
@{[$tr->subject]}

Test report will not be sent.

DUPLICATE_REPORT
            
            return;
        }
    }

    # Set debug and transport options, if supported
    $tr->debug( $config->{debug} ) if defined $config->{debug};
    my $transport = $config->{transport} || 'Net::SMTP';
    if (length $transport && ( $transport !~ /\ANet::SMTP|Mail::Send\z/ )) {
        $CPAN::Frontend->mywarn(
            "CPAN::Reporter: '$config->{transport}' is not a valid transport option." .
            " Falling back to Net::SMTP\n"
        );
        $transport = 'Net::SMTP';
    }
    $tr->transport( $transport );

    # prepare mail transport
    $tr->from( $config->{email_from} );
    $tr->address( $config->{email_to} ) if $config->{email_to};
    if ( $config->{smtp_server} ) {
        my @mx = split " ", $config->{smtp_server};
        $tr->mx( \@mx );
    }
    
    # Populate the test report
    $tr->comments( _report_text( $result ) );
    $tr->via( 'CPAN::Reporter ' . $CPAN::Reporter::VERSION );
    my @cc = _should_copy_author( $result, $config );

    # prompt for editing report
    if ( _prompt( $config, "edit_report", $tr->grade ) =~ /^y/ ) {
        my $editor = $config->{editor};
        local $ENV{VISUAL} = $editor if $editor; ## no critic
        $tr->edit_comments;
    }
    
    # send_*_report can override send_report
    my $send_config = defined $config->{"send_$phase\_report"}
                    ? "send_$phase\_report"
                    : "send_report" ;
    if ( _prompt( $config, $send_config, $tr->grade ) =~ /^y/ ) {
        $CPAN::Frontend->myprint( "CPAN::Reporter: sending test report with '" . $tr->grade . 
              "' to " . join(q{, }, $tr->address, @cc) . "\n");
        if ( $tr->send( @cc ) ) {
            CPAN::Reporter::History::_record_history( $result ) 
                if not $is_duplicate;
        }
        else {
            $CPAN::Frontend->mywarn( "CPAN::Reporter: " . $tr->errstr . "\n");
        }
    }
    else {
        $CPAN::Frontend->myprint("CPAN::Reporter: test report will not be sent\n");
    }

    return;
}

#--------------------------------------------------------------------------#
# _downgrade_known_causes
# Downgrade failure/unknown grade if we can determine a cause
# If platform not supported => 'na'
# If perl version is too low => 'na'
# If stated prereqs missing => 'discard'
#--------------------------------------------------------------------------#

sub _downgrade_known_causes {
    my ($result) = @_;
    my ($grade, $output) = ( $result->{grade}, $result->{output} );
    my $msg = $result->{grade_msg} || q{};

    # shortcut unless fail/unknown; but PL might look like pass but actually
    # have "OS Unsupported" messages if someone printed message and then
    # did "exit 0"
    return if $grade eq 'na';
    return if $grade eq 'pass' && $result->{phase} ne 'PL';

    # get prereqs
    _expand_result( $result ); 

    # if process was halted with a signal, just set for discard and return
    if ( $result->{exit_value} & 127 ) {
        $result->{grade} = 'discard';
        $result->{grade_msg} = 'Command interrupted';
        return;
    }

    # look for perl version error messages from various programs
    # "Error evaling..." type errors happen on Perl < 5.006 when modules
    # define their version with "our $VERSION = ..."
    my $version_error;
    for my $line ( @$output ) {
        if( $line =~ /Perl .*? required.*?--this is only/ims ||
            $line =~ /ERROR: perl: Version .*? is installed, but we need version/ims ||
            $line =~ /ERROR: perl \(.*?\) is installed, but we need version/ims ||
            $line =~ /Error evaling version line 'BEGIN/ims ||
            $line =~ /Could not eval '/ims
        ) {
            $version_error++;
            last;
        }
    }

    # check for explicit version error or just a perl version prerequisite
    if ( $version_error || $result->{prereq_pm} =~ m{^\s+!\s+perl\s}ims ) {
        $grade = 'na';
        $msg = 'Perl version too low';
    }
    # check again for unsupported OS in case we took 'fail' from exit value
    elsif ( grep { /No support for OS|OS unsupported/ims } @{$output} ) {
        $grade = 'na';
        $msg = 'This platform is not supported';
    }
    # check the prereq report for missing or failure flag '!'
    elsif ( $grade ne 'pass' && $result->{prereq_pm} =~ m{n/a}ims ) {
        $grade = 'discard';
        $msg = 'Prerequisite missing';
    }
    elsif ( $grade ne 'pass' && $result->{prereq_pm} =~ m{^\s+!}ims ) {
        $grade = 'discard';
        $msg = 'Prerequisite version too low';
    }
    # in PL stage -- if pass but no Makefile or Build, then this should 
    # be recorded as a discard
    elsif ( $result->{phase} eq 'PL' && $grade eq 'pass' 
         && ! -f 'Makefile' && ! -f 'Build' 
    ) {
        $grade = 'discard';
        $msg = 'No Makefile or Build file found';
    }
    
    # store results
    $result->{grade} = $grade;
    $result->{grade_msg} = $msg;

    return;
}

#--------------------------------------------------------------------------#
# _expand_result - add expensive information like prerequisites and
# toolchain that should only be generated if a report will actually
# be sent
#--------------------------------------------------------------------------#

sub _expand_result {
    my $result = shift;
    return if $result->{expanded}++; # only do this once
    $result->{prereq_pm} = _prereq_report( $result->{dist} );
    $result->{env_vars} = _env_report();
    $result->{special_vars} = _special_vars_report();
    $result->{toolchain_versions} = _toolchain_report( $result );
    $result->{perl_version} = CPAN::Reporter::History::_format_perl_version();
    return;
}

#--------------------------------------------------------------------------#
# _env_report
#--------------------------------------------------------------------------#

# Entries bracketed with "/" are taken to be a regex; otherwise literal
my @env_vars= qw(
    /PERL/
    /LC_/
    LANG
    LANGUAGE
    PATH
    SHELL
    COMSPEC
    TERM
    TEMP
    TMPDIR
    AUTOMATED_TESTING
    /AUTHOR_TEST/
    INCLUDE
    LIB
    LD_LIBRARY_PATH
    PROCESSOR_IDENTIFIER
    NUMBER_OF_PROCESSORS
);

sub _env_report {
    my @vars_found;
    for my $var ( @env_vars ) {
        if ( $var =~ m{^/(.+)/$} ) {
            push @vars_found, grep { /$1/ } keys %ENV;
        }
        else {
            push @vars_found, $var if exists $ENV{$var};
        }
    }

    my $report = "";
    for my $var ( sort @vars_found ) {
        my $value = $ENV{$var};
        $value = '[undef]' if ! defined $value;
        $report .= "    $var = $value\n";
    }
    return $report;
}

#--------------------------------------------------------------------------#
# _has_recursive_make
#
# Ignore Makefile.PL in t directories 
#--------------------------------------------------------------------------#

sub _has_recursive_make {
    my $PL_count = 0;
    File::Find::find(
        sub {
            if ( $_ eq 't' ) {
                $File::Find::prune = 1;
            }
            elsif ( $_ eq 'Makefile.PL' ) {
                $PL_count++;
            }
        },
        File::Spec->curdir()
    );
    return $PL_count > 1;
}

#--------------------------------------------------------------------------#
# _init_result -- create and return a hash of values for use in 
# report evaluation and dispatch
#
# takes same argument format as grade_*()
#--------------------------------------------------------------------------#

sub _init_result {
    my ($phase, $dist, $system_command, $output, $exit_value) = @_;
    
    unless ( defined $output && defined $exit_value ) {
        my $missing;
        if ( ! defined $output && ! defined $exit_value ) {
            $missing = "exit value and output"
        }
        elsif ( defined $output && !defined $exit_value ) {
            $missing =  "exit value"
        }
        else {
            $missing = "output";
        }
        $CPAN::Frontend->mywarn(
            "CPAN::Reporter: had errors capturing $missing. Tests abandoned"
        );
        return;
    }

    my $result = {
        phase => $phase,
        dist => $dist,
        command => $system_command,
        is_make => _is_make( $system_command ),
        output => ref $output eq 'ARRAY' ? $output : [ split /\n/, $output ],
        exit_value => $exit_value,
        # Note: pretty_id is like "DAGOLDEN/CPAN-Reporter-0.40.tar.gz"
        dist_basename => basename($dist->pretty_id),
        dist_name => $dist->base_id,
    };

    # Used in messages to user
    $result->{PL_file} = $result->{is_make} ? "Makefile.PL" : "Build.PL";
    $result->{make_cmd} = $result->{is_make} ? $Config{make} : "Build";

    # CPAN might fail to find an author object for some strange dists
    my $author = $dist->author;
    $result->{author} = defined $author ? $author->fullname : "Author";
    $result->{author_id} = defined $author ? $author->id : "" ;

    return $result;
}

#--------------------------------------------------------------------------#
# _is_make
#--------------------------------------------------------------------------#

sub _is_make {
    my $command = shift;
    return $command =~ m{\b(?:\S*make|Makefile.PL)\b}ims ? 1 : 0;
}

#--------------------------------------------------------------------------#
# _max_length
#--------------------------------------------------------------------------#

sub _max_length {
    my ($first, @rest) = @_;
    my $max = length $first;
    for my $term ( @rest ) {
        $max = length $term if length $term > $max;
    }
    return $max;
}

    
#--------------------------------------------------------------------------#
# _parse_tap_harness
# 
# As of Test::Harness 2.99_02, the final line is provided by TAP::Harness
# as "Result: STATUS" where STATUS is "PASS", "FAIL" or "NOTESTS"
#--------------------------------------------------------------------------#


sub _parse_tap_harness {
    my ($line) = @_;
    if ( $line =~ m{^Result:\s+([A-Z]+)} ) {
        if    ( $1 eq 'PASS' ) {
            return ('pass', 'All tests successful');
        }
        elsif ( $1 eq 'FAIL' ) {
            return ('fail', 'One or more tests failed');
        }
        elsif ( $1 eq 'NOTESTS' ) {
            return ('unknown', 'No tests were run');
        }
    }
    elsif ( $line =~ m{Bailout called\.\s+Further testing stopped}ms ) {
        return ( 'fail', 'Bailed out of tests');
    }
    return;
}

#--------------------------------------------------------------------------#
# _parse_test_harness
#
# Output strings taken from Test::Harness::
# _show_results()  -- for versions < 2.57_03 
# get_results()    -- for versions >= 2.57_03
#--------------------------------------------------------------------------#

sub _parse_test_harness {
    my ($line) = @_;
    if ( $line =~ m{^All tests successful}ms ) {
        return ( 'pass', 'All tests successful' );
    }
    elsif ( $line =~ m{^FAILED--no tests were run}ms ) {
        return ( 'unknown', 'No tests were run' );
    }
    elsif ( $line =~ m{^FAILED--.*--no output}ms ) {
        return ( 'unknown', 'No tests were run');
    }
    elsif ( $line =~ m{FAILED--Further testing stopped}ms ) {
        return ( 'fail', 'Bailed out of tests');
    }
    elsif ( $line =~ m{^Failed }ms ) {  # must be lowercase
        return ( 'fail', 'One or more tests failed');
    }
    else {
        return;
    }
}

#--------------------------------------------------------------------------#
# _prereq_report
#--------------------------------------------------------------------------#

sub _prereq_report {
    my $dist = shift;
    my (%need, %have, %prereq_met, $report);
    
    my $prereq_pm = $dist->prereq_pm;

    if ( ref $prereq_pm eq 'HASH' ) {
        # is it the new CPAN style with requires/build_requires?
        if (join(q{ }, sort keys %$prereq_pm) eq "build_requires requires") {
            $need{requires} = $prereq_pm->{requires} 
                if  ref $prereq_pm->{requires} eq 'HASH';
            $need{build_requires} = $prereq_pm->{build_requires} 
                if ref $prereq_pm->{build_requires} eq 'HASH';
        }
        else {
            $need{requires} = $prereq_pm;
        }
    }

    # see what prereqs are satisfied in subprocess
    for my $section ( qw/requires build_requires/ ) {
        next unless ref $need{$section} eq 'HASH';
        my @prereq_list = %{ $need{$section} };
        next unless @prereq_list;
        my $prereq_results = _version_finder( @prereq_list );
        for my $mod ( keys %{$prereq_results} ) {
            $have{$section}{$mod} = $prereq_results->{$mod}{have};
            $prereq_met{$section}{$mod} = $prereq_results->{$mod}{met};
        }
    }
    
    # find formatting widths 
    my ($name_width, $need_width, $have_width) = (6, 4, 4);
    for my $section ( qw/requires build_requires/ ) {
        for my $module ( keys %{ $need{$section} } ) {
            my $name_length = length $module;
            my $need_length = length $need{$section}{$module};
            my $have_length = length $have{$section}{$module};
            $name_width = $name_length if $name_length > $name_width;
            $need_width = $need_length if $need_length > $need_width;
            $have_width = $have_length if $have_length > $have_width;
        }
    }

    my $format_str = 
        "  \%1s \%-${name_width}s \%-${need_width}s \%-${have_width}s\n";

    # generate the report
    for my $section ( qw/requires build_requires/ ) {
        if ( keys %{ $need{$section} } ) {
            $report .= "$section:\n\n";
            $report .= sprintf( $format_str, " ", qw/Module Need Have/ );
            $report .= sprintf( $format_str, " ", 
                                 "-" x $name_width, 
                                 "-" x $need_width,
                                 "-" x $have_width );
        }
        for my $module ( sort { lc $a cmp lc $b } keys %{ $need{$section} } ) {
            my $need = $need{$section}{$module};
            my $have = $have{$section}{$module};
            my $bad = $prereq_met{$section}{$module} ? " " : "!";
            $report .= 
                sprintf( $format_str, $bad, $module, $need, $have);
        }
    }
    
    return $report || "    No requirements found\n";
}

#--------------------------------------------------------------------------#
# _print_grade_msg -
#--------------------------------------------------------------------------#

sub _print_grade_msg {
    my ($phase, $result) = @_;
    my ($grade, $msg) = ($result->{grade}, $result->{grade_msg});
    $CPAN::Frontend->myprint( "CPAN::Reporter: $phase result is '$grade'");
    $CPAN::Frontend->myprint(", $msg") if defined $msg && length $msg;
    $CPAN::Frontend->myprint(".\n");
    return;
}

#--------------------------------------------------------------------------#
# _prompt
#
# Note: always returns lowercase
#--------------------------------------------------------------------------#

sub _prompt {
    my ($config, $option, $grade, $extra) = @_;
    $extra ||= q{};

    my %spec = CPAN::Reporter::Config::_config_spec();

    my $dispatch = CPAN::Reporter::Config::_validate_grade_action_pair(
        $option, join(q{ }, "default:no", $config->{$option} || '')
    );
    my $action = $dispatch->{$grade} || $dispatch->{default};
    my $intro = $spec{$option}{prompt} . $extra . " (yes/no)";
    my $prompt;
    if     ( $action =~ m{^ask/yes}i ) { 
        $prompt = CPAN::Shell::colorable_makemaker_prompt( $intro, "yes" );
    }
    elsif  ( $action =~ m{^ask(/no)?}i ) {
        $prompt = CPAN::Shell::colorable_makemaker_prompt( $intro, "no" );
    }
    else { 
        $prompt = $action;
    }
    return lc $prompt;
}

#--------------------------------------------------------------------------#
# _report_text
#--------------------------------------------------------------------------#

my %intro_para = (
    'pass' => <<'HERE',
Thank you for uploading your work to CPAN.  Congratulations!
All tests were successful.
HERE

    'fail' => <<'HERE',
Thank you for uploading your work to CPAN.  However, there was a problem
testing your distribution.

If you think this report is invalid, please consult the CPAN Testers Wiki
for suggestions on how to avoid getting FAIL reports for missing library
or binary dependencies, unsupported operating systems, and so on:

http://cpantest.grango.org/wiki/CPANAuthorNotes
HERE

    'unknown' => <<'HERE',
Thank you for uploading your work to CPAN.  However, attempting to
test your distribution gave an inconclusive result.  

This could be because you did not define tests, tests could not be 
found, because your tests were interrupted before they finished, or 
because the results of the tests could not be parsed.  You may wish to 
consult the CPAN Testers Wiki:

http://cpantest.grango.org/wiki/CPANAuthorNotes
HERE

    'na' => <<'HERE',
Thank you for uploading your work to CPAN.  While attempting to build or test 
this distribution, the distribution signaled that support is not available 
either for this operating system or this version of Perl.  Nevertheless, any 
diagnostic output produced is provided below for reference.  If this is not 
what you expect, you may wish to consult the CPAN Testers Wiki:

http://cpantest.grango.org/wiki/CPANAuthorNotes
HERE
    
);

sub _report_text {
    my $data = shift;
    my $test_log = join(q{},@{$data->{output}});
    if ( length $test_log > MAX_OUTPUT_LENGTH ) {
        $test_log = substr( $test_log, 0, MAX_OUTPUT_LENGTH) . "\n";
        my $max_k = int(MAX_OUTPUT_LENGTH/1000) . "K";
        $test_log .= "\n[Output truncated after $max_k]\n\n";
    }
    # Flag automated report
    my $default_comment = $ENV{AUTOMATED_TESTING} 
        ? "this report is from an automated smoke testing program\nand was not reviewed by a human for accuracy" 
        : "none provided" ;
        
    # generate report
    my $output = << "ENDREPORT";
Dear $data->{author},
    
This is a computer-generated report for $data->{dist_name}
on perl $data->{perl_version}, created by CPAN-Reporter-$CPAN::Reporter::VERSION\. 

$intro_para{ $data->{grade} }
Sections of this report:

    * Tester comments
    * Program output
    * Prerequisites
    * Environment and other context

------------------------------
TESTER COMMENTS
------------------------------

Additional comments from tester: 

$default_comment

------------------------------
PROGRAM OUTPUT
------------------------------

Output from '$data->{command}':

$test_log
------------------------------
PREREQUISITES
------------------------------

Prerequisite modules loaded:

$data->{prereq_pm}
------------------------------
ENVIRONMENT AND OTHER CONTEXT
------------------------------

Environment variables:

$data->{env_vars}
Perl special variables (and OS-specific diagnostics, for MSWin32):

$data->{special_vars}
Perl module toolchain versions installed:

$data->{toolchain_versions}
ENDREPORT

    return $output;
}


#--------------------------------------------------------------------------#
# _should_copy_author
#--------------------------------------------------------------------------#

sub _should_copy_author {
    my ($result, $config) = @_;

    # User prompts for action
    my $author_email = $result->{author_id} 
                     ? "$result->{author_id}\@cpan.org"
                     : q{};
    if ( ! $author_email ) {
        $CPAN::Frontend->mywarn( "CPAN::Reporter: couldn't determine author_id and won't cc author.\n");
        return;
    }

    # Skip if distribution name matches the cc_skipfile
    if ( $config->{cc_skipfile} && -r $config->{cc_skipfile} ) {
        my $cc_skipfile = IO::File->new( $config->{cc_skipfile}, "r" );
        my $dist_id = $result->{dist}->pretty_id;
        while ( my $pattern = <$cc_skipfile> ) {
            chomp($pattern);
            # ignore comments
            next if substr($pattern,0,1) eq '#';
            # if it doesn't match, continue with next pattern
            next if $dist_id !~ /$pattern/i;
            # if it matches, warn and return
            $CPAN::Frontend->myprint( << "END_SKIP_DIST" );
CPAN::Reporter: '$dist_id' matched against the cc_skipfile.  Won't copy author.
END_SKIP_DIST
            return;
        }
    }

    # Don't copy author on perls with patchlevels
    return if $Config{perl_patchlevel};

    # Finally, prompt user if necessary
    if ( _prompt( $config, "cc_author", $result->{grade}, "($author_email)?") =~ /^y/ ) {
        return $author_email;
    }
    else {
        return;
    }
}

#--------------------------------------------------------------------------#
# _special_vars_report
#--------------------------------------------------------------------------#

sub _special_vars_report {
    my $special_vars = << "HERE";
    \$^X = $^X
    \$UID/\$EUID = $< / $>
    \$GID = $(
    \$EGID = $)
HERE
    if ( $^O eq 'MSWin32' && eval "require Win32" ) { ## no critic
        my @getosversion = Win32::GetOSVersion();
        my $getosversion = join(", ", @getosversion);
        $special_vars .= "    Win32::GetOSName = " . Win32::GetOSName() . "\n";
        $special_vars .= "    Win32::GetOSVersion = $getosversion\n";
        $special_vars .= "    Win32::FsType = " . Win32::FsType() . "\n";
        $special_vars .= "    Win32::IsAdminUser = " . Win32::IsAdminUser() . "\n";
    }
    return $special_vars;
}

#--------------------------------------------------------------------------#
# _split_redirect
#--------------------------------------------------------------------------#

sub _split_redirect {
    my $command = shift;
    my ($cmd, $prefix) = ($command =~ m{\A(.+?)(\|.*)\z});
    if (defined $cmd) {
        return ($cmd, $prefix);
    }
    else { # didn't match a redirection
        return $command
    }
}

#--------------------------------------------------------------------------#
# _temp_filename -- stand-in for File::Temp for backwards compatibility
#
# takes an optional prefix, adds 8 random chars and returns 
# an absolute pathname
#
# NOTE -- manual unlink required
#--------------------------------------------------------------------------#

# @CHARS from File::Temp
my @CHARS = (qw/ A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
                 a b c d e f g h i j k l m n o p q r s t u v w x y z
                 0 1 2 3 4 5 6 7 8 9 _
             /);

sub _temp_filename {
    my ($prefix) = @_;
    $prefix = q{} unless defined $prefix;
    $prefix .= $CHARS[ int( rand(@CHARS) ) ] for 0 .. 7;
    return File::Spec->catfile(File::Spec->tmpdir(), $prefix);
}

#--------------------------------------------------------------------------#
# _timeout_wrapper
#--------------------------------------------------------------------------#

sub _timeout_wrapper {
    my ($cmd, $timeout) = @_;
    
    # protect shell quotes
    $cmd = quotemeta($cmd);

    my $wrapper = sprintf << 'HERE', $timeout, $cmd, $cmd;
use strict;
my ($pid, $exitcode);
eval {
    local $SIG{CHLD};
    local $SIG{ALRM} = sub {die 'Timeout'};
    $pid = fork;
    die "Cannot fork: $!\n" unless defined $pid;
    if ($pid) { #parent
        alarm %s;
        my $wstat = waitpid $pid, 0;
        $exitcode = $wstat == -1 ? -1 : $?;
    } else {    #child
        setpgrp; # new process group for targeted kill
        exec "%s";
    }
};
alarm 0;
if ($pid && $@ =~ /Timeout/){
    kill -9, $pid;
    my $wstat = waitpid $pid, 0;
    $exitcode = $wstat == -1 ? -1 : $?;
}
elsif ($@) {
    die $@;
}
print "(%s exited with $exitcode)\n";
HERE
    return $wrapper;
}

#--------------------------------------------------------------------------#
# _timeout_wrapper_win32
#--------------------------------------------------------------------------#

sub _timeout_wrapper_win32 {
    my ($cmd, $timeout) = @_;

    $timeout ||= 0;  # just in case upstream doesn't guarantee it

    eval "use Win32::Job ();";
    if ($@) {
        $CPAN::Frontend->mywarn( << 'HERE' );
CPAN::Reporter: you need Win32::Job for inactivity_timeout support.
Continuing without timeout...
HERE
        return;
    }

    my ($program) = split " ", $cmd;
    if (! File::Spec->file_name_is_absolute( $program ) ) {
        my $exe = $program . ".exe";
        my ($path) = grep { -e File::Spec->catfile($_,$exe) }
                     split /$Config{path_sep}/, $ENV{PATH};
        if (! $path) {
            $CPAN::Frontend->mywarn( << "HERE" );
CPAN::Reporter: can't locate $exe in the PATH. 
Continuing without timeout...
HERE
            return;
        }
        $program = File::Spec->catfile($path,$exe);
    }

    # protect shell quotes and other things
    $_ = quotemeta($_) for ($program, $cmd);

    my $wrapper = sprintf << 'HERE', $program, $cmd, $timeout;
use strict;
use Win32::Job;
my $executable = "%s";
my $cmd_line = "%s";
my $timeout = %s;

my $job = Win32::Job->new() or die $^E;
my $ppid = $job->spawn($executable, $cmd_line);
$job->run($timeout);
my $status = $job->status;
my $exitcode = $status->{$ppid}{exitcode};
if ( $exitcode == 293 ) {
    $exitcode = 9; # map Win32::Job kill (293) to SIGKILL (9)
}
elsif ( $exitcode & 255 ) {
    $exitcode = $exitcode << 8; # how perl expects it
}
print "($cmd_line exited with $exitcode)\n";
HERE
    return $wrapper;
}

#--------------------------------------------------------------------------#-
# _toolchain_report
#--------------------------------------------------------------------------#

my @toolchain_mods= qw(
    CPAN
    Cwd
    ExtUtils::CBuilder
    ExtUtils::Command
    ExtUtils::Install
    ExtUtils::MakeMaker
    ExtUtils::Manifest
    ExtUtils::ParseXS
    File::Spec
    Module::Build
    Module::Signature
    Test::Harness
    Test::More
    version
    YAML
    YAML::Syck
);

sub _toolchain_report {
    my ($result) = @_;

    my $installed = _version_finder( map { $_ => 0 } @toolchain_mods );
    $result->{toolchain} = $installed;

    my $mod_width = _max_length( keys %$installed );
    my $ver_width = _max_length( 
        map { $installed->{$_}{have} } keys %$installed  
    );

    my $format = "    \%-${mod_width}s \%-${ver_width}s\n";

    my $report = "";
    $report .= sprintf( $format, "Module", "Have" );
    $report .= sprintf( $format, "-" x $mod_width, "-" x $ver_width );

    for my $var ( sort keys %$installed ) {
        $report .= sprintf("    \%-${mod_width}s \%-${ver_width}s\n",
                            $var, $installed->{$var}{have} );
    }

    return $report;
}



#--------------------------------------------------------------------------#
# _version_finder
#
# module => version pairs
#
# This is done via an external program to show installed versions exactly
# the way they would be found when test programs are run.  This means that
# any updates to PERL5LIB will be reflected in the results.
#
# File-finding logic taken from CPAN::Module::inst_file().  Logic to 
# handle newer Module::Build prereq syntax is taken from
# CPAN::Distribution::unsat_prereq()
#
#--------------------------------------------------------------------------#

my $version_finder = $INC{'CPAN/Reporter/PrereqCheck.pm'};

sub _version_finder {
    my %prereqs = @_;

    my $perl = Probe::Perl->find_perl_interpreter();
    my @prereq_results;
    
    my $prereq_input = _temp_filename( 'CPAN-Reporter-PI-' );
    my $fh = IO::File->new( $prereq_input, "w" )
        or die "Could not create temporary '$prereq_input' for prereq analysis: $!";
    $fh->print( map { "$_ $prereqs{$_}\n" } keys %prereqs );
    $fh->close;

    my $prereq_result = qx/$perl $version_finder < $prereq_input/;

    unlink $prereq_input;

    my %result;
    for my $line ( split "\n", $prereq_result ) {
        next unless length $line;
        my ($mod, $met, $have) = split " ", $line;
        unless ( defined($mod) && defined($met) && defined($have) ) {
            $CPAN::Frontend->mywarn(
                "Error parsing output from CPAN::Reporter::PrereqCheck:\n" .
                $line
            );
            next;
        }
        $result{$mod}{have} = $have;
        $result{$mod}{met} = $met;
    }
    return \%result;
}

1; #this line is important and will help the module return a true value

__END__

#--------------------------------------------------------------------------#
# pod documentation 
#--------------------------------------------------------------------------#

=begin wikidoc

= NAME

CPAN::Reporter - Adds CPAN Testers reporting to CPAN.pm

= VERSION

This documentation describes version %%VERSION%%.

= SYNOPSIS

From the CPAN shell:

 cpan> install CPAN::Reporter
 cpan> reload cpan
 cpan> o conf init test_report

= DESCRIPTION

The CPAN Testers project captures and analyses detailed results from building
and testing CPAN distributions on multiple operating systems and multiple
versions of Perl.  This provides valuable feedback to module authors and
potential users to identify bugs or platform compatibility issues and improves
the overall quality and value of CPAN.

One way individuals can contribute is to send a report for each module that
they test or install.  CPAN::Reporter is an add-on for the CPAN.pm module to
send the results of building and testing modules to the CPAN Testers project.
Full support for CPAN::Reporter is available in CPAN.pm as of version 1.92.

= GETTING STARTED

== Installation

The first step in using CPAN::Reporter is to install it using whatever
version of CPAN.pm is already installed.  CPAN.pm will be upgraded as
a dependency if necessary.

 cpan> install CPAN::Reporter

If CPAN.pm was upgraded, it needs to be reloaded.

 cpan> reload cpan

== Configuration

If upgrading from a very old version of CPAN.pm, users may be prompted to renew
their configuration settings, including the 'test_report' option to enable
CPAN::Reporter.  

If not prompted automatically, users should manually initialize CPAN::Reporter
support.  After enabling CPAN::Reporter, CPAN.pm will automatically continue
with interactive configuration of CPAN::Reporter options.

 cpan> o conf init test_report

Users will need to enter an email address in one of the following formats:

 johndoe@example.com
 John Doe <johndoe@example.com>
 "John Q. Public" <johnqpublic@example.com>

Because {cpan-testers} uses a mailing list to collect test reports, it is
helpful if the email address provided is subscribed to the list.  Otherwise,
test reports will be held until manually reviewed and approved.  Subscribing an
account to the cpan-testers list is as easy as sending a blank email to
cpan-testers-subscribe@perl.org and replying to the confirmation email.

Users will also be prompted to enter the name of an outbound email server.  It
is recommended to use an email server provided by the user's ISP or company.
Alternatively, leave this blank to attempt to send email directly to perl.org.

Users that are new to CPAN::Reporter should accept the recommended values
for other configuration options.

After completing interactive configuration, be sure to commit (save) the CPAN
configuration changes.

 cpan> o conf commit

See [CPAN::Reporter::Config] for advanced configuration settings.

== Using CPAN::Reporter

Once CPAN::Reporter is enabled and configured, test or install modules with
CPAN.pm as usual.  

For example, to force CPAN to repeat tests for CPAN::Reporter to see how it
works:

 cpan> force test CPAN::Reporter

When distribution tests fail, users will be prompted to edit the report to add
addition information.

= UNDERSTANDING TEST GRADES

CPAN::Reporter will assign one of the following grades to the report:

* {pass} -- all tests were successful  

* {fail} -- one or more tests failed, one or more test files died during
testing or no test output was seen

* {na} -- tests could not be run on this platform or version of perl

* {unknown} -- no test files could be found (either t/*.t or test.pl) or 
a result could not be determined from test output (e.g tests may have hung 
and been interrupted)

In returning results to CPAN.pm, "pass" and "unknown" are considered successful
attempts to "make test" or "Build test" and will not prevent installation.
"fail" and "na" are considered to be failures and CPAN.pm will not install
unless forced.

If prerequisites specified in {Makefile.PL} or {Build.PL} are not available,
no report will be generated and a failure will be signaled to CPAN.pm.

= PRIVACY WARNING

CPAN::Reporter includes information in the test report about environment
variables and special Perl variables that could be affecting test results in
order to help module authors interpret the results of the tests.  This includes
information about paths, terminal, locale, user/group ID, installed toolchain
modules (e.g. ExtUtils::MakeMaker) and so on.  

These have been intentionally limited to items that should not cause harmful
personal information to be revealed -- it does ~not~ include your entire
environment.  Nevertheless, please do not use CPAN::Reporter if you are
concerned about the disclosure of this information as part of your test report.  

Users wishing to review this information may choose to edit the report 
prior to sending it.

= BUGS

Please report any bugs or feature using the CPAN Request Tracker.  
Bugs can be submitted through the web interface at 
[http://rt.cpan.org/Dist/Display.html?Queue=CPAN-Reporter]

When submitting a bug or request, please include a test-file or a patch to an
existing test-file that illustrates the bug or desired feature.

= SEE ALSO

Information about CPAN::Testers:

* [CPAN::Testers] -- overview of CPAN Testers architecture stack
* [http://cpantesters.perl.org] -- project home with all reports
* [http://cpantest.grango.org] -- documentation and wiki

Additional Documentation: 

* [CPAN::Reporter::Config] -- advanced configuration settings
* [CPAN::Reporter::FAQ] -- hints and tips

= AUTHOR

David A. Golden (DAGOLDEN)

= COPYRIGHT AND LICENSE

Copyright (c) 2006, 2007, 2008 by David A. Golden

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at 
[http://www.apache.org/licenses/LICENSE-2.0]

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

=end wikidoc

=cut
