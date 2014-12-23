package CPANPLUS::Internals::Constants;

use strict;

use CPANPLUS::Error;

use Config;
use File::Spec;
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

require Exporter;
use vars    qw[$VERSION @ISA @EXPORT];

use Package::Constants;


$VERSION    = 0.01;
@ISA        = qw[Exporter];
@EXPORT     = Package::Constants->list( __PACKAGE__ );


sub constants { @EXPORT };

use constant INSTALLER_BUILD
                            => 'CPANPLUS::Dist::Build';
use constant INSTALLER_MM   => 'CPANPLUS::Dist::MM';    
use constant INSTALLER_SAMPLE   
                            => 'CPANPLUS::Dist::Sample';
use constant INSTALLER_BASE => 'CPANPLUS::Dist::Base';                            

use constant SHELL_DEFAULT  => 'CPANPLUS::Shell::Default';
use constant SHELL_CLASSIC  => 'CPANPLUS::Shell::Classic';

use constant CONFIG         => 'CPANPLUS::Config';
use constant CONFIG_USER    => 'CPANPLUS::Config::User';
use constant CONFIG_SYSTEM  => 'CPANPLUS::Config::System';
use constant CONFIG_BOXED   => 'CPANPLUS::Config::Boxed';

use constant TARGET_CREATE  => 'create';
use constant TARGET_PREPARE => 'prepare';
use constant TARGET_INSTALL => 'install';
use constant TARGET_IGNORE  => 'ignore';

use constant ON_WIN32       => $^O eq 'MSWin32';
use constant ON_NETWARE     => $^O eq 'NetWare';
use constant ON_CYGWIN      => $^O eq 'cygwin';
use constant ON_VMS         => $^O eq 'VMS';

use constant DOT_CPANPLUS   => ON_VMS ? '_cpanplus' : '.cpanplus'; 

use constant OPT_AUTOFLUSH  => '-MCPANPLUS::Internals::Utils::Autoflush';

use constant UNKNOWN_DL_LOCATION
                            => 'UNKNOWN-ORIGIN';   

use constant NMAKE          => 'nmake.exe';
use constant NMAKE_URL      => 
                        'ftp://ftp.microsoft.com/Softlib/MSLFILES/nmake15.exe';

use constant INSTALL_VIA_PACKAGE_MANAGER 
                            => sub { my $fmt = $_[0] or return;
                                     return 1 if $fmt ne INSTALLER_BUILD and
                                                 $fmt ne INSTALLER_MM;
                            };                                                 

use constant IS_CODEREF     => sub { ref $_[-1] eq 'CODE' };
use constant IS_MODOBJ      => sub { UNIVERSAL::isa($_[-1], 
                                            'CPANPLUS::Module') }; 
use constant IS_FAKE_MODOBJ => sub { UNIVERSAL::isa($_[-1],
                                            'CPANPLUS::Module::Fake') };
use constant IS_AUTHOBJ     => sub { UNIVERSAL::isa($_[-1],
                                            'CPANPLUS::Module::Author') };
use constant IS_FAKE_AUTHOBJ
                            => sub { UNIVERSAL::isa($_[-1],
                                            'CPANPLUS::Module::Author::Fake') };

use constant IS_CONFOBJ     => sub { UNIVERSAL::isa($_[-1],
                                            'CPANPLUS::Configure') };

use constant IS_RVOBJ       => sub { UNIVERSAL::isa($_[-1],
                                            'CPANPLUS::Backend::RV') };
                                            
use constant IS_INTERNALS_OBJ
                            => sub { UNIVERSAL::isa($_[-1],
                                            'CPANPLUS::Internals') };                                            
                                            
use constant IS_FILE        => sub { return 1 if -e $_[-1] };                                            

use constant FILE_EXISTS    => sub {  
                                    my $file = $_[-1];
                                    return 1 if IS_FILE->($file);
                                    local $Carp::CarpLevel = 
                                            $Carp::CarpLevel+2;
                                    error(loc(  q[File '%1' does not exist],
                                                $file));
                                    return;
                            };    

