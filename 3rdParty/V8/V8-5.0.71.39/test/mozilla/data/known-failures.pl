#!/usr/bin/perl
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

use strict;
use Getopt::Mixed "nextOption";

# predeclarations
sub debug;
sub usage;
sub parse_options;
sub escape_string;
sub escape_pattern;
sub unescape_pattern;

# option arguments

my $option_desc = "b=s branch>b T=s buildtype>T R=s repo>R t=s testtype>t o=s os>o K=s kernel>K A=s arch>A M=s memory>M z=s timezone>z J=s jsoptions>J l=s rawlogfile>l f=s failurelogfile>f r=s patterns>r O=s outputprefix>O D debug>D";

my $testid;
my $branch;
my $repo;
my $buildtype;
my $testtype;
my $rawlogfile;
my $failurelogfile;
my $os;
my $patterns;
my $timezone;
my $jsoptions;
my $outputprefix;
my $arch;
my $kernel;
my $memory;
my $debug = $ENV{DEBUG};

# pattern variables

my $knownfailurebranchpattern;
my $failurebranchpattern;
my $knownfailureospattern;
my $failureospattern;
my $knownfailurerepopattern;
my $failurerepopattern;
my $knownfailurebuildtypepattern;
my $failurebuildtypepattern;
my $knownfailuretesttypepattern;
my $failuretesttypepattern;
my $knownfailuretimezonepattern;
my $failuretimezonepattern;
my $knownfailurejsoptionspattern;
my $failurejsoptionspattern;
my $knownfailurearchpattern;
my $failurearchpattern;
my $knownfailurekernelpattern;
my $failurekernelpattern;
my $knownfailurememorypattern;
my $failurememorypattern;

my @patterns;
my $pattern;
my @failures;
my @fixes;
my @excludedtests;
my $excludedtest;
my $excludedfile;
my %includedtests = {};
my $includedfile;
my @results;

my $regchars = '\[\^\-\]\|\{\}\?\*\+\.\<\>\$\(\)';


&parse_options;

my $jsdir = $ENV{TEST_JSDIR};

if (!defined($jsdir)) {
     $jsdir = "/work/mozilla/mozilla.com/test.mozilla.com/www/tests/mozilla.org/js";
}

my @excludedfiles = ("excluded-$branch-$testtype-$buildtype.tests");
my @includedfiles = ("included-$branch-$testtype-$buildtype.tests");

 # create working patterns file consisting of matches to users selection
 # and which has the test description patterns escaped

 # remove the excluded tests from the possible fixes log


