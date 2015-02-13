package File::Spec::Win32;

use strict;

use vars qw(@ISA $VERSION);
require File::Spec::Unix;

$VERSION = '3.2702';

@ISA = qw(File::Spec::Unix);

# Some regexes we use for path splitting
my $DRIVE_RX = '[a-zA-Z]:';
my $UNC_RX = '(?:\\\\\\\\|//)[^\\\\/]+[\\\\/][^\\\\/]+';
my $VOL_RX = "(?:$DRIVE_RX|$UNC_RX)";


=head1 NAME

File::Spec::Win32 - methods for Win32 file specs

=head1 SYNOPSIS

 require File::Spec::Win32; # Done internally by File::Spec if needed

=head1 DESCRIPTION

See File::Spec::Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=over 4

=item devnull

Returns a string representation of the null device.

=cut

sub devnull {
    return "nul";
}

sub rootdir () { '\\' }


=item tmpdir

Returns a string representation of the first existing directory
from the following list:

    $ENV{TMPDIR}
    $ENV{TEMP}
    $ENV{TMP}
    SYS:/temp
    C:\system\temp
    C:/temp
    /tmp
    /

The SYS:/temp is preferred in Novell NetWare and the C:\system\temp
for Symbian (the File::Spec::Win32 is used also for those platforms).

Since Perl 5.8.0, if running under taint mode, and if the environment
variables are tainted, they are not used.

=cut

my $tmpdir;
sub tmpdir {
    return $tmpdir if defined $tmpdir;
    $tmpdir = $_[0]->_tmpdir( map( $ENV{$_}, qw(TMPDIR TEMP TMP) ),
			      'SYS:/temp',
			      'C:\system\temp',
			      'C:/temp',
			      '/tmp',
			      '/'  );
}

=item case_tolerant

MSWin32 case-tolerance depends on GetVolumeInformation() $ouFsFlags == FS_CASE_SENSITIVE,
indicating the case significance when comparing file specifications.
Since XP FS_CASE_SENSITIVE is effectively disabled for the NT subsystem.
See http://cygwin.com/ml/cygwin/2007-07/msg00891.html
Accepts an optional drive-letter argument.
Default: 1

=cut

sub case_tolerant () {
  use Win32;
  my @ver = Win32::GetOSVersion();
  # From XP on this is disabled.
  if ($ver[4] >= 2 and $ver[1] >= 5 and $ver[2] >= 1) { return 1; }
  if ($ver[4] < 2) { return 1; } # Win32s,95,ME are always case_tolerant.
  eval { require Win32API::File; } or return 1;
  my $drive = shift;
  $drive = shift if $drive =~ /^File::Spec/;
  $drive ||= $ENV{SYSTEMDRIVE} || substr($ENV{WINDIR}, 0, 2);
  my $osFsType = "\0"x256;
  my $osVolName = "\0"x256;
  my $ouFsFlags = 0;
  Win32API::File::GetVolumeInformation($drive, $osVolName, 256, [], [], $ouFsFlags, $osFsType, 256 );
  if ($ouFsFlags & Win32API::File::FS_CASE_SENSITIVE()) { return 0; }
  else { return 1; }
}

=item file_name_is_absolute

As of right now, this returns 2 if the path is absolute with a
volume, 1 if it's absolute with no volume, 0 otherwise.

=cut

sub file_name_is_absolute {

    my ($self,$file) = @_;

    if ($file =~ m{^($VOL_RX)}o) {
      my $vol = $1;
      return ($vol =~ m{^$UNC_RX}o ? 2
	      : $file =~ m{^$DRIVE_RX[\\/]}o ? 2
	      : 0);
    }
    return $file =~  m{^[\\/]} ? 1 : 0;
}

=item catfile

Concatenate one or more directory names and a filename to form a
complete path ending with a filename

=cut

