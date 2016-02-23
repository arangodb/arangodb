package CPANPLUS::Internals::Constants::Report;

use strict;
use CPANPLUS::Error;

use File::Spec;
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

require Exporter;
use vars    qw[$VERSION @ISA @EXPORT];

use Package::Constants;


$VERSION    = 0.01;
@ISA        = qw[Exporter];
@EXPORT     = Package::Constants->list( __PACKAGE__ );

### for the version
require CPANPLUS::Internals;

### OS to regex map ###
my %OS = (
    Amiga       => 'amigaos',
    Atari       => 'mint',
    BSD         => 'bsdos|darwin|freebsd|openbsd|netbsd',
    Be          => 'beos',
    BeOS        => 'beos',
    Cygwin      => 'cygwin',
    Darwin      => 'darwin',
    EBCDIC      => 'os390|os400|posix-bc|vmesa',
    HPUX        => 'hpux',
    Linux       => 'linux',
    MSDOS       => 'dos|os2|MSWin32|cygwin',
    'bin\\d*Mac'=> 'MacOS|darwin', # binMac, bin56Mac, bin58Mac...
    Mac         => 'MacOS|darwin',
    MacPerl     => 'MacOS',
    MacOS       => 'MacOS|darwin',
    MacOSX      => 'darwin',
    MPE         => 'mpeix',
    MPEiX       => 'mpeix',
    OS2         => 'os2',
    Plan9       => 'plan9',
    RISCOS      => 'riscos',
    SGI         => 'irix',
    Solaris     => 'solaris',
    Unix        => 'aix|bsdos|darwin|dgux|dynixptx|freebsd|'.
                   'linux|hpux|machten|netbsd|next|openbsd|dec_osf|'.
                   'svr4|sco_sv|unicos|unicosmk|solaris|sunos',
    VMS         => 'VMS',
    VOS         => 'VOS',
    Win32       => 'MSWin32|cygwin',
    Win32API    => 'MSWin32|cygwin',
);

use constant GRADE_FAIL     => 'fail';
use constant GRADE_PASS     => 'pass';
use constant GRADE_NA       => 'na';
use constant GRADE_UNKNOWN  => 'unknown';

use constant MAX_REPORT_SEND
                            => 2;

use constant CPAN_TESTERS_EMAIL
                            => 'cpan-testers@perl.org';

### the cpan mail account for this user ###
use constant CPAN_MAIL_ACCOUNT
                            => sub {
                                my $username = shift or return;
                                return $username . '@cpan.org';
                            };

### check if this module is platform specific and if we're on that
### specific platform. Alternately, the module is not platform specific
### and we're always OK to send out test results.
use constant RELEVANT_TEST_RESULT
                            => sub {
                                my $mod  = shift or return;
                                my $name = $mod->module;
                                my $specific;
                                for my $platform (keys %OS) {
                                    if( $name =~ /\b$platform\b/i ) {
                                        # beware the Mac != MAC
                                        next if($platform eq 'Mac' &&
                                                $name !~ /\b$platform\b/);
                                        $specific++;
                                        return 1 if
                                            $^O =~ /^(?:$OS{$platform})$/
                                    }
                                };
                                return $specific ? 0 : 1;
                            };

use constant UNSUPPORTED_OS
                            => sub {
                                my $buffer = shift or return;
                                if( $buffer =~
                                        /No support for OS|OS unsupported/im ) {
                                    return 1;
                                }
                                return 0;
                          };                                            

use constant PERL_VERSION_TOO_LOW
                            => sub {
                                my $buffer = shift or return;
                                # ExtUtils::MakeMaker format
                                if( $buffer =~
                                        /Perl .*? required--this is only .*?/m ) {
                                    return 1;
                                }
                                # Module::Build format
                                if( $buffer =~
                                        /ERROR:( perl:)? Version .*?( of perl)? is installed, but we need version >= .*?/m ) {
                                    return 1;
                                }
                                return 0;
                          };                                            

use constant NO_TESTS_DEFINED
                            => sub {
                                my $buffer = shift or return;
                                if( $buffer =~
                                  /(No tests defined( for [\w:]+ extension)?\.)/
                                  and $buffer !~ /\*\.t/m and
                                      $buffer !~ /test\.pl/m
                                ) { 
                                    return $1 
                                }
                                
                                return;
                            };

### what stage did the test fail? ###
use constant TEST_FAIL_STAGE
                            => sub {
                                my $buffer = shift or return;
                                return $buffer =~ /(MAKE [A-Z]+).*/
                                    ? lc $1 :
                                    'fetch';
                            };


