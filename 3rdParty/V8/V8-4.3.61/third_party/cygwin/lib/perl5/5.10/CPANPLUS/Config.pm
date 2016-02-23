package CPANPLUS::Config;

use strict;
use warnings;

use base 'Object::Accessor';

use base 'CPANPLUS::Internals::Utils';

use Config;
use File::Spec;
use Module::Load;
use CPANPLUS;
use CPANPLUS::Error;
use CPANPLUS::Internals::Constants;

use File::Basename              qw[dirname];
use IPC::Cmd                    qw[can_run];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';
use Module::Load::Conditional   qw[check_install];


=pod

=head1 NAME

CPANPLUS::Config

=head1 DESCRIPTION

This module contains defaults and heuristics for configuration 
information for CPANPLUS. To change any of these values, please
see the documentation in C<CPANPLUS::Configure>.

Below you'll find a list of configuration types and keys, and
their meaning.

=head1 CONFIGURATION

=cut

### BAH! you can't have POD interleaved with a hash
### declaration.. so declare every entry seperatedly :(
my $Conf = {
    '_fetch' => {
        'blacklist' => [ 'ftp' ],
    },
    
    ### _source, _build and _mirror are supposed to be static
    ### no changes should be needed unless pause/cpan changes
    '_source' => {
        'hosts'             => 'MIRRORED.BY',
        'auth'              => '01mailrc.txt.gz',
        'stored'            => 'sourcefiles',
        'dslip'             => '03modlist.data.gz',
        'update'            => '86400',
        'mod'               => '02packages.details.txt.gz',
        'custom_index'      => 'packages.txt',
    },
    '_build' => {
        'plugins'           => 'plugins',
        'moddir'            => 'build',
        'startdir'          => '',
        'distdir'           => 'dist',
        'autobundle'        => 'autobundle',
        'autobundle_prefix' => 'Snapshot',
        'autdir'            => 'authors',
        'install_log_dir'   => 'install-logs',
        'custom_sources'    => 'custom-sources',
        'sanity_check'      => 1,
    },
    '_mirror' => {
        'base'              => 'authors/id/',
        'auth'              => 'authors/01mailrc.txt.gz',
        'dslip'             => 'modules/03modlist.data.gz',
        'mod'               => 'modules/02packages.details.txt.gz'
    },
};

=head2 Section 'conf'

=over 4

=item hosts

An array ref containing hosts entries to be queried for packages.

An example entry would like this:

    {   'scheme' => 'ftp',
        'path' => '/pub/CPAN/',
        'host' => 'ftp.cpan.org'
    },

=cut

    ### default host list
    $Conf->{'conf'}->{'hosts'} = [
            {
                'scheme' => 'ftp',
                'path' => '/pub/CPAN/',
                'host' => 'ftp.cpan.org'
            },
            {
                'scheme' => 'http',
                'path' => '/',
                'host' => 'www.cpan.org'
            },
            {
                'scheme' => 'ftp',
                'path' => '/pub/CPAN/',
                'host' => 'ftp.nl.uu.net'
            },
            {
                'scheme' => 'ftp',
                'path' => '/pub/CPAN/',
                'host' => 'cpan.valueclick.com'
            },
            {
                'scheme' => 'ftp',
                'path' => '/pub/languages/perl/CPAN/',
                'host' => 'ftp.funet.fi'
            }
        ];
        
=item allow_build_interactivity

Boolean flag to indicate whether 'perl Makefile.PL' and similar
are run interactively or not. Defaults to 'true'.

=cut

        $Conf->{'conf'}->{'allow_build_interactivity'} = 1;

=item base

The directory CPANPLUS keeps all it's build and state information in.
Defaults to ~/.cpanplus.

=cut

       $Conf->{'conf'}->{'base'} = File::Spec->catdir(
                                        __PACKAGE__->_home_dir, DOT_CPANPLUS );

=item buildflags

Any flags to be passed to 'perl Build.PL'. See C<perldoc Module::Build>
for details. Defaults to an empty string.

