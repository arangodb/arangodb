package CPANPLUS::Configure::Setup;

use strict;
use vars    qw(@ISA);

use base    qw[CPANPLUS::Internals::Utils];
use base    qw[Object::Accessor];

use Config;
use Term::UI;
use Module::Load;
use Term::ReadLine;


use CPANPLUS::Internals::Utils;
use CPANPLUS::Internals::Constants;
use CPANPLUS::Error;

use IPC::Cmd                    qw[can_run];
use Params::Check               qw[check];
use Module::Load::Conditional   qw[check_install];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

### silence Term::UI
$Term::UI::VERBOSE = 0;

#Can't ioctl TIOCGETP: Unknown error
#Consider installing Term::ReadKey from CPAN site nearby
#        at http://www.perl.com/CPAN
#Or use
#        perl -MCPAN -e shell
#to reach CPAN. Falling back to 'stty'.
#        If you do not want to see this warning, set PERL_READLINE_NOWARN
#in your environment.
#'stty' is not recognized as an internal or external command,
#operable program or batch file.
#Cannot call `stty': No such file or directory at C:/Perl/site/lib/Term/ReadLine/

### setting this var in the meantime to avoid this warning ###
$ENV{PERL_READLINE_NOWARN} = 1;


sub new {
    my $class = shift;
    my %hash  = @_;

    my $tmpl = {
        configure_object => { },
        term             => { },
        backend          => { },
        autoreply        => { default => 0, },
        skip_mirrors     => { default => 0, },
        use_previous     => { default => 1, },
        config_type      => { default => CONFIG_USER },
    };

    my $args = check( $tmpl, \%hash ) or return;

    ### initialize object
    my $obj = $class->SUPER::new( keys %$tmpl );
    for my $acc ( $obj->ls_accessors ) {
        $obj->$acc( $args->{$acc} );
    }     
    
    ### otherwise there's a circular use ###
    load CPANPLUS::Configure;
    load CPANPLUS::Backend;

    $obj->configure_object( CPANPLUS::Configure->new() )
        unless $obj->configure_object;
        
    $obj->backend( CPANPLUS::Backend->new( $obj->configure_object ) )
        unless $obj->backend;

    ### use empty string in case user only has T::R::Stub -- it complains
    $obj->term( Term::ReadLine->new('') ) 
        unless $obj->term;

    ### enable autoreply if that was passed ###
    $Term::UI::AUTOREPLY = $obj->autoreply;

    return $obj;
}

