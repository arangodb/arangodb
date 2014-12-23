package CPAN::Reporter::PrereqCheck;
use strict;
use vars qw/$VERSION/;
$VERSION = '1.13'; 
$VERSION = eval $VERSION;

use ExtUtils::MakeMaker;
use File::Spec;
use CPAN::Version;

_run() if ! caller();

sub _run {
    my %saw_mod;
    # read module and prereq string from STDIN
    local *DEVNULL;
    open DEVNULL, ">" . File::Spec->devnull;
    while ( <> ) {
        m/^(\S+)\s+([^\n]*)/;
        my ($mod, $need) = ($1, $2);
        die "Couldn't read module for '$_'" unless $mod;
        $need = 0 if not defined $need;
        
        # only evaluate a module once
        next if $saw_mod{$mod}++;

        # get installed version from file with EU::MM
        my($have, $inst_file, $dir, @packpath);
        if ( $mod eq "perl" ) { 
            $have = $];
        }
        else {
            @packpath = split( /::/, $mod );
            $packpath[-1] .= ".pm";
            if (@packpath == 1 && $packpath[0] eq "readline.pm") {
                unshift @packpath, "Term", "ReadLine"; # historical reasons
            }
            INCDIR:
            foreach my $dir (@INC) {
                my $pmfile = File::Spec->catfile($dir,@packpath);
                if (-f $pmfile){
                    $inst_file = $pmfile;
                    last INCDIR;
                }
            }
            
            # get version from file or else report missing
            if ( defined $inst_file ) {
                $have = MM->parse_version($inst_file);
                $have = "0" if ! defined $have || $have eq 'undef';
                # report broken if it can't be loaded
                select DEVNULL; # try to suppress spurious newlines
                if ( ! eval "require $mod" ) {
                    select STDOUT;
                    print "$mod 0 broken\n";
                    next;
                }
                select STDOUT;
            }
            else {
                print "$mod 0 n/a\n";
                next;
            }
        }

        # complex requirements are comma separated
        my ( @requirements ) = split /\s*,\s*/, $need;

        my $passes = 0;
        RQ: 
        for my $rq (@requirements) {
            if ($rq =~ s|>=\s*||) {
                # no-op -- just trimmed string
            } elsif ($rq =~ s|>\s*||) {
                if (CPAN::Version->vgt($have,$rq)){
                    $passes++;
                }
                next RQ;
            } elsif ($rq =~ s|!=\s*||) {
                if (CPAN::Version->vcmp($have,$rq)) { 
                    $passes++; # didn't match
                }
                next RQ;
            } elsif ($rq =~ s|<=\s*||) {
                if (! CPAN::Version->vgt($have,$rq)){
                    $passes++;
                }
                next RQ;
            } elsif ($rq =~ s|<\s*||) {
                if (CPAN::Version->vlt($have,$rq)){
                    $passes++;
                }
                next RQ;
            }
            # if made it here, then it's a normal >= comparison
            if (! CPAN::Version->vlt($have, $rq)){
                $passes++;
            }
        }
        my $ok = $passes == @requirements ? 1 : 0;
        print "$mod $ok $have\n"
    }
    return;
}

1;

__END__

#--------------------------------------------------------------------------#
# pod documentation 
#--------------------------------------------------------------------------#

=begin wikidoc

= NAME

CPAN::Reporter::PrereqCheck - Modulino for prerequisite tests

= VERSION

This documentation describes version %%VERSION%%.

= SYNOPSIS

 require CPAN::Reporter::PrereqCheck;
 my $prereq_check = $INC{'CPAN/Reporter/PrereqCheck.pm'};
 my $result = qx/$perl $prereq_check < $prereq_file/;

= DESCRIPTION

This modulino determines whether a list of prerequisite modules are
available and, if so, their version number.  It is designed to be run
as a script in order to provide this information from the perspective of
a subprocess, just like CPAN::Reporter's invocation of {perl Makefile.PL}
and so on.

It reads a module name and prerequisite string pair from each line of input
and prints out the module name, 0 or 1 depending on whether the prerequisite
is satisifed, and the installed module version.  If the module is not
available, it will print "n/a" for the version.  If the module is available
but can't be loaded, it will print "broken" for the version.  Modules 
without a version will be treated as being of version "0".

No user serviceable parts are inside.  This modulino is packaged for 
internal use by CPAN::Reporter.

= BUGS

Please report any bugs or feature using the CPAN Request Tracker.  
Bugs can be submitted through the web interface at 
[http://rt.cpan.org/Dist/Display.html?Queue=CPAN-Reporter]

When submitting a bug or request, please include a test-file or a patch to an
existing test-file that illustrates the bug or desired feature.

= SEE ALSO

* [CPAN::Reporter] -- main documentation

= AUTHOR

David A. Golden (DAGOLDEN)

= COPYRIGHT AND LICENSE

Copyright (c) 2006, 2007 by David A. Golden

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
