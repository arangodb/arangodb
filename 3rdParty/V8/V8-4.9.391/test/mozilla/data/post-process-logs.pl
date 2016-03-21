#!/usr/bin/perl -w
# -*- Mode: Perl; tab-width: 4; indent-tabs-mode: nil; -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla JavaScript Testing Utilities
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s): Bob Clary <bclary@bclary.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

use Getopt::Mixed "nextOption";
use File::Temp qw/ tempfile tempdir /;
use File::Basename;

sub dbg;
sub outresults;
sub outputrecord;

local $file;
local $temp;
my $debug = $ENV{DEBUG};
my $test_dir = $ENV{TEST_DIR};

die "FATAL ERROR: environment variable TEST_DIR must be set to the Sisyphus root directory" unless ($test_dir);

# required for mac os x 10.5 to prevent sort from 
# complaining about illegal byte sequences
$ENV{LC_ALL} = 'C';

(undef, $temp) = tempfile();

open TEMP, ">$temp" or
    die "FATAL ERROR: Unable to open temporary file $temp for writing: $!\n";

local ($test_id,
       $tmp_test_id,
       $tmp_test_exit_status,
       %test_id,
       %test_reported,
       $test_result, 
       $test_type, 
       $tmp_test_type,
       $test_description, 
       $test_jsoptions,
       @messages,
       $test_processortype, 
       $test_kernel, 
       $test_suite,
       $test_exit_status,
       @expected_exit_code_list,
       $expected_exit_code,
       $exit_code,
       $state);

local $test_memory = 0;
local %test_reported = ();
local $test_repo = 'CVS';

$test_jsoptions = 'none';

