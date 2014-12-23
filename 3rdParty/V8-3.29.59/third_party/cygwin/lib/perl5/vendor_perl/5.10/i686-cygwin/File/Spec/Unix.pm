package File::Spec::Unix;

use strict;
use vars qw($VERSION);

$VERSION = '3.2701';

=head1 NAME

File::Spec::Unix - File::Spec for Unix, base for other File::Spec modules

=head1 SYNOPSIS

 require File::Spec::Unix; # Done automatically by File::Spec

=head1 DESCRIPTION

Methods for manipulating file specifications.  Other File::Spec
modules, such as File::Spec::Mac, inherit from File::Spec::Unix and
override specific methods.

=head1 METHODS

=over 2

=item canonpath()

No physical check on the filesystem, but a logical cleanup of a
path. On UNIX eliminates successive slashes and successive "/.".

    $cpath = File::Spec->canonpath( $path ) ;

Note that this does *not* collapse F<x/../y> sections into F<y>.  This
is by design.  If F</foo> on your system is a symlink to F</bar/baz>,
then F</foo/../quux> is actually F</bar/quux>, not F</quux> as a naive
F<../>-removal would give you.  If you want to do this kind of
processing, you probably want C<Cwd>'s C<realpath()> function to
actually traverse the filesystem cleaning up paths like this.

=cut