use constant FILE_READABLE  => sub {  
                                    my $file = $_[-1];
                                    return 1 if -e $file && -r _;
                                    local $Carp::CarpLevel = 
                                            $Carp::CarpLevel+2;
                                    error( loc( q[File '%1' is not readable ].
                                                q[or does not exist], $file));
                                    return;
                            };    
use constant IS_DIR         => sub { return 1 if -d $_[-1] };

use constant DIR_EXISTS     => sub { 
                                    my $dir = $_[-1];
                                    return 1 if IS_DIR->($dir);
                                    local $Carp::CarpLevel = 
                                            $Carp::CarpLevel+2;                                    
                                    error(loc(q[Dir '%1' does not exist],
                                            $dir));
                                    return;
                            };   
                    
                            ### On VMS, if the $Config{make} is either MMK 
                            ### or MMS, then the makefile is 'DESCRIP.MMS'.
use constant MAKEFILE       => sub { my $file =
                                        (ON_VMS and 
                                         $Config::Config{make} =~ /MM[S|K]/i)
                                            ? 'DESCRIP.MMS'
                                            : 'Makefile';

                                    return @_
                                        ? File::Spec->catfile( @_, $file )
                                        : $file;
                            };                   
use constant MAKEFILE_PL    => sub { return @_
                                        ? File::Spec->catfile( @_,
                                                            'Makefile.PL' )
                                        : 'Makefile.PL';
                            }; 
use constant BUILD_PL       => sub { return @_
                                        ? File::Spec->catfile( @_,
                                                            'Build.PL' )
                                        : 'Build.PL';
                            };
                            
use constant BLIB           => sub { return @_
                                        ? File::Spec->catfile(@_, 'blib')
                                        : 'blib';
                            };                  

use constant LIB            => 'lib';
use constant LIB_DIR        => sub { return @_
                                        ? File::Spec->catdir(@_, LIB)
                                        : LIB;
                            }; 
use constant AUTO           => 'auto';                            
use constant LIB_AUTO_DIR   => sub { return @_
                                        ? File::Spec->catdir(@_, LIB, AUTO)
                                        : File::Spec->catdir(LIB, AUTO)
                            }; 
use constant ARCH           => 'arch';
use constant ARCH_DIR       => sub { return @_
                                        ? File::Spec->catdir(@_, ARCH)
                                        : ARCH;
                            }; 
use constant ARCH_AUTO_DIR  => sub { return @_
                                        ? File::Spec->catdir(@_,ARCH,AUTO)
                                        : File::Spec->catdir(ARCH,AUTO)
                            };                            

use constant BLIB_LIBDIR    => sub { return @_
                                        ? File::Spec->catdir(
                                                @_, BLIB->(), LIB )
                                        : File::Spec->catdir( BLIB->(), LIB );
                            };  

use constant CONFIG_USER_LIB_DIR => sub { 
                                    require CPANPLUS::Internals::Utils;
                                    LIB_DIR->(
                                        CPANPLUS::Internals::Utils->_home_dir,
                                        DOT_CPANPLUS
                                    );
                                };        
use constant CONFIG_USER_FILE    => sub {
                                    File::Spec->catfile(
                                        CONFIG_USER_LIB_DIR->(),
                                        split('::', CONFIG_USER),
                                    ) . '.pm';
                                };
use constant CONFIG_SYSTEM_FILE  => sub {
                                    require CPANPLUS::Internals;
                                    require File::Basename;
                                    my $dir = File::Basename::dirname(
                                        $INC{'CPANPLUS/Internals.pm'}
                                    );
                                
                                    ### XXX use constants
                                    File::Spec->catfile( 
                                        $dir, qw[Config System.pm]
                                    );
                                };        
      
use constant README         => sub { my $obj = $_[0];
                                     my $pkg = $obj->package_name;
                                     $pkg .= '-' . $obj->package_version .
                                             '.readme';
                                     return $pkg;
                            };
use constant OPEN_FILE      => sub {
                                    my($file, $mode) = (@_, '');
                                    my $fh;
                                    open $fh, "$mode" . $file
                                        or error(loc(
                                            "Could not open file '%1': %2",
                                             $file, $!));
                                    return $fh if $fh;
                                    return;
                            };      
         
