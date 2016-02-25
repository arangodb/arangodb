package CPANPLUS::Internals::Utils;

use strict;

use CPANPLUS::Error;
use CPANPLUS::Internals::Constants;

use Cwd;
use File::Copy;
use Params::Check               qw[check];
use Module::Load::Conditional   qw[can_load];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

local $Params::Check::VERBOSE = 1;

=pod

=head1 NAME

CPANPLUS::Internals::Utils

=head1 SYNOPSIS

    my $bool = $cb->_mkdir( dir => 'blah' );
    my $bool = $cb->_chdir( dir => 'blah' );
    my $bool = $cb->_rmdir( dir => 'blah' );

    my $bool = $cb->_move( from => '/some/file', to => '/other/file' );
    my $bool = $cb->_move( from => '/some/dir',  to => '/other/dir' );

    my $cont = $cb->_get_file_contents( file => '/path/to/file' );


    my $version = $cb->_perl_version( perl => $^X );

=head1 DESCRIPTION

C<CPANPLUS::Internals::Utils> holds a few convenience functions for
CPANPLUS libraries.

=head1 METHODS

=head2 $cb->_mkdir( dir => '/some/dir' )

C<_mkdir> creates a full path to a directory.

Returns true on success, false on failure.

=cut

sub _mkdir {
    my $self = shift;

    my %hash = @_;

    my $tmpl = {
        dir     => { required => 1 },
    };

    my $args = check( $tmpl, \%hash ) or (
        error(loc( Params::Check->last_error ) ), return
    );       

    unless( can_load( modules => { 'File::Path' => 0.0 } ) ) {
        error( loc("Could not use File::Path! This module should be core!") );
        return;
    }

    eval { File::Path::mkpath($args->{dir}) };

    if($@) {
        chomp($@);
        error(loc(qq[Could not create directory '%1': %2], $args->{dir}, $@ ));
        return;
    }

    return 1;
}

=pod

=head2 $cb->_chdir( dir => '/some/dir' )

C<_chdir> changes directory to a dir.

Returns true on success, false on failure.

=cut

sub _chdir {
    my $self = shift;
    my %hash = @_;

    my $tmpl = {
        dir     => { required => 1, allow => DIR_EXISTS },
    };

    my $args = check( $tmpl, \%hash ) or return;

    unless( chdir $args->{dir} ) {
        error( loc(q[Could not chdir into '%1'], $args->{dir}) );
        return;
    }

    return 1;
}

=pod

=head2 $cb->_rmdir( dir => '/some/dir' );

Removes a directory completely, even if it is non-empty.

Returns true on success, false on failure.

=cut

sub _rmdir {
    my $self = shift;
    my %hash = @_;

    my $tmpl = {
        dir     => { required => 1, allow => IS_DIR },
    };

    my $args = check( $tmpl, \%hash ) or return;

    unless( can_load( modules => { 'File::Path' => 0.0 } ) ) {
        error( loc("Could not use File::Path! This module should be core!") );
        return;
    }

    eval { File::Path::rmtree($args->{dir}) };

    if($@) {
        chomp($@);
        error(loc(qq[Could not delete directory '%1': %2], $args->{dir}, $@ ));
        return;
    }

    return 1;
}

=pod

=head2 $cb->_perl_version ( perl => 'some/perl/binary' );

C<_perl_version> returns the version of a certain perl binary.
It does this by actually running a command.

Returns the perl version on success and false on failure.

=cut

sub _perl_version {
    my $self = shift;
    my %hash = @_;

    my $perl;
    my $tmpl = {
        perl    => { required => 1, store => \$perl },
    };

    check( $tmpl, \%hash ) or return;
    
    my $perl_version;
    ### special perl, or the one we are running under?
    if( $perl eq $^X ) {
        ### just load the config        
        require Config;
        $perl_version = $Config::Config{version};
        
    } else {
        my $cmd  = $perl .
                ' -MConfig -eprint+Config::config_vars+version';
        ($perl_version) = (`$cmd` =~ /version='(.*)'/);
    }
    
    return $perl_version if defined $perl_version;
    return;
}

=pod

