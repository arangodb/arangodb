# $Id: newgetopt.pl,v 1.18 2001-09-21 15:34:59+02 jv Exp $

# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
# It is now just a wrapper around the Getopt::Long module.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
# Suggested alternative: Getopt::Long

{   package newgetopt;

    # Values for $order. See GNU getopt.c for details.
    $REQUIRE_ORDER = 0;
    $PERMUTE = 1;
    $RETURN_IN_ORDER = 2;

    # Handle POSIX compliancy.
    if ( defined $ENV{"POSIXLY_CORRECT"} ) {
	$autoabbrev = 0;	# no automatic abbrev of options (???)
	$getopt_compat = 0;	# disallow '+' to start options
	$option_start = "(--|-)";
	$order = $REQUIRE_ORDER;
	$bundling = 0;
	$passthrough = 0;
    }
    else {
	$autoabbrev = 1;	# automatic abbrev of options
	$getopt_compat = 1;	# allow '+' to start options
	$option_start = "(--|-|\\+)";
	$order = $PERMUTE;
	$bundling = 0;
	$passthrough = 0;
    }

    # Other configurable settings.
    $debug = 0;			# for debugging
    $ignorecase = 1;		# ignore case when matching options
    $argv_end = "--";		# don't change this!
}

use Getopt::Long;

################ Subroutines ################

sub NGetOpt {

    $Getopt::Long::debug = $newgetopt::debug 
	if defined $newgetopt::debug;
    $Getopt::Long::autoabbrev = $newgetopt::autoabbrev 
	if defined $newgetopt::autoabbrev;
    $Getopt::Long::getopt_compat = $newgetopt::getopt_compat 
	if defined $newgetopt::getopt_compat;
    $Getopt::Long::option_start = $newgetopt::option_start 
	if defined $newgetopt::option_start;
    $Getopt::Long::order = $newgetopt::order 
	if defined $newgetopt::order;
    $Getopt::Long::bundling = $newgetopt::bundling 
	if defined $newgetopt::bundling;
    $Getopt::Long::ignorecase = $newgetopt::ignorecase 
	if defined $newgetopt::ignorecase;
    $Getopt::Long::ignorecase = $newgetopt::ignorecase 
	if defined $newgetopt::ignorecase;
    $Getopt::Long::passthrough = $newgetopt::passthrough 
	if defined $newgetopt::passthrough;

    &GetOptions;
}

################ Package return ################

1;

################ End of newgetopt.pl ################