foreach $excludedfile ( @excludedfiles ) {
    open EXCLUDED, "<$jsdir/$excludedfile" or die "Unable to open excluded file $jsdir/$excludedfile: $!\n";
    while (<EXCLUDED>) {
        chomp;

        next if ($_ =~ /^\#/);

        s/\s+$//;

        push @excludedtests, ($_);
    }
    close EXCLUDED;
}

@excludedtests = sort @excludedtests;

foreach $includedfile ( @includedfiles ) {
    open INCLUDED, "<$jsdir/$includedfile" or die "Unable to open included file $jsdir/$includedfile: $!\n";
    while (<INCLUDED>) {
        chomp;

        next if ($_ =~ /^\#/);

        s/\s+$//;

        $includedtests{$_} = 1;
    }
    close INCLUDED;
}

debug "loading patterns $patterns";
debug "pattern filter: ^TEST_ID=[^,]*, TEST_BRANCH=$knownfailurebranchpattern, TEST_REPO=$knownfailurerepopattern, TEST_BUILDTYPE=$knownfailurebuildtypepattern, TEST_TYPE=$knownfailuretesttypepattern, TEST_OS=$knownfailureospattern, TEST_KERNEL=$knownfailurekernelpattern, TEST_PROCESSORTYPE=$knownfailurearchpattern, TEST_MEMORY=$knownfailurememorypattern, TEST_TIMEZONE=$knownfailuretimezonepattern, TEST_OPTIONS=$knownfailurejsoptionspattern,";

open PATTERNS, "<$patterns" or die "Unable to open known failure patterns file $patterns: $!\n";
while (<PATTERNS>) {
    chomp;

    s/\s+$//;

    ($testid) = $_ =~ /^TEST_ID=([^,]*),/;

    if (!$includedtests{$testid})
    {
        debug "test $testid was not included during this run";
    }
    elsif ($_ =~ /^TEST_ID=[^,]*, TEST_BRANCH=$knownfailurebranchpattern, TEST_REPO=$knownfailurerepopattern, TEST_BUILDTYPE=$knownfailurebuildtypepattern, TEST_TYPE=$knownfailuretesttypepattern, TEST_OS=$knownfailureospattern, TEST_KERNEL=$knownfailurekernelpattern, TEST_PROCESSORTYPE=$knownfailurearchpattern, TEST_MEMORY=$knownfailurememorypattern, TEST_TIMEZONE=$knownfailuretimezonepattern, TEST_OPTIONS=$knownfailurejsoptionspattern,/) {
        debug "adding pattern  : $_";
        push @patterns, (escape_pattern($_));   
    }
    else {
        debug "skipping pattern: $_";
    }

}
close PATTERNS;

 # create a working copy of the current failures which match the users selection

debug "failure filter: ^TEST_ID=[^,]*, TEST_BRANCH=$failurebranchpattern, TEST_REPO=$failurerepopattern, TEST_BUILDTYPE=$failurebuildtypepattern, TEST_TYPE=$failuretesttypepattern, TEST_OS=$failureospattern, TEST_KERNEL=$failurekernelpattern, TEST_PROCESSORTYPE=$failurearchpattern, TEST_MEMORY=$failurememorypattern, TEST_TIMEZONE=$failuretimezonepattern, TEST_OPTIONS=$failurejsoptionspattern, TEST_RESULT=FAIL[^,]*,/";

if (defined($rawlogfile)) {

    $failurelogfile = "$outputprefix-results-failures.log";
    my $alllog      = "$outputprefix-results-all.log";

    debug "writing failures $failurelogfile";

    open INPUTLOG, "$jsdir/post-process-logs.pl $rawlogfile |" or die "Unable to open $rawlogfile $!\n";
    open ALLLOG, ">$alllog" or die "Unable to open $alllog $!\n";
    open FAILURELOG, ">$failurelogfile" or die "Unable to open $failurelogfile $!\n";

    while (<INPUTLOG>) {
        chomp;

        print ALLLOG "$_\n";

        if ($_ =~ /^TEST_ID=[^,]*, TEST_BRANCH=$failurebranchpattern, TEST_REPO=$failurerepopattern, TEST_BUILDTYPE=$failurebuildtypepattern, TEST_TYPE=$failuretesttypepattern, TEST_OS=$failureospattern, TEST_KERNEL=$failurekernelpattern, TEST_PROCESSORTYPE=$failurearchpattern, TEST_MEMORY=$failurememorypattern, TEST_TIMEZONE=$failuretimezonepattern, TEST_OPTIONS=$failurejsoptionspattern, TEST_RESULT=FAIL[^,]*,/) {
            debug "failure: $_";
            push @failures, ($_);
            print FAILURELOG "$_\n";
        }
    }
    close INPUTLOG;
    my $inputrc = $?;
    close ALLLOG;
    close FAILURELOG;

    die "FATAL ERROR in post-process-logs.pl" if $inputrc != 0;
}
else 
{
    debug "loading failures $failurelogfile";

    my $failurelogfilemode;

    if ($failurelogfile =~ /\.bz2$/)
    {
        $failurelogfilemode = "bzcat $failurelogfile|";
    }
    elsif ($failurelogfile =~ /\.gz$/)
    {
        $failurelogfilemode = "zcat $failurelogfile|";
    }
    else
    {
        $failurelogfilemode = "<$failurelogfile";
    }

    open FAILURES, "$failurelogfilemode" or die "Unable to open current failure log $failurelogfile: $!\n";
    while (<FAILURES>) {
        chomp;

        if ($_ =~ /^TEST_ID=[^,]*, TEST_BRANCH=$failurebranchpattern, TEST_REPO=$failurerepopattern, TEST_BUILDTYPE=$failurebuildtypepattern, TEST_TYPE=$failuretesttypepattern, TEST_OS=$failureospattern, TEST_KERNEL=$failurekernelpattern, TEST_PROCESSORTYPE=$failurearchpattern, TEST_MEMORY=$failurememorypattern, TEST_TIMEZONE=$failuretimezonepattern, TEST_OPTIONS=$failurejsoptionspattern, TEST_RESULT=FAIL[^,]*,/) {
            debug "failure: $_";
            push @failures, ($_);
        }
    }
    close FAILURES;
}

debug "finding fixed bugs";

unlink "$outputprefix-results-possible-fixes.log";

foreach $pattern (@patterns) {
    # look for known failure patterns that don't have matches in the 
    # the current failures selected by the user.

    debug "searching for matches to $pattern\n";

    @results = grep m@^$pattern@, @failures;

    if ($debug) {
        my $failure;
        foreach $failure (@failures) {
            if ($failure =~ $pattern) {
                debug "MATCH: $pattern - $failure\n";
            }
            else {
                debug "NOMATCH: $pattern - $failure\n";
            }
        }
    }
    if ($#results == -1) {
        debug "fix: '$pattern'";
        push @fixes, ($pattern)
    }
}

foreach $excludedtest ( @excludedtests ) {
    # remove any potential fixes which are due to the test being excluded

    if ($debug) {
        @results = grep m@$excludedtest@, @fixes;
        if ($#results > -1) {
            print "excluding: " . (join ', ', @results) . "\n";
        }
    }

    @results = grep !m@$excludedtest@, @fixes;

    @fixes = @results;
}

my $fix;
open OUTPUT, ">$outputprefix-results-possible-fixes.log" or die "Unable to open $outputprefix-results-possible-fixes.log: $!";
foreach $fix (@fixes) {
    print OUTPUT unescape_pattern($fix) . "\n";
    if ($debug) {
        debug "fix: $fix";
    }
}
close OUTPUT;

print STDOUT "log: $outputprefix-results-possible-fixes.log\n";

debug "finding regressions";

my $pass = 0;
my $changed = ($#patterns != -1);

debug "changed=$changed, \$#patterns=$#patterns, \$#failures=$#failures";

while ($changed) {

    $pass = $pass + 1;

    $changed = 0;

    debug "pass $pass";

    foreach $pattern (@patterns) {

        debug "Pattern: $pattern";

        my @nomatches = grep !m@^$pattern@, @failures;
        my @matches   = grep m@^$pattern@, @failures;

        if ($debug) {
            my $temp = join ', ', @nomatches;
            debug "nomatches: $#nomatches $temp";
            $temp = join ', ', @matches;
            debug "matches: $#matches $temp";
        }

        @failures = @nomatches;

        if ($#matches > -1) {
            $changed = 1;
        }

        debug "*****************************************";
    }

}

debug "\$#excludedtests=$#excludedtests, \$#failures=$#failures";

foreach $excludedtest ( @excludedtests ) {

    if ($debug) {
        @results = grep m@$excludedtest@, @failures;
        if ($#results > -1) {
            print "excluding: " . (join ', ', @results) . "\n";
        }
    }

    @results = grep !m@$excludedtest@, @failures;

    debug "\$#results=$#results, \$excludedtest=$excludedtest, \$#failures=$#failures";

    @failures = @results;
}

debug "possible regressions: \$#failures=$#failures";

open OUTPUT, ">$outputprefix-results-possible-regressions.log" or die "Unable to open $outputprefix-results-possible-regressions.log: $!";

my $failure;
foreach $failure (@failures) {
    print OUTPUT "$failure\n";
    if ($debug) {
        debug "regression: $failure";
    }
}
close OUTPUT;

print STDOUT "log: $outputprefix-results-possible-regressions.log\n";


sub debug {
    if ($debug) {
        my $msg = shift;
        print STDERR "DEBUG: $msg\n";
    }
}

sub usage {

    my $msg = shift;

    print STDERR <<EOF;

usage: $msg

known-failures.pl [-b|--branch] branch 
                  [-T|--buildtype] buildtype 
                  [-t|--testtype] testtype 
                  [-o|--os] os
                  [-K|--kernel] kernel
                  [-A|--arch] arch
                  [-M|--memory] memory
                  [-z|--timezone] timezone 
                  [-J|--jsoptions] jsoptions
                  [-r|--patterns] patterns 
                  ([-f|--failurelogfile] failurelogfile|[-l|--logfile] rawlogfile])
                  [-O|--outputprefix] outputprefix
                  [-D]

    variable            description
    ===============     ============================================================
    -b branch           branch 1.8.0, 1.8.1, 1.9.0, all
    -R repository       CVS for 1.8.0, 1.8.1, 1.9.0 branches, 
                        mercurial repository name for 1.9.1 and later branches
                        (\`basename http://hg.mozilla.org/repository\`)
    -T buildtype        build type opt, debug, all
    -t testtype         test type browser, shell, all
    -o os               operating system nt, darwin, linux, all
    -K kernel           kernel, all or a specific pattern
    -A arch             architecture, all or a specific pattern
    -M memory           memory in Gigabytes, all or a specific pattern
    -z timezone         -0400, -0700, etc. default to user\'s zone
    -J jsoptions        JavaScript options
    -l rawlogfile       raw logfile
    -f failurelogfile   failure logfile
    -r patterns         known failure patterns
    -O outputprefix     output files will be generated with this prefix
    -D                  turn on debugging output
EOF

    exit(2);
}

sub parse_options {
    my ($option, $value);

    Getopt::Mixed::init ($option_desc);
    $Getopt::Mixed::order = $Getopt::Mixed::RETURN_IN_ORDER;

    while (($option, $value) = nextOption()) {

        if ($option eq "b") {
            $branch = $value;
        }
        elsif ($option eq "R") {
            $repo = $value;
        }
        elsif ($option eq "T") {
            $buildtype = $value;
        }
        elsif ($option eq "t") {
            $testtype = $value;
        }
        elsif ($option eq "o") {
            $os = $value;
        }
        elsif ($option eq "K") {
            $kernel = $value;
        }
        elsif ($option eq "A") {
            $arch = $value;
        }
        elsif ($option eq "M") {
            $memory = $value;
        }
        elsif ($option eq "z") {
            $timezone = $value;
        }
        elsif ($option eq "J") {
            my (@s, $j);

            if (! $value) {
                $jsoptions = 'none';
            }
            else {
                $value =~ s/(-\w) (\w)/$1$2/g; 
                @s = sort split / /, $value; 
                $j = join(" ", @s); 
                $j =~ s/(-\w)(\w)/$1 $2/g; 
                $jsoptions = $j;
            }
        }
        elsif ($option eq "r") {
            $patterns = $value;
        }
        elsif ($option eq "l") {
            $rawlogfile = $value;
        }
        elsif ($option eq "f") {
            $failurelogfile = $value;
        }
        elsif ($option eq "O") {
            $outputprefix = $value;
        }
        elsif ($option eq "D") {
            $debug = 1;
        }

    }

    if ($debug) {
        print "branch=$branch, buildtype=$buildtype, testtype=$testtype, os=$os, kernel=$kernel, arch=$arch, memory=$memory, timezone=$timezone, jsoptions=$jsoptions, patterns=$patterns, rawlogfile=$rawlogfile failurelogfile=$failurelogfile, outputprefix=$outputprefix\n";
    }
    Getopt::Mixed::cleanup();

    if ( !defined($branch) ) {
        usage "missing branch";
    }

    if (!defined($buildtype)) {
        usage "missing buildtype";
    }

    if (!defined($testtype)) {
        usage "missing testtype";
    }

    if (!defined($os)) { 
        usage "missing os";
    }

    if (!defined($memory)) {
        $memory = 'all';
    }

    if (!defined($timezone)) {
        usage "missing timezone";
    }

    if (!defined($jsoptions)) {
        $jsoptions = 'none';
    }

    if (!defined($patterns)) {
        usage "missing patterns";
    }

    if (!defined($rawlogfile) && !defined($failurelogfile)) {
        usage "missing logfile";
    }

    if (!defined($outputprefix)) {
        usage "missing outputprefix";
    }

    if ($branch eq "all") {
        $knownfailurebranchpattern = "[^,]*";
        $failurebranchpattern      = "[^,]*";
    }
    else {
        $knownfailurebranchpattern = "($branch|.*)";
        $knownfailurebranchpattern =~ s/\./\\./g;

        $failurebranchpattern = "$branch";
        $failurebranchpattern =~ s/\./\\./g;
    }

    if ($repo eq "all" || $repo eq ".*") {
        $knownfailurerepopattern = "[^,]*";
        $failurerepopattern      = "[^,]*";
    }
    else {
        $knownfailurerepopattern = "($repo|\\.\\*)";
        $failurerepopattern      = "$repo";
    }

    if ($buildtype eq "opt") {
        $knownfailurebuildtypepattern = "(opt|\\.\\*)";
        $failurebuildtypepattern      = "opt";
    }
    elsif ($buildtype eq "debug") {
        $knownfailurebuildtypepattern = "(debug|\\.\\*)";
        $failurebuildtypepattern      = "debug";
    }
    elsif ($buildtype eq "all") {
        $knownfailurebuildtypepattern = "[^,]*";
        $failurebuildtypepattern      = "[^,]*";
    }

    if ($testtype eq "shell") {
        $knownfailuretesttypepattern = "(shell|\\.\\*)";
        $failuretesttypepattern      = "shell";
    }
    elsif ($testtype eq "browser") {
        $knownfailuretesttypepattern = "(browser|\\.\\*)";
        $failuretesttypepattern      = "browser";
    }
    elsif ($testtype eq "all") {
        $knownfailuretesttypepattern = "[^,]*";
        $failuretesttypepattern      = "[^,]*";
    }

    if ($os eq "nt") {
        $knownfailureospattern     = "(nt|\\.\\*)";
        $failureospattern          = "nt";
    }
    elsif ($os eq "darwin") {
        $knownfailureospattern     = "(darwin|\\.\\*)";
        $failureospattern          = "darwin";
    }
    elsif ($os eq "linux") {
        $knownfailureospattern     = "(linux|\\.\\*)";
        $failureospattern          = "linux";
    }
    elsif ($os eq "all") {
        $knownfailureospattern     = "[^,]*";
        $failureospattern          = "[^,]*";
    }

    if ($kernel ne  "all") {
        $knownfailurekernelpattern = "(" . $kernel . "|\\.\\*)";
        $failurekernelpattern      = "$kernel";
    }
    else {
        $knownfailurekernelpattern = "[^,]*";
        $failurekernelpattern      = "[^,]*";
    }

    if ($arch ne "all") {
        $knownfailurearchpattern = "(" . $arch . "|\\.\\*)";
        $failurearchpattern      = "$arch";
    }
    else {
        $knownfailurearchpattern = "[^,]*";
        $failurearchpattern      = "[^,]*";
    }

    if ($memory ne  "all") {
        $knownfailurememorypattern = "(" . $memory . "|\\.\\*)";
        $failurememorypattern      = "$memory";
    }
    else {
        $knownfailurememorypattern = "[^,]*";
        $failurememorypattern      = "[^,]*";
    }

    if ($timezone eq "all") {
        $knownfailuretimezonepattern = "[^,]*";
        $failuretimezonepattern      = "[^,]*";
    }
    else {
        $knownfailuretimezonepattern = "(" . escape_string($timezone) . "|\\.\\*)";
        $failuretimezonepattern      = escape_string("$timezone");
    }

    if ($jsoptions eq "all") {
        $knownfailurejsoptionspattern = "[^,]*";
        $failurejsoptionspattern      = "[^,]*";
    }
    else {
        $knownfailurejsoptionspattern = "(" . escape_string($jsoptions) . "|\\.\\*)";
        $failurejsoptionspattern      = escape_string("$jsoptions");
    }

}

sub escape_string {
    my $s = shift;

    # replace unescaped regular expression characters in the 
    # string so they are not interpreted as regexp chars
    # when matching descriptions. leave the escaped regexp chars
    # `regexp` alone so they can be unescaped later and used in 
    # pattern matching.

    # see perldoc perlre

    $s =~ s/\\/\\\\/g;

    # escape non word chars that aren't surrounded by ``
    $s =~ s/(?<!`)([$regchars])(?!`)/\\$1/g;
    $s =~ s/(?<!`)([$regchars])(?=`)/\\$1/g;
    $s =~ s/(?<=`)([$regchars])(?!`)/\\$1/g;

    # unquote the regchars
    $s =~ s/\`([^\`])\`/$1/g;

    debug "escape_string  : $s";

    return "$s";

}

sub escape_pattern {

    my $line = shift;

    chomp;

    my ($leading, $trailing) = $line =~ /(.*TEST_DESCRIPTION=)(.*)/;

    #    debug "escape_pattern: before: $leading$trailing";

    $trailing = escape_string($trailing);

    debug "escape_pattern  : $leading$trailing";

    return "$leading$trailing";

}

sub unescape_pattern {
    my $line = shift;

    chomp;

    my ($leading, $trailing) = $line =~ /(.*TEST_DESCRIPTION=)(.*)/;

    # quote the unescaped non word chars
    $trailing =~ s/(?<!\\)([$regchars])/`$1`/g;

    # unescape the escaped non word chars
    $trailing =~ s/\\([$regchars])/$1/g;

    $trailing =~ s/\\\\/\\/g;

    debug "unescape_pattern: after: $leading$trailing";

    return "$leading$trailing";
}

####


1;