=head2 $cb->_version_to_number( version => $version );

Returns a proper module version, or '0.0' if none was available.

=cut

sub _version_to_number {
    my $self = shift;
    my %hash = @_;

    my $version;
    my $tmpl = {
        version => { default => '0.0', store => \$version },
    };

    check( $tmpl, \%hash ) or return;

    return $version if $version =~ /^\.?\d/;
    return '0.0';
}

=pod

=head2 $cb->_whoami

Returns the name of the subroutine you're currently in.

=cut

sub _whoami { my $name = (caller 1)[3]; $name =~ s/.+:://; $name }

=pod

=head2 _get_file_contents( file => $file );

Returns the contents of a file

=cut

sub _get_file_contents {
    my $self = shift;
    my %hash = @_;

    my $file;
    my $tmpl = {
        file => { required => 1, store => \$file }
    };

    check( $tmpl, \%hash ) or return;

    my $fh = OPEN_FILE->($file) or return;
    my $contents = do { local $/; <$fh> };

    return $contents;
}

=pod $cb->_move( from => $file|$dir, to => $target );

Moves a file or directory to the target.

Returns true on success, false on failure.

=cut

sub _move {
    my $self = shift;
    my %hash = @_;

    my $from; my $to;
    my $tmpl = {
        file    => { required => 1, allow => [IS_FILE,IS_DIR],
                        store => \$from },
        to      => { required => 1, store => \$to }
    };

    check( $tmpl, \%hash ) or return;

    if( File::Copy::move( $from, $to ) ) {
        return 1;
    } else {
        error(loc("Failed to move '%1' to '%2': %3", $from, $to, $!));
        return;
    }
}

=pod $cb->_copy( from => $file|$dir, to => $target );

Moves a file or directory to the target.

Returns true on success, false on failure.

=cut

sub _copy {
    my $self = shift;
    my %hash = @_;
    
    my($from,$to);
    my $tmpl = {
        file    =>{ required => 1, allow => [IS_FILE,IS_DIR],
                        store => \$from },
        to      => { required => 1, store => \$to }
    };

    check( $tmpl, \%hash ) or return;

    if( File::Copy::copy( $from, $to ) ) {
        return 1;
    } else {
        error(loc("Failed to copy '%1' to '%2': %3", $from, $to, $!));
        return;
    }
}

=head2 $cb->_mode_plus_w( file => '/path/to/file' );

Sets the +w bit for the file.

Returns true on success, false on failure.

=cut

sub _mode_plus_w {
    my $self = shift;
    my %hash = @_;
    
    require File::stat;
    
    my $file;
    my $tmpl = {
        file    => { required => 1, allow => IS_FILE, store => \$file },
    };
    
    check( $tmpl, \%hash ) or return;
    
    ### set the mode to +w for a file and +wx for a dir
    my $x       = File::stat::stat( $file );
    my $mask    = -d $file ? 0100 : 0200;
    
    if( $x and chmod( $x->mode|$mask, $file ) ) {
        return 1;

    } else {        
        error(loc("Failed to '%1' '%2': '%3'", 'chmod +w', $file, $!));
        return;
    }
}    

=head2 $uri = $cb->_host_to_uri( scheme => SCHEME, host => HOST, path => PATH );

Turns a CPANPLUS::Config style C<host> entry into an URI string.

Returns the uri on success, and false on failure

=cut

sub _host_to_uri {
    my $self = shift;
    my %hash = @_;
    
    my($scheme, $host, $path);
    my $tmpl = {
        scheme  => { required => 1,             store => \$scheme },
        host    => { default  => 'localhost',   store => \$host },
        path    => { default  => '',            store => \$path },
    };       

    check( $tmpl, \%hash ) or return;

    ### it's an URI, so unixify the path.
    ### VMS has a special method for just that
    $path = ON_VMS
                ? VMS::Filespec::unixify($path) 
                : File::Spec::Unix->catdir( File::Spec->splitdir( $path ) );

    return "$scheme://" . File::Spec::Unix->catdir( $host, $path ); 
}

=head2 $cb->_vcmp( VERSION, VERSION );

Normalizes the versions passed and does a '<=>' on them, returning the result.

=cut

sub _vcmp {
    my $self = shift;
    my ($x, $y) = @_;
    
    s/_//g foreach $x, $y;

    return $x <=> $y;
}

=head2 $cb->_home_dir

Returns the user's homedir, or C<cwd> if it could not be found

=cut

sub _home_dir {
    my @os_home_envs = qw( APPDATA HOME USERPROFILE WINDIR SYS$LOGIN );

    for my $env ( @os_home_envs ) {
        next unless exists $ENV{ $env };
        next unless defined $ENV{ $env } && length $ENV{ $env };
        return $ENV{ $env } if -d $ENV{ $env };
    }

    return cwd();
}

=head2 $path = $cb->_safe_path( path => $path );

Returns a path that's safe to us on Win32 and VMS. 

Only cleans up the path on Win32 if the path exists.

On VMS, it encodes dots to _ using C<VMS::Filespec::vmsify>

=cut

sub _safe_path {
    my $self = shift;
    
    my %hash = @_;
    
    my $path;
    my $tmpl = {
        path  => { required => 1,     store => \$path },
    };       

    check( $tmpl, \%hash ) or return;
    
    if( ON_WIN32 ) {
        ### only need to fix it up if there's spaces in the path   
        return $path unless $path =~ /\s+/;
        
        ### clean up paths if we are on win32
        return Win32::GetShortPathName( $path ) || $path;

    } elsif ( ON_VMS ) {
        ### XXX According to John Malmberg, there's an VMS issue:
        ### catdir on VMS can not currently deal with directory components
        ### with dots in them.  
        ### Fixing this is a a three step procedure, which will work for 
        ### VMS in its traditional ODS-2 mode, and it will also work if 
        ### VMS is in the ODS-5 mode that is being implemented.
        ### If the path is already in VMS syntax, assume that we are done.
 
        ### VMS format is a path with a trailing ']' or ':'
        return $path if $path =~ /\:|\]$/;

        ### 1. Make sure that the value to be converted, $path is 
        ### in UNIX directory syntax by appending a '/' to it.
        $path .= '/' unless $path =~ m|/$|;

        ### 2. Use VMS::Filespec::vmsify($path . '/') to convert the dots to
        ### underscores if needed.  The trailing '/' is needed as so that
        ### C<vmsify> knows that it should use directory translation instead of
        ### filename translation, as filename translation leaves one dot.
        $path = VMS::Filespec::vmsify( $path );

        ### 3. Use $path = File::Spec->splitdir( VMS::Filespec::vmsify( 
        ### $path . '/') to remove the directory delimiters.

        ### From John Malmberg:
        ### File::Spec->catdir will put the path back together.
        ### The '/' trick only works if the string is a directory name 
        ### with UNIX style directory delimiters or no directory delimiters.  
        ### It is to force vmsify to treat the input specification as UNIX.
        ###
        ### There is a VMS::Filespec::unixpath() to do the appending of the '/'
        ### to the specification, which will do a VMS::Filespec::vmsify() 
        ### if needed.
        ### However it is not a good idea to call vmsify() on a pathname
        ### returned by unixify(), and it is not a good idea to call unixify()
        ### on a pathname returned by vmsify().  Because of the nature of the
        ### conversion, not all file specifications can make the round trip.
        ###
        ### I think that directory specifications can safely make the round
        ### trip, but not ones containing filenames.
        $path = File::Spec->catdir( File::Spec->splitdir( $path ) )
    }
    
    return $path;
}


