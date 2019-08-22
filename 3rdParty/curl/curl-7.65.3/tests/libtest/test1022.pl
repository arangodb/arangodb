#!/usr/bin/env perl
# Determine if curl-config --version matches the curl --version
if ( $#ARGV != 2 )
{
    print "Usage: $0 curl-config-script curl-version-output-file version|vernum\n";
    exit 3;
}

my $what=$ARGV[2];

# Read the output of curl --version
open(CURL, "$ARGV[1]") || die "Can't open curl --version list in $ARGV[1]\n";
$_ = <CURL>;
chomp;
/libcurl\/([\.\d]+((-DEV)|(-\d+))?)/;
my $version = $1;
close CURL;

my $curlconfigversion;

# Read the output of curl-config --version/--vernum
open(CURLCONFIG, "sh $ARGV[0] --$what|") || die "Can't get curl-config --$what list\n";
$_ = <CURLCONFIG>;
chomp;
my $filever=$_;
if ( $what eq "version" ) {
    if($filever =~ /^libcurl ([\.\d]+((-DEV)|(-\d+))?)$/) {
        $curlconfigversion = $1;
    }
    else {
        $curlconfigversion = "illegal value";
    }
}
else { # "vernum" case
    # Convert hex version to decimal for comparison's sake
    if($filever =~ /^(..)(..)(..)$/) {
        $curlconfigversion = hex($1) . "." . hex($2) . "." . hex($3);
    }
    else {
        $curlconfigversion = "illegal value";
    }

    # Strip off the -DEV from the curl version if it's there
    $version =~ s/-\w*$//;
}
close CURLCONFIG;

my $different = $version ne $curlconfigversion;
if ($different || !$version) {
    print "Mismatch in --version:\n";
    print "curl:        $version\n";
    print "curl-config: $curlconfigversion\n";
    exit 1;
}