=cut

        $Conf->{'conf'}->{'buildflags'} = '';

=item cpantest

Boolean flag to indicate whether or not to mail test results of module
installations to C<http://testers.cpan.org>. Defaults to 'false'.

=cut

        $Conf->{'conf'}->{'cpantest'} = 0;

=item cpantest_mx

String holding an explicit mailserver to use when sending out emails
for C<http://testers.cpan.org>. An empty string will use your system
settings. Defaults to an empty string.

=cut

        $Conf->{'conf'}->{'cpantest_mx'} = '';

=item debug

Boolean flag to enable or disable extensive debuggging information.
Defaults to 'false'.

=cut

        $Conf->{'conf'}->{'debug'} = 0;

=item dist_type

Default distribution type to use when building packages. See C<cpan2dist>
or C<CPANPLUS::Dist> for details. An empty string will not use any 
package building software. Defaults to an empty string.

=cut

        $Conf->{'conf'}->{'dist_type'} = '';

=item email

Email address to use for anonymous ftp access and as C<from> address
when sending emails. Defaults to an C<example.com> address.

=cut

        $Conf->{'conf'}->{'email'} = DEFAULT_EMAIL;

=item extractdir

String containing the directory where fetched archives should be 
extracted. An empty string will use a directory under your C<base>
directory. Defaults to an empty string.

=cut

        $Conf->{'conf'}->{'extractdir'} = '';

=item fetchdir

String containing the directory where fetched archives should be 
stored. An empty string will use a directory under your C<base>
directory. Defaults to an empty string.

=cut

        $Conf->{'conf'}->{'fetchdir'} = '';

=item flush

Boolean indicating whether build failures, cache dirs etc should
be flushed after every operation or not. Defaults to 'true'.

=cut

        $Conf->{'conf'}->{'flush'} = 1;

=item force

Boolean indicating whether files should be forcefully overwritten
if they exist, modules should be installed when they fail tests,
etc. Defaults to 'false'.

=cut

        $Conf->{'conf'}->{'force'} = 0;

=item lib

An array ref holding directories to be added to C<@INC> when CPANPLUS
starts up. Defaults to an empty array reference.

=cut

        $Conf->{'conf'}->{'lib'} = [];

=item makeflags

A string holding flags that will be passed to the C<make> program
when invoked. Defaults to an empty string.

=cut

        $Conf->{'conf'}->{'makeflags'} = '';

=item makemakerflags

A string holding flags that will be passed to C<perl Makefile.PL>
when invoked. Defaults to an empty string.

=cut

        $Conf->{'conf'}->{'makemakerflags'} = '';

=item md5

A boolean indicating whether or not md5 checks should be done when
an archive is fetched. Defaults to 'true' if you have C<Digest::MD5>
installed, 'false' otherwise.

=cut

        $Conf->{'conf'}->{'md5'} = ( 
                            check_install( module => 'Digest::MD5' ) ? 1 : 0 );

=item no_update

A boolean indicating whether or not C<CPANPLUS>' source files should be
updated or not. Defaults to 'false'.

=cut

        $Conf->{'conf'}->{'no_update'} = 0;

=item passive

A boolean indicating whether or not to use passive ftp connections.
Defaults to 'true'.

=cut

        $Conf->{'conf'}->{'passive'} = 1;

=item prefer_bin

A boolean indicating whether or not to prefer command line programs 
over perl modules. Defaults to 'false' unless you do not have 
C<Compress::Zlib> installed (as that would mean we could not extract
C<.tar.gz> files)

=cut
        ### if we dont have c::zlib, we'll need to use /bin/tar or we
        ### can not extract any files. Good time to change the default
        $Conf->{'conf'}->{'prefer_bin'} = 
                                (eval {require Compress::Zlib; 1} ? 0 : 1 );

=item prefer_makefile

A boolean indicating whether or not prefer a C<Makefile.PL> over a 
C<Build.PL> file if both are present. Defaults to 'true'.

=cut

        $Conf->{'conf'}->{'prefer_makefile'} = 1;

=item prereqs