=head2 ($pkg, $version, $ext) = $cb->_split_package_string( package => PACKAGE_STRING );

Splits the name of a CPAN package string up in it's package, version 
and extension parts.

For example, C<Foo-Bar-1.2.tar.gz> would return the following parts:

    Package:    Foo-Bar
    Version:    1.2
    Extension:  tar.gz

=cut

{   my $del_re = qr/[-_\+]/i;           # delimiter between elements
    my $pkg_re = qr/[a-z]               # any letters followed by 
                    [a-z\d]*            # any letters, numbers
                    (?i:\.pm)?          # followed by '.pm'--authors do this :(
                    (?:                 # optionally repeating:
                        $del_re         #   followed by a delimiter
                        [a-z]           #   any letters followed by 
                        [a-z\d]*        #   any letters, numbers                        
                        (?i:\.pm)?      # followed by '.pm'--authors do this :(
                    )*
                /xi;   
    
    my $ver_re = qr/[a-z]*\d+[a-z]*     # contains a digit and possibly letters
                    (?:
                        [-._]           # followed by a delimiter
                        [a-z\d]+        # and more digits and or letters
                    )*?
                /xi;
 
    my $ext_re = qr/[a-z]               # a letter, followed by
                    [a-z\d]*            # letters and or digits, optionally
                    (?:                 
                        \.              #   followed by a dot and letters
                        [a-z\d]+        #   and or digits (like .tar.bz2)
                    )?                  #   optionally
                /xi;

    my $ver_ext_re = qr/
                        ($ver_re+)      # version, optional
                        (?:
                            \.          # a literal .
                            ($ext_re)   # extension,
                        )?              # optional, but requires version
                /xi;
                
    ### composed regex for CPAN packages
    my $full_re = qr/
                    ^
                    ($pkg_re+)          # package
                    (?: 
                        $del_re         # delimiter
                        $ver_ext_re     # version + extension
                    )?
                    $                    
                /xi;
                
    ### composed regex for perl packages
    my $perl    = PERL_CORE;
    my $perl_re = qr/
                    ^
                    ($perl)             # package name for 'perl'
                    (?:
                        $ver_ext_re     # version + extension
                    )?
                    $
                /xi;       


sub _split_package_string {
        my $self = shift;
        my %hash = @_;
        
        my $str;
        my $tmpl = { package => { required => 1, store => \$str } };
        check( $tmpl, \%hash ) or return;
        
        
        ### 2 different regexes, one for the 'perl' package, 
        ### one for ordinary CPAN packages.. try them both, 
        ### first match wins.
        for my $re ( $full_re, $perl_re ) {
            
            ### try the next if the match fails
            $str =~ $re or next;

            my $pkg = $1 || ''; 
            my $ver = $2 || '';
            my $ext = $3 || '';

            ### this regex resets the capture markers!
            ### strip the trailing delimiter
            $pkg =~ s/$del_re$//;
            
            ### strip the .pm package suffix some authors insist on adding
            $pkg =~ s/\.pm$//i;

            return ($pkg, $ver, $ext );
        }
        
        return;
    }
}