use constant OPEN_DIR       => sub { 
                                    my $dir = shift;
                                    my $dh;
                                    opendir $dh, $dir or error(loc(
                                        "Could not open dir '%1': %2", $dir, $!
                                    ));
                                    
                                    return $dh if $dh;
                                    return;
                            };

use constant READ_DIR       => sub { 
                                    my $dir = shift;
                                    my $dh  = OPEN_DIR->( $dir ) or return;
                                    
                                    ### exclude . and ..
                                    my @files =  grep { $_ !~ /^\.{1,2}/ }
                                                    readdir($dh);

                                    ### Remove trailing dot on VMS when
                                    ### using VMS syntax.
                                    if( ON_VMS ) {
                                        s/(?<!\^)\.$// for @files;
                                    }
                                    
                                    return @files;
                            };  

use constant STRIP_GZ_SUFFIX 
                            => sub {
                                    my $file = $_[0] or return;
                                    $file =~ s/.gz$//i;
                                    return $file;
                            };            
                                        
use constant CHECKSUMS      => 'CHECKSUMS';
use constant PGP_HEADER     => '-----BEGIN PGP SIGNED MESSAGE-----';
use constant ENV_CPANPLUS_CONFIG
                            => 'PERL5_CPANPLUS_CONFIG';
use constant ENV_CPANPLUS_IS_EXECUTING
                            => 'PERL5_CPANPLUS_IS_EXECUTING';
use constant DEFAULT_EMAIL  => 'cpanplus@example.com';   
use constant CPANPLUS_UA    => sub { ### for the version number ###
                                     require CPANPLUS::Internals;
                                     "CPANPLUS/$CPANPLUS::Internals::VERSION" 
                                };
use constant TESTERS_URL    => sub {
                                    "http://testers.cpan.org/show/" .
                                    $_[0] .".yaml" 
                                };
use constant TESTERS_DETAILS_URL
                            => sub {
                                    'http://testers.cpan.org/show/' .
                                    $_[0] . '.html';
                                };         

use constant CREATE_FILE_URI    
                            => sub { 
                                    my $dir = $_[0] or return;
                                    return $dir =~ m|^/| 
                                        ? 'file://'  . $dir
                                        : 'file:///' . $dir;   
                            };        

use constant EMPTY_DSLIP    => '     ';

use constant CUSTOM_AUTHOR_ID
                            => 'LOCAL';

use constant DOT_SHELL_DEFAULT_RC
                            => '.shell-default.rc';

use constant PREREQ_IGNORE  => 0;                
use constant PREREQ_INSTALL => 1;
use constant PREREQ_ASK     => 2;
use constant PREREQ_BUILD   => 3;
use constant BOOLEANS       => [0,1];
use constant CALLING_FUNCTION   
                            => sub { my $lvl = $_[0] || 0;
                                     return join '::', (caller(2+$lvl))[3] 
                                };
use constant PERL_CORE      => 'perl';

use constant GET_XS_FILES   => sub { my $dir = $_[0] or return;
                                     require File::Find;
                                     my @files;
                                     File::Find::find( 
                                        sub { push @files, $File::Find::name
                                                if $File::Find::name =~ /\.xs$/i
                                        }, $dir );
                                           
                                     return @files;
                                };  

use constant INSTALL_LOG_FILE 
                            => sub { my $obj  = shift or return;
                                     my $name = $obj->name; $name =~ s/::/-/g;
                                     $name .= '-'. $obj->version;
                                     $name .= '-'. scalar(time) . '.log';
                                     return $name;
                                };                                        

use constant ON_OLD_CYGWIN  => do { ON_CYGWIN and $] < 5.008 
                                    ? loc(
                                       "Your perl version for %1 is too low; ".
                                       "Require %2 or higher for this function",
                                       $^O, '5.8.0' )
                                    : '';                                                                           
                                };

### XXX these 2 are probably obsolete -- check & remove;
use constant DOT_EXISTS     => '.exists'; 

use constant QUOTE_PERL_ONE_LINER 
                            => sub { my $line = shift or return;

                                     ### use double quotes on these systems
                                     return qq["$line"] 
                                        if ON_WIN32 || ON_NETWARE || ON_VMS;

                                     ### single quotes on the rest
                                     return qq['$line'];
                            };   

1;              

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
