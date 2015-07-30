package CPANPLUS::Dist::Build::Constants;

use strict;
use File::Spec;

BEGIN {

    require Exporter;
    use vars    qw[$VERSION @ISA @EXPORT];
  
    $VERSION    = 0.01;
    @ISA        = qw[Exporter];
    @EXPORT     = qw[ BUILD_DIR BUILD ];
}


use constant BUILD_DIR      => sub { return @_
                                        ? File::Spec->catdir($_[0], '_build')
                                        : '_build';
                            }; 
use constant BUILD          => sub { my $file = @_
                                        ? File::Spec->catfile($_[0], 'Build')
                                        : 'Build';
                                        
                                     ### on VMS, '.com' is appended when
                                     ### creating the Build file
                                     $file .= '.com' if $^O eq 'VMS';     
                                     
                                     return $file;
                            };
                            
1;


# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