use constant MISSING_PREREQS_LIST
                            => sub {
                                my $buffer = shift;
                                my @list = map { s/.pm$//; s|/|::|g; $_ }
                                    ($buffer =~
                                        m/\bCan\'t locate (\S+) in \@INC/g);
                                
                                ### make sure every missing prereq is only 
                                ### listed ones
                                {   my %seen;
                                    @list = grep { !$seen{$_}++ } @list
                                }

                                return @list;
                            };

use constant MISSING_EXTLIBS_LIST
                            => sub {
                                my $buffer = shift;
                                my @list = 
                                    ($buffer =~
                                        m/No library found for -l([-\w]+)/g);

                                return @list;
                            };

use constant REPORT_MESSAGE_HEADER
                            => sub {
                                my ($version, $author) = @_;
                                return << ".";

Dear $author,
    
This is a computer-generated error report created automatically by
CPANPLUS, version $version. Testers personal comments may appear 
at the end of this report.

.
                            };

use constant REPORT_MESSAGE_FAIL_HEADER
                            => sub {
                                my($stage, $buffer) = @_;
                                return << ".";

Thank you for uploading your work to CPAN.  However, it appears that
there were some problems testing your distribution.

TEST RESULTS:

Below is the error stack from stage '$stage':

$buffer

.
                            };

use constant REPORT_MISSING_PREREQS
                            => sub {
                                my ($author,$email,@missing) = @_;
                                $author = ($author && $email) 
                                            ? "$author ($email)" 
                                            : 'Your Name Here';
                                
                                my $modules = join "\n", @missing;
                                my $prereqs = join "\n", 
                                    map {"\t'$_'\t=> '0',".
                                         " # or a minimum working version"}
                                    @missing;

                                return << ".";

MISSING PREREQUISITES:

It was observed that the test suite seem to fail without these modules:

$modules

As such, adding the prerequisite module(s) to 'PREREQ_PM' in your
Makefile.PL should solve this problem.  For example:

WriteMakefile(
    AUTHOR      => '$author',
    ... # other information
    PREREQ_PM   => {
$prereqs
    }
);

If you are interested in making a more flexible Makefile.PL that can
probe for missing dependencies and install them, ExtUtils::AutoInstall
at <http://search.cpan.org/dist/ExtUtils-AutoInstall/> may be
worth a look.

Thanks! :-)

.
                            };

use constant REPORT_MISSING_TESTS
                            => sub {
                                return << ".";
RECOMMENDATIONS:

It would be very helpful if you could include even a simple test 
script in the next release, so people can verify which platforms
can successfully install them, as well as avoid regression bugs?

A simple 't/use.t' that says:

#!/usr/bin/env perl -w
use strict;
use Test;
BEGIN { plan tests => 1 }

use Your::Module::Here; ok(1);
exit;
__END__

would be appreciated.  If you are interested in making a more robust
test suite, please see the Test::Simple, Test::More and Test::Tutorial
documentation at <http://search.cpan.org/dist/Test-Simple/>.

Thanks!  :-)

.
                            };

use constant REPORT_LOADED_PREREQS 
                            => sub {
                                my $mod = shift;
                                my $cb  = $mod->parent;
                                my $prq = $mod->status->prereqs || {};

                                ### not every prereq may be coming from CPAN
                                ### so maybe we wont find it in our module
                                ### tree at all... 
                                ### skip ones that cant be found in teh list
                                ### as reported in #12723
                                my @prq = grep { defined }
                                          map { $cb->module_tree($_) }
                                          sort keys %$prq;
                                
                                ### no prereqs?
                                return '' unless @prq;

                                ### some apparently, list what we loaded
                                my $str = << ".";
PREREQUISITES:

Here is a list of prerequisites you specified and versions we 
managed to load:
                                
.
                                $str .= join '', 
                                        map { sprintf "\t%s %-30s %8s %8s\n", 
                                              @$_
                                        
                                        } [' ', 'Module Name', 'Have', 'Want'],
                                          map { my $want = $prq->{$_->name};
                                              [ do { $_->is_uptodate( 
                                                    version => $want
                                                   ) ? ' ' : '!' 
                                                },
                                                $_->name,
                                                $_->installed_version,
                                                $want
                                              ],
                                        ### might be empty entries in there
                                        } grep { defined $_ } @prq;   
                                
                                return $str;
                            };

use constant REPORT_TESTS_SKIPPED 
                            => sub {
                                return << ".";

******************************** NOTE ********************************
***                                                                ***
***    The tests for this module were skipped during this build    ***
***                                                                ***
**********************************************************************

.
                            };
                            
use constant REPORT_MESSAGE_FOOTER
                            => sub {
                                return << ".";

******************************** NOTE ********************************
The comments above are created mechanically, possibly without manual
checking by the sender.  As there are many people performing automatic
tests on each upload to CPAN, it is likely that you will receive 
identical messages about the same problem.

If you believe that the message is mistaken, please reply to the first
one with correction and/or additional informations, and do not take
it personally.  We appreciate your patience. :)
**********************************************************************

Additional comments:
 
.
                             };

1;

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