sub catfile {
    shift;

    # Legacy / compatibility support
    #
    shift, return _canon_cat( "/", @_ )
	if $_[0] eq "";

    # Compatibility with File::Spec <= 3.26:
    #     catfile('A:', 'foo') should return 'A:\foo'.
    return _canon_cat( ($_[0].'\\'), @_[1..$#_] )
        if $_[0] =~ m{^$DRIVE_RX\z}o;

    return _canon_cat( @_ );
}

sub catdir {
    shift;

    # Legacy / compatibility support
    #
    return ""
    	unless @_;
    shift, return _canon_cat( "/", @_ )
	if $_[0] eq "";

    # Compatibility with File::Spec <= 3.26:
    #     catdir('A:', 'foo') should return 'A:\foo'.
    return _canon_cat( ($_[0].'\\'), @_[1..$#_] )
        if $_[0] =~ m{^$DRIVE_RX\z}o;

    return _canon_cat( @_ );
}

sub path {
    my @path = split(';', $ENV{PATH});
    s/"//g for @path;
    @path = grep length, @path;
    unshift(@path, ".");
    return @path;
}

=item canonpath

No physical check on the filesystem, but a logical cleanup of a
path. On UNIX eliminated successive slashes and successive "/.".
On Win32 makes 

	dir1\dir2\dir3\..\..\dir4 -> \dir\dir4 and even
	dir1\dir2\dir3\...\dir4   -> \dir\dir4

=cut

sub canonpath {
    # Legacy / compatibility support
    #
    return $_[1] if !defined($_[1]) or $_[1] eq '';
    return _canon_cat( $_[1] );
}

=item splitpath

    ($volume,$directories,$file) = File::Spec->splitpath( $path );
    ($volume,$directories,$file) = File::Spec->splitpath( $path, $no_file );

Splits a path into volume, directory, and filename portions. Assumes that 
the last file is a path unless the path ends in '\\', '\\.', '\\..'
or $no_file is true.  On Win32 this means that $no_file true makes this return 
( $volume, $path, '' ).

Separators accepted are \ and /.

Volumes can be drive letters or UNC sharenames (\\server\share).

The results can be passed to L</catpath> to get back a path equivalent to
(usually identical to) the original path.

=cut

sub splitpath {
    my ($self,$path, $nofile) = @_;
    my ($volume,$directory,$file) = ('','','');
    if ( $nofile ) {
        $path =~ 
            m{^ ( $VOL_RX ? ) (.*) }sox;
        $volume    = $1;
        $directory = $2;
    }
    else {
        $path =~ 
            m{^ ( $VOL_RX ? )
                ( (?:.*[\\/](?:\.\.?\Z(?!\n))?)? )
                (.*)
             }sox;
        $volume    = $1;
        $directory = $2;
        $file      = $3;
    }

    return ($volume,$directory,$file);
}


=item splitdir

The opposite of L<catdir()|File::Spec/catdir()>.

    @dirs = File::Spec->splitdir( $directories );

$directories must be only the directory portion of the path on systems 
that have the concept of a volume or that have path syntax that differentiates
files from directories.

Unlike just splitting the directories on the separator, leading empty and 
trailing directory entries can be returned, because these are significant
on some OSs. So,

    File::Spec->splitdir( "/a/b/c" );

Yields:

    ( '', 'a', 'b', '', 'c', '' )

=cut

sub splitdir {
    my ($self,$directories) = @_ ;
    #
    # split() likes to forget about trailing null fields, so here we
    # check to be sure that there will not be any before handling the
    # simple case.
    #
    if ( $directories !~ m|[\\/]\Z(?!\n)| ) {
        return split( m|[\\/]|, $directories );
    }
    else {
        #
        # since there was a trailing separator, add a file name to the end, 
        # then do the split, then replace it with ''.
        #
        my( @directories )= split( m|[\\/]|, "${directories}dummy" ) ;
        $directories[ $#directories ]= '' ;
        return @directories ;
    }
}


=item catpath

Takes volume, directory and file portions and returns an entire path. Under
Unix, $volume is ignored, and this is just like catfile(). On other OSs,
the $volume become significant.

=cut

sub catpath {
    my ($self,$volume,$directory,$file) = @_;

    # If it's UNC, make sure the glue separator is there, reusing
    # whatever separator is first in the $volume
    my $v;
    $volume .= $v
        if ( (($v) = $volume =~ m@^([\\/])[\\/][^\\/]+[\\/][^\\/]+\Z(?!\n)@s) &&
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
        my $sep = $1 ? $1 : '\\' ;
        $volume .= $sep ;
    }

    $volume .= $file ;

    return $volume ;
}

sub _same {
  lc($_[1]) eq lc($_[2]);
}

sub rel2abs {
    my ($self,$path,$base ) = @_;

    my $is_abs = $self->file_name_is_absolute($path);

    # Check for volume (should probably document the '2' thing...)
    return $self->canonpath( $path ) if $is_abs == 2;

    if ($is_abs) {
      # It's missing a volume, add one
      my $vol = ($self->splitpath( $self->_cwd() ))[0];
      return $self->canonpath( $vol . $path );
    }

    if ( !defined( $base ) || $base eq '' ) {
      require Cwd ;
      $base = Cwd::getdcwd( ($self->splitpath( $path ))[0] ) if defined &Cwd::getdcwd ;
      $base = $self->_cwd() unless defined $base ;
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

    return $self->canonpath( $path ) ;
}

=back

=head2 Note For File::Spec::Win32 Maintainers

Novell NetWare inherits its File::Spec behaviour from File::Spec::Win32.

=head1 COPYRIGHT

Copyright (c) 2004,2007 by the Perl 5 Porters.  All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 SEE ALSO

See L<File::Spec> and L<File::Spec::Unix>.  This package overrides the
implementation of these methods, not the semantics.

=cut


sub _canon_cat(@)				# @path -> path
{
    my $first  = shift;
    my $volume = $first =~ s{ \A ([A-Za-z]:) ([\\/]?) }{}x	# drive letter
    	       ? ucfirst( $1 ).( $2 ? "\\" : "" )
	       : $first =~ s{ \A (?:\\\\|//) ([^\\/]+)
				 (?: [\\/] ([^\\/]+) )?
	       			 [\\/]? }{}xs			# UNC volume
	       ? "\\\\$1".( defined $2 ? "\\$2" : "" )."\\"
	       : $first =~ s{ \A [\\/] }{}x			# root dir
	       ? "\\"
	       : "";
    my $path   = join "\\", $first, @_;

    $path =~ tr#\\/#\\\\#s;		# xx/yy --> xx\yy & xx\\yy --> xx\yy

    					# xx/././yy --> xx/yy
    $path =~ s{(?:
		(?:\A|\\)		# at begin or after a slash
		\.
		(?:\\\.)*		# and more
		(?:\\|\z) 		# at end or followed by slash
	       )+			# performance boost -- I do not know why
	     }{\\}gx;

    # XXX I do not know whether more dots are supported by the OS supporting
    #     this ... annotation (NetWare or symbian but not MSWin32).
    #     Then .... could easily become ../../.. etc:
    # Replace \.\.\. by (\.\.\.+)  and substitute with
    # { $1 . ".." . "\\.." x (length($2)-2) }gex
	     				# ... --> ../..
    $path =~ s{ (\A|\\)			# at begin or after a slash
    		\.\.\.
		(?=\\|\z) 		# at end or followed by slash
	     }{$1..\\..}gx;
    					# xx\yy\..\zz --> xx\zz
    while ( $path =~ s{(?:
		(?:\A|\\)		# at begin or after a slash
		[^\\]+			# rip this 'yy' off
		\\\.\.
		(?<!\A\.\.\\\.\.)	# do *not* replace ^..\..
		(?<!\\\.\.\\\.\.)	# do *not* replace \..\..
		(?:\\|\z) 		# at end or followed by slash
	       )+			# performance boost -- I do not know why
	     }{\\}sx ) {}

    $path =~ s#\A\\##;			# \xx --> xx  NOTE: this is *not* root
    $path =~ s#\\\z##;			# xx\ --> xx

    if ( $volume =~ m#\\\z# )
    {					# <vol>\.. --> <vol>\
	$path =~ s{ \A			# at begin
		    \.\.
		    (?:\\\.\.)*		# and more
		    (?:\\|\z) 		# at end or followed by slash
		 }{}x;

	return $1			# \\HOST\SHARE\ --> \\HOST\SHARE
	    if    $path eq ""
	      and $volume =~ m#\A(\\\\.*)\\\z#s;
    }
    return $path ne "" || $volume ? $volume.$path : ".";
}

1;
