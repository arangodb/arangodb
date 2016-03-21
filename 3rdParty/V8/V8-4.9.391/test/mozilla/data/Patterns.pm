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
# Portions created by the Initial Developer are Copyright (C) 2008
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

package Patterns;

sub getuniversekey
{
    my ($machinerecord, $excludeduniversefield) = @_;
    my $i;
    my $key = '';

    if ($DEBUG) {
        dbg("getuniversekey: \$machinerecord=" . recordtostring($machinerecord) . ", \$excludeduniversefield=$excludeduniversefield");
    }

    for ($i = 0; $i < @universefields; $i++)
    {
        if ($DEBUG) {
            dbg("getuniversekey: \$universefields[$i]=$universefields[$i]");
        }

        if ($universefields[$i] ne $excludeduniversefield)
        {
            $key .= $machinerecord->{$universefields[$i]}
        }
    }

    if ($DEBUG) {
        dbg("getuniversekey=$key");
    }

    return $key;
}

sub getuniverse
{
    my ($universekey, $excludeduniversefield) = @_;
    my $i;
    my $value;
    my $testrun;
    my @universe = ();
    my %universehash = ();

    if ($DEBUG) {
        dbg("getuniverse: \$universekey=$universekey, \$excludeduniversefield=$excludeduniversefield");
    }

    for ($i = 0; $i < @testruns; $i++)
    {
        $testrun = $testruns[$i];
        if ($DEBUG) {
            dbg("getuniverse: \$testruns[$i]=" . recordtostring($testrun));
        }
        $testrununiversekey = getuniversekey($testrun, $excludeduniversefield);
        if ($DEBUG) {
            dbg("getuniverse: \$testrununiversekey=$testrununiversekey");
        }
        if ($testrununiversekey =~ /$universekey/)
        {
            if ($DEBUG) {
                dbg("getuniverse: matched \$testrununiversekey=$testrununiversekey to \$universekey=$universekey");
            }
            $value = $testrun->{$excludeduniversefield};
            
            if ($DEBUG) {
                dbg("getuniverse: \$testrun->{$excludeduniversefield}=$value");
            }

            if (! $universehash{$value} )
            {
                if ($DEBUG) {
                    dbg("getuniverse: pushing $value");
                }
                push @universe, ($value);
                $universehash{$value} = 1;
            }
        }
    }
    @universe = sort @universe;
    if ($DEBUG) {
        dbg("getuniverse=" . join(',', @universe));
    }
    return @universe;
}

sub recordtostring
{
    my ($record) = @_;
    my $j;
    my $line   = '';
    my $field;

    for ($j = 0; $j < @recordfields - 1; $j++)
    {
        $field = $recordfields[$j];
        if ($DEBUG) {
            dbg("recordtostring: \$field=$field, \$record->{$field}=$record->{$field}");
        }
        $line .= "$field=$record->{$field}, ";
    }
    $field = $recordfields[$#recordfields];
    if ($DEBUG) {
        dbg("recordtodtring: \$field=$field, \$record->{$field}= $record->{$field}");
    }
    $line .= "$field=$record->{$field}";

    return $line;
}

sub dumprecords
{
    my $record;
    my $line;
    my $prevline = '';
    my $i;

    if ($DEBUG) {
        dbg("dumping records");
    }

#    @records = sort sortrecords @records;

    for ($i = 0; $i < @records; $i++)
    {
        $record = $records[$i];
        $line   = recordtostring($record);
        if ($line eq $prevline)
        {
            dbg("DUPLICATE $line") if ($DEBUG);
        }
        else
        {
            print "$line\n";
            $prevline = $line;
        }
    }
}

sub sortrecords
{
    return recordtostring($a) cmp recordtostring($b);
}

sub dbg 
{
    if ($DEBUG)
    {
        print STDERR "DEBUG: " . join(" ", @_) . "\n";
    }
}

sub copyreference
{
    my ($fromreference) = @_;
    my $toreference = {};
    my $key;

    foreach $key (keys %{$fromreference})
    {
        $toreference->{$key} = $fromreference->{$key};
    }
    return $toreference;
}

BEGIN 
{
    dbg("begin");

    my $test_dir = $ENV{TEST_DIR} || "/work/mozilla/mozilla.com/test.mozilla.com/www";

    $DEBUG = $ENV{DEBUG};

    @recordfields   = ('TEST_ID', 'TEST_BRANCH', 'TEST_REPO', 'TEST_BUILDTYPE', 'TEST_TYPE', 'TEST_OS', 'TEST_KERNEL', 'TEST_PROCESSORTYPE', 'TEST_MEMORY', 'TEST_TIMEZONE', 'TEST_OPTIONS', 'TEST_RESULT', 'TEST_EXITSTATUS', 'TEST_DESCRIPTION');
    @sortkeyfields  = ('TEST_ID', 'TEST_RESULT', 'TEST_EXITSTATUS', 'TEST_DESCRIPTION', 'TEST_BRANCH', 'TEST_REPO', 'TEST_BUILDTYPE', 'TEST_TYPE', 'TEST_OS', 'TEST_KERNEL', 'TEST_PROCESSORTYPE', 'TEST_MEMORY', 'TEST_TIMEZONE', 'TEST_OPTIONS');
    @universefields = ('TEST_BRANCH', 'TEST_REPO', 'TEST_BUILDTYPE', 'TEST_TYPE', 'TEST_OS', 'TEST_KERNEL', 'TEST_PROCESSORTYPE', 'TEST_MEMORY', 'TEST_TIMEZONE', 'TEST_OPTIONS');

    @records = ();

    @testruns = ();

    my $UNIVERSE = $ENV{TEST_UNIVERSE} || "$test_dir/tests/mozilla.org/js/universe.data";

    dbg "UNIVERSE=$UNIVERSE";

    open TESTRUNS, "<$UNIVERSE" or die "$?";

    while (<TESTRUNS>) {

        chomp;

        dbg("BEGIN: \$_=$_\n");

        my $record = {};

        my ($test_os, $test_kernel, $test_processortype, $test_memory, $test_timezone, $test_jsoptions, $test_branch, $test_repo, $test_buildtype, $test_type) = $_ =~ 
            /^TEST_OS=([^,]*), TEST_KERNEL=([^,]*), TEST_PROCESSORTYPE=([^,]*), TEST_MEMORY=([^,]*), TEST_TIMEZONE=([^,]*), TEST_OPTIONS=([^,]*), TEST_BRANCH=([^,]*), TEST_REPO=([^,]*), TEST_BUILDTYPE=([^,]*), TEST_TYPE=([^,]*)/;

        $record->{TEST_ID}            = 'dummy';
        $record->{TEST_RESULT}        = 'dummy';
        $record->{TEST_EXITSTATUS}    = 'dummy';
        $record->{TEST_DESCRIPTION}   = 'dummy';

        $record->{TEST_BRANCH}        = $test_branch;
        $record->{TEST_REPO}          = $test_repo;
        $record->{TEST_BUILDTYPE}     = $test_buildtype;
        $record->{TEST_TYPE}          = $test_type;
        $record->{TEST_OS}            = $test_os;
        $record->{TEST_KERNEL}        = $test_kernel;
        $record->{TEST_PROCESSORTYPE} = $test_processortype;
        $record->{TEST_MEMORY}        = $test_memory;
        $record->{TEST_TIMEZONE}      = $test_timezone;
        $record->{TEST_OPTIONS}       = $test_jsoptions;

        dbg("BEGIN: testrun: " . recordtostring($record));

        push @testruns, ($record);
    }

    close TESTRUNS;

}

1;
