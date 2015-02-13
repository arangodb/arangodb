package CPANPLUS::Internals::Fetch;

use strict;

use CPANPLUS::Error;
use CPANPLUS::Internals::Constants;

use File::Fetch;
use File::Spec;
use Cwd                         qw[cwd];
use IPC::Cmd                    qw[run];
use Params::Check               qw[check];
use Module::Load::Conditional   qw[can_load];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

$Params::Check::VERBOSE = 1;

=pod

=head1 NAME

CPANPLUS::Internals::Fetch

=head1 SYNOPSIS

    my $output = $cb->_fetch(
                        module      => $modobj,
                        fetchdir    => '/path/to/save/to',
                        verbose     => BOOL,
                        force       => BOOL,
                    );

    $cb->_add_fail_host( host => 'foo.com' );
    $cb->_host_ok(       host => 'foo.com' );


=head1 DESCRIPTION

CPANPLUS::Internals::Fetch fetches files from either ftp, http, file
or rsync mirrors.

This is the rough flow:

    $cb->_fetch
        Delegate to File::Fetch;


=head1 METHODS

=cut

=head1 $path = _fetch( module => $modobj, [fetchdir => '/path/to/save/to', fetch_from => 'scheme://path/to/fetch/from', verbose => BOOL, force => BOOL, prefer_bin => BOOL] )

C<_fetch> will fetch files based on the information in a module
object. You always need a module object. If you want a fake module
object for a one-off fetch, look at C<CPANPLUS::Module::Fake>.

C<fetchdir> is the place to save the file to. Usually this
information comes from your configuration, but you can override it
expressly if needed.

C<fetch_from> lets you specify an URI to get this file from. If you
do not specify one, your list of configured hosts will be probed to
download the file from.

C<force> forces a new download, even if the file already exists.

C<verbose> simply indicates whether or not to print extra messages.

C<prefer_bin> indicates whether you prefer the use of commandline
programs over perl modules. Defaults to your corresponding config
setting.

C<_fetch> figures out, based on the host list, what scheme to use and
from there, delegates to C<File::Fetch> do the actual fetching.

Returns the path of the output file on success, false on failure.

Note that you can set a C<blacklist> on certain methods in the config.
Simply add the identifying name of the method (ie, C<lwp>) to:
    $conf->_set_fetch( blacklist => ['lwp'] );

And the C<LWP> function will be skipped by C<File::Fetch>.

=cut