sub init {
    my $self = shift;
    my $term = $self->term;
    
    ### default setting, unless changed
    $self->config_type( CONFIG_USER ) unless $self->config_type;
    
    my $save = loc('Save & exit');
    my $exit = loc('Quit without saving');
    my @map  = (
        # key on the display                        # method to dispatch to
        [ loc('Select Configuration file')      => '_save_where'        ],
        [ loc('Setup CLI Programs')             => '_setup_program'     ],
        [ loc('Setup CPANPLUS Home directory')  => '_setup_base'        ],
        [ loc('Setup FTP/Email settings')       => '_setup_ftp'         ],
        [ loc('Setup basic preferences')        => '_setup_conf'        ],
        [ loc('Setup installer settings')       => '_setup_installer'   ],
        [ loc('Select mirrors'),                => '_setup_hosts'       ],      
        [ loc('Edit configuration file')        => '_edit'              ],    
        [ $save                                 => '_save'              ],
        [ $exit                                 => 1                    ],             
    );

    my @keys = map { $_->[0] } @map;    # sorted keys
    my %map  = map { @$_     } @map;    # lookup hash
   
    PICK_SECTION: {
        print loc("
=================>      MAIN MENU       <=================        
        
Welcome to the CPANPLUS configuration. Please select which
parts you wish to configure

Defaults are taken from your current configuration.
If you would save now, your settings would be written to:
    
    %1
    
        ", $self->config_type );
    
        my $choice = $term->get_reply(
                            prompt  => "Section to configure:",
                            choices => \@keys,
                            default => $keys[0]
                        );       
               
        ### exit configuration?
        if( $choice eq $exit ) {
            print loc("
Quitting setup, changes will not be saved.
            ");
            return 1;
        }      
            
        my $method = $map{$choice};
        
        my $rv = $self->$method or print loc("
There was an error setting up this section. You might want to try again
        ");

        ### was it save & exit?
        if( $choice eq $save and $rv ) {
            print loc("
Quitting setup, changes are saved to '%1'
            ", $self->config_type 
            );
            return 1;
        }

        ### otherwise, present choice again
        redo PICK_SECTION;
    }  

    return 1;
}



### sub that figures out what kind of config type the user wants
sub _save_where {
    my $self = shift;
    my $term = $self->term;
    my $conf = $self->configure_object;


    ASK_CONFIG_TYPE: {
    
        print loc( q[  
Where would you like to save your CPANPLUS Configuration file?

If you want to configure CPANPLUS for this user only, 
select the '%1' option.
The file will then be saved in your homedirectory.

If you are the system administrator of this machine, 
and would like to make this config available globally, 
select the '%2' option.
The file will be then be saved in your CPANPLUS 
installation directory.

        ], CONFIG_USER, CONFIG_SYSTEM );
    

        ### ask what config type we should save to
        my $type = $term->get_reply(
                        prompt  => loc("Type of configuration file"),
                        default => $self->config_type || CONFIG_USER,
                        choices => [CONFIG_USER, CONFIG_SYSTEM],
                  );
    
        my $file = $conf->_config_pm_to_file( $type );
        
        ### can we save to this file?
        unless( $conf->can_save( $file ) ) {
            error(loc(
                "Can not save to file '%1'-- please check permissions " .
                "and try again", $file       
            ));
            
            redo ASK_CONFIG_FILE;
        } 
        
        ### you already have the file -- are we allowed to overwrite
        ### or should we try again?
        if ( -e $file and -w _ ) {
            print loc(q[
I see you already have this file:
    %1

If you continue & save this file, the previous version will be overwritten.

            ], $file );
            
            redo ASK_CONFIG_TYPE 
                unless $term->ask_yn(
                    prompt  => loc( "Shall I overwrite it?"),
                    default => 'n',
                );
        }
        
        print $/, loc("Using '%1' as your configuration type", $type);
        
        return $self->config_type($type);
    }            
}


### setup the build & cache dirs
sub _setup_base {
    my $self = shift;
    my $term = $self->term;
    my $conf = $self->configure_object;

    my $base = $conf->get_conf('base');
    my $home = File::Spec->catdir( $self->_home_dir, DOT_CPANPLUS );
    
    print loc("
CPANPLUS needs a directory of its own to cache important index
files and maybe keep a temporary mirror of CPAN files.  
This may be a site-wide directory or a personal directory.

For a single-user installation, we suggest using your home directory.

");

    my $where;
    ASK_HOME_DIR: {
        my $other = loc('Somewhere else');
        if( $base and ($base ne $home) ) {
            print loc("You have several choices:");

            $where = $term->get_reply(
                        prompt  => loc('Please pick one'),
                        choices => [$home, $base, $other],
                        default => $home,
                    );
        } else {
            $where = $base;
        }

        if( $where and -d $where ) {
            print loc("
I see you already have a directory:
    %1
    
            "), $where;

            my $yn = $term->ask_yn(
                            prompt  => loc('Should I use it?'),
                            default => 'y',
                        );
            $where = '' unless $yn;
        }

        if( $where and ($where ne $other) and not -d $where ) {
            if (!$self->_mkdir( dir => $where ) ) {
                print   "\n", loc("Unable to create directory '%1'", $where);
                redo ASK_HOME_DIR;
            }

        } elsif( not $where or ($where eq $other) ) {
            print loc("
First of all, I'd like to create this directory.

            ");

            NEW_HOME: {
                $where = $term->get_reply(
                                prompt  => loc('Where shall I create it?'),
                                default => $home,
                            );

                my $again;
                if( -d $where and not -w _ ) {
                    print "\n", loc("I can't seem to write in this directory");
                    $again++;
                } elsif (!$self->_mkdir( dir => $where ) ) {
                    print "\n", loc("Unable to create directory '%1'", $where);
                    $again++;
                }

                if( $again ) {
                    print "\n", loc('Please select another directory'), "\n\n";
                    redo NEW_HOME;
                }
            }
        }
    }

    ### tidy up the path and store it
    $where = File::Spec->rel2abs($where);
    $conf->set_conf( base => $where );

    ### create subdirectories ###
    my @dirs =
        File::Spec->catdir( $where, $self->_perl_version(perl => $^X),
                            $conf->_get_build('moddir') ),
        map {
            File::Spec->catdir( $where, $conf->_get_build($_) )
        } qw[autdir distdir];

    for my $dir ( @dirs ) {
        unless( $self->_mkdir( dir => $dir ) ) {
            warn loc("I wasn't able to create '%1'", $dir), "\n";
        }
    }

    ### clear away old storable images before 0.031
    for my $src (qw[dslip mailrc packages]) {
        1 while unlink File::Spec->catfile( $where, $src );

    }

    print loc(q[
Your CPANPLUS build and cache directory has been set to:
    %1
    
    ], $where);

    return 1;
}

sub _setup_ftp {
    my $self = shift;
    my $term = $self->term;
    my $conf = $self->configure_object;

    #########################
    ## are you a pacifist? ##
    #########################

    print loc("
If you are connecting through a firewall or proxy that doesn't handle
FTP all that well you can use passive FTP.

");

    my $yn = $term->ask_yn(
                prompt  => loc("Use passive FTP?"),
                default => $conf->get_conf('passive'),
            );

    $conf->set_conf(passive => $yn);

    ### set the ENV var as well, else it won't get set till AFTER
    ### the configuration is saved. but we fetch files BEFORE that.
    $ENV{FTP_PASSIVE} = $yn;

    print "\n";
    print $yn
            ? loc("I will use passive FTP.")
            : loc("I won't use passive FTP.");
    print "\n";

    #############################
    ## should fetches timeout? ##
    #############################

    print loc("
CPANPLUS can specify a network timeout for downloads (in whole seconds).
If none is desired (or to skip this question), enter '0'.

");

    my $timeout = 0 + $term->get_reply(
                prompt  => loc("Network timeout for downloads"),
                default => $conf->get_conf('timeout') || 0,
                allow   => qr/(?!\D)/,            ### whole numbers only
            );

    $conf->set_conf(timeout => $timeout);

    print "\n";
    print $timeout
            ? loc("The network timeout for downloads is %1 seconds.", $timeout)
            : loc("The network timeout for downloads is not set.");
    print "\n";

    ############################
    ## where can I reach you? ##
    ############################

    print loc("
What email address should we send as our anonymous password when
fetching modules from CPAN servers?  Some servers will NOT allow you to
connect without a valid email address, or at least something that looks
like one.
Also, if you choose to report test results at some point, a valid email
is required for the 'from' field, so choose wisely.

    ");

    my $other   = 'Something else';
    my @choices = (DEFAULT_EMAIL, $Config{cf_email}, $other);
    my $current = $conf->get_conf('email');

    ### if your current address is not in the list, add it to the choices
    unless (grep { $_ eq $current } @choices) {
	   unshift @choices, $current;
    }
    
    my $email = $term->get_reply(
                    prompt  => loc('Which email address shall I use?'),
                    default => $current || $choices[0],
                    choices => \@choices,
                );

    if( $email eq $other ) {
        EMAIL: {
            $email = $term->get_reply(
                        prompt  => loc('Email address: '),
                    );
            
            unless( $self->_valid_email($email) ) {
                print loc("
You did not enter a valid email address, please try again!
                ") if length $email;

                redo EMAIL;
            }
        }
    }

    print loc("
Your 'email' is now:
    %1
    
    ", $email);

    $conf->set_conf( email => $email );

    return 1;
}


### commandline programs
sub _setup_program {
    my $self = shift;
    my $term = $self->term;
    my $conf = $self->configure_object;

    print loc("
CPANPLUS can use command line utilities to do certain
tasks, rather than use perl modules.

If you wish to use a certain command utility, just enter
the full path (or accept the default). If you do not wish
to use it, enter a single space.

Note that the paths you provide should not contain spaces, which is
needed to make a distinction between program name and options to that
program. For Win32 machines, you can use the short name for a path,
like '%1'.
", 'c:\Progra~1\prog.exe' );

    for my $prog ( sort $conf->options( type => 'program') ) {
        PROGRAM: {
            print "\n", loc("Where can I find your '%1' utility? ".
                      "(Enter a single space to disable)", $prog ), "\n";
            
            my $loc = $term->get_reply(
                            prompt  => "Path to your '$prog'",
                            default => $conf->get_program( $prog ),
                        );       
                        
            ### empty line clears it            
            my $cmd     = $loc =~ /^\s*$/ ? undef : $loc;
            my ($bin)   = $cmd =~ /^(\S+)/;
            
            ### did you provide a valid program ?
            if( $bin and not can_run( $bin ) ) {
                print "\n";
                print loc("Can not find the binary '%1' in your path!", $bin);
                redo PROGRAM;
            }

            ### make is special -- we /need/ it!
            if( $prog eq 'make' and not $bin ) {
                print loc(
                    "==> Without your '%1' utility, I can not function! <==",
                    'make'
                );
                print loc("Please provide one!");
                
                ### show win32 where to download
                if ( $^O eq 'MSWin32' ) {            
                    print loc("You can get '%1' from:", NMAKE);
                    print "\t". NMAKE_URL ."\n";
                }
                print "\n";
                redo PROGRAM;                    
            }

            $conf->set_program( $prog => $cmd );
            print $cmd
                ? loc(  "Your '%1' utility has been set to '%2'.", 
                        $prog, $cmd )
                : loc(  "Your '%1' has been disabled.", $prog );           
            print "\n";
        }
    }
    
    return 1;
}    

sub _setup_installer {
    my $self = shift;
    my $term = $self->term;
    my $conf = $self->configure_object;

    my $none = 'None';
    {   
        print loc("
CPANPLUS uses binary programs as well as Perl modules to accomplish
various tasks. Normally, CPANPLUS will prefer the use of Perl modules
over binary programs.

You can change this setting by making CPANPLUS prefer the use of
certain binary programs if they are available.

        ");
        
        ### default to using binaries if we don't have compress::zlib only
        ### -- it'll get very noisy otherwise
        my $type = 'prefer_bin';
        my $yn = $term->ask_yn(
            prompt  => loc("Should I prefer the use of binary programs?"),
            default => $conf->get_conf( $type ),
        );

        print $yn
                ? loc("Ok, I will prefer to use binary programs if possible.")
                : loc("Ok, I will prefer to use Perl modules if possible.");
        print "\n\n";


        $conf->set_conf( $type => $yn );
    }

    {
        print loc("
Makefile.PL is run by perl in a separate process, and accepts various
flags that controls the module's installation.  For instance, if you
would like to install modules to your private user directory, set
'makemakerflags' to:

LIB=~/perl/lib INSTALLMAN1DIR=~/perl/man/man1 INSTALLMAN3DIR=~/perl/man/man3

and be sure that you do NOT set UNINST=1 in 'makeflags' below.

Enter a name=value list separated by whitespace, but quote any embedded
spaces that you want to preserve.  (Enter a space to clear any existing
settings.)

If you don't understand this question, just press ENTER.

        ");

        my $type = 'makemakerflags';
        my $flags = $term->get_reply(
                            prompt  => 'Makefile.PL flags?',
                            default => $conf->get_conf($type),
                    );

        $flags = '' if $flags eq $none || $flags !~ /\S/;

        print   "\n", loc("Your '%1' have been set to:", 'Makefile.PL flags'),
                "\n    ", ( $flags ? $flags : loc('*nothing entered*')),
                "\n\n";

        $conf->set_conf( $type => $flags );
    }

    {
        print loc("
Like Makefile.PL, we run 'make' and 'make install' as separate processes.
If you have any parameters (e.g. '-j3' in dual processor systems) you want
to pass to the calls, please specify them here.

In particular, 'UNINST=1' is recommended for root users, unless you have
fine-tuned ideas of where modules should be installed in the \@INC path.

Enter a name=value list separated by whitespace, but quote any embedded
spaces that you want to preserve.  (Enter a space to clear any existing
settings.)

Again, if you don't understand this question, just press ENTER.

        ");
        my $type        = 'makeflags';
        my $flags   = $term->get_reply(
                                prompt  => 'make flags?',
                                default => $conf->get_conf($type),
                            );

        $flags = '' if $flags eq $none || $flags !~ /\S/;

        print   "\n", loc("Your '%1' have been set to:", $type),
                "\n    ", ( $flags ? $flags : loc('*nothing entered*')),
                "\n\n";

        $conf->set_conf( $type => $flags );
    }

    {
        print loc("
An alternative to ExtUtils::MakeMaker and Makefile.PL there's a module
called Module::Build which uses a Build.PL.

If you would like to specify any flags to pass when executing the
Build.PL (and Build) script, please enter them below.

For instance, if you would like to install modules to your private
user directory, you could enter:

    install_base=/my/private/path

Or to uninstall old copies of modules before updating, you might
want to enter:

    uninst=1

Again, if you don't understand this question, just press ENTER.

        ");

        my $type    = 'buildflags';
        my $flags   = $term->get_reply(
                                prompt  => 'Build.PL and Build flags?',
                                default => $conf->get_conf($type),
                            );

        $flags = '' if $flags eq $none || $flags !~ /\S/;

        print   "\n", loc("Your '%1' have been set to:",
                            'Build.PL and Build flags'),
                "\n    ", ( $flags ? $flags : loc('*nothing entered*')),
                "\n\n";

        $conf->set_conf( $type => $flags );
    }

    ### use EU::MM or module::build? ###
    {
        print loc("
Some modules provide both a Build.PL (Module::Build) and a Makefile.PL
(ExtUtils::MakeMaker).  By default, CPANPLUS prefers Makefile.PL.

Module::Build support is not bundled standard with CPANPLUS, but 
requires you to install 'CPANPLUS::Dist::Build' from CPAN.

Although Module::Build is a pure perl solution, which means you will
not need a 'make' binary, it does have some limitations. The most
important is that CPANPLUS is unable to uninstall any modules installed
by Module::Build.

Again, if you don't understand this question, just press ENTER.

        ");
        my $type = 'prefer_makefile';
        my $yn = $term->ask_yn(
                    prompt  => loc("Prefer Makefile.PL over Build.PL?"),
                    default => $conf->get_conf($type),
                 );

        $conf->set_conf( $type => $yn );
    }

    {
        print loc('
If you like, CPANPLUS can add extra directories to your @INC list during
startup. These will just be used by CPANPLUS and will not change your
external environment or perl interpreter.  Enter a space separated list of
pathnames to be added to your @INC, quoting any with embedded whitespace.
(To clear the current value enter a single space.)

        ');

        my $type    = 'lib';
        my $flags = $term->get_reply(
                        prompt  => loc('Additional @INC directories to add?'),
                        default => (join " ", @{$conf->get_conf($type) || []} ),
                    );

        my $lib;
        unless( $flags =~ /\S/ ) {
            $lib = [];
        } else {
            (@$lib) = $flags =~  m/\s*("[^"]+"|'[^']+'|[^\s]+)/g;
        }

        print "\n", loc("Your additional libs are now:"), "\n";

        print scalar @$lib
                        ? map { "    $_\n" } @$lib
                        : "    ", loc("*nothing entered*"), "\n";
        print "\n\n";

        $conf->set_conf( $type => $lib );
    }
    
    return 1;
}    
    

sub _setup_conf {
    my $self = shift;
    my $term = $self->term;
    my $conf = $self->configure_object;

    my $none = 'None';
    {
        ############
        ## noisy? ##
        ############

        print loc("
In normal operation I can just give you basic information about what I
am doing, or I can be more verbose and give you every little detail.

        ");

        my $type = 'verbose';
        my $yn   = $term->ask_yn(
                            prompt  => loc("Should I be verbose?"),
                            default => $conf->get_conf( $type ),                        );

        print "\n";
        print $yn
                ? loc("You asked for it!")
                : loc("I'll try to be quiet");

        $conf->set_conf( $type => $yn );
    }

    {
        #######################
        ## flush you animal! ##
        #######################

        print loc("
In the interest of speed, we keep track of what modules were installed
successfully and which failed in the current session.  We can flush this
data automatically, or you can explicitly issue a 'flush' when you want
to purge it.

        ");

        my $type = 'flush';
        my $yn   = $term->ask_yn(
                            prompt  => loc("Flush automatically?"),
                            default => $conf->get_conf( $type ),
                        );

        print "\n";
        print $yn
                ? loc("I'll flush after every full module install.")
                : loc("I won't flush until you tell me to.");

        $conf->set_conf( $type => $yn );
    }

    {
        #####################
        ## force installs? ##
        #####################

        print loc("
Usually, when a test fails, I won't install the module, but if you
prefer, I can force the install anyway.

        ");

        my $type = 'force';
        my $yn   = $term->ask_yn(
                        prompt  => loc("Force installs?"),
                        default => $conf->get_conf( $type ),
                    );

        print "\n";
        print $yn
                ? loc("I will force installs.")
                : loc("I won't force installs.");

        $conf->set_conf( $type => $yn );
    }

    {
        ###################
        ## about prereqs ##
        ###################

        print loc("
Sometimes a module will require other modules to be installed before it
will work.  CPANPLUS can attempt to install these for you automatically
if you like, or you can do the deed yourself.

If you would prefer that we NEVER try to install extra modules
automatically, select NO.  (Usually you will want this set to YES.)

If you would like to build modules to satisfy testing or prerequisites,
but not actually install them, select BUILD.

NOTE: This feature requires you to flush the 'lib' cache for longer
running programs (refer to the CPANPLUS::Backend documentations for
more details).

Otherwise, select ASK to have us ask your permission to install them.

        ");

        my $type = 'prereqs';
        
        my @map = (
            [ PREREQ_IGNORE,                                # conf value 
              loc('No, do not install prerequisites'),      # UI Value   
              loc("I won't install prerequisites")          # diag message
            ],
            [ PREREQ_INSTALL,
              loc('Yes, please install prerequisites'),  
              loc("I will install prerequisites")     
            ],
            [ PREREQ_ASK,    
              loc('Ask me before installing a prerequisite'),  
              loc("I will ask permission to install") 
            ],
            [ PREREQ_BUILD,  
              loc('Build prerequisites, but do not install them'),
              loc( "I will only build, but not install prerequisites" )
            ],
        );
       
        my %reply = map { $_->[1] => $_->[0] } @map; # choice => value
        my %diag  = map { $_->[1] => $_->[2] } @map; # choice => diag message
        my %conf  = map { $_->[0] => $_->[1] } @map; # value => ui choice
        
        my $reply   = $term->get_reply(
                        prompt  => loc('Follow prerequisites?'),
                        default => $conf{ $conf->get_conf( $type ) },
                        choices => [ @conf{ sort keys %conf } ],
                    );
        print "\n";
        
        my $value = $reply{ $reply };
        my $diag  = $diag{  $reply };

        $conf->set_conf( $type => $value );
        print $diag, "\n";
    }

    {   print loc("
Modules in the CPAN archives are protected with md5 checksums.

This requires the Perl module Digest::MD5 to be installed (which
CPANPLUS can do for you later);

        ");
        my $type    = 'md5';
        
        my $yn = $term->ask_yn(
                    prompt  => loc("Shall I use the MD5 checksums?"),
                    default => $conf->get_conf( $type ),
                );

        print $yn
                ? loc("I will use the MD5 checksums if you have it")
                : loc("I won't use the MD5 checksums");

        $conf->set_conf( $type => $yn );

    }

    
    {   ###########################################
        ## sally sells seashells by the seashore ##
        ###########################################

        print loc("
By default CPANPLUS uses its own shell when invoked.  If you would prefer
a different shell, such as one you have written or otherwise acquired,
please enter the full name for your shell module.

        ");

        my $type    = 'shell';
        my $other   = 'Other';
        my @choices = (qw|  CPANPLUS::Shell::Default
                            CPANPLUS::Shell::Classic |, 
                            $other );
        my $default = $conf->get_conf($type);

        unshift @choices, $default unless grep { $_ eq $default } @choices;

        my $reply = $term->get_reply(
            prompt  => loc('Which CPANPLUS shell do you want to use?'),
            default => $default,
            choices => \@choices,
        );

        if( $reply eq $other ) {
            SHELL: {
                $reply = $term->get_reply(
                    prompt => loc(  'Please enter the name of the shell '.
                                    'you wish to use: '),
                );

                unless( check_install( module => $reply ) ) {
                    print "\n", 
                          loc("Could not find '$reply' in your path " .
                          "-- please try again"), 
                          "\n";
                    redo SHELL;
                }
            }
        }

        print "\n", loc("Your shell is now:   %1", $reply), "\n\n";

        $conf->set_conf( $type => $reply );
    }

    {
        ###################
        ## use storable? ##
        ###################

        print loc("
To speed up the start time of CPANPLUS, and maintain a cache over
multiple runs, we can use Storable to freeze some information.
Would you like to do this?

");
        my $type    = 'storable';
        my $yn      = $term->ask_yn(
                                prompt  => loc("Use Storable?"),
                                default => $conf->get_conf( $type ) ? 1 : 0,
                            );
        print "\n";
        print $yn
                ? loc("I will use Storable if you have it")
                : loc("I will not use Storable");

        $conf->set_conf( $type => $yn );
    }

    {
        ###################
        ## use cpantest? ##
        ###################

        print loc("
CPANPLUS has support for the Test::Reporter module, which can be utilized
to report success and failures of modules installed by CPANPLUS.  Would
you like to do this?  Note that you will still be prompted before
sending each report.

If you don't have all the required modules installed yet, you should
consider installing '%1'

This package bundles all the required modules to enable test reporting
and querying from CPANPLUS.
You can do so straight after this installation.

        ", 'Bundle::CPANPLUS::Test::Reporter');

        my $type = 'cpantest';
        my $yn   = $term->ask_yn(
                        prompt  => loc('Report test results?'),
                        default => $conf->get_conf( $type ) ? 1 : 0,
                    );

        print "\n";
        print $yn
                ? loc("I will prompt you to report test results")
                : loc("I won't prompt you to report test results");

        $conf->set_conf( $type => $yn );
    }

    {
        ###################################
        ## use cryptographic signatures? ##
        ###################################

        print loc("
The Module::Signature extension allows CPAN authors to sign their
distributions using PGP signatures.  Would you like to check for
module's cryptographic integrity before attempting to install them?
Note that this requires either the 'gpg' utility or Crypt::OpenPGP
to be installed.

        ");
        my $type = 'signature';

        my $yn = $term->ask_yn(
                            prompt  => loc('Shall I check module signatures?'),
                            default => $conf->get_conf($type) ? 1 : 0,
                        );

        print "\n";
        print $yn
                ? loc("Ok, I will attempt to check module signatures.")
                : loc("Ok, I won't attempt to check module signatures.");

        $conf->set_conf( $type => $yn );
    }

    return 1;
}

sub _setup_hosts {
    my $self = shift;
    my $term = $self->term;
    my $conf = $self->configure_object;


    if( scalar @{ $conf->get_conf('hosts') } ) {

        my $hosts;
        for my $href ( @{$conf->get_conf('hosts')} ) {
            $hosts .= "\t$href->{scheme}://$href->{host}$href->{path}\n";
        }

        print loc("
I see you already have some hosts selected:

$hosts

If you'd like to stick with your current settings, just select 'Yes'.
Otherwise, select 'No' and you can reconfigure your hosts

");
        my $yn = $term->ask_yn(
                        prompt  => loc("Would you like to keep your current hosts?"),
                        default => 'y',
                    );
        return 1 if $yn;
    }

    my @hosts;
    MAIN: {

        print loc("
Now we need to know where your favorite CPAN sites are located. Make a
list of a few sites (just in case the first on the array won't work).

If you are mirroring CPAN to your local workstation, specify a file:
URI by picking the CUSTOM option.

Otherwise, let us fetch the official CPAN mirror list and you can pick
the mirror that suits you best from a list by using the MIRROR option;
First, pick a nearby continent and country. Then, you will be presented
with a list of URLs of CPAN mirrors in the country you selected. Select
one or more of those URLs.

Note, the latter option requires a working net connection.

You can select VIEW to see your current selection and QUIT when you
are done.

");

        my $reply = $term->get_reply(
                        prompt  => loc('Please choose an option'),
                        choices => [qw|Mirror Custom View Quit|],
                        default => 'Mirror',
                    );

        goto MIRROR if $reply eq 'Mirror';
        goto CUSTOM if $reply eq 'Custom';
        goto QUIT   if $reply eq 'Quit';

        $self->_view_hosts(@hosts) if $reply eq 'View';
        redo MAIN;
    }

    my $mirror_file;
    my $hosts;
    MIRROR: {
        $mirror_file    ||= $self->_get_mirrored_by               or return;
        $hosts          ||= $self->_parse_mirrored_by($mirror_file) or return;

        my ($continent, $country, $host) = $self->_guess_from_timezone( $hosts );

        CONTINENT: {
            my %seen;
            my @choices =   sort map {
                                $_->{'continent'}
                            } grep {
                                not $seen{$_->{'continent'}}++
                            } values %$hosts;
            push @choices,  qw[Custom Up Quit];

            my $reply   = $term->get_reply(
                                prompt  => loc('Pick a continent'),
                                default => $continent,
                                choices => \@choices,
                            );

            goto MAIN   if $reply eq 'Up';
            goto CUSTOM if $reply eq 'Custom';
            goto QUIT   if $reply eq 'Quit';

            $continent = $reply;
        }

        COUNTRY: {
            my %seen;
            my @choices =   sort map {
                                $_->{'country'}
                            } grep {
                                not $seen{$_->{'country'}}++
                            } grep {
                                ($_->{'continent'} eq $continent)
                            } values %$hosts;
            push @choices,  qw[Custom Up Quit];

            my $reply   = $term->get_reply(
                                prompt  => loc('Pick a country'),
                                default => $country,
                                choices => \@choices,
                            );

            goto CONTINENT  if $reply eq 'Up';
            goto CUSTOM     if $reply eq 'Custom';
            goto QUIT       if $reply eq 'Quit';

            $country = $reply;
        }

        HOST: {
            my @list =  grep {
                            $_->{'continent'}   eq $continent and
                            $_->{'country'}     eq $country
                        } values %$hosts;

            my %map; my $default;
            for my $href (@list) {
                for my $con ( @{$href->{'connections'}} ) {
                    next unless length $con->{'host'};

                    my $entry   = $con->{'scheme'} . '://' . $con->{'host'};
                    $default    = $entry if $con->{'host'} eq $host;

                    $map{$entry} = $con;
                }
            }

            CHOICE: {
                
                ### doesn't play nice with Term::UI :(
                ### should make t::ui figure out pager opens
                #$self->_pager_open;     # host lists might be long
            
                print loc("
You can enter multiple sites by seperating them by a space.
For example:
    1 4 2 5
                ");    
            
                my @reply = $term->get_reply(
                                    prompt  => loc('Please pick a site: '),
                                    choices => [sort(keys %map), 
                                                qw|Custom View Up Quit|],
                                    default => $default,
                                    multi   => 1,
                            );
                #$self->_pager_close;
    

                goto COUNTRY    if grep { $_ eq 'Up' }      @reply;
                goto CUSTOM     if grep { $_ eq 'Custom' }  @reply;
                goto QUIT       if grep { $_ eq 'Quit' }    @reply;

                ### add the host, but only if it's not on the stack already ###
                unless(  grep { $_ eq 'View' } @reply ) {
                    for my $reply (@reply) {
                        if( grep { $_ eq $map{$reply} } @hosts ) {
                            print loc("Host '%1' already selected", $reply);
                            print "\n\n";
                        } else {
                            push @hosts, $map{$reply}
                        }
                    }
                }

                $self->_view_hosts(@hosts);

                goto QUIT if $self->autoreply;
                redo CHOICE;
            }
        }
    }

    CUSTOM: {
        print loc("
If there are any additional URLs you would like to use, please add them
now.  You may enter them separately or as a space delimited list.

We provide a default fall-back URL, but you are welcome to override it
with e.g. 'http://www.cpan.org/' if LWP, wget or curl is installed.

(Enter a single space when you are done, or to simply skip this step.)

Note that if you want to use a local depository, you will have to enter
as follows:

file://server/path/to/cpan

if the file is on a server on your local network or as:

file:///path/to/cpan

if the file is on your local disk. Note the three /// after the file: bit

");

        CHOICE: {
            my $reply = $term->get_reply(
                            prompt  => loc("Additionals host(s) to add: "),
                            default => '',
                        );

            last CHOICE unless $reply =~ /\S/;

            my $href = $self->_parse_host($reply);

            if( $href ) {
                push @hosts, $href
                    unless grep {
                        $href->{'scheme'}   eq $_->{'scheme'}   and
                        $href->{'host'}     eq $_->{'host'}     and
                        $href->{'path'}     eq $_->{'path'}
                    } @hosts;

                last CHOICE if $self->autoreply;
            } else {
                print loc("Invalid uri! Please try again!");
            }

            $self->_view_hosts(@hosts);

            redo CHOICE;
        }

        DONE: {

            print loc("
Where would you like to go now?

Please pick one of the following options or Quit when you are done

");
            my $answer = $term->get_reply(
                                    prompt  => loc("Where to now?"),
                                    default => 'Quit',
                                    choices => [qw|Mirror Custom View Quit|],
                                );

            if( $answer eq 'View' ) {
                $self->_view_hosts(@hosts);
                redo DONE;
            }

            goto MIRROR if $answer eq 'Mirror';
            goto CUSTOM if $answer eq 'Custom';
            goto QUIT   if $answer eq 'Quit';
        }
    }

    QUIT: {
        $conf->set_conf( hosts => \@hosts );

        print loc("
Your host configuration has been saved

");
    }

    return 1;
}

sub _view_hosts {
    my $self    = shift;
    my @hosts   = @_;

    print "\n\n";

    if( scalar @hosts ) {
        my $i = 1;
        for my $host (@hosts) {

            ### show full path on file uris, otherwise, just show host
            my $path = join '', (
                            $host->{'scheme'} eq 'file'
                                ? ( ($host->{'host'} || '[localhost]'),
                                    $host->{path} )
                                : $host->{'host'}
                        );

            printf "%-40s %30s\n",
                loc("Selected %1",$host->{'scheme'} . '://' . $path ),
                loc("%quant(%2,host) selected thus far.", $i);
            $i++;
        }
    } else {
        print loc("No hosts selected so far.");
    }

    print "\n\n";

    return 1;
}

sub _get_mirrored_by {
    my $self = shift;
    my $cpan = $self->backend;
    my $conf = $self->configure_object;

    print loc("
Now, we are going to fetch the mirror list for first-time configurations.
This may take a while...

");

    ### use the enew configuratoin ###
    $cpan->configure_object( $conf );

    load CPANPLUS::Module::Fake;
    load CPANPLUS::Module::Author::Fake;

    my $mb = CPANPLUS::Module::Fake->new(
                    module      => $conf->_get_source('hosts'),
                    path        => '',
                    package     => $conf->_get_source('hosts'),
                    author      => CPANPLUS::Module::Author::Fake->new(
                                        _id => $cpan->_id ),
                    _id         => $cpan->_id,
                );

    my $file = $cpan->_fetch(   fetchdir => $conf->get_conf('base'),
                                module   => $mb );

    return $file if $file;
    return;
}

sub _parse_mirrored_by {
    my $self = shift;
    my $file = shift;

    -s $file or return;

    my $fh = new FileHandle;
    $fh->open("$file")
        or (
            warn(loc('Could not open file "%1": %2', $file, $!)),
            return
        );

    ### slurp the file in ###
    { local $/; $file = <$fh> }

    ### remove comments ###
    $file =~ s/#.*$//gm;

    $fh->close;

    ### sample host entry ###
    #     ftp.sun.ac.za:
    #       frequency        = "daily"
    #       dst_ftp          = "ftp://ftp.sun.ac.za/CPAN/CPAN/"
    #       dst_location     = "Stellenbosch, South Africa, Africa (-26.1992 28.0564)"
    #       dst_organisation = "University of Stellenbosch"
    #       dst_timezone     = "+2"
    #       dst_contact      = "ftpadm@ftp.sun.ac.za"
    #       dst_src          = "ftp.funet.fi"
    #
    #     # dst_dst          = "ftp://ftp.sun.ac.za/CPAN/CPAN/"
    #     # dst_contact      = "mailto:ftpadm@ftp.sun.ac.za
    #     # dst_src          = "ftp.funet.fi"

    ### host name as key, rest of the entry as value ###
    my %hosts = $file =~ m/([a-zA-Z0-9\-\.]+):\s+((?:\w+\s+=\s+".*?"\s+)+)/gs;

    while (my($host,$data) = each %hosts) {

        my $href;
        map {
            s/^\s*//;
            my @a = split /\s*=\s*/;
            $a[1] =~ s/^"(.+?)"$/$1/g;
            $href->{ pop @a } = pop @a;
        } grep /\S/, split /\n/, $data;

        ($href->{city_area}, $href->{country}, $href->{continent},
            $href->{latitude}, $href->{longitude} ) =
            $href->{dst_location} =~
                m/
                    #Aizu-Wakamatsu, Tohoku-chiho, Fukushima
                    ^"?(
                         (?:[^,]+?)\s*         # city
                         (?:
                             (?:,\s*[^,]+?)\s* # optional area
                         )*?                   # some have multiple areas listed
                     )

                     #Japan
                     ,\s*([^,]+?)\s*           # country

                     #Asia
                     ,\s*([^,]+?)\s*           # continent

                     # (37.4333 139.9821)
                     \((\S+)\s+(\S+?)\)"?$       # (latitude longitude)
                 /sx;

        ### parse the different hosts, store them in config format ###
        my @list;

        for my $type (qw[dst_ftp dst_rsync dst_http]) {
	    my $path = $href->{$type};
	    next unless $path =~ /\w/;
	    if ($type eq 'dst_rsync' && $path !~ /^rsync:/) {
		$path =~ s{::}{/};
		$path = "rsync://$path/";
	    }
            my $parts = $self->_parse_host($path);
            push @list, $parts;
        }

        $href->{connections}    = \@list;
        $hosts{$host}           = $href;
    }

    return \%hosts;
}

sub _parse_host {
    my $self = shift;
    my $host = shift;

    my @parts = $host =~ m|^(\w*)://([^/]*)(/.*)$|s;

    my $href;
    for my $key (qw[scheme host path]) {
        $href->{$key} = shift @parts;
    }

    return if lc($href->{'scheme'}) ne 'file' and !$href->{'host'};
    return if !$href->{'path'};

    return $href;
}

## tries to figure out close hosts based on your timezone
##
## Currently can only report on unique items for each of zones, countries, and
## sites.  In the future this will be combined with something else (perhaps a
## ping?) to narrow down multiple choices.
##
## Tries to return the best zone, country, and site for your location.  Any non-
## unique items will be set to undef instead.
##
## (takes hashref, returns array)
##
sub _guess_from_timezone {
    my $self  = shift;
    my $hosts = shift;
    my (%zones, %countries, %sites);

    ### autrijus - build time zone table
    my %freq_weight = (
        'hourly'        => 2400,
        '4 times a day' =>  400,
        '4x daily'      =>  400,
        'daily'         =>  100,
        'twice daily'   =>   50,
        'weekly'        =>   15,
    );

    while (my ($site, $host) = each %{$hosts}) {
        my ($zone, $continent, $country, $frequency) =
            @{$host}{qw/dst_timezone continent country frequency/};


        # skip non-well-formed ones
        next unless $continent and $country and $zone =~ /^[-+]?\d+(?::30)?/;
        ### fix style
        chomp $zone;
        $zone =~ s/:30/.5/;
        $zone =~ s/^\+//;
        $zone =~ s/"//g;

        $zones{$zone}{$continent}++;
        $countries{$zone}{$continent}{$country}++;
        $sites{$zone}{$continent}{$country}{$site} = $freq_weight{$frequency};
    }

    use Time::Local;
    my $offset = ((timegm(localtime) - timegm(gmtime)) / 3600);

    local $_;

    ## pick the entry with most country/site/frequency, one level each;
    ## note it has to be sorted -- otherwise we're depending on the hash order.
    ## also, the list context assignment (pick first one) is deliberate.

    my ($continent) = map {
        (sort { ($_->{$b} <=> $_->{$a}) or $b cmp $a } keys(%{$_}))
    } $zones{$offset};

    my ($country) = map {
        (sort { ($_->{$b} <=> $_->{$a}) or $b cmp $a } keys(%{$_}))
    } $countries{$offset}{$continent};

    my ($site) = map {
        (sort { ($_->{$b} <=> $_->{$a}) or $b cmp $a } keys(%{$_}))
    } $sites{$offset}{$continent}{$country};

    return ($continent, $country, $site);
} # _guess_from_timezone


### big big regex, stolen to check if you enter a valid address
{
    my $RFC822PAT; # RFC pattern to match for valid email address

    sub _valid_email {
        my $self = shift;
        if (!$RFC822PAT) {
            my $esc        = '\\\\'; my $Period      = '\.'; my $space      = '\040';
            my $tab         = '\t';  my $OpenBR     = '\[';  my $CloseBR    = '\]';
            my $OpenParen  = '\(';   my $CloseParen  = '\)'; my $NonASCII   = '\x80-\xff';
            my $ctrl        = '\000-\037';                   my $CRlist     = '\012\015';

            my $qtext = qq/[^$esc$NonASCII$CRlist\"]/;
            my $dtext = qq/[^$esc$NonASCII$CRlist$OpenBR$CloseBR]/;
            my $quoted_pair = qq< $esc [^$NonASCII] >; # an escaped character
            my $ctext   = qq< [^$esc$NonASCII$CRlist()] >;
            my $Cnested = qq< $OpenParen $ctext* (?: $quoted_pair $ctext* )* $CloseParen >;
            my $comment = qq< $OpenParen $ctext* (?: (?: $quoted_pair | $Cnested ) $ctext* )* $CloseParen >;
            my $X = qq< [$space$tab]* (?: $comment [$space$tab]* )* >;
            my $atom_char  = qq/[^($space)<>\@,;:\".$esc$OpenBR$CloseBR$ctrl$NonASCII]/;
            my $atom = qq< $atom_char+ (?!$atom_char) >;
            my $quoted_str = qq< \" $qtext * (?: $quoted_pair $qtext * )* \" >;
            my $word = qq< (?: $atom | $quoted_str ) >;
            my $domain_ref  = $atom;
            my $domain_lit  = qq< $OpenBR (?: $dtext | $quoted_pair )* $CloseBR >;
            my $sub_domain  = qq< (?: $domain_ref | $domain_lit) $X >;
            my $domain = qq< $sub_domain (?: $Period $X $sub_domain)* >;
            my $route = qq< \@ $X $domain (?: , $X \@ $X $domain )* : $X >;
            my $local_part = qq< $word $X (?: $Period $X $word $X )* >;
            my $addr_spec  = qq< $local_part \@ $X $domain >;
            my $route_addr = qq[ < $X (?: $route )?  $addr_spec > ];
            my $phrase_ctrl = '\000-\010\012-\037'; # like ctrl, but without tab
            my $phrase_char = qq/[^()<>\@,;:\".$esc$OpenBR$CloseBR$NonASCII$phrase_ctrl]/;
            my $phrase = qq< $word $phrase_char * (?: (?: $comment | $quoted_str ) $phrase_char * )* >;
            $RFC822PAT = qq< $X (?: $addr_spec | $phrase $route_addr) >;
        }

        return scalar ($_[0] =~ /$RFC822PAT/ox);
    }
}






1;


sub _edit {
    my $self    = shift;
    my $conf    = $self->configure_object;
    my $file    = shift || $conf->_config_pm_to_file( $self->config_type );
    my $editor  = shift || $conf->get_program('editor');
    my $term    = $self->term;

    unless( $editor ) {
        print loc("
I'm sorry, I can't find a suitable editor, so I can't offer you
post-configuration editing of the config file

");
        return 1;
    }

    ### save the thing first, so there's something to edit
    $self->_save;

    return !system("$editor $file");
}

sub _save {
    my $self = shift;
    my $conf = $self->configure_object;
    
    return $conf->save( $self->config_type );
}    

1;
