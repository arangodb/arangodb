package File::Spec::OS2;

use strict;
use vars qw(@ISA $VERSION);
require File::Spec::Unix;

$VERSION = '3.2701';

@ISA = qw(File::Spec::Unix);

sub devnull {
    return "/dev/nul";
}

sub case_tolerant {
    return 1;
}

sub file_name_is_absolute {
    my ($self,$file) = @_;
    return scalar($file =~ m{^([a-z]:)?[\\/]}is);
}

sub path {
    my $path = $ENV{PATH};
    $path =~ s:\\:/:g;
    my @path = split(';',$path);
    foreach (@path) { $_ = '.' if $_ eq '' }
    return @path;
}

sub _cwd {
    # In OS/2 the "require Cwd" is unnecessary bloat.
    return Cwd::sys_cwd();
}

my $tmpdir;
sub tmpdir {
    return $tmpdir if defined $tmpdir;
    my @d = @ENV{qw(TMPDIR TEMP TMP)};	# function call could autovivivy
    $tmpdir = $_[0]->_tmpdir( @d, '/tmp', '/'  );
}

sub catdir {
    my $self = shift;
    my @args = @_;
    foreach (@args) {
	tr[\\][/];
        # append a backslash to each argument unless it has one there
        $_ .= "/" unless m{/$};
    }
    return $self->canonpath(join('', @args));
}

sub canonpath {
    my ($self,$path) = @_;
    return unless defined $path;

    $path =~ s/^([a-z]:)/\l$1/s;
    $path =~ s|\\|/|g;
    $path =~ s|([^/])/+|$1/|g;                  # xx////xx  -> xx/xx
    $path =~ s|(/\.)+/|/|g;                     # xx/././xx -> xx/xx
    $path =~ s|^(\./)+(?=[^/])||s;		# ./xx      -> xx
    $path =~ s|/\Z(?!\n)||
             unless $path =~ m#^([a-z]:)?/\Z(?!\n)#si;# xx/       -> xx
    $path =~ s{^/\.\.$}{/};                     # /..    -> /
    1 while $path =~ s{^/\.\.}{};               # /../xx -> /xx
    return $path;
}