sub _fetch {
    my $self = shift;
    my $conf = $self->configure_object;
    my %hash = @_;

    local $Params::Check::NO_DUPLICATES = 0;

    my ($modobj, $verbose, $force, $fetch_from);
    my $tmpl = {
        module      => { required => 1, allow => IS_MODOBJ, store => \$modobj },
        fetchdir    => { default => $conf->get_conf('fetchdir') },
        fetch_from  => { default => '', store => \$fetch_from },
        force       => { default => $conf->get_conf('force'),
                            store => \$force },
        verbose     => { default => $conf->get_conf('verbose'),
                            store => \$verbose },
        prefer_bin  => { default => $conf->get_conf('prefer_bin') },
    };


    my $args = check( $tmpl, \%hash ) or return;

    ### check if we already downloaded the thing ###
    if( (my $where = $modobj->status->fetch()) && !$force ) {
        msg(loc("Already fetched '%1' to '%2', " .
                "won't fetch again without force",
                $modobj->module, $where ), $verbose );
        return $where;
    }

    my ($remote_file, $local_file, $local_path);

    ### build the local path to downlaod to ###
    {
        $local_path =   $args->{fetchdir} ||
                        File::Spec->catdir(
                            $conf->get_conf('base'),
                            $modobj->path,
                        );

        ### create the path if it doesn't exist ###
        unless( -d $local_path ) {
            unless( $self->_mkdir( dir => $local_path ) ) {
                msg( loc("Could not create path '%1'", $local_path), $verbose);
                return;
            }
        }

        $local_file = File::Spec->rel2abs(
                        File::Spec->catfile(
                                    $local_path,
                                    $modobj->package,
                        )
                    );
    }

    ### do we already have the file? ###
    if( -e $local_file ) {

        if( $args->{force} ) {

            ### some fetches will fail if the files exist already, so let's
            ### delete them first
            unlink $local_file
                or msg( loc("Could not delete %1, some methods may " .
                            "fail to force a download", $local_file), $verbose);
         } else {

            ### store where we fetched it ###
            $modobj->status->fetch( $local_file );

            return $local_file;
        }
    }


    ### we got a custom URI 
    if ( $fetch_from ) {
        my $abs = $self->__file_fetch(  from    => $fetch_from,
                                        to      => $local_path,
                                        verbose => $verbose );
                                        
        unless( $abs ) {
            error(loc("Unable to download '%1'", $fetch_from));
            return;
        }            

        ### store where we fetched it ###
        $modobj->status->fetch( $abs );

        return $abs;

    ### we will get it from one of our mirrors
    } else {
        ### build the remote path to download from ###
        {   $remote_file = File::Spec::Unix->catfile(
                                        $modobj->path,
                                        $modobj->package,
                                    );
            unless( $remote_file ) {
                error( loc('No remote file given for download') );
                return;
            }
        }
    
        ### see if we even have a host or a method to use to download with ###
        my $found_host;
        my @maybe_bad_host;
    
        HOST: {
            ### F*CKING PIECE OF F*CKING p4 SHIT makes 
            ### '$File :: Fetch::SOME_VAR'
            ### into a meta variable and starts substituting the file name...
            ### GRAAAAAAAAAAAAAAAAAAAAAAH!
            ### use ' to combat it!
    
            ### set up some flags for File::Fetch ###
            local $File'Fetch::BLACKLIST    = $conf->_get_fetch('blacklist');
            local $File'Fetch::TIMEOUT      = $conf->get_conf('timeout');
            local $File'Fetch::DEBUG        = $conf->get_conf('debug');
            local $File'Fetch::FTP_PASSIVE  = $conf->get_conf('passive');
            local $File'Fetch::FROM_EMAIL   = $conf->get_conf('email');
            local $File'Fetch::PREFER_BIN   = $conf->get_conf('prefer_bin');
            local $File'Fetch::WARN         = $verbose;
    
    
            ### loop over all hosts we have ###
            for my $host ( @{$conf->get_conf('hosts')} ) {
                $found_host++;
    
                my $where;

                ### file:// uris are special and need parsing
                if( $host->{'scheme'} eq 'file' ) {    
    
                    ### the full path in the native format of the OS
                    my $host_spec = 
                            File::Spec->file_name_is_absolute( $host->{'path'} )
                                ? $host->{'path'}
                                : File::Spec->rel2abs( $host->{'path'} );
    
                    ### there might be volumes involved on vms/win32
                    if( ON_WIN32 or ON_VMS ) {
                        
                        ### now extract the volume in order to be Win32 and 
                        ### VMS friendly.
                        ### 'no_file' indicates that there's no file part
                        ### of this path, so we only get 2 bits returned.
                        my ($vol, $host_path) = File::Spec->splitpath(
                                                    $host_spec, 'no_file' 
                                                );
                        
                        ### and split up the directories
                        my @host_dirs = File::Spec->splitdir( $host_path );
        
                        ### if we got a volume we pretend its a directory for 
                        ### the sake of the file:// url
                        if( defined $vol and $vol ) {
    
                            ### D:\foo\bar needs to be encoded as D|\foo\bar
                            ### For details, see the following link:
                            ###   http://en.wikipedia.org/wiki/File://
                            ### The RFC doesnt seem to address Windows volume
                            ### descriptors but it does address VMS volume
                            ### descriptors, however wikipedia covers a bit of
                            ### history regarding win32
                            $vol =~ s/:$/|/ if ON_WIN32; 
                            
                            $vol =~ s/:// if ON_VMS;
    
                            ### XXX i'm not sure what cases this is addressing.
                            ### this comes straight from dmq's file:// patches
                            ### for win32. --kane
                            ### According to dmq, the best summary is:
                            ### "if file:// urls dont look right on VMS reuse
                            ### the win32 logic and see if that fixes things"
             
                            ### first element not empty? Might happen on VMS.
                            ### prepend the volume in that case.
                            if( $host_dirs[0] ) {
                                unshift @host_dirs, $vol;
                            
                            ### element empty? reuse it to store the volume
                            ### encoded as a directory name. (Win32/VMS)
                            } else {
                                $host_dirs[0] = $vol;
                            }                    
                        }
        
                        ### now it's in UNIX format, which is the same format
                        ### as used for URIs
                        $host_spec = File::Spec::Unix->catdir( @host_dirs ); 
                    }

                    ### now create the file:// uri from the components               
                    $where = CREATE_FILE_URI->(
                                    File::Spec::Unix->catfile(
                                        $host->{'host'} || '',
                                        $host_spec,
                                        $remote_file,
                                    )      
                                );     

                ### its components will be in unix format, for a http://,
                ### ftp:// or any other style of URI
                } else {     
                    my $mirror_path = File::Spec::Unix->catfile(
                                            $host->{'path'}, $remote_file
                                        );
    
                    my %args = ( scheme => $host->{scheme},
                                 host   => $host->{host},
                                 path   => $mirror_path,
                                );
                    
                    $where = $self->_host_to_uri( %args );
                }
    
                my $abs = $self->__file_fetch(  from    => $where, 
                                                to      => $local_path,
                                                verbose => $verbose );    
                
                ### we got a path back?
                if( $abs ) {
                    ### store where we fetched it ###
                    $modobj->status->fetch( $abs );
        
                    ### this host is good, the previous ones are apparently
                    ### not, so mark them as such.
                    $self->_add_fail_host( host => $_ ) for @maybe_bad_host;
                        
                    return $abs;
                }
                
                ### so we tried to get the file but didn't actually fetch it --
                ### there's a chance this host is bad. mark it as such and 
                ### actually flag it back if we manage to get the file 
                ### somewhere else
                push @maybe_bad_host, $host;
            }
        }
    
        $found_host
            ? error(loc("Fetch failed: host list exhausted " .
                        "-- are you connected today?"))
            : error(loc("No hosts found to download from " .
                        "-- check your config"));
    }
    
    return;
}