while ($file = shift @ARGV)
{
    @messages = ();

    dbg "file: $file";

    my $filename = basename $file;

    dbg "filename: $file";

    local ($test_date, $test_product, $test_branchid, $test_buildtype,
           $test_os,
           $test_machine,$test_global_target) = split /,/, $filename;

    $test_branchid =~ s/[^0-9.]//g;
    $test_global_target =~ s/.log$//;

    local ($test_timezone) = $test_date;
    $test_timezone =~ s/.*([-+]\d{4,4})/$1/;

    my $filemode;

    if ($file =~ /\.bz2$/)
    {
        $filemode = "bzcat $file|";
    }
    elsif ($file =~ /\.gz$/)
    {
        $filemode = "zcat $file|";
    }
    else
    {
        $filemode = "<$file";
    }

    open FILE, "$filemode" or die "FATAL ERROR: unable to open $file for reading: $!\n";

    dbg "process header with environment variables used in test";

    while (<FILE>)
    {
        $state = 'failure';

        chomp;

        # remove carriage returns, bels and other annoyances.
        $_ =~ s/[\r]$//;
        $_ =~ s/[\r]/CR/g;
        $_ =~ s/[\x01-\x08]//g;
        $_ =~ s/\s+$//;

        if ($debug)
        {
            dbg "\nINPUT: $_";
        }

        last if ( $_ =~ /^include:/);

        if (($envval) = $_ =~ /^environment: TEST_MOZILLA_HG=http:\/\/hg.mozilla.org.*\/([^\/]+)/ )
        {
            $test_repo = $envval;
        }
        elsif (($envvar, $envval) = $_ =~ /^environment: (TEST_[A-Z0-9_]*)=(.*)/ )
        {
            dbg "envvar=$envvar, envval=$envval";
            if ($envvar =~ /TEST_KERNEL/)
            {
                $envval =~ s/([0-9]+)\.([0-9]+)\.([0-9]+).*/$1.$2.$3/;
                dbg "found TEST_KERNEL";
            }
            $envvar =~ tr/A-Z/a-z/;
            $$envvar = $envval;
            dbg $envvar . "=" . $$envvar;
        }
        elsif (($envval) = $_ =~ /^environment: OSID=(.*)/ )
        {
            $test_os = $envval;
        }

        if ($_ =~ /^arguments: javascriptoptions/)
        {
            my ($o, @s, $j);

            ($o) = $_ =~ /^arguments: javascriptoptions=(.*)/;
            $o =~ s/(-\w) (\w)/$1$2/g; 
            @s = sort split / /, $o; 
            $j = join(" ", @s); 
            $j =~ s/(-\w)(\w)/$1 $2/g; 

            $test_jsoptions = $j || "none";
            dbg "javascriptoptions: $test_jsoptions";
        }
    }

    if ($test_product eq "js")
    {
        $test_type       = "shell";
    }
    elsif ($test_product eq "firefox" || $test_product eq "thunderbird" ||
        $test_product eq "fennec")
    {
        $test_buildtype  = "nightly" unless $test_buildtype;
        $test_type       = "browser";
    }

#  Expected sequence if all output written to the log.
#
#    Input                     
# -----------------------------
# JavaScriptTest: Begin Run    
# JavaScriptTest: Begin Test t;
# jstest: t                    
# t:.*EXIT STATUS:             
# JavaScriptTest: End Test t   
# JavaScriptTest: End Run      
# EOF                          
# 
    %test_id         = ();
    @messages        = ();
    $test_exit_status = '';
    $state           = 'idle';

    while (<FILE>)
    {
        chomp;

        if ($debug)
        {
            dbg "\nINPUT: '$_'";
        }

        $_ =~ s/[\r]$//;
        $_ =~ s/[\r]/CR/g;
        $_ =~ s/[\x01-\x08]//g;
        $_ =~ s/\s+$//;

        if ( /^JavaScriptTest: Begin Run/)
        {
            dbg "Begin Run";

            if ($state eq 'idle')
            {
                $state = 'beginrun';
            }
            else
            {
                warn "WARNING: state: $state, expected: idle, log: $file";
                $state = 'beginrun';
            }
        }
        elsif ( ($tmp_test_id) = $_ =~ /^JavaScriptTest: Begin Test ([^ ]*)/)
        {
            dbg "Begin Test: $tmp_test_id";

            if ($state eq 'beginrun' || $state eq 'endtest')
            {
                $state = 'runningtest';
            }
            else
            {
                warn "WARNING: state: $state, expected: beginrun, endtest, log: $file";
                $state = 'runningtest';
            }

            $test_id{$state}         = $tmp_test_id;
            @messages                = ();
            @expected_exit_code_list = ();
            $expected_exit_code      = ();

            $test_id          = '';
            $test_result      = '';
            $test_exit_status = 'NORMAL'; # default to normal, so subtests will have a NORMAL status
            $test_description = '';

            push @expected_exit_code_list, (3) if ($tmp_test_id =~ /-n.js$/);
            
        }
        elsif ( ($expected_exit_code) = $_ =~ /WE EXPECT EXIT CODE ([0-9]*)/ ) 
        {
            dbg "Expected Exit Code: $expected_exit_code";

            push @expected_exit_code_list, ($expected_exit_code);
        }
        elsif ( ($tmp_test_id) = $_ =~ /^jstest: (.*?) *bug:/)
        {
            dbg "jstest: $tmp_test_id";

#            if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
#            {
#                warn "WARNING: state: $state, expected runningtest, reportingtest. mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
#            }

            if ($state eq 'runningtest')
            {
                $state = 'reportingtest';
            }
            elsif ($state eq 'reportingtest')
            {
                $state = 'reportingtest';
            }
            else
            {
                warn "WARNING: test_id: $test_id{$state}, state: $state, expected: runningtest, reportingtest, log: $file";
                $state = 'reportingtest';
            }

            ($test_result)      = $_ =~ /result: (.*?) *type:/;
            ($tmp_test_type)    = $_ =~ /type: (.*?) *description:/;

            die "FATAL ERROR: test_id: $test_id{$state}, jstest test type mismatch: start test_type: $test_type, current test_type: $tmp_test_type, test state: $state, log: $file" 
                if ($test_type ne $tmp_test_type);

            ($test_description) = $_ =~ /description: (.*)/;

            if (!$test_description)
            {
                $test_description = "";
            }
            $test_description .= '; messages: ' . (join '; ', @messages) . ';';

            outputrecord $tmp_test_id, $test_description, $test_result;

            $test_id{$state} = $tmp_test_id;
        }
        elsif ( $state ne 'idle' && (($tmp_test_id) = $_ =~ /^([^:]*):.* EXIT STATUS: NORMAL/))
        {
            $test_exit_status = 'NORMAL';
            dbg "Exit Status Normal: $tmp_test_id, $test_exit_status";

            if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
            {
                warn "WARNING: state: $state, mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
            }

            if ($state eq 'reportingtest' || $state eq 'runningtest')
            {
                $state = 'exitedtest';
            }
            else
            {
                warn "WARNING: state: $state, expected: reportingtest, runningtest, log: $file";
                $state = 'exitedtest';
            }

            if (! $test_reported{$tmp_test_id})
            {
                dbg "No test results reported: $tmp_test_id";

                $test_result = 'FAILED';
                $test_description = 'No test results reported; messages: ' . (join '; ', @messages) . ';';
                
                outputrecord $tmp_test_id, $test_description, $test_result;
            }

            $test_id{$state} = $tmp_test_id;
        }
        elsif ( $state ne 'idle' && (($tmp_test_id) = $_ =~ /^([^:]*):.* EXIT STATUS: TIMED OUT/))
        {
            $test_exit_status = 'TIMED OUT';
            dbg "Exit Status Timed Out: $tmp_test_id, $test_exit_status";

            if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
            {
                warn "WARNING: state: $state, mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
            }

            if ($state eq 'reportingtest' || $state eq 'runningtest')
            {
                $state = 'exitedtest';
            }
            else
            {
                dbg "state: $state, expected: reportingtest, runningtest";
                $state = 'exitedtest';
            }

            $test_result     = 'FAILED';
            $test_description .= '; messages: ' . (join '; ', @messages) . ';';
            
            outputrecord $tmp_test_id, $test_description, $test_result;

            $test_id{$state} = $tmp_test_id;
        }
        elsif ( $state ne 'idle' && (($tmp_test_id, $tmp_test_exit_status) = $_ =~ /^([^:]*):.* EXIT STATUS: (CRASHED signal [0-9]+ [A-Z]+) \([0-9.]+ seconds\)/))
        {
            $test_exit_status = $tmp_test_exit_status;
            dbg "Exit Status Crashed: $tmp_test_id, $test_exit_status";

            if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
            {
                warn "WARNING: state: $state, mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
            }

            if ($state eq 'reportingtest' || $state eq 'runningtest')
            {
                $state = 'exitedtest';
            }
            else
            {
                dbg "state: $state, expected: reportingtest, runningtest";
                $state = 'exitedtest';
            }

            $test_result     = 'FAILED';
            $test_description .= '; messages: ' . (join '; ', @messages) . ';';
            
            outputrecord $tmp_test_id, $test_description, $test_result;

            $test_id{$state} = $tmp_test_id;
        }
        elsif ( $state ne 'idle' && (($tmp_test_id, $tmp_test_exit_status) = $_ =~ /^([^:]*):.* EXIT STATUS: (ABNORMAL [0-9]+) \([0-9.]+ seconds\)/))
        {
            $test_exit_status = $tmp_test_exit_status;
            dbg "Exit Status Abnormal: $tmp_test_id, $test_exit_status";

            if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
            {
                warn "WARNING: state: $state, mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
            }

            if ($state eq 'reportingtest'  || $state eq 'runningtest')
            {
                $state = 'exitedtest';
            }
            else
            {
                dbg "state: $state, expected: reportingtest, runningtest";
                $state = 'exitedtest';
            }

            ($exit_code) = $test_exit_status =~ /ABNORMAL ([0-9]+)/;

            if (grep /$exit_code/, @expected_exit_code_list)
            {
                $test_result = 'PASSED';
            }
            else
            {
                $test_result = 'FAILED';
            }

            $test_description .= '; messages: ' . (join '; ', @messages) . ';';

            dbg "Exit Code: $exit_code, Test Result: $test_result, Expected Exit Codes: " . (join '; ', @expected_exit_code_list);

            outputrecord $tmp_test_id, $test_description, $test_result;

            $test_id{$state} = $tmp_test_id;
        }
        elsif ( ($tmp_test_id) = $_ =~ /^JavaScriptTest: End Test ([^ ]*)/)
        {
            dbg "End Test: $tmp_test_id";

            if ($test_id{$state} && $tmp_test_id ne $test_id{$state})
            {
                warn "WARNING: state: $state, mismatched test_id: expected: $tmp_test_id, actual: $test_id{$state}, log: $file";
            }

            if ($state eq 'exitedtest' || $state eq 'runningtest' || $state eq 'reportingtest')
            {
                $state = 'endtest';
            }
            else
            {
                warn "WARNING: state: $state, expected: runningtest, reportingtest, exitedtest, log: $file";
                $state = 'endtest';
            }

            $test_id{$state} = $tmp_test_id;
        }
        elsif ( /^JavaScriptTest: End Run/)
        {
            dbg "End Run";

            if ($state eq 'endtest')
            {
                $state = 'endrun';
            }
            else
            {
                warn "WARNING: state: $state, expected: endtest, log: $file";
                $state = 'endrun';
            }
        }
        elsif ($_ && 
               !/^\s+$/ && 
               !/^(STATUS:| *PASSED!| *FAILED!)/ && 
               !/^JavaScriptTest:/ && 
               !/^[*][*][*]/ && 
               !/^[-+]{2,2}(WEBSHELL|DOMWINDOW)/ && 
               !/^Spider:/ && 
               !/real.*user.*sys.*$/ && 
               !/user.*system.*elapsed/)
        {
            if ('runningtest, reportingtest' =~ /$state/ && $#messages < 1000)
            {
                # limit the number of processed and collected messages since firefox can
                # go berserk and dump a couple of million output lines for a single test
                # if things go horribly wrong.
                if (/error: can.t allocate region/ || /set a breakpoint in malloc_error_break/ || 
                    /set a breakpoint in szone_error to debug/ || /malloc:.*mmap/ || /vm_allocate/ ||
                    /terminate called after throwing an instance of .*bad_alloc/)
                {
                    dbg "Adding message: $_ converted to /$test_id{$state}:0: out of memory";
                    push @messages, ('/' . $test_id{$state} . ':0: out of memory');
                }
                elsif (/\.js, line [0-9]+: out of memory/ )
                {
                    s/\.js, line ([0-9]+): out of memory/\.js:$1:/;
                    dbg "Adding message: $_ converted to /$test_id{$state}:0: out of memory";
                    push @messages, ('/' . $test_id{$state} . ':0: out of memory');
                }
                else
                {
                    dbg "Adding message: $_";
                    push @messages, ($_);
                }
            }
        }
        elsif ($debug)
        {
            dbg "Skipping: $_";
        } 

        if ($debug)
        {
            if ($test_id{$state})
            {
                dbg "test_id{$state}=$test_id{$state}, " . (join '; ', @messages);
            }
            else
            {
                dbg "state=$state, " . (join '; ', @messages);
            }
        }
    }
    if ($state eq 'endrun')
    {
        $state = 'success';
    }
    die "FATAL ERROR: Test run terminated prematurely. state: $state, log: $file" if ($state ne 'success');

}
close FILE;
close TEMP;

undef $test_branchid;
undef $test_date;
undef $test_buildtype;
undef $test_machine;
undef $test_product;
undef $test_suite;

outresults;

unlink $temp;

sub dbg {
    if ($debug)
    {
        my $msg = shift;
        print STDERR "DEBUG: $msg\n";
    }
}

sub outresults
{
    dbg "sorting temp file $temp";
    system("sort -u < $temp");
    dbg "finished sorting";
}

sub outputrecord
{
    my ($test_id, $test_description, $test_result) = @_;

    # cut off the extra jstest: summaries as they duplicate the other
    # output and follow it.
    $test_description =~ s/jstest:.*//;

#    if (length($test_description) > 6000)
#    {
#        $test_description = substr($test_description, 0, 6000);
#    }
#

    my $output = 
        "TEST_ID=$test_id, " .
        "TEST_BRANCH=$test_branchid, " .
        "TEST_REPO=$test_repo, " .
        "TEST_BUILDTYPE=$test_buildtype, " .
        "TEST_TYPE=$test_type, " .
        "TEST_OS=$test_os, " .
        "TEST_KERNEL=$test_kernel, " .
        "TEST_PROCESSORTYPE=$test_processortype, " .
        "TEST_MEMORY=$test_memory, " .
        "TEST_TIMEZONE=$test_timezone, " . 
        "TEST_OPTIONS=$test_jsoptions, " .
        "TEST_RESULT=$test_result, " .
        "TEST_EXITSTATUS=$test_exit_status, " .
        "TEST_DESCRIPTION=$test_description, " .
        "TEST_MACHINE=$test_machine, " .
        "TEST_DATE=$test_date" .
        "\n";

    if ($debug)
    {
        dbg "RECORD: $output";
    }
    print TEMP $output;

    $test_reported{$test_id} = 1;
    @messages = ();
}