A digit indicating what to do when a package you are installing has a
prerequisite. Options are:

    0   Do not install
    1   Install
    2   Ask
    3   Ignore  (dangerous, install will probably fail!)

The default is to ask.

=cut

        $Conf->{'conf'}->{'prereqs'} = PREREQ_ASK;

=item shell

A string holding the shell class you wish to start up when starting
C<CPANPLUS> in interactive mode.

Defaults to C<CPANPLUS::Shell::Default>, the default CPANPLUS shell.

=cut

        $Conf->{'conf'}->{'shell'} = 'CPANPLUS::Shell::Default';

=item show_startup_tip

A boolean indicating whether or not to show start up tips in the 
interactive shell. Defaults to 'true'.

=cut

        $Conf->{'conf'}->{'show_startup_tip'} = 1;

=item signature

A boolean indicating whether or not check signatures if packages are
signed. Defaults to 'true' if you have C<gpg> or C<Crypt::OpenPGP> 
installed, 'false' otherwise.

=cut

        $Conf->{'conf'}->{'signature'} = do {
            check_install( module => 'Module::Signature', version => '0.06' )
            and ( can_run('gpg') || 
                  check_install(module => 'Crypt::OpenPGP')
            );
        } ? 1 : 0;

=item skiptest

A boolean indicating whether or not to skip tests when installing modules.
Defaults to 'false'.

=cut

        $Conf->{'conf'}->{'skiptest'} = 0;

=item storable

A boolean indicating whether or not to use C<Storable> to write compiled
source file information to disk. This makes for faster startup and look
up times, but takes extra diskspace. Defaults to 'true' if you have 
C<Storable> installed and 'false' if you don't.

=cut

       $Conf->{'conf'}->{'storable'} = 
                        ( check_install( module => 'Storable' ) ? 1 : 0 );

=item timeout

Digit indicating the time before a fetch request times out (in seconds).
Defaults to 300.

=cut

        $Conf->{'conf'}->{'timeout'} = 300;

=item verbose

A boolean indicating whether or not C<CPANPLUS> runs in verbose mode.
Defaults to 'true' if you have the environment variable 
C<PERL5_CPANPLUS_VERBOSE> set to true, 'false' otherwise.

It is recommended you run with verbose enabled, but it is disabled
for historical reasons.

=cut

        $Conf->{'conf'}->{'verbose'} = $ENV{PERL5_CPANPLUS_VERBOSE} || 0;

=item write_install_log

A boolean indicating whether or not to write install logs after installing
a module using the interactive shell. Defaults to 'true'.


=cut

        $Conf->{'conf'}->{'write_install_logs'} = 1;

=back
    
=head2 Section 'program'

=cut

    ### Paths get stripped of whitespace on win32 in the constructor
    ### sudo gets emptied if there's no need for it in the constructor

=over 4

=item editor

A string holding the path to your editor of choice. Defaults to your
$ENV{EDITOR}, $ENV{VISIUAL}, 'vi' or 'pico' programs, in that order.

=cut

        $Conf->{'program'}->{'editor'} = do {
            $ENV{'EDITOR'}  || $ENV{'VISUAL'} ||
            can_run('vi')   || can_run('pico')
        };

=item make

A string holding the path to your C<make> binary. Looks for the C<make>
program used to build perl or failing that, a C<make> in your path.

=cut

        $Conf->{'program'}->{'make'} = 
            can_run($Config{'make'}) || can_run('make');

=item pager

A string holding the path to your pager of choice. Defaults to your
$ENV{PAGER}, 'less' or 'more' programs, in that order.

=cut

        $Conf->{'program'}->{'pager'} = 
            $ENV{'PAGER'} || can_run('less') || can_run('more');

        ### no one uses this feature anyway, and it's only working for EU::MM
        ### and not for module::build
        #'perl'      => '',

=item shell

A string holding the path to your login shell of choice. Defaults to your
$ENV{SHELL} setting, or $ENV{COMSPEC} on Windows.

