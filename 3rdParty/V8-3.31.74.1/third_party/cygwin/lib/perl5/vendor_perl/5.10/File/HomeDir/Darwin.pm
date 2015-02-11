package File::HomeDir::Darwin;

# Basic implementation for the Dawin family of operating systems.
# This includes (most prominently) Mac OS X.

use 5.005;
use strict;
use File::HomeDir::Unix ();
use Carp ();
use Cwd ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '0.80';
	@ISA     = 'File::HomeDir::Unix';
}

# Load early if in a forking environment and we have
# prefork, or at run-time if not.
SCOPE: {
	local $@;
	eval "use prefork 'Mac::Files'";
}





#####################################################################
# Current User Methods

sub my_home {
	my ($class) = @_;
	require Mac::Files;
	$class->_find_folder(
		Mac::Files::kCurrentUserFolderType(),
		);
}

sub my_desktop {
	my ($class) = @_;
	require Mac::Files;
	$class->_find_folder(
		Mac::Files::kDesktopFolderType(),
		);
}

sub my_documents {
	my ($class) = @_;
	require Mac::Files;
	$class->_find_folder(
		Mac::Files::kDocumentsFolderType(),
		);
}

sub my_data {
	my ($class) = @_;
	require Mac::Files;
	$class->_find_folder(
		Mac::Files::kApplicationSupportFolderType(),
		);
}

sub my_music {
	my ($class) = @_;
	require Mac::Files;
	$class->_find_folder(
		Mac::Files::kMusicDocumentsFolderType(),
		);
}

sub my_pictures {
	my ($class) = @_;
	require Mac::Files;
	$class->_find_folder(
		Mac::Files::kPictureDocumentsFolderType(),
		);
}

sub my_videos {
	my ($class) = @_;
	require Mac::Files;
	$class->_find_folder(
		Mac::Files::kMovieDocumentsFolderType(),
		);
}

sub _find_folder {
	my ($class, $name) = @_;
	require Mac::Files;
	my $folder = Mac::Files::FindFolder(
		Mac::Files::kUserDomain(),
		$name,
		);
	return unless defined $folder;
	unless ( -d $folder ) {
		# Make sure that symlinks resolve to directories.
		return unless -l $folder;
		my $dir = readlink $folder or return;
		return unless -d $dir;
	}
	return Cwd::abs_path($folder);
}





#####################################################################
# Arbitrary User Methods

sub users_home {
	my $class = shift;
	my $home  = $class->SUPER::users_home(@_);
	return Cwd::abs_path($home);
}

# in theory this can be done, but for now, let's cheat, since the
# rest is Hard
sub users_desktop {
	my ($class, $name) = @_;
	return undef if $name eq 'root';
	$class->_to_user( $class->my_desktop, $name );
}

sub users_documents {
	my ($class, $name) = @_;
	return undef if $name eq 'root';
	$class->_to_user( $class->my_documents, $name );
}

sub users_data {
	my ($class, $name) = @_;
	$class->_to_user( $class->my_data, $name )
		|| $class->users_home($name);
}

# cheap hack ... not entirely reliable, perhaps, but ... c'est la vie, since
# there's really no other good way to do it at this time, that i know of -- pudge
sub _to_user {
	my ($class, $path, $name) = @_;
	my $my_home    = $class->my_home;
	my $users_home = $class->users_home($name);
	$path =~ s/^Q$my_home/$users_home/;
	return $path;
}

1;

=pod

=head1 NAME

File::HomeDir::Darwin - find your home and other directories, on Darwin (OS X)

=head1 DESCRIPTION

This module provides Darwin-specific implementations for determining
common user directories.  In normal usage this module will always be
used via L<File::HomeDir>.

=head1 SYNOPSIS

  use File::HomeDir;
  
  # Find directories for the current user
  $home    = File::HomeDir->my_home;      # /Users/mylogin
  $desktop = File::HomeDir->my_desktop;   # /Users/mylogin/Desktop
  $docs    = File::HomeDir->my_documents; # /Users/mylogin/Documents
  $music   = File::HomeDir->my_music;     # /Users/mylogin/Music
  $pics    = File::HomeDir->my_pictures;  # /Users/mylogin/Pictures
  $videos  = File::HomeDir->my_videos;    # /Users/mylogin/Movies
  $data    = File::HomeDir->my_data;      # /Users/mylogin/Library/Application Support

=head1 TODO

=over 4

=item * Fallback to Unix if no Mac::Carbon available

=item * Test with Mac OS (versions 7, 8, 9)

=item * Some better way for users_* ?

=back