sub splitpath {
    my ($self,$path, $nofile) = @_;
    my ($volume,$directory,$file) = ('','','');
    if ( $nofile ) {
        $path =~ 
            m{^( (?:[a-zA-Z]:|(?:\\\\|//)[^\\/]+[\\/][^\\/]+)? ) 
                 (.*)
             }xs;
        $volume    = $1;
        $directory = $2;
    }
    else {
        $path =~ 
            m{^ ( (?: [a-zA-Z]: |
                      (?:\\\\|//)[^\\/]+[\\/][^\\/]+
                  )?
                )
                ( (?:.*[\\\\/](?:\.\.?\Z(?!\n))?)? )
                (.*)
             }xs;
        $volume    = $1;
        $directory = $2;
        $file      = $3;
    }

    return ($volume,$directory,$file);
}


sub splitdir {
    my ($self,$directories) = @_ ;
    split m|[\\/]|, $directories, -1;
}


sub catpath {
    my ($self,$volume,$directory,$file) = @_;

    # If it's UNC, make sure the glue separator is there, reusing
    # whatever separator is first in the $volume
    $volume .= $1
        if ( $volume =~ m@^([\\/])[\\/][^\\/]+[\\/][^\\/]+\Z(?!\n)@s &&
             $directory =~ m@^[^\\/]@s
           ) ;

    $volume .= $directory ;

    # If the volume is not just A:, make sure the glue separator is 
    # there, reusing whatever separator is first in the $volume if possible.
    if ( $volume !~ m@^[a-zA-Z]:\Z(?!\n)@s &&
         $volume =~ m@[^\\/]\Z(?!\n)@      &&
         $file   =~ m@[^\\/]@
       ) {
        $volume =~ m@([\\/])@ ;
        my $sep = $1 ? $1 : '/' ;
        $volume .= $sep ;
    }

    $volume .= $file ;

    return $volume ;
}


sub abs2rel {
    my($self,$path,$base) = @_;

    # Clean up $path
    if ( ! $self->file_name_is_absolute( $path ) ) {
        $path = $self->rel2abs( $path ) ;
    } else {
        $path = $self->canonpath( $path ) ;
    }

    # Figure out the effective $base and clean it up.
    if ( !defined( $base ) || $base eq '' ) {
	$base = $self->_cwd();
    } elsif ( ! $self->file_name_is_absolute( $base ) ) {
        $base = $self->rel2abs( $base ) ;
    } else {
        $base = $self->canonpath( $base ) ;
    }

    # Split up paths
    my ( $path_volume, $path_directories, $path_file ) = $self->splitpath( $path, 1 ) ;
    my ( $base_volume, $base_directories ) = $self->splitpath( $base, 1 ) ;
    return $path unless $path_volume eq $base_volume;

    # Now, remove all leading components that are the same
    my @pathchunks = $self->splitdir( $path_directories );
    my @basechunks = $self->splitdir( $base_directories );

    while ( @pathchunks && 
            @basechunks && 
            lc( $pathchunks[0] ) eq lc( $basechunks[0] ) 
          ) {
        shift @pathchunks ;
        shift @basechunks ;
    }

    # No need to catdir, we know these are well formed.
    $path_directories = CORE::join( '/', @pathchunks );
    $base_directories = CORE::join( '/', @basechunks );

    # $base_directories now contains the directories the resulting relative
    # path must ascend out of before it can descend to $path_directory.  So, 
    # replace all names with $parentDir

    #FA Need to replace between backslashes...
    $base_directories =~ s|[^\\/]+|..|g ;

    # Glue the two together, using a separator if necessary, and preventing an
    # empty result.

    #FA Must check that new directories are not empty.
    if ( $path_directories ne '' && $base_directories ne '' ) {
        $path_directories = "$base_directories/$path_directories" ;
    } else {
        $path_directories = "$base_directories$path_directories" ;
    }

    return $self->canonpath( 
        $self->catpath( "", $path_directories, $path_file ) 
    ) ;
}


sub rel2abs {
    my ($self,$path,$base ) = @_;

    if ( ! $self->file_name_is_absolute( $path ) ) {

        if ( !defined( $base ) || $base eq '' ) {
	    $base = $self->_cwd();
        }
        elsif ( ! $self->file_name_is_absolute( $base ) ) {
            $base = $self->rel2abs( $base ) ;
        }
        else {
            $base = $self->canonpath( $base ) ;
        }

        my ( $path_directories, $path_file ) =
            ($self->splitpath( $path, 1 ))[1,2] ;

        my ( $base_volume, $base_directories ) =
            $self->splitpath( $base, 1 ) ;

        $path = $self->catpath( 
            $base_volume, 
            $self->catdir( $base_directories, $path_directories ), 
            $path_file
        ) ;
    }

    return $self->canonpath( $path ) ;
}

1;
__END__

=head1 NAME

File::Spec::OS2 - methods for OS/2 file specs

=head1 SYNOPSIS

 require File::Spec::OS2; # Done internally by File::Spec if needed

=head1 DESCRIPTION

See L<File::Spec> and L<File::Spec::Unix>.  This package overrides the
implementation of these methods, not the semantics.

Amongst the changes made for OS/2 are...

=over 4

=item tmpdir

Modifies the list of places temp directory information is looked for.

    $ENV{TMPDIR}
    $ENV{TEMP}
    $ENV{TMP}
    /tmp
    /

=item splitpath

Volumes can be drive letters or UNC sharenames (\\server\share).

=back

=head1 COPYRIGHT

Copyright (c) 2004 by the Perl 5 Porters.  All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