=cut

        $Conf->{'program'}->{'shell'} = $^O eq 'MSWin32' 
                                        ? $ENV{COMSPEC} 
                                        : $ENV{SHELL};

=item sudo

A string holding the path to your C<sudo> binary if your install path
requires super user permissions. Looks for C<sudo> in your path, or 
remains empty if you do not require super user permissiosn to install.

=cut

        $Conf->{'program'}->{'sudo'} = do {

            ### let's assume you dont need sudo,
            ### unless one of the below criteria tells us otherwise
            my $sudo = undef;
            
            ### you're a normal user, you might need sudo
            if( $> ) {
    
                ### check for all install dirs!
                ### installsiteman3dir is a 5.8'ism.. don't check
                ### it on 5.6.x...            
                ### you have write permissions to the installdir,
                ### you don't need sudo
                if( -w $Config{'installsitelib'} &&
                    ( defined $Config{'installsiteman3dir'} && 
                      -w $Config{'installsiteman3dir'} 
                    ) && -w $Config{'installsitebin'} 
                ) {                    
                    $sudo = undef;
                    
                ### you have PERL_MM_OPT set to some alternate
                ### install place. You probably have write permissions
                ### to that
                } elsif ( $ENV{'PERL_MM_OPT'} and 
                          $ENV{'PERL_MM_OPT'} =~ /INSTALL|LIB|PREFIX/
                ) {
                    $sudo = undef;

                ### you probably don't have write permissions
                } else {                
                    $sudo = can_run('sudo');
                }
            }  
            
            ### and return the value
            $sudo;
        };

=item perlwrapper

A string holding the path to the C<cpanp-run-perl> utility bundled
with CPANPLUS, which is used to enable autoflushing in spawned processes.

=cut

        ### perlwrapper that allows us to turn on autoflushing                        
        $Conf->{'program'}->{'perlwrapper'} = sub { 
            my $name = 'cpanp-run-perl';

            my @bins = do{
                require Config;
                my $ver  = $Config::Config{version};
                
                ### if we are running with 'versiononly' enabled,
                ### all binaries will have the perlversion appended
                ### ie, cpanp will become cpanp5.9.5
                ### so prefer the versioned binary in that case
                $Config::Config{versiononly}
                        ? ($name.$ver, $name)
                        : ($name, $name.$ver);
            };

            ### patch from Steve Hay Fri 29 Jun 2007 14:26:02 GMT+02:00
            ### Msg-Id: <4684FA5A.7030506@uk.radan.com>
            ### look for files with a ".bat" extension as well on Win32
            @bins = map { $_, "$_.bat" } @bins if $^O eq 'MSWin32';

            my $path;
            BIN: for my $bin (@bins) {
                
                ### parallel to your cpanp/cpanp-boxed
                my $maybe = File::Spec->rel2abs(
                                File::Spec->catfile( dirname($0), $bin )
                            );        
                $path = $maybe and last BIN if -f $maybe;
        
                ### parallel to your CPANPLUS.pm:
                ### $INC{cpanplus}/../bin/cpanp-run-perl
                $maybe = File::Spec->rel2abs(
                            File::Spec->catfile( 
                                dirname($INC{'CPANPLUS.pm'}),
                                '..',   # lib dir
                                'bin',  # bin dir
                                $bin,   # script
                            )
                         );
                $path = $maybe and last BIN if -f $maybe;
                         
                ### you installed CPANPLUS in a custom prefix,
                ### so go paralel to /that/. PREFIX=/tmp/cp
                ### would put cpanp-run-perl in /tmp/cp/bin and
                ### CPANPLUS.pm in
                ### /tmp/cp/lib/perl5/site_perl/5.8.8
                $maybe = File::Spec->rel2abs(
                            File::Spec->catfile( 
                                dirname( $INC{'CPANPLUS.pm'} ),
                                '..', '..', '..', '..', # 4x updir
                                'bin',                  # bin dir
                                $bin,                   # script
                            )
                         );
                $path = $maybe and last BIN if -f $maybe;

                ### in your path -- take this one last, the
                ### previous two assume extracted tarballs
                ### or user installs
                ### note that we don't use 'can_run' as it's
                ### not an executable, just a wrapper...
                ### prefer anything that's found in the path paralel to your $^X
                for my $dir (File::Spec->rel2abs( dirname($^X) ),
                             split(/\Q$Config::Config{path_sep}\E/, $ENV{PATH}),
                             File::Spec->curdir, 
                ) {             

                    ### On VMS the path could be in UNIX format, and we
                    ### currently need it to be in VMS format
                    $dir = VMS::Filespec::vmspath($dir) if ON_VMS;

                    $maybe = File::Spec->catfile( $dir, $bin );
                    $path = $maybe and last BIN if -f $maybe;
                }
            }          
                
            ### we should have a $path by now ideally, if so return it
            return $path if defined $path;
            
            ### if not, warn about it and give sensible default.
            ### XXX try to be a no-op instead then.. 
            ### cross your fingers...
            ### pass '-P' to perl: "run program through C 
            ### preprocessor before compilation"
            ### XXX using -P actually changes the way some Makefile.PLs
            ### are executed, so don't do that... --kane
            error(loc(
                "Could not find the '%1' binary in your path".
                "--this may be a problem.\n".
                "Please locate this program and set ".
                "your '%2' config entry to its path.\n".
                "From the default shell, you can do this by typing:\n\n".
                "  %3\n".
                "  %4\n",
                $name, 'perlwrapper', 
                's program perlwrapper FULL_PATH_TO_CPANP_RUN_PERL',
                's save'
             ));                                        
             return '';
        }->();
        