{   my %escapes = map {
        chr($_) => sprintf("%%%02X", $_)
    } 0 .. 255;  
    
    sub _uri_encode {
        my $self = shift;
        my %hash = @_;
        
        my $str;
        my $tmpl = {
            uri => { store => \$str, required => 1 }
        };
        
        check( $tmpl, \%hash ) or return;

        ### XXX taken straight from URI::Encode
        ### Default unsafe characters.  RFC 2732 ^(uric - reserved)
        $str =~ s|([^A-Za-z0-9\-_.!~*'()])|$escapes{$1}|g;
    
        return $str;          
    }
    
    
    sub _uri_decode {
        my $self = shift;
        my %hash = @_;
        
        my $str;
        my $tmpl = {
            uri => { store => \$str, required => 1 }
        };
        
        check( $tmpl, \%hash ) or return;
    
        ### XXX use unencode routine in utils?
        $str =~ s/%([0-9A-Fa-f]{2})/chr(hex($1))/eg; 
    
        return $str;    
    }
}

sub _update_timestamp {
    my $self = shift;
    my %hash = @_;
    
    my $file;
    my $tmpl = {
        file => { required => 1, store => \$file, allow => FILE_EXISTS }
    };
    
    check( $tmpl, \%hash ) or return;
   
    ### `touch` the file, so windoze knows it's new -jmb
    ### works on *nix too, good fix -Kane
    ### make sure it is writable first, otherwise the `touch` will fail

    my $now = time;
    unless( chmod( 0644, $file) && utime ($now, $now, $file) ) {
        error( loc("Couldn't touch %1", $file) );
        return;
    }
    
    return 1;
}


1;

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