sub __file_fetch {
    my $self = shift;
    my $conf = $self->configure_object;
    my %hash = @_;

    my ($where, $local_path, $verbose);
    my $tmpl = {
        from    => { required   => 1, store => \$where },
        to      => { required   => 1, store => \$local_path },
        verbose => { default    => $conf->get_conf('verbose'),
                     store      => \$verbose },
    };
    
    check( $tmpl, \%hash ) or return;

    msg(loc("Trying to get '%1'", $where ), $verbose );

    ### build the object ###
    my $ff = File::Fetch->new( uri => $where );

    ### sanity check ###
    error(loc("Bad uri '%1'",$where)), return unless $ff;

    if( my $file = $ff->fetch( to => $local_path ) ) {
        unless( -e $file && -s _ ) {
            msg(loc("'%1' said it fetched '%2', but it was not created",
                    'File::Fetch', $file), $verbose);

        } else {
            my $abs = File::Spec->rel2abs( $file );
            return $abs;
        }

    } else {
        error(loc("Fetching of '%1' failed: %2", $where, $ff->error));
    }

    return;
}

=pod

=head2 _add_fail_host( host => $host_hashref )

Mark a particular host as bad. This makes C<CPANPLUS::Internals::Fetch>
skip it in fetches until this cache is flushed.

=head2 _host_ok( host => $host_hashref )

Query the cache to see if this host is ok, or if it has been flagged
as bad.

Returns true if the host is ok, false otherwise.

=cut

{   ### caching functions ###

    sub _add_fail_host {
        my $self = shift;
        my %hash = @_;

        my $host;
        my $tmpl = {
            host => { required      => 1, default   => {},
                      strict_type   => 1, store     => \$host },
        };

        check( $tmpl, \%hash ) or return;

        return $self->_hosts->{$host} = 1;
    }

    sub _host_ok {
        my $self = shift;
        my %hash = @_;

        my $host;
        my $tmpl = {
            host => { required => 1, store => \$host },
        };

        check( $tmpl, \%hash ) or return;

        return $self->_hosts->{$host} ? 0 : 1;
    }
}


1;

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
