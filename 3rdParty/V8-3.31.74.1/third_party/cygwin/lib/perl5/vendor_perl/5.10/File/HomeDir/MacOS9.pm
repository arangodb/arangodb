package File::HomeDir::MacOS9;

# Half-assed implementation for the legacy Mac OS9 operating system.
# Provided mainly to provide legacy compatibility. May be removed at
# a later date.

use 5.005;
use strict;
use File::HomeDir::Driver ();
use Carp ();

use vars qw{$VERSION @ISA};
BEGIN {
	$VERSION = '0.80';
	@ISA     = 'File::HomeDir::Driver';
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
	my $class = shift;

	# Try for $ENV{HOME} if we have it
	if ( defined $ENV{HOME} ) {
		return $ENV{HOME};
	}

	### DESPERATION SETS IN

	# We could use the desktop
	eval {
		my $home = $class->my_desktop;
		return $home if $home and -d $home;
	};

	# Desperation on any platform
	SCOPE: {
		# On some platforms getpwuid dies if called at all
		local $SIG{'__DIE__'} = '';
		my $home = (getpwuid($<))[7];
		return $home if $home and -d $home;
	}

	Carp::croak("Could not locate current user's home directory");
}

sub my_desktop {
	my $class = shift;

	# Find the desktop via Mac::Files
	local $SIG{'__DIE__'} = '';
	require Mac::Files;
	my $home = Mac::Files::FindFolder(
		Mac::Files::kOnSystemDisk(),
		Mac::Files::kDesktopFolderType(),
		);
	return $home if $home and -d $home;

	Carp::croak("Could not locate current user's desktop");
}





#####################################################################
# General User Methods

sub users_home {
	my ($class, $name) = @_;

	SCOPE: {
		# On some platforms getpwnam dies if called at all
		local $SIG{'__DIE__'} = '';
		my $home = (getpwnam($name))[7];
		return $home if defined $home and -d $home;
	}

	Carp::croak("Failed to find home directory for user '$name'");
}

1;