sub canonpath {
    my ($self,$path) = @_;
    return unless defined $path;
    
    # Handle POSIX-style node names beginning with double slash (qnx, nto)
    # (POSIX says: "a pathname that begins with two successive slashes
    # may be interpreted in an implementation-defined manner, although
    # more than two leading slashes shall be treated as a single slash.")
    my $node = '';
    my $double_slashes_special = $^O eq 'qnx' || $^O eq 'nto';
    if ( $double_slashes_special && $path =~ s{^(//[^/]+)(?:/|\z)}{/}s ) {
      $node = $1;
    }
    # This used to be
    # $path =~ s|/+|/|g unless ($^O eq 'cygwin');
    # but that made tests 29, 30, 35, 46, and 213 (as of #13272) to fail
    # (Mainly because trailing "" directories didn't get stripped).
    # Why would cygwin avoid collapsing multiple slashes into one? --jhi
    $path =~ s|/{2,}|/|g;                            # xx////xx  -> xx/xx
    $path =~ s{(?:/\.)+(?:/|\z)}{/}g;                # xx/././xx -> xx/xx
    $path =~ s|^(?:\./)+||s unless $path eq "./";    # ./xx      -> xx
    $path =~ s|^/(?:\.\./)+|/|;                      # /../../xx -> xx
    $path =~ s|^/\.\.$|/|;                         # /..       -> /
    $path =~ s|/\z|| unless $path eq "/";          # xx/       -> xx
    return "$node$path";
}

=item catdir()

Concatenate two or more directory names to form a complete path ending
with a directory. But remove the trailing slash from the resulting
string, because it doesn't look good, isn't necessary and confuses
OS2. Of course, if this is the root directory, don't cut off the
trailing slash :-)

=cut

sub catdir {
    my $self = shift;

    $self->canonpath(join('/', @_, '')); # '' because need a trailing '/'
}

=item catfile

Concatenate one or more directory names and a filename to form a
complete path ending with a filename

=cut

sub catfile {
    my $self = shift;
    my $file = $self->canonpath(pop @_);
    return $file unless @_;
    my $dir = $self->catdir(@_);
    $dir .= "/" unless substr($dir,-1) eq "/";
    return $dir.$file;
}

=item curdir

Returns a string representation of the current directory.  "." on UNIX.

=cut

sub curdir () { '.' }

=item devnull

Returns a string representation of the null device. "/dev/null" on UNIX.

=cut

sub devnull () { '/dev/null' }

=item rootdir

Returns a string representation of the root directory.  "/" on UNIX.

=cut

sub rootdir () { '/' }

=item tmpdir

Returns a string representation of the first writable directory from
the following list or the current directory if none from the list are
writable:

    $ENV{TMPDIR}
    /tmp

Since perl 5.8.0, if running under taint mode, and if $ENV{TMPDIR}
is tainted, it is not used.

=cut

my $tmpdir;
sub _tmpdir {
    return $tmpdir if defined $tmpdir;
    my $self = shift;
    my @dirlist = @_;
    {
	no strict 'refs';
	if (${"\cTAINT"}) { # Check for taint mode on perl >= 5.8.0
            require Scalar::Util;
	    @dirlist = grep { ! Scalar::Util::tainted($_) } @dirlist;
	}
    }
    foreach (@dirlist) {
	next unless defined && -d && -w _;
	$tmpdir = $_;
	last;
    }
    $tmpdir = $self->curdir unless defined $tmpdir;
    $tmpdir = defined $tmpdir && $self->canonpath($tmpdir);
    return $tmpdir;
}

sub tmpdir {
    return $tmpdir if defined $tmpdir;
    $tmpdir = $_[0]->_tmpdir( $ENV{TMPDIR}, "/tmp" );
}

=item updir

Returns a string representation of the parent directory.  ".." on UNIX.

=cut

sub updir () { '..' }

=item no_upwards

Given a list of file names, strip out those that refer to a parent
directory. (Does not strip symlinks, only '.', '..', and equivalents.)

=cut

sub no_upwards {
    my $self = shift;
    return grep(!/^\.{1,2}\z/s, @_);
}

=item case_tolerant

Returns a true or false value indicating, respectively, that alphabetic
is not or is significant when comparing file specifications.

=cut

sub case_tolerant () { 0 }

=item file_name_is_absolute

Takes as argument a path and returns true if it is an absolute path.

This does not consult the local filesystem on Unix, Win32, OS/2 or Mac 
OS (Classic).  It does consult the working environment for VMS (see
L<File::Spec::VMS/file_name_is_absolute>).

=cut

sub file_name_is_absolute {
    my ($self,$file) = @_;
    return scalar($file =~ m:^/:s);
}

=item path

Takes no argument, returns the environment variable PATH as an array.

=cut

sub path {
    return () unless exists $ENV{PATH};
    my @path = split(':', $ENV{PATH});
    foreach (@path) { $_ = '.' if $_ eq '' }
    return @path;
}

=item join

join is the same as catfile.

=cut

sub join {
    my $self = shift;
    return $self->catfile(@_);
}

=item splitpath

    ($volume,$directories,$file) = File::Spec->splitpath( $path );
    ($volume,$directories,$file) = File::Spec->splitpath( $path, $no_file );

Splits a path into volume, directory, and filename portions. On systems
with no concept of volume, returns '' for volume. 

For systems with no syntax differentiating filenames from directories, 
assumes that the last file is a path unless $no_file is true or a 
trailing separator or /. or /.. is present. On Unix this means that $no_file
true makes this return ( '', $path, '' ).

The directory portion may or may not be returned with a trailing '/'.

The results can be passed to L</catpath()> to get back a path equivalent to
(usually identical to) the original path.

=cut

sub splitpath {
    my ($self,$path, $nofile) = @_;

    my ($volume,$directory,$file) = ('','','');

    if ( $nofile ) {
        $directory = $path;
    }
    else {
        $path =~ m|^ ( (?: .* / (?: \.\.?\z )? )? ) ([^/]*) |xs;
        $directory = $1;
        $file      = $2;
    }

    return ($volume,$directory,$file);
}


=item splitdir

The opposite of L</catdir()>.

    @dirs = File::Spec->splitdir( $directories );

$directories must be only the directory portion of the path on systems 
that have the concept of a volume or that have path syntax that differentiates
files from directories.

Unlike just splitting the directories on the separator, empty
directory names (C<''>) can be returned, because these are significant
on some OSs.

On Unix,

    File::Spec->splitdir( "/a/b//c/" );

Yields:

    ( '', 'a', 'b', '', 'c', '' )

=cut

sub splitdir {
    return split m|/|, $_[1], -1;  # Preserve trailing fields
}


=item catpath()

Takes volume, directory and file portions and returns an entire path. Under
Unix, $volume is ignored, and directory and file are concatenated.  A '/' is
inserted if needed (though if the directory portion doesn't start with
'/' it is not added).  On other OSs, $volume is significant.

=cut

sub catpath {
    my ($self,$volume,$directory,$file) = @_;

    if ( $directory ne ''                && 
         $file ne ''                     && 
         substr( $directory, -1 ) ne '/' && 
         substr( $file, 0, 1 ) ne '/' 
    ) {
        $directory .= "/$file" ;
    }
    else {
        $directory .= $file ;
    }

    return $directory ;
}

=item abs2rel

Takes a destination path and an optional base path returns a relative path
from the base path to the destination path:

    $rel_path = File::Spec->abs2rel( $path ) ;
    $rel_path = File::Spec->abs2rel( $path, $base ) ;

If $base is not present or '', then L<cwd()|Cwd> is used. If $base is
relative, then it is converted to absolute form using
L</rel2abs()>. This means that it is taken to be relative to
L<cwd()|Cwd>.

On systems that have a grammar that indicates filenames, this ignores the 
$base filename. Otherwise all path components are assumed to be
directories.

If $path is relative, it is converted to absolute form using L</rel2abs()>.
This means that it is taken to be relative to L<cwd()|Cwd>.

No checks against the filesystem are made.  On VMS, there is
interaction with the working environment, as logicals and
macros are expanded.

Based on code written by Shigio Yamaguchi.

=cut

sub abs2rel {
    my($self,$path,$base) = @_;
    $base = $self->_cwd() unless defined $base and length $base;

    ($path, $base) = map $self->canonpath($_), $path, $base;

    if (grep $self->file_name_is_absolute($_), $path, $base) {
	($path, $base) = map $self->rel2abs($_), $path, $base;
    }
    else {
	# save a couple of cwd()s if both paths are relative
	($path, $base) = map $self->catdir('/', $_), $path, $base;
    }

    my ($path_volume) = $self->splitpath($path, 1);
    my ($base_volume) = $self->splitpath($base, 1);

    # Can't relativize across volumes
    return $path unless $path_volume eq $base_volume;

    my $path_directories = ($self->splitpath($path, 1))[1];
    my $base_directories = ($self->splitpath($base, 1))[1];

    # For UNC paths, the user might give a volume like //foo/bar that
    # strictly speaking has no directory portion.  Treat it as if it
    # had the root directory for that volume.
    if (!length($base_directories) and $self->file_name_is_absolute($base)) {
      $base_directories = $self->rootdir;
    }

    # Now, remove all leading components that are the same
    my @pathchunks = $self->splitdir( $path_directories );
    my @basechunks = $self->splitdir( $base_directories );

    if ($base_directories eq $self->rootdir) {
      shift @pathchunks;
      return $self->canonpath( $self->catpath('', $self->catdir( @pathchunks ), '') );
    }

    while (@pathchunks && @basechunks && $self->_same($pathchunks[0], $basechunks[0])) {
        shift @pathchunks ;
        shift @basechunks ;
    }
    return $self->curdir unless @pathchunks || @basechunks;

    # $base now contains the directories the resulting relative path 
    # must ascend out of before it can descend to $path_directory.
    my $result_dirs = $self->catdir( ($self->updir) x @basechunks, @pathchunks );
    return $self->canonpath( $self->catpath('', $result_dirs, '') );
}

sub _same {
  $_[1] eq $_[2];
}

=item rel2abs()

Converts a relative path to an absolute path. 

    $abs_path = File::Spec->rel2abs( $path ) ;
    $abs_path = File::Spec->rel2abs( $path, $base ) ;

If $base is not present or '', then L<cwd()|Cwd> is used. If $base is
relative, then it is converted to absolute form using
L</rel2abs()>. This means that it is taken to be relative to
L<cwd()|Cwd>.

On systems that have a grammar that indicates filenames, this ignores
the $base filename. Otherwise all path components are assumed to be
directories.

If $path is absolute, it is cleaned up and returned using L</canonpath()>.

No checks against the filesystem are made.  On VMS, there is
interaction with the working environment, as logicals and
macros are expanded.

Based on code written by Shigio Yamaguchi.

=cut

sub rel2abs {
    my ($self,$path,$base ) = @_;

    # Clean up $path
    if ( ! $self->file_name_is_absolute( $path ) ) {
        # Figure out the effective $base and clean it up.
        if ( !defined( $base ) || $base eq '' ) {
	    $base = $self->_cwd();
        }
        elsif ( ! $self->file_name_is_absolute( $base ) ) {
            $base = $self->rel2abs( $base ) ;
        }
        else {
            $base = $self->canonpath( $base ) ;
        }

        # Glom them together
        $path = $self->catdir( $base, $path ) ;
    }

    return $self->canonpath( $path ) ;
}

=back

=head1 COPYRIGHT

Copyright (c) 2004 by the Perl 5 Porters.  All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 SEE ALSO

L<File::Spec>

=cut

# Internal routine to File::Spec, no point in making this public since
# it is the standard Cwd interface.  Most of the platform-specific
# File::Spec subclasses use this.
sub _cwd {
    require Cwd;
    Cwd::getcwd();
}


# Internal method to reduce xx\..\yy -> yy
sub _collapse {
    my($fs, $path) = @_;

    my $updir  = $fs->updir;
    my $curdir = $fs->curdir;

    my($vol, $dirs, $file) = $fs->splitpath($path);
    my @dirs = $fs->splitdir($dirs);
    pop @dirs if @dirs && $dirs[-1] eq '';

    my @collapsed;
    foreach my $dir (@dirs) {
        if( $dir eq $updir              and   # if we have an updir
            @collapsed                  and   # and something to collapse
            length $collapsed[-1]       and   # and its not the rootdir
            $collapsed[-1] ne $updir    and   # nor another updir
            $collapsed[-1] ne $curdir         # nor the curdir
          ) 
        {                                     # then
            pop @collapsed;                   # collapse
        }
        else {                                # else
            push @collapsed, $dir;            # just hang onto it
        }
    }

    return $fs->catpath($vol,
                        $fs->catdir(@collapsed),
                        $file
                       );
}


1;