=back

=cut

sub new {
    my $class   = shift;
    my $obj     = $class->SUPER::new;

    $obj->mk_accessors( keys %$Conf );

    for my $acc ( keys %$Conf ) {
        my $subobj = Object::Accessor->new;
        $subobj->mk_accessors( keys %{$Conf->{$acc}} );

        ### read in all the settings from the sub accessors;
        for my $subacc ( $subobj->ls_accessors ) {
            $subobj->$subacc( $Conf->{$acc}->{$subacc} );
        }

        ### now store it in the parent object
        $obj->$acc( $subobj );
    }
    
    $obj->_clean_up_paths;
    
    ### shut up IPC::Cmd warning about not findin IPC::Run on win32
    $IPC::Cmd::WARN = 0;
    
    return $obj;
}

sub _clean_up_paths {
    my $self = shift;

    ### clean up paths if we are on win32
    if( $^O eq 'MSWin32' ) {
        for my $pgm ( $self->program->ls_accessors ) {
            my $path = $self->program->$pgm;

            ### paths with whitespace needs to be shortened
            ### for shell outs.
            if ($path and $path =~ /\s+/) {
                my($prog, $args);

                ### patch from Steve Hay, 13nd of June 2007
                ### msg-id: <467012A4.6060705@uk.radan.com>
                ### windows directories are not allowed to end with 
                ### a space, so any occurrence of '\w\s+/\w+' means
                ### we're dealing with arguments, not directory
                ### names.
                if ($path =~ /^(.*?)(\s+\/.*$)/) {
                    ($prog, $args) = ($1, $2);
                
                ### otherwise, there are no arguments
                } else {
                    ($prog, $args) = ($path, '');
                }
                
                $prog = Win32::GetShortPathName( $prog );
                $self->program->$pgm( $prog . $args );
            }
        }
    }

    return 1;
}

1;

=pod

=head1 BUG REPORTS

Please report bugs or other issues to E<lt>bug-cpanplus@rt.cpan.org<gt>.

=head1 AUTHOR

This module by Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 COPYRIGHT

The CPAN++ interface (of which this module is a part of) is copyright (c) 
2001 - 2007, Jos Boumans E<lt>kane@cpan.orgE<gt>. All rights reserved.

This library is free software; you may redistribute and/or modify it 
under the same terms as Perl itself.

=head1 SEE ALSO

L<CPANPLUS::Backend>, L<CPANPLUS::Configure::Setup>, L<CPANPLUS::Configure>

=cut

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
