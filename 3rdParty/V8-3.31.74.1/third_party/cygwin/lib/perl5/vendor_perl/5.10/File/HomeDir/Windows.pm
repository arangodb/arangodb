package File::HomeDir::Windows;

# Generalised implementation for the entire Windows family of operating
# systems.

use 5.005;
use strict;
use File::HomeDir::Driver ();
use Carp ();
use File::Spec ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '0.80';
	@ISA     = 'File::HomeDir::Driver';
}

sub CREATE () { 1 }





#####################################################################
# Current User Methods

sub my_home {
	my $class = shift;

	# A lot of unix people and unix-derived tools rely on
	# the ability to overload HOME. We will support it too
	# so that they can replace raw HOME calls with File::HomeDir.
	if ( exists $ENV{HOME} and $ENV{HOME} ) {
		return $ENV{HOME};
	}

	# Do we have a user profile?
	if ( exists $ENV{USERPROFILE} and $ENV{USERPROFILE} ) {
		return $ENV{USERPROFILE};
	}

	# Some Windows use something like $ENV{HOME}
	if ( exists $ENV{HOMEDRIVE} and exists $ENV{HOMEPATH} and $ENV{HOMEDRIVE} and $ENV{HOMEPATH} ) {
		return File::Spec->catpath(
			$ENV{HOMEDRIVE}, $ENV{HOMEPATH}, '',
			);
	}

	return undef;
}

sub my_desktop {
	my $class = shift;

	# The most correct way to find the desktop
	SCOPE: {
		require Win32;
		my $dir = Win32::GetFolderPath(Win32::CSIDL_DESKTOP(), CREATE);
		return $dir if $dir and -d $dir;
	}

	# MSWindows sets WINDIR, MS WinNT sets USERPROFILE.
	foreach my $e ( 'USERPROFILE', 'WINDIR' ) {
		next unless $ENV{$e};
		my $desktop = File::Spec->catdir($ENV{$e}, 'Desktop');
		return $desktop if $desktop and -d $desktop;
	}

	# As a last resort, try some hard-wired values
	foreach my $fixed (
		# The reason there are both types of slash here is because
		# this set of paths has been kept from thethe original version
		# of File::HomeDir::Win32 (before it was rewritten).
		# I can only assume this is Cygwin-related stuff.
		"C:\\windows\\desktop",
		"C:\\win95\\desktop",
		"C:/win95/desktop",
		"C:/windows/desktop",
	) {
		return $fixed if -d $fixed;
	}

	return undef;
}

sub my_documents {
	my $class = shift;

	# The most correct way to find my documents
	SCOPE: {
		require Win32;
		my $dir = Win32::GetFolderPath(Win32::CSIDL_PERSONAL(), CREATE);
		return $dir if $dir and -d $dir;
	}

	return undef;
}

sub my_data {
	my $class = shift;

	# The most correct way to find my documents
	SCOPE: {
		require Win32;
		my $dir = Win32::GetFolderPath(Win32::CSIDL_LOCAL_APPDATA(), CREATE);
		return $dir if $dir and -d $dir;
	}

	return undef;
}

sub my_music {
	my $class = shift;

	# The most correct way to find my music
	SCOPE: {
		require Win32;
		my $dir = Win32::GetFolderPath(Win32::CSIDL_MYMUSIC(), CREATE);
		return $dir if $dir and -d $dir;
	}

	return undef;
}

sub my_pictures {
	my $class = shift;

	# The most correct way to find my pictures
	SCOPE: {
		require Win32;
		my $dir = Win32::GetFolderPath(Win32::CSIDL_MYPICTURES(), CREATE);
		return $dir if $dir and -d $dir;
	}

	return undef;
}

sub my_videos {
	my $class = shift;

	# The most correct way to find my videos
	SCOPE: {
		require Win32;
		my $dir = Win32::GetFolderPath(Win32::CSIDL_MYVIDEO(), CREATE);
		return $dir if $dir and -d $dir;
	}

	return undef;
}

1;

=pod

=head1 NAME

File::HomeDir::Windows - find your home and other directories, on Windows

=head1 DESCRIPTION

This module provides Windows-specific implementations for determining
common user directories.  In normal usage this module will always be
used via L<File::HomeDir>.

=head1 SYNOPSIS

  use File::HomeDir;
  
  # Find directories for the current user (eg. using Windows XP Professional)
  $home    = File::HomeDir->my_home;        # C:\Documents and Settings\mylogin
  $desktop = File::HomeDir->my_desktop;     # C:\Documents and Settings\mylogin\Desktop
  $docs    = File::HomeDir->my_documents;   # C:\Documents and Settings\mylogin\My Documents
  $music   = File::HomeDir->my_music;       # C:\Documents and Settings\mylogin\My Documents\My Music
  $pics    = File::HomeDir->my_pictures;    # C:\Documents and Settings\mylogin\My Documents\My Pictures
  $videos  = File::HomeDir->my_videos;      # C:\Documents and Settings\mylogin\My Documents\My Video
  $data    = File::HomeDir->my_data;        # C:\Documents and Settings\mylogin\Local Settings\Application Data
