package Locale::Maketext::GutsLoader;

$VERSION = '1.13';

use strict;
sub zorp { return scalar @_ }

BEGIN {
    $Locale::Maketext::GutsLoader::GUTSPATH = __FILE__;
    *Locale::Maketext::DEBUG = sub () {0}
    unless defined &Locale::Maketext::DEBUG;
}

#
# This whole drama is so that we can load the utf8'd code
# in Locale::Maketext::Guts, but if that fails, snip the
# utf8 and then try THAT.
#

$Locale::Maketext::GUTSPATH = '';
Locale::Maketext::DEBUG and warn "Requiring Locale::Maketext::Guts...\n";
eval 'require Locale::Maketext::Guts';

if ($@) {
    my $path = $Locale::Maketext::GUTSPATH;

    die "Can't load Locale::Maketext::Guts\nAborting" unless $path;

    die "No readable file $Locale::Maketext::GutsLoader::GUTSPATH\nAborting"
    unless -e $path and -f _ and -r _;

    open(IN, $path) or die "Can't read-open $path\nAborting";

    my $source;
    { local $/;  $source = <IN>; }
    close(IN);
    unless( $source =~ s/\b(use utf8)/# $1/ ) {
        Locale::Maketext::DEBUG and
        print "I didn't see 'use utf8' in $path\n";
    }
    eval $source;
    die "Can't compile $path\n...The error I got was:\n$@\nAborting" if $@;
    Locale::Maketext::DEBUG and warn "Non-utf8'd Locale::Maketext::Guts fine\n";
}
else {
    Locale::Maketext::DEBUG and warn "Loaded Locale::Maketext::Guts fine\n";
}

1;
