# Getopt::Long.pm -- Universal options parsing

package Getopt::Long;

# RCS Status      : $Id: Long.pm,v 2.74 2007/09/29 13:40:13 jv Exp $
# Author          : Johan Vromans
# Created On      : Tue Sep 11 15:00:12 1990
# Last Modified By: Johan Vromans
# Last Modified On: Sat Sep 29 15:38:55 2007
# Update Count    : 1571
# Status          : Released

################ Copyright ################

# This program is Copyright 1990,2007 by Johan Vromans.
# This program is free software; you can redistribute it and/or
# modify it under the terms of the Perl Artistic License or the
# GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any
# later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# If you do not have a copy of the GNU General Public License write to
# the Free Software Foundation, Inc., 675 Mass Ave, Cambridge,
# MA 02139, USA.

################ Module Preamble ################

use 5.004;

use strict;

use vars qw($VERSION);
$VERSION        =  2.37;
# For testing versions only.
use vars qw($VERSION_STRING);
$VERSION_STRING = "2.37";

use Exporter;
use vars qw(@ISA @EXPORT @EXPORT_OK);
@ISA = qw(Exporter);

# Exported subroutines.
sub GetOptions(@);		# always
sub GetOptionsFromArray($@);	# on demand
sub GetOptionsFromString($@);	# on demand
sub Configure(@);		# on demand
sub HelpMessage(@);		# on demand
sub VersionMessage(@);		# in demand

BEGIN {
    # Init immediately so their contents can be used in the 'use vars' below.
    @EXPORT    = qw(&GetOptions $REQUIRE_ORDER $PERMUTE $RETURN_IN_ORDER);
    @EXPORT_OK = qw(&HelpMessage &VersionMessage &Configure
		    &GetOptionsFromArray &GetOptionsFromString);
}

# User visible variables.
use vars @EXPORT, @EXPORT_OK;
use vars qw($error $debug $major_version $minor_version);
# Deprecated visible variables.
use vars qw($autoabbrev $getopt_compat $ignorecase $bundling $order
	    $passthrough);
# Official invisible variables.
use vars qw($genprefix $caller $gnu_compat $auto_help $auto_version $longprefix);

# Public subroutines.
sub config(@);			# deprecated name

# Private subroutines.
sub ConfigDefaults();
sub ParseOptionSpec($$);
sub OptCtl($);
sub FindOption($$$$$);
sub ValidValue ($$$$$);

################ Local Variables ################

# $requested_version holds the version that was mentioned in the 'use'
# or 'require', if any. It can be used to enable or disable specific
# features.
my $requested_version = 0;

################ Resident subroutines ################

sub ConfigDefaults() {
    # Handle POSIX compliancy.
    if ( defined $ENV{"POSIXLY_CORRECT"} ) {
	$genprefix = "(--|-)";
	$autoabbrev = 0;		# no automatic abbrev of options
	$bundling = 0;			# no bundling of single letter switches
	$getopt_compat = 0;		# disallow '+' to start options
	$order = $REQUIRE_ORDER;
    }
    else {
	$genprefix = "(--|-|\\+)";
	$autoabbrev = 1;		# automatic abbrev of options
	$bundling = 0;			# bundling off by default
	$getopt_compat = 1;		# allow '+' to start options
	$order = $PERMUTE;
    }
    # Other configurable settings.
    $debug = 0;			# for debugging
    $error = 0;			# error tally
    $ignorecase = 1;		# ignore case when matching options
    $passthrough = 0;		# leave unrecognized options alone
    $gnu_compat = 0;		# require --opt=val if value is optional
    $longprefix = "(--)";       # what does a long prefix look like
}

# Override import.
sub import {
    my $pkg = shift;		# package
    my @syms = ();		# symbols to import
    my @config = ();		# configuration
    my $dest = \@syms;		# symbols first
    for ( @_ ) {
	if ( $_ eq ':config' ) {
	    $dest = \@config;	# config next
	    next;
	}
	push(@$dest, $_);	# push
    }
    # Hide one level and call super.
    local $Exporter::ExportLevel = 1;
    push(@syms, qw(&GetOptions)) if @syms; # always export GetOptions
    $pkg->SUPER::import(@syms);
    # And configure.
    Configure(@config) if @config;
}

################ Initialization ################

# Values for $order. See GNU getopt.c for details.
($REQUIRE_ORDER, $PERMUTE, $RETURN_IN_ORDER) = (0..2);
# Version major/minor numbers.
($major_version, $minor_version) = $VERSION =~ /^(\d+)\.(\d+)/;

ConfigDefaults();

################ OO Interface ################

package Getopt::Long::Parser;

# Store a copy of the default configuration. Since ConfigDefaults has
# just been called, what we get from Configure is the default.
my $default_config = do {
    Getopt::Long::Configure ()
};

sub new {
    my $that = shift;
    my $class = ref($that) || $that;
    my %atts = @_;

    # Register the callers package.
    my $self = { caller_pkg => (caller)[0] };

    bless ($self, $class);

    # Process config attributes.
    if ( defined $atts{config} ) {
	my $save = Getopt::Long::Configure ($default_config, @{$atts{config}});
	$self->{settings} = Getopt::Long::Configure ($save);
	delete ($atts{config});
    }
    # Else use default config.
    else {
	$self->{settings} = $default_config;
    }

    if ( %atts ) {		# Oops
	die(__PACKAGE__.": unhandled attributes: ".
	    join(" ", sort(keys(%atts)))."\n");
    }

    $self;
}

sub configure {
    my ($self) = shift;

    # Restore settings, merge new settings in.
    my $save = Getopt::Long::Configure ($self->{settings}, @_);

    # Restore orig config and save the new config.
    $self->{settings} = Getopt::Long::Configure ($save);
}

sub getoptions {
    my ($self) = shift;

    # Restore config settings.
    my $save = Getopt::Long::Configure ($self->{settings});

    # Call main routine.
    my $ret = 0;
    $Getopt::Long::caller = $self->{caller_pkg};

    eval {
	# Locally set exception handler to default, otherwise it will
	# be called implicitly here, and again explicitly when we try
	# to deliver the messages.
	local ($SIG{__DIE__}) = '__DEFAULT__';
	$ret = Getopt::Long::GetOptions (@_);
    };

    # Restore saved settings.
    Getopt::Long::Configure ($save);

    # Handle errors and return value.
    die ($@) if $@;
    return $ret;
}

package Getopt::Long;

################ Back to Normal ################

# Indices in option control info.
# Note that ParseOptions uses the fields directly. Search for 'hard-wired'.
use constant CTL_TYPE    => 0;
#use constant   CTL_TYPE_FLAG   => '';
#use constant   CTL_TYPE_NEG    => '!';
#use constant   CTL_TYPE_INCR   => '+';
#use constant   CTL_TYPE_INT    => 'i';
#use constant   CTL_TYPE_INTINC => 'I';
#use constant   CTL_TYPE_XINT   => 'o';
#use constant   CTL_TYPE_FLOAT  => 'f';
#use constant   CTL_TYPE_STRING => 's';

use constant CTL_CNAME   => 1;

use constant CTL_DEFAULT => 2;

use constant CTL_DEST    => 3;
 use constant   CTL_DEST_SCALAR => 0;
 use constant   CTL_DEST_ARRAY  => 1;
 use constant   CTL_DEST_HASH   => 2;
 use constant   CTL_DEST_CODE   => 3;

use constant CTL_AMIN    => 4;
use constant CTL_AMAX    => 5;

# FFU.
#use constant CTL_RANGE   => ;
#use constant CTL_REPEAT  => ;

# Rather liberal patterns to match numbers.
use constant PAT_INT   => "[-+]?_*[0-9][0-9_]*";
use constant PAT_XINT  =>
  "(?:".
	  "[-+]?_*[1-9][0-9_]*".
  "|".
	  "0x_*[0-9a-f][0-9a-f_]*".
  "|".
	  "0b_*[01][01_]*".
  "|".
	  "0[0-7_]*".
  ")";
use constant PAT_FLOAT => "[-+]?[0-9._]+(\.[0-9_]+)?([eE][-+]?[0-9_]+)?";

sub GetOptions(@) {
    # Shift in default array.
    unshift(@_, \@ARGV);
    # Try to keep caller() and Carp consitent.
    goto &GetOptionsFromArray;
}

sub GetOptionsFromString($@) {
    my ($string) = shift;
    require Text::ParseWords;
    my $args = [ Text::ParseWords::shellwords($string) ];
    $caller ||= (caller)[0];	# current context
    my $ret = GetOptionsFromArray($args, @_);
    return ( $ret, $args ) if wantarray;
    if ( @$args ) {
	$ret = 0;
	warn("GetOptionsFromString: Excess data \"@$args\" in string \"$string\"\n");
    }
    $ret;
}

sub GetOptionsFromArray($@) {

    my ($argv, @optionlist) = @_;	# local copy of the option descriptions
    my $argend = '--';		# option list terminator
    my %opctl = ();		# table of option specs
    my $pkg = $caller || (caller)[0];	# current context
				# Needed if linkage is omitted.
    my @ret = ();		# accum for non-options
    my %linkage;		# linkage
    my $userlinkage;		# user supplied HASH
    my $opt;			# current option
    my $prefix = $genprefix;	# current prefix

    $error = '';

    if ( $debug ) {
	# Avoid some warnings if debugging.
	local ($^W) = 0;
	print STDERR
	  ("Getopt::Long $Getopt::Long::VERSION (",
	   '$Revision: 2.74 $', ") ",
	   "called from package \"$pkg\".",
	   "\n  ",
	   "argv: (@$argv)",
	   "\n  ",
	   "autoabbrev=$autoabbrev,".
	   "bundling=$bundling,",
	   "getopt_compat=$getopt_compat,",
	   "gnu_compat=$gnu_compat,",
	   "order=$order,",
	   "\n  ",
	   "ignorecase=$ignorecase,",
	   "requested_version=$requested_version,",
	   "passthrough=$passthrough,",
	   "genprefix=\"$genprefix\",",
	   "longprefix=\"$longprefix\".",
	   "\n");
    }

    # Check for ref HASH as first argument.
    # First argument may be an object. It's OK to use this as long
    # as it is really a hash underneath.
    $userlinkage = undef;
    if ( @optionlist && ref($optionlist[0]) and
	 UNIVERSAL::isa($optionlist[0],'HASH') ) {
	$userlinkage = shift (@optionlist);
	print STDERR ("=> user linkage: $userlinkage\n") if $debug;
    }

    # See if the first element of the optionlist contains option
    # starter characters.
    # Be careful not to interpret '<>' as option starters.
    if ( @optionlist && $optionlist[0] =~ /^\W+$/
	 && !($optionlist[0] eq '<>'
	      && @optionlist > 0
	      && ref($optionlist[1])) ) {
	$prefix = shift (@optionlist);
	# Turn into regexp. Needs to be parenthesized!
	$prefix =~ s/(\W)/\\$1/g;
	$prefix = "([" . $prefix . "])";
	print STDERR ("=> prefix=\"$prefix\"\n") if $debug;
    }

    # Verify correctness of optionlist.
    %opctl = ();
    while ( @optionlist ) {
	my $opt = shift (@optionlist);

	unless ( defined($opt) ) {
	    $error .= "Undefined argument in option spec\n";
	    next;
	}

	# Strip leading prefix so people can specify "--foo=i" if they like.
	$opt = $+ if $opt =~ /^$prefix+(.*)$/s;

	if ( $opt eq '<>' ) {
	    if ( (defined $userlinkage)
		&& !(@optionlist > 0 && ref($optionlist[0]))
		&& (exists $userlinkage->{$opt})
		&& ref($userlinkage->{$opt}) ) {
		unshift (@optionlist, $userlinkage->{$opt});
	    }
	    unless ( @optionlist > 0
		    && ref($optionlist[0]) && ref($optionlist[0]) eq 'CODE' ) {
		$error .= "Option spec <> requires a reference to a subroutine\n";
		# Kill the linkage (to avoid another error).
		shift (@optionlist)
		  if @optionlist && ref($optionlist[0]);
		next;
	    }
	    $linkage{'<>'} = shift (@optionlist);
	    next;
	}

	# Parse option spec.
	my ($name, $orig) = ParseOptionSpec ($opt, \%opctl);
	unless ( defined $name ) {
	    # Failed. $orig contains the error message. Sorry for the abuse.
	    $error .= $orig;
	    # Kill the linkage (to avoid another error).
	    shift (@optionlist)
	      if @optionlist && ref($optionlist[0]);
	    next;
	}

	# If no linkage is supplied in the @optionlist, copy it from
	# the userlinkage if available.
	if ( defined $userlinkage ) {
	    unless ( @optionlist > 0 && ref($optionlist[0]) ) {
		if ( exists $userlinkage->{$orig} &&
		     ref($userlinkage->{$orig}) ) {
		    print STDERR ("=> found userlinkage for \"$orig\": ",
				  "$userlinkage->{$orig}\n")
			if $debug;
		    unshift (@optionlist, $userlinkage->{$orig});
		}
		else {
		    # Do nothing. Being undefined will be handled later.
		    next;
		}
	    }
	}

	# Copy the linkage. If omitted, link to global variable.
	if ( @optionlist > 0 && ref($optionlist[0]) ) {
	    print STDERR ("=> link \"$orig\" to $optionlist[0]\n")
		if $debug;
	    my $rl = ref($linkage{$orig} = shift (@optionlist));

	    if ( $rl eq "ARRAY" ) {
		$opctl{$name}[CTL_DEST] = CTL_DEST_ARRAY;
	    }
	    elsif ( $rl eq "HASH" ) {
		$opctl{$name}[CTL_DEST] = CTL_DEST_HASH;
	    }
	    elsif ( $rl eq "SCALAR" || $rl eq "REF" ) {
#		if ( $opctl{$name}[CTL_DEST] == CTL_DEST_ARRAY ) {
#		    my $t = $linkage{$orig};
#		    $$t = $linkage{$orig} = [];
#		}
#		elsif ( $opctl{$name}[CTL_DEST] == CTL_DEST_HASH ) {
#		}
#		else {
		    # Ok.
#		}
	    }
	    elsif ( $rl eq "CODE" ) {
		# Ok.
	    }
	    else {
		$error .= "Invalid option linkage for \"$opt\"\n";
	    }
	}
	else {
	    # Link to global $opt_XXX variable.
	    # Make sure a valid perl identifier results.
	    my $ov = $orig;
	    $ov =~ s/\W/_/g;
	    if ( $opctl{$name}[CTL_DEST] == CTL_DEST_ARRAY ) {
		print STDERR ("=> link \"$orig\" to \@$pkg","::opt_$ov\n")
		    if $debug;
		eval ("\$linkage{\$orig} = \\\@".$pkg."::opt_$ov;");
	    }
	    elsif ( $opctl{$name}[CTL_DEST] == CTL_DEST_HASH ) {
		print STDERR ("=> link \"$orig\" to \%$pkg","::opt_$ov\n")
		    if $debug;
		eval ("\$linkage{\$orig} = \\\%".$pkg."::opt_$ov;");
	    }
	    else {
		print STDERR ("=> link \"$orig\" to \$$pkg","::opt_$ov\n")
		    if $debug;
		eval ("\$linkage{\$orig} = \\\$".$pkg."::opt_$ov;");
	    }
	}
    }

    # Bail out if errors found.
    die ($error) if $error;
    $error = 0;

    # Supply --version and --help support, if needed and allowed.
    if ( defined($auto_version) ? $auto_version : ($requested_version >= 2.3203) ) {
	if ( !defined($opctl{version}) ) {
	    $opctl{version} = ['','version',0,CTL_DEST_CODE,undef];
	    $linkage{version} = \&VersionMessage;
	}
	$auto_version = 1;
    }
    if ( defined($auto_help) ? $auto_help : ($requested_version >= 2.3203) ) {
	if ( !defined($opctl{help}) && !defined($opctl{'?'}) ) {
	    $opctl{help} = $opctl{'?'} = ['','help',0,CTL_DEST_CODE,undef];
	    $linkage{help} = \&HelpMessage;
	}
	$auto_help = 1;
    }

    # Show the options tables if debugging.
    if ( $debug ) {
	my ($arrow, $k, $v);
	$arrow = "=> ";
	while ( ($k,$v) = each(%opctl) ) {
	    print STDERR ($arrow, "\$opctl{$k} = $v ", OptCtl($v), "\n");
	    $arrow = "   ";
	}
    }

    # Process argument list
    my $goon = 1;
    while ( $goon && @$argv > 0 ) {

	# Get next argument.
	$opt = shift (@$argv);
	print STDERR ("=> arg \"", $opt, "\"\n") if $debug;

	# Double dash is option list terminator.
	if ( $opt eq $argend ) {
	  push (@ret, $argend) if $passthrough;
	  last;
	}

	# Look it up.
	my $tryopt = $opt;
	my $found;		# success status
	my $key;		# key (if hash type)
	my $arg;		# option argument
	my $ctl;		# the opctl entry

	($found, $opt, $ctl, $arg, $key) =
	  FindOption ($argv, $prefix, $argend, $opt, \%opctl);

	if ( $found ) {

	    # FindOption undefines $opt in case of errors.
	    next unless defined $opt;

	    my $argcnt = 0;
	    while ( defined $arg ) {

		# Get the canonical name.
		print STDERR ("=> cname for \"$opt\" is ") if $debug;
		$opt = $ctl->[CTL_CNAME];
		print STDERR ("\"$ctl->[CTL_CNAME]\"\n") if $debug;

		if ( defined $linkage{$opt} ) {
		    print STDERR ("=> ref(\$L{$opt}) -> ",
				  ref($linkage{$opt}), "\n") if $debug;

		    if ( ref($linkage{$opt}) eq 'SCALAR'
			 || ref($linkage{$opt}) eq 'REF' ) {
			if ( $ctl->[CTL_TYPE] eq '+' ) {
			    print STDERR ("=> \$\$L{$opt} += \"$arg\"\n")
			      if $debug;
			    if ( defined ${$linkage{$opt}} ) {
			        ${$linkage{$opt}} += $arg;
			    }
		            else {
			        ${$linkage{$opt}} = $arg;
			    }
			}
			elsif ( $ctl->[CTL_DEST] == CTL_DEST_ARRAY ) {
			    print STDERR ("=> ref(\$L{$opt}) auto-vivified",
					  " to ARRAY\n")
			      if $debug;
			    my $t = $linkage{$opt};
			    $$t = $linkage{$opt} = [];
			    print STDERR ("=> push(\@{\$L{$opt}, \"$arg\")\n")
			      if $debug;
			    push (@{$linkage{$opt}}, $arg);
			}
			elsif ( $ctl->[CTL_DEST] == CTL_DEST_HASH ) {
			    print STDERR ("=> ref(\$L{$opt}) auto-vivified",
					  " to HASH\n")
			      if $debug;
			    my $t = $linkage{$opt};
			    $$t = $linkage{$opt} = {};
			    print STDERR ("=> \$\$L{$opt}->{$key} = \"$arg\"\n")
			      if $debug;
			    $linkage{$opt}->{$key} = $arg;
			}
			else {
			    print STDERR ("=> \$\$L{$opt} = \"$arg\"\n")
			      if $debug;
			    ${$linkage{$opt}} = $arg;
		        }
		    }
		    elsif ( ref($linkage{$opt}) eq 'ARRAY' ) {
			print STDERR ("=> push(\@{\$L{$opt}, \"$arg\")\n")
			    if $debug;
			push (@{$linkage{$opt}}, $arg);
		    }
		    elsif ( ref($linkage{$opt}) eq 'HASH' ) {
			print STDERR ("=> \$\$L{$opt}->{$key} = \"$arg\"\n")
			    if $debug;
			$linkage{$opt}->{$key} = $arg;
		    }
		    elsif ( ref($linkage{$opt}) eq 'CODE' ) {
			print STDERR ("=> &L{$opt}(\"$opt\"",
				      $ctl->[CTL_DEST] == CTL_DEST_HASH ? ", \"$key\"" : "",
				      ", \"$arg\")\n")
			    if $debug;
			my $eval_error = do {
			    local $@;
			    local $SIG{__DIE__}  = '__DEFAULT__';
			    eval {
				&{$linkage{$opt}}
				  (Getopt::Long::CallBack->new
				   (name    => $opt,
				    ctl     => $ctl,
				    opctl   => \%opctl,
				    linkage => \%linkage,
				    prefix  => $prefix,
				   ),
				   $ctl->[CTL_DEST] == CTL_DEST_HASH ? ($key) : (),
				   $arg);
			    };
			    $@;
			};
			print STDERR ("=> die($eval_error)\n")
			  if $debug && $eval_error ne '';
			if ( $eval_error =~ /^!/ ) {
			    if ( $eval_error =~ /^!FINISH\b/ ) {
				$goon = 0;
			    }
			}
			elsif ( $eval_error ne '' ) {
			    warn ($eval_error);
			    $error++;
			}
		    }
		    else {
			print STDERR ("Invalid REF type \"", ref($linkage{$opt}),
				      "\" in linkage\n");
			die("Getopt::Long -- internal error!\n");
		    }
		}
		# No entry in linkage means entry in userlinkage.
		elsif ( $ctl->[CTL_DEST] == CTL_DEST_ARRAY ) {
		    if ( defined $userlinkage->{$opt} ) {
			print STDERR ("=> push(\@{\$L{$opt}}, \"$arg\")\n")
			    if $debug;
			push (@{$userlinkage->{$opt}}, $arg);
		    }
		    else {
			print STDERR ("=>\$L{$opt} = [\"$arg\"]\n")
			    if $debug;
			$userlinkage->{$opt} = [$arg];
		    }
		}
		elsif ( $ctl->[CTL_DEST] == CTL_DEST_HASH ) {
		    if ( defined $userlinkage->{$opt} ) {
			print STDERR ("=> \$L{$opt}->{$key} = \"$arg\"\n")
			    if $debug;
			$userlinkage->{$opt}->{$key} = $arg;
		    }
		    else {
			print STDERR ("=>\$L{$opt} = {$key => \"$arg\"}\n")
			    if $debug;
			$userlinkage->{$opt} = {$key => $arg};
		    }
		}
		else {
		    if ( $ctl->[CTL_TYPE] eq '+' ) {
			print STDERR ("=> \$L{$opt} += \"$arg\"\n")
			  if $debug;
			if ( defined $userlinkage->{$opt} ) {
			    $userlinkage->{$opt} += $arg;
			}
			else {
			    $userlinkage->{$opt} = $arg;
			}
		    }
		    else {
			print STDERR ("=>\$L{$opt} = \"$arg\"\n") if $debug;
			$userlinkage->{$opt} = $arg;
		    }
		}

		$argcnt++;
		last if $argcnt >= $ctl->[CTL_AMAX] && $ctl->[CTL_AMAX] != -1;
		undef($arg);

		# Need more args?
		if ( $argcnt < $ctl->[CTL_AMIN] ) {
		    if ( @$argv ) {
			if ( ValidValue($ctl, $argv->[0], 1, $argend, $prefix) ) {
			    $arg = shift(@$argv);
			    $arg =~ tr/_//d if $ctl->[CTL_TYPE] =~ /^[iIo]$/;
			    ($key,$arg) = $arg =~ /^([^=]+)=(.*)/
			      if $ctl->[CTL_DEST] == CTL_DEST_HASH;
			    next;
			}
			warn("Value \"$$argv[0]\" invalid for option $opt\n");
			$error++;
		    }
		    else {
			warn("Insufficient arguments for option $opt\n");
			$error++;
		    }
		}

		# Any more args?
		if ( @$argv && ValidValue($ctl, $argv->[0], 0, $argend, $prefix) ) {
		    $arg = shift(@$argv);
		    $arg =~ tr/_//d if $ctl->[CTL_TYPE] =~ /^[iIo]$/;
		    ($key,$arg) = $arg =~ /^([^=]+)=(.*)/
		      if $ctl->[CTL_DEST] == CTL_DEST_HASH;
		    next;
		}
	    }
	}

	# Not an option. Save it if we $PERMUTE and don't have a <>.
	elsif ( $order == $PERMUTE ) {
	    # Try non-options call-back.
	    my $cb;
	    if ( (defined ($cb = $linkage{'<>'})) ) {
		print STDERR ("=> &L{$tryopt}(\"$tryopt\")\n")
		  if $debug;
		my $eval_error = do {
		    local $@;
		    local $SIG{__DIE__}  = '__DEFAULT__';
		    eval { &$cb ($tryopt) };
		    $@;
		};
		print STDERR ("=> die($eval_error)\n")
		  if $debug && $eval_error ne '';
		if ( $eval_error =~ /^!/ ) {
		    if ( $eval_error =~ /^!FINISH\b/ ) {
			$goon = 0;
		    }
		}
		elsif ( $eval_error ne '' ) {
		    warn ($eval_error);
		    $error++;
		}
	    }
	    else {
		print STDERR ("=> saving \"$tryopt\" ",
			      "(not an option, may permute)\n") if $debug;
		push (@ret, $tryopt);
	    }
	    next;
	}

	# ...otherwise, terminate.
	else {
	    # Push this one back and exit.
	    unshift (@$argv, $tryopt);
	    return ($error == 0);
	}

    }

    # Finish.
    if ( @ret && $order == $PERMUTE ) {
	#  Push back accumulated arguments
	print STDERR ("=> restoring \"", join('" "', @ret), "\"\n")
	    if $debug;
	unshift (@$argv, @ret);
    }

    return ($error == 0);
}

# A readable representation of what's in an optbl.
sub OptCtl ($) {
    my ($v) = @_;
    my @v = map { defined($_) ? ($_) : ("<undef>") } @$v;
    "[".
      join(",",
	   "\"$v[CTL_TYPE]\"",
	   "\"$v[CTL_CNAME]\"",
	   "\"$v[CTL_DEFAULT]\"",
	   ("\$","\@","\%","\&")[$v[CTL_DEST] || 0],
	   $v[CTL_AMIN] || '',
	   $v[CTL_AMAX] || '',
#	   $v[CTL_RANGE] || '',
#	   $v[CTL_REPEAT] || '',
	  ). "]";
}

# Parse an option specification and fill the tables.
sub ParseOptionSpec ($$) {
    my ($opt, $opctl) = @_;

    # Match option spec.
    if ( $opt !~ m;^
		   (
		     # Option name
		     (?: \w+[-\w]* )
		     # Alias names, or "?"
		     (?: \| (?: \? | \w[-\w]* )? )*
		   )?
		   (
		     # Either modifiers ...
		     [!+]
		     |
		     # ... or a value/dest/repeat specification
		     [=:] [ionfs] [@%]? (?: \{\d*,?\d*\} )?
		     |
		     # ... or an optional-with-default spec
		     : (?: -?\d+ | \+ ) [@%]?
		   )?
		   $;x ) {
	return (undef, "Error in option spec: \"$opt\"\n");
    }

    my ($names, $spec) = ($1, $2);
    $spec = '' unless defined $spec;

    # $orig keeps track of the primary name the user specified.
    # This name will be used for the internal or external linkage.
    # In other words, if the user specifies "FoO|BaR", it will
    # match any case combinations of 'foo' and 'bar', but if a global
    # variable needs to be set, it will be $opt_FoO in the exact case
    # as specified.
    my $orig;

    my @names;
    if ( defined $names ) {
	@names =  split (/\|/, $names);
	$orig = $names[0];
    }
    else {
	@names = ('');
	$orig = '';
    }

    # Construct the opctl entries.
    my $entry;
    if ( $spec eq '' || $spec eq '+' || $spec eq '!' ) {
	# Fields are hard-wired here.
	$entry = [$spec,$orig,undef,CTL_DEST_SCALAR,0,0];
    }
    elsif ( $spec =~ /^:(-?\d+|\+)([@%])?$/ ) {
	my $def = $1;
	my $dest = $2;
	my $type = $def eq '+' ? 'I' : 'i';
	$dest ||= '$';
	$dest = $dest eq '@' ? CTL_DEST_ARRAY
	  : $dest eq '%' ? CTL_DEST_HASH : CTL_DEST_SCALAR;
	# Fields are hard-wired here.
	$entry = [$type,$orig,$def eq '+' ? undef : $def,
		  $dest,0,1];
    }
    else {
	my ($mand, $type, $dest) =
	  $spec =~ /^([=:])([ionfs])([@%])?(\{(\d+)?(,)?(\d+)?\})?$/;
	return (undef, "Cannot repeat while bundling: \"$opt\"\n")
	  if $bundling && defined($4);
	my ($mi, $cm, $ma) = ($5, $6, $7);
	return (undef, "{0} is useless in option spec: \"$opt\"\n")
	  if defined($mi) && !$mi && !defined($ma) && !defined($cm);

	$type = 'i' if $type eq 'n';
	$dest ||= '$';
	$dest = $dest eq '@' ? CTL_DEST_ARRAY
	  : $dest eq '%' ? CTL_DEST_HASH : CTL_DEST_SCALAR;
	# Default minargs to 1/0 depending on mand status.
	$mi = $mand eq '=' ? 1 : 0 unless defined $mi;
	# Adjust mand status according to minargs.
	$mand = $mi ? '=' : ':';
	# Adjust maxargs.
	$ma = $mi ? $mi : 1 unless defined $ma || defined $cm;
	return (undef, "Max must be greater than zero in option spec: \"$opt\"\n")
	  if defined($ma) && !$ma;
	return (undef, "Max less than min in option spec: \"$opt\"\n")
	  if defined($ma) && $ma < $mi;

	# Fields are hard-wired here.
	$entry = [$type,$orig,undef,$dest,$mi,$ma||-1];
    }

    # Process all names. First is canonical, the rest are aliases.
    my $dups = '';
    foreach ( @names ) {

	$_ = lc ($_)
	  if $ignorecase > (($bundling && length($_) == 1) ? 1 : 0);

	if ( exists $opctl->{$_} ) {
	    $dups .= "Duplicate specification \"$opt\" for option \"$_\"\n";
	}

	if ( $spec eq '!' ) {
	    $opctl->{"no$_"} = $entry;
	    $opctl->{"no-$_"} = $entry;
	    $opctl->{$_} = [@$entry];
	    $opctl->{$_}->[CTL_TYPE] = '';
	}
	else {
	    $opctl->{$_} = $entry;
	}
    }

    if ( $dups && $^W ) {
	foreach ( split(/\n+/, $dups) ) {
	    warn($_."\n");
	}
    }
    ($names[0], $orig);
}

# Option lookup.
sub FindOption ($$$$$) {

    # returns (1, $opt, $ctl, $arg, $key) if okay,
    # returns (1, undef) if option in error,
    # returns (0) otherwise.

    my ($argv, $prefix, $argend, $opt, $opctl) = @_;

    print STDERR ("=> find \"$opt\"\n") if $debug;

    return (0) unless $opt =~ /^$prefix(.*)$/s;
    return (0) if $opt eq "-" && !defined $opctl->{''};

    $opt = $+;
    my $starter = $1;

    print STDERR ("=> split \"$starter\"+\"$opt\"\n") if $debug;

    my $optarg;			# value supplied with --opt=value
    my $rest;			# remainder from unbundling

    # If it is a long option, it may include the value.
    # With getopt_compat, only if not bundling.
    if ( ($starter=~/^$longprefix$/
          || ($getopt_compat && ($bundling == 0 || $bundling == 2)))
	  && $opt =~ /^([^=]+)=(.*)$/s ) {
	$opt = $1;
	$optarg = $2;
	print STDERR ("=> option \"", $opt,
		      "\", optarg = \"$optarg\"\n") if $debug;
    }

    #### Look it up ###

    my $tryopt = $opt;		# option to try

    if ( $bundling && $starter eq '-' ) {

	# To try overrides, obey case ignore.
	$tryopt = $ignorecase ? lc($opt) : $opt;

	# If bundling == 2, long options can override bundles.
	if ( $bundling == 2 && length($tryopt) > 1
	     && defined ($opctl->{$tryopt}) ) {
	    print STDERR ("=> $starter$tryopt overrides unbundling\n")
	      if $debug;
	}
	else {
	    $tryopt = $opt;
	    # Unbundle single letter option.
	    $rest = length ($tryopt) > 0 ? substr ($tryopt, 1) : '';
	    $tryopt = substr ($tryopt, 0, 1);
	    $tryopt = lc ($tryopt) if $ignorecase > 1;
	    print STDERR ("=> $starter$tryopt unbundled from ",
			  "$starter$tryopt$rest\n") if $debug;
	    $rest = undef unless $rest ne '';
	}
    }

    # Try auto-abbreviation.
    elsif ( $autoabbrev ) {
	# Sort the possible long option names.
	my @names = sort(keys (%$opctl));
	# Downcase if allowed.
	$opt = lc ($opt) if $ignorecase;
	$tryopt = $opt;
	# Turn option name into pattern.
	my $pat = quotemeta ($opt);
	# Look up in option names.
	my @hits = grep (/^$pat/, @names);
	print STDERR ("=> ", scalar(@hits), " hits (@hits) with \"$pat\" ",
		      "out of ", scalar(@names), "\n") if $debug;

	# Check for ambiguous results.
	unless ( (@hits <= 1) || (grep ($_ eq $opt, @hits) == 1) ) {
	    # See if all matches are for the same option.
	    my %hit;
	    foreach ( @hits ) {
		my $hit = $_;
		$hit = $opctl->{$hit}->[CTL_CNAME]
		  if defined $opctl->{$hit}->[CTL_CNAME];
		$hit{$hit} = 1;
	    }
	    # Remove auto-supplied options (version, help).
	    if ( keys(%hit) == 2 ) {
		if ( $auto_version && exists($hit{version}) ) {
		    delete $hit{version};
		}
		elsif ( $auto_help && exists($hit{help}) ) {
		    delete $hit{help};
		}
	    }
	    # Now see if it really is ambiguous.
	    unless ( keys(%hit) == 1 ) {
		return (0) if $passthrough;
		warn ("Option ", $opt, " is ambiguous (",
		      join(", ", @hits), ")\n");
		$error++;
		return (1, undef);
	    }
	    @hits = keys(%hit);
	}

	# Complete the option name, if appropriate.
	if ( @hits == 1 && $hits[0] ne $opt ) {
	    $tryopt = $hits[0];
	    $tryopt = lc ($tryopt) if $ignorecase;
	    print STDERR ("=> option \"$opt\" -> \"$tryopt\"\n")
		if $debug;
	}
    }

    # Map to all lowercase if ignoring case.
    elsif ( $ignorecase ) {
	$tryopt = lc ($opt);
    }

    # Check validity by fetching the info.
    my $ctl = $opctl->{$tryopt};
    unless  ( defined $ctl ) {
	return (0) if $passthrough;
	# Pretend one char when bundling.
	if ( $bundling == 1 && length($starter) == 1 ) {
	    $opt = substr($opt,0,1);
            unshift (@$argv, $starter.$rest) if defined $rest;
	}
	warn ("Unknown option: ", $opt, "\n");
	$error++;
	return (1, undef);
    }
    # Apparently valid.
    $opt = $tryopt;
    print STDERR ("=> found ", OptCtl($ctl),
		  " for \"", $opt, "\"\n") if $debug;

    #### Determine argument status ####

    # If it is an option w/o argument, we're almost finished with it.
    my $type = $ctl->[CTL_TYPE];
    my $arg;

    if ( $type eq '' || $type eq '!' || $type eq '+' ) {
	if ( defined $optarg ) {
	    return (0) if $passthrough;
	    warn ("Option ", $opt, " does not take an argument\n");
	    $error++;
	    undef $opt;
	}
	elsif ( $type eq '' || $type eq '+' ) {
	    # Supply explicit value.
	    $arg = 1;
	}
	else {
	    $opt =~ s/^no-?//i;	# strip NO prefix
	    $arg = 0;		# supply explicit value
	}
	unshift (@$argv, $starter.$rest) if defined $rest;
	return (1, $opt, $ctl, $arg);
    }

    # Get mandatory status and type info.
    my $mand = $ctl->[CTL_AMIN];

    # Check if there is an option argument available.
    if ( $gnu_compat && defined $optarg && $optarg eq '' ) {
	return (1, $opt, $ctl, $type eq 's' ? '' : 0) ;#unless $mand;
	$optarg = 0 unless $type eq 's';
    }

    # Check if there is an option argument available.
    if ( defined $optarg
	 ? ($optarg eq '')
	 : !(defined $rest || @$argv > 0) ) {
	# Complain if this option needs an argument.
#	if ( $mand && !($type eq 's' ? defined($optarg) : 0) ) {
	if ( $mand ) {
	    return (0) if $passthrough;
	    warn ("Option ", $opt, " requires an argument\n");
	    $error++;
	    return (1, undef);
	}
	if ( $type eq 'I' ) {
	    # Fake incremental type.
	    my @c = @$ctl;
	    $c[CTL_TYPE] = '+';
	    return (1, $opt, \@c, 1);
	}
	return (1, $opt, $ctl,
		defined($ctl->[CTL_DEFAULT]) ? $ctl->[CTL_DEFAULT] :
		$type eq 's' ? '' : 0);
    }

    # Get (possibly optional) argument.
    $arg = (defined $rest ? $rest
	    : (defined $optarg ? $optarg : shift (@$argv)));

    # Get key if this is a "name=value" pair for a hash option.
    my $key;
    if ($ctl->[CTL_DEST] == CTL_DEST_HASH && defined $arg) {
	($key, $arg) = ($arg =~ /^([^=]*)=(.*)$/s) ? ($1, $2)
	  : ($arg, defined($ctl->[CTL_DEFAULT]) ? $ctl->[CTL_DEFAULT] :
	     ($mand ? undef : ($type eq 's' ? "" : 1)));
	if (! defined $arg) {
	    warn ("Option $opt, key \"$key\", requires a value\n");
	    $error++;
	    # Push back.
	    unshift (@$argv, $starter.$rest) if defined $rest;
	    return (1, undef);
	}
    }

    #### Check if the argument is valid for this option ####

    my $key_valid = $ctl->[CTL_DEST] == CTL_DEST_HASH ? "[^=]+=" : "";

    if ( $type eq 's' ) {	# string
	# A mandatory string takes anything.
	return (1, $opt, $ctl, $arg, $key) if $mand;

	# Same for optional string as a hash value
	return (1, $opt, $ctl, $arg, $key)
	  if $ctl->[CTL_DEST] == CTL_DEST_HASH;

	# An optional string takes almost anything.
	return (1, $opt, $ctl, $arg, $key)
	  if defined $optarg || defined $rest;
	return (1, $opt, $ctl, $arg, $key) if $arg eq "-"; # ??

	# Check for option or option list terminator.
	if ($arg eq $argend ||
	    $arg =~ /^$prefix.+/) {
	    # Push back.
	    unshift (@$argv, $arg);
	    # Supply empty value.
	    $arg = '';
	}
    }

    elsif ( $type eq 'i'	# numeric/integer
            || $type eq 'I'	# numeric/integer w/ incr default
	    || $type eq 'o' ) { # dec/oct/hex/bin value

	my $o_valid = $type eq 'o' ? PAT_XINT : PAT_INT;

	if ( $bundling && defined $rest
	     && $rest =~ /^($key_valid)($o_valid)(.*)$/si ) {
	    ($key, $arg, $rest) = ($1, $2, $+);
	    chop($key) if $key;
	    $arg = ($type eq 'o' && $arg =~ /^0/) ? oct($arg) : 0+$arg;
	    unshift (@$argv, $starter.$rest) if defined $rest && $rest ne '';
	}
	elsif ( $arg =~ /^$o_valid$/si ) {
	    $arg =~ tr/_//d;
	    $arg = ($type eq 'o' && $arg =~ /^0/) ? oct($arg) : 0+$arg;
	}
	else {
	    if ( defined $optarg || $mand ) {
		if ( $passthrough ) {
		    unshift (@$argv, defined $rest ? $starter.$rest : $arg)
		      unless defined $optarg;
		    return (0);
		}
		warn ("Value \"", $arg, "\" invalid for option ",
		      $opt, " (",
		      $type eq 'o' ? "extended " : '',
		      "number expected)\n");
		$error++;
		# Push back.
		unshift (@$argv, $starter.$rest) if defined $rest;
		return (1, undef);
	    }
	    else {
		# Push back.
		unshift (@$argv, defined $rest ? $starter.$rest : $arg);
		if ( $type eq 'I' ) {
		    # Fake incremental type.
		    my @c = @$ctl;
		    $c[CTL_TYPE] = '+';
		    return (1, $opt, \@c, 1);
		}
		# Supply default value.
		$arg = defined($ctl->[CTL_DEFAULT]) ? $ctl->[CTL_DEFAULT] : 0;
	    }
	}
    }

    elsif ( $type eq 'f' ) { # real number, int is also ok
	# We require at least one digit before a point or 'e',
	# and at least one digit following the point and 'e'.
	# [-]NN[.NN][eNN]
	my $o_valid = PAT_FLOAT;
	if ( $bundling && defined $rest &&
	     $rest =~ /^($key_valid)($o_valid)(.*)$/s ) {
	    $arg =~ tr/_//d;
	    ($key, $arg, $rest) = ($1, $2, $+);
	    chop($key) if $key;
	    unshift (@$argv, $starter.$rest) if defined $rest && $rest ne '';
	}
	elsif ( $arg =~ /^$o_valid$/ ) {
	    $arg =~ tr/_//d;
	}
	else {
	    if ( defined $optarg || $mand ) {
		if ( $passthrough ) {
		    unshift (@$argv, defined $rest ? $starter.$rest : $arg)
		      unless defined $optarg;
		    return (0);
		}
		warn ("Value \"", $arg, "\" invalid for option ",
		      $opt, " (real number expected)\n");
		$error++;
		# Push back.
		unshift (@$argv, $starter.$rest) if defined $rest;
		return (1, undef);
	    }
	    else {
		# Push back.
		unshift (@$argv, defined $rest ? $starter.$rest : $arg);
		# Supply default value.
		$arg = 0.0;
	    }
	}
    }
    else {
	die("Getopt::Long internal error (Can't happen)\n");
    }
    return (1, $opt, $ctl, $arg, $key);
}

sub ValidValue ($$$$$) {
    my ($ctl, $arg, $mand, $argend, $prefix) = @_;

    if ( $ctl->[CTL_DEST] == CTL_DEST_HASH ) {
	return 0 unless $arg =~ /[^=]+=(.*)/;
	$arg = $1;
    }

    my $type = $ctl->[CTL_TYPE];

    if ( $type eq 's' ) {	# string
	# A mandatory string takes anything.
	return (1) if $mand;

	return (1) if $arg eq "-";

	# Check for option or option list terminator.
	return 0 if $arg eq $argend || $arg =~ /^$prefix.+/;
	return 1;
    }

    elsif ( $type eq 'i'	# numeric/integer
            || $type eq 'I'	# numeric/integer w/ incr default
	    || $type eq 'o' ) { # dec/oct/hex/bin value

	my $o_valid = $type eq 'o' ? PAT_XINT : PAT_INT;
	return $arg =~ /^$o_valid$/si;
    }

    elsif ( $type eq 'f' ) { # real number, int is also ok
	# We require at least one digit before a point or 'e',
	# and at least one digit following the point and 'e'.
	# [-]NN[.NN][eNN]
	my $o_valid = PAT_FLOAT;
	return $arg =~ /^$o_valid$/;
    }
    die("ValidValue: Cannot happen\n");
}

# Getopt::Long Configuration.
sub Configure (@) {
    my (@options) = @_;

    my $prevconfig =
      [ $error, $debug, $major_version, $minor_version,
	$autoabbrev, $getopt_compat, $ignorecase, $bundling, $order,
	$gnu_compat, $passthrough, $genprefix, $auto_version, $auto_help,
	$longprefix ];

    if ( ref($options[0]) eq 'ARRAY' ) {
	( $error, $debug, $major_version, $minor_version,
	  $autoabbrev, $getopt_compat, $ignorecase, $bundling, $order,
	  $gnu_compat, $passthrough, $genprefix, $auto_version, $auto_help,
	  $longprefix ) = @{shift(@options)};
    }

    my $opt;
    foreach $opt ( @options ) {
	my $try = lc ($opt);
	my $action = 1;
	if ( $try =~ /^no_?(.*)$/s ) {
	    $action = 0;
	    $try = $+;
	}
	if ( ($try eq 'default' or $try eq 'defaults') && $action ) {
	    ConfigDefaults ();
	}
	elsif ( ($try eq 'posix_default' or $try eq 'posix_defaults') ) {
	    local $ENV{POSIXLY_CORRECT};
	    $ENV{POSIXLY_CORRECT} = 1 if $action;
	    ConfigDefaults ();
	}
	elsif ( $try eq 'auto_abbrev' or $try eq 'autoabbrev' ) {
	    $autoabbrev = $action;
	}
	elsif ( $try eq 'getopt_compat' ) {
	    $getopt_compat = $action;
            $genprefix = $action ? "(--|-|\\+)" : "(--|-)";
	}
	elsif ( $try eq 'gnu_getopt' ) {
	    if ( $action ) {
		$gnu_compat = 1;
		$bundling = 1;
		$getopt_compat = 0;
                $genprefix = "(--|-)";
		$order = $PERMUTE;
	    }
	}
	elsif ( $try eq 'gnu_compat' ) {
	    $gnu_compat = $action;
	}
	elsif ( $try =~ /^(auto_?)?version$/ ) {
	    $auto_version = $action;
	}
	elsif ( $try =~ /^(auto_?)?help$/ ) {
	    $auto_help = $action;
	}
	elsif ( $try eq 'ignorecase' or $try eq 'ignore_case' ) {
	    $ignorecase = $action;
	}
	elsif ( $try eq 'ignorecase_always' or $try eq 'ignore_case_always' ) {
	    $ignorecase = $action ? 2 : 0;
	}
	elsif ( $try eq 'bundling' ) {
	    $bundling = $action;
	}
	elsif ( $try eq 'bundling_override' ) {
	    $bundling = $action ? 2 : 0;
	}
	elsif ( $try eq 'require_order' ) {
	    $order = $action ? $REQUIRE_ORDER : $PERMUTE;
	}
	elsif ( $try eq 'permute' ) {
	    $order = $action ? $PERMUTE : $REQUIRE_ORDER;
	}
	elsif ( $try eq 'pass_through' or $try eq 'passthrough' ) {
	    $passthrough = $action;
	}
	elsif ( $try =~ /^prefix=(.+)$/ && $action ) {
	    $genprefix = $1;
	    # Turn into regexp. Needs to be parenthesized!
	    $genprefix = "(" . quotemeta($genprefix) . ")";
	    eval { '' =~ /$genprefix/; };
	    die("Getopt::Long: invalid pattern \"$genprefix\"") if $@;
	}
	elsif ( $try =~ /^prefix_pattern=(.+)$/ && $action ) {
	    $genprefix = $1;
	    # Parenthesize if needed.
	    $genprefix = "(" . $genprefix . ")"
	      unless $genprefix =~ /^\(.*\)$/;
	    eval { '' =~ m"$genprefix"; };
	    die("Getopt::Long: invalid pattern \"$genprefix\"") if $@;
	}
	elsif ( $try =~ /^long_prefix_pattern=(.+)$/ && $action ) {
	    $longprefix = $1;
	    # Parenthesize if needed.
	    $longprefix = "(" . $longprefix . ")"
	      unless $longprefix =~ /^\(.*\)$/;
	    eval { '' =~ m"$longprefix"; };
	    die("Getopt::Long: invalid long prefix pattern \"$longprefix\"") if $@;
	}
	elsif ( $try eq 'debug' ) {
	    $debug = $action;
	}
	else {
	    die("Getopt::Long: unknown config parameter \"$opt\"")
	}
    }
    $prevconfig;
}

# Deprecated name.
sub config (@) {
    Configure (@_);
}

# Issue a standard message for --version.
#
# The arguments are mostly the same as for Pod::Usage::pod2usage:
#
#  - a number (exit value)
#  - a string (lead in message)
#  - a hash with options. See Pod::Usage for details.
#
sub VersionMessage(@) {
    # Massage args.
    my $pa = setup_pa_args("version", @_);

    my $v = $main::VERSION;
    my $fh = $pa->{-output} ||
      ($pa->{-exitval} eq "NOEXIT" || $pa->{-exitval} < 2) ? \*STDOUT : \*STDERR;

    print $fh (defined($pa->{-message}) ? $pa->{-message} : (),
	       $0, defined $v ? " version $v" : (),
	       "\n",
	       "(", __PACKAGE__, "::", "GetOptions",
	       " version ",
	       defined($Getopt::Long::VERSION_STRING)
	         ? $Getopt::Long::VERSION_STRING : $VERSION, ";",
	       " Perl version ",
	       $] >= 5.006 ? sprintf("%vd", $^V) : $],
	       ")\n");
    exit($pa->{-exitval}) unless $pa->{-exitval} eq "NOEXIT";
}

# Issue a standard message for --help.
#
# The arguments are the same as for Pod::Usage::pod2usage:
#
#  - a number (exit value)
#  - a string (lead in message)
#  - a hash with options. See Pod::Usage for details.
#
sub HelpMessage(@) {
    eval {
	require Pod::Usage;
	import Pod::Usage;
	1;
    } || die("Cannot provide help: cannot load Pod::Usage\n");

    # Note that pod2usage will issue a warning if -exitval => NOEXIT.
    pod2usage(setup_pa_args("help", @_));

}

# Helper routine to set up a normalized hash ref to be used as
# argument to pod2usage.
sub setup_pa_args($@) {
    my $tag = shift;		# who's calling

    # If called by direct binding to an option, it will get the option
    # name and value as arguments. Remove these, if so.
    @_ = () if @_ == 2 && $_[0] eq $tag;

    my $pa;
    if ( @_ > 1 ) {
	$pa = { @_ };
    }
    else {
	$pa = shift || {};
    }

    # At this point, $pa can be a number (exit value), string
    # (message) or hash with options.

    if ( UNIVERSAL::isa($pa, 'HASH') ) {
	# Get rid of -msg vs. -message ambiguity.
	$pa->{-message} = $pa->{-msg};
	delete($pa->{-msg});
    }
    elsif ( $pa =~ /^-?\d+$/ ) {
	$pa = { -exitval => $pa };
    }
    else {
	$pa = { -message => $pa };
    }

    # These are _our_ defaults.
    $pa->{-verbose} = 0 unless exists($pa->{-verbose});
    $pa->{-exitval} = 0 unless exists($pa->{-exitval});
    $pa;
}

# Sneak way to know what version the user requested.
sub VERSION {
    $requested_version = $_[1];
    shift->SUPER::VERSION(@_);
}

package Getopt::Long::CallBack;

sub new {
    my ($pkg, %atts) = @_;
    bless { %atts }, $pkg;
}

sub name {
    my $self = shift;
    ''.$self->{name};
}

use overload
  # Treat this object as an oridinary string for legacy API.
  '""'	   => \&name,
  '0+'	   => sub { 0 },
  fallback => 1;

1;

################ Documentation ################

=head1 NAME

Getopt::Long - Extended processing of command line options

=head1 SYNOPSIS

  use Getopt::Long;
  my $data   = "file.dat";
  my $length = 24;
  my $verbose;
  $result = GetOptions ("length=i" => \$length,    # numeric
                        "file=s"   => \$data,      # string
			"verbose"  => \$verbose);  # flag

=head1 DESCRIPTION

The Getopt::Long module implements an extended getopt function called
GetOptions(). This function adheres to the POSIX syntax for command
line options, with GNU extensions. In general, this means that options
have long names instead of single letters, and are introduced with a
double dash "--". Support for bundling of command line options, as was
the case with the more traditional single-letter approach, is provided
but not enabled by default.

=head1 Command Line Options, an Introduction

Command line operated programs traditionally take their arguments from
the command line, for example filenames or other information that the
program needs to know. Besides arguments, these programs often take
command line I<options> as well. Options are not necessary for the
program to work, hence the name 'option', but are used to modify its
default behaviour. For example, a program could do its job quietly,
but with a suitable option it could provide verbose information about
what it did.

Command line options come in several flavours. Historically, they are
preceded by a single dash C<->, and consist of a single letter.

    -l -a -c

Usually, these single-character options can be bundled:

    -lac

Options can have values, the value is placed after the option
character. Sometimes with whitespace in between, sometimes not:

    -s 24 -s24

Due to the very cryptic nature of these options, another style was
developed that used long names. So instead of a cryptic C<-l> one
could use the more descriptive C<--long>. To distinguish between a
bundle of single-character options and a long one, two dashes are used
to precede the option name. Early implementations of long options used
a plus C<+> instead. Also, option values could be specified either
like

    --size=24

or

    --size 24

The C<+> form is now obsolete and strongly deprecated.

=head1 Getting Started with Getopt::Long

Getopt::Long is the Perl5 successor of C<newgetopt.pl>. This was the
first Perl module that provided support for handling the new style of
command line options, hence the name Getopt::Long. This module also
supports single-character options and bundling. Single character
options may be any alphabetic character, a question mark, and a dash.
Long options may consist of a series of letters, digits, and dashes.
Although this is currently not enforced by Getopt::Long, multiple
consecutive dashes are not allowed, and the option name must not end
with a dash.

To use Getopt::Long from a Perl program, you must include the
following line in your Perl program:

    use Getopt::Long;

This will load the core of the Getopt::Long module and prepare your
program for using it. Most of the actual Getopt::Long code is not
loaded until you really call one of its functions.

In the default configuration, options names may be abbreviated to
uniqueness, case does not matter, and a single dash is sufficient,
even for long option names. Also, options may be placed between
non-option arguments. See L<Configuring Getopt::Long> for more
details on how to configure Getopt::Long.

=head2 Simple options

The most simple options are the ones that take no values. Their mere
presence on the command line enables the option. Popular examples are:

    --all --verbose --quiet --debug

Handling simple options is straightforward:

    my $verbose = '';	# option variable with default value (false)
    my $all = '';	# option variable with default value (false)
    GetOptions ('verbose' => \$verbose, 'all' => \$all);

The call to GetOptions() parses the command line arguments that are
present in C<@ARGV> and sets the option variable to the value C<1> if
the option did occur on the command line. Otherwise, the option
variable is not touched. Setting the option value to true is often
called I<enabling> the option.

The option name as specified to the GetOptions() function is called
the option I<specification>. Later we'll see that this specification
can contain more than just the option name. The reference to the
variable is called the option I<destination>.

GetOptions() will return a true value if the command line could be
processed successfully. Otherwise, it will write error messages to
STDERR, and return a false result.

=head2 A little bit less simple options

Getopt::Long supports two useful variants of simple options:
I<negatable> options and I<incremental> options.

A negatable option is specified with an exclamation mark C<!> after the
option name:

    my $verbose = '';	# option variable with default value (false)
    GetOptions ('verbose!' => \$verbose);

Now, using C<--verbose> on the command line will enable C<$verbose>,
as expected. But it is also allowed to use C<--noverbose>, which will
disable C<$verbose> by setting its value to C<0>. Using a suitable
default value, the program can find out whether C<$verbose> is false
by default, or disabled by using C<--noverbose>.

An incremental option is specified with a plus C<+> after the
option name:

    my $verbose = '';	# option variable with default value (false)
    GetOptions ('verbose+' => \$verbose);

Using C<--verbose> on the command line will increment the value of
C<$verbose>. This way the program can keep track of how many times the
option occurred on the command line. For example, each occurrence of
C<--verbose> could increase the verbosity level of the program.

=head2 Mixing command line option with other arguments

Usually programs take command line options as well as other arguments,
for example, file names. It is good practice to always specify the
options first, and the other arguments last. Getopt::Long will,
however, allow the options and arguments to be mixed and 'filter out'
all the options before passing the rest of the arguments to the
program. To stop Getopt::Long from processing further arguments,
insert a double dash C<--> on the command line:

    --size 24 -- --all

In this example, C<--all> will I<not> be treated as an option, but
passed to the program unharmed, in C<@ARGV>.

=head2 Options with values

For options that take values it must be specified whether the option
value is required or not, and what kind of value the option expects.

Three kinds of values are supported: integer numbers, floating point
numbers, and strings.

If the option value is required, Getopt::Long will take the
command line argument that follows the option and assign this to the
option variable. If, however, the option value is specified as
optional, this will only be done if that value does not look like a
valid command line option itself.

    my $tag = '';	# option variable with default value
    GetOptions ('tag=s' => \$tag);

In the option specification, the option name is followed by an equals
sign C<=> and the letter C<s>. The equals sign indicates that this
option requires a value. The letter C<s> indicates that this value is
an arbitrary string. Other possible value types are C<i> for integer
values, and C<f> for floating point values. Using a colon C<:> instead
of the equals sign indicates that the option value is optional. In
this case, if no suitable value is supplied, string valued options get
an empty string C<''> assigned, while numeric options are set to C<0>.

=head2 Options with multiple values

Options sometimes take several values. For example, a program could
use multiple directories to search for library files:

    --library lib/stdlib --library lib/extlib

To accomplish this behaviour, simply specify an array reference as the
destination for the option:

    GetOptions ("library=s" => \@libfiles);

Alternatively, you can specify that the option can have multiple
values by adding a "@", and pass a scalar reference as the
destination:

    GetOptions ("library=s@" => \$libfiles);

Used with the example above, C<@libfiles> (or C<@$libfiles>) would
contain two strings upon completion: C<"lib/srdlib"> and
C<"lib/extlib">, in that order. It is also possible to specify that
only integer or floating point numbers are acceptable values.

Often it is useful to allow comma-separated lists of values as well as
multiple occurrences of the options. This is easy using Perl's split()
and join() operators:

    GetOptions ("library=s" => \@libfiles);
    @libfiles = split(/,/,join(',',@libfiles));

Of course, it is important to choose the right separator string for
each purpose.

Warning: What follows is an experimental feature.

Options can take multiple values at once, for example

    --coordinates 52.2 16.4 --rgbcolor 255 255 149

This can be accomplished by adding a repeat specifier to the option
specification. Repeat specifiers are very similar to the C<{...}>
repeat specifiers that can be used with regular expression patterns.
For example, the above command line would be handled as follows:

    GetOptions('coordinates=f{2}' => \@coor, 'rgbcolor=i{3}' => \@color);

The destination for the option must be an array or array reference.

It is also possible to specify the minimal and maximal number of
arguments an option takes. C<foo=s{2,4}> indicates an option that
takes at least two and at most 4 arguments. C<foo=s{,}> indicates one
or more values; C<foo:s{,}> indicates zero or more option values.

=head2 Options with hash values

If the option destination is a reference to a hash, the option will
take, as value, strings of the form I<key>C<=>I<value>. The value will
be stored with the specified key in the hash.

    GetOptions ("define=s" => \%defines);

Alternatively you can use:

    GetOptions ("define=s%" => \$defines);

When used with command line options:

    --define os=linux --define vendor=redhat

the hash C<%defines> (or C<%$defines>) will contain two keys, C<"os">
with value C<"linux> and C<"vendor"> with value C<"redhat">. It is
also possible to specify that only integer or floating point numbers
are acceptable values. The keys are always taken to be strings.

=head2 User-defined subroutines to handle options

Ultimate control over what should be done when (actually: each time)
an option is encountered on the command line can be achieved by
designating a reference to a subroutine (or an anonymous subroutine)
as the option destination. When GetOptions() encounters the option, it
will call the subroutine with two or three arguments. The first
argument is the name of the option. For a scalar or array destination,
the second argument is the value to be stored. For a hash destination,
the second arguments is the key to the hash, and the third argument
the value to be stored. It is up to the subroutine to store the value,
or do whatever it thinks is appropriate.

A trivial application of this mechanism is to implement options that
are related to each other. For example:

    my $verbose = '';	# option variable with default value (false)
    GetOptions ('verbose' => \$verbose,
	        'quiet'   => sub { $verbose = 0 });

Here C<--verbose> and C<--quiet> control the same variable
C<$verbose>, but with opposite values.

If the subroutine needs to signal an error, it should call die() with
the desired error message as its argument. GetOptions() will catch the
die(), issue the error message, and record that an error result must
be returned upon completion.

If the text of the error message starts with an exclamation mark C<!>
it is interpreted specially by GetOptions(). There is currently one
special command implemented: C<die("!FINISH")> will cause GetOptions()
to stop processing options, as if it encountered a double dash C<-->.

=head2 Options with multiple names

Often it is user friendly to supply alternate mnemonic names for
options. For example C<--height> could be an alternate name for
C<--length>. Alternate names can be included in the option
specification, separated by vertical bar C<|> characters. To implement
the above example:

    GetOptions ('length|height=f' => \$length);

The first name is called the I<primary> name, the other names are
called I<aliases>. When using a hash to store options, the key will
always be the primary name.

Multiple alternate names are possible.

=head2 Case and abbreviations

Without additional configuration, GetOptions() will ignore the case of
option names, and allow the options to be abbreviated to uniqueness.

    GetOptions ('length|height=f' => \$length, "head" => \$head);

This call will allow C<--l> and C<--L> for the length option, but
requires a least C<--hea> and C<--hei> for the head and height options.

=head2 Summary of Option Specifications

Each option specifier consists of two parts: the name specification
and the argument specification.

The name specification contains the name of the option, optionally
followed by a list of alternative names separated by vertical bar
characters.

    length	      option name is "length"
    length|size|l     name is "length", aliases are "size" and "l"

The argument specification is optional. If omitted, the option is
considered boolean, a value of 1 will be assigned when the option is
used on the command line.

The argument specification can be

=over 4

=item !

The option does not take an argument and may be negated by prefixing
it with "no" or "no-". E.g. C<"foo!"> will allow C<--foo> (a value of
1 will be assigned) as well as C<--nofoo> and C<--no-foo> (a value of
0 will be assigned). If the option has aliases, this applies to the
aliases as well.

Using negation on a single letter option when bundling is in effect is
pointless and will result in a warning.

=item +

The option does not take an argument and will be incremented by 1
every time it appears on the command line. E.g. C<"more+">, when used
with C<--more --more --more>, will increment the value three times,
resulting in a value of 3 (provided it was 0 or undefined at first).

The C<+> specifier is ignored if the option destination is not a scalar.

=item = I<type> [ I<desttype> ] [ I<repeat> ]

The option requires an argument of the given type. Supported types
are:

=over 4

=item s

String. An arbitrary sequence of characters. It is valid for the
argument to start with C<-> or C<-->.

=item i

Integer. An optional leading plus or minus sign, followed by a
sequence of digits.

=item o

Extended integer, Perl style. This can be either an optional leading
plus or minus sign, followed by a sequence of digits, or an octal
string (a zero, optionally followed by '0', '1', .. '7'), or a
hexadecimal string (C<0x> followed by '0' .. '9', 'a' .. 'f', case
insensitive), or a binary string (C<0b> followed by a series of '0'
and '1').

=item f

Real number. For example C<3.14>, C<-6.23E24> and so on.

=back

The I<desttype> can be C<@> or C<%> to specify that the option is
list or a hash valued. This is only needed when the destination for
the option value is not otherwise specified. It should be omitted when
not needed.

The I<repeat> specifies the number of values this option takes per
occurrence on the command line. It has the format C<{> [ I<min> ] [ C<,> [ I<max> ] ] C<}>.

I<min> denotes the minimal number of arguments. It defaults to 1 for
options with C<=> and to 0 for options with C<:>, see below. Note that
I<min> overrules the C<=> / C<:> semantics.

I<max> denotes the maximum number of arguments. It must be at least
I<min>. If I<max> is omitted, I<but the comma is not>, there is no
upper bound to the number of argument values taken.

=item : I<type> [ I<desttype> ]

Like C<=>, but designates the argument as optional.
If omitted, an empty string will be assigned to string values options,
and the value zero to numeric options.

Note that if a string argument starts with C<-> or C<-->, it will be
considered an option on itself.

=item : I<number> [ I<desttype> ]

Like C<:i>, but if the value is omitted, the I<number> will be assigned.

=item : + [ I<desttype> ]

Like C<:i>, but if the value is omitted, the current value for the
option will be incremented.

=back

=head1 Advanced Possibilities

=head2 Object oriented interface

Getopt::Long can be used in an object oriented way as well:

    use Getopt::Long;
    $p = new Getopt::Long::Parser;
    $p->configure(...configuration options...);
    if ($p->getoptions(...options descriptions...)) ...

Configuration options can be passed to the constructor:

    $p = new Getopt::Long::Parser
             config => [...configuration options...];

=head2 Thread Safety

Getopt::Long is thread safe when using ithreads as of Perl 5.8.  It is
I<not> thread safe when using the older (experimental and now
obsolete) threads implementation that was added to Perl 5.005.

=head2 Documentation and help texts

Getopt::Long encourages the use of Pod::Usage to produce help
messages. For example:

    use Getopt::Long;
    use Pod::Usage;

    my $man = 0;
    my $help = 0;

    GetOptions('help|?' => \$help, man => \$man) or pod2usage(2);
    pod2usage(1) if $help;
    pod2usage(-exitstatus => 0, -verbose => 2) if $man;

    __END__

    =head1 NAME

    sample - Using Getopt::Long and Pod::Usage

    =head1 SYNOPSIS

    sample [options] [file ...]

     Options:
       -help            brief help message
       -man             full documentation

    =head1 OPTIONS

    =over 8

    =item B<-help>

    Print a brief help message and exits.

    =item B<-man>

    Prints the manual page and exits.

    =back

    =head1 DESCRIPTION

    B<This program> will read the given input file(s) and do something
    useful with the contents thereof.

    =cut

See L<Pod::Usage> for details.

=head2 Parsing options from an arbitrary array

By default, GetOptions parses the options that are present in the
global array C<@ARGV>. A special entry C<GetOptionsFromArray> can be
used to parse options from an arbitrary array.

    use Getopt::Long qw(GetOptionsFromArray);
    $ret = GetOptionsFromArray(\@myopts, ...);

When used like this, the global C<@ARGV> is not touched at all.

The following two calls behave identically:

    $ret = GetOptions( ... );
    $ret = GetOptionsFromArray(\@ARGV, ... );

=head2 Parsing options from an arbitrary string

A special entry C<GetOptionsFromString> can be used to parse options
from an arbitrary string.

    use Getopt::Long qw(GetOptionsFromString);
    $ret = GetOptionsFromString($string, ...);

The contents of the string are split into arguments using a call to
C<Text::ParseWords::shellwords>. As with C<GetOptionsFromArray>, the
global C<@ARGV> is not touched.

It is possible that, upon completion, not all arguments in the string
have been processed. C<GetOptionsFromString> will, when called in list
context, return both the return status and an array reference to any
remaining arguments:

    ($ret, $args) = GetOptionsFromString($string, ... );

If any arguments remain, and C<GetOptionsFromString> was not called in
list context, a message will be given and C<GetOptionsFromString> will
return failure.

=head2 Storing options values in a hash

Sometimes, for example when there are a lot of options, having a
separate variable for each of them can be cumbersome. GetOptions()
supports, as an alternative mechanism, storing options values in a
hash.

To obtain this, a reference to a hash must be passed I<as the first
argument> to GetOptions(). For each option that is specified on the
command line, the option value will be stored in the hash with the
option name as key. Options that are not actually used on the command
line will not be put in the hash, on other words,
C<exists($h{option})> (or defined()) can be used to test if an option
was used. The drawback is that warnings will be issued if the program
runs under C<use strict> and uses C<$h{option}> without testing with
exists() or defined() first.

    my %h = ();
    GetOptions (\%h, 'length=i');	# will store in $h{length}

For options that take list or hash values, it is necessary to indicate
this by appending an C<@> or C<%> sign after the type:

    GetOptions (\%h, 'colours=s@');	# will push to @{$h{colours}}

To make things more complicated, the hash may contain references to
the actual destinations, for example:

    my $len = 0;
    my %h = ('length' => \$len);
    GetOptions (\%h, 'length=i');	# will store in $len

This example is fully equivalent with:

    my $len = 0;
    GetOptions ('length=i' => \$len);	# will store in $len

Any mixture is possible. For example, the most frequently used options
could be stored in variables while all other options get stored in the
hash:

    my $verbose = 0;			# frequently referred
    my $debug = 0;			# frequently referred
    my %h = ('verbose' => \$verbose, 'debug' => \$debug);
    GetOptions (\%h, 'verbose', 'debug', 'filter', 'size=i');
    if ( $verbose ) { ... }
    if ( exists $h{filter} ) { ... option 'filter' was specified ... }

=head2 Bundling

With bundling it is possible to set several single-character options
at once. For example if C<a>, C<v> and C<x> are all valid options,

    -vax

would set all three.

Getopt::Long supports two levels of bundling. To enable bundling, a
call to Getopt::Long::Configure is required.

The first level of bundling can be enabled with:

    Getopt::Long::Configure ("bundling");

Configured this way, single-character options can be bundled but long
options B<must> always start with a double dash C<--> to avoid
ambiguity. For example, when C<vax>, C<a>, C<v> and C<x> are all valid
options,

    -vax

would set C<a>, C<v> and C<x>, but

    --vax

would set C<vax>.

The second level of bundling lifts this restriction. It can be enabled
with:

    Getopt::Long::Configure ("bundling_override");

Now, C<-vax> would set the option C<vax>.

When any level of bundling is enabled, option values may be inserted
in the bundle. For example:

    -h24w80

is equivalent to

    -h 24 -w 80

When configured for bundling, single-character options are matched
case sensitive while long options are matched case insensitive. To
have the single-character options matched case insensitive as well,
use:

    Getopt::Long::Configure ("bundling", "ignorecase_always");

It goes without saying that bundling can be quite confusing.

=head2 The lonesome dash

Normally, a lone dash C<-> on the command line will not be considered
an option. Option processing will terminate (unless "permute" is
configured) and the dash will be left in C<@ARGV>.

It is possible to get special treatment for a lone dash. This can be
achieved by adding an option specification with an empty name, for
example:

    GetOptions ('' => \$stdio);

A lone dash on the command line will now be a legal option, and using
it will set variable C<$stdio>.

=head2 Argument callback

A special option 'name' C<< <> >> can be used to designate a subroutine
to handle non-option arguments. When GetOptions() encounters an
argument that does not look like an option, it will immediately call this
subroutine and passes it one parameter: the argument name.

For example:

    my $width = 80;
    sub process { ... }
    GetOptions ('width=i' => \$width, '<>' => \&process);

When applied to the following command line:

    arg1 --width=72 arg2 --width=60 arg3

This will call
C<process("arg1")> while C<$width> is C<80>,
C<process("arg2")> while C<$width> is C<72>, and
C<process("arg3")> while C<$width> is C<60>.

This feature requires configuration option B<permute>, see section
L<Configuring Getopt::Long>.

=head1 Configuring Getopt::Long

Getopt::Long can be configured by calling subroutine
Getopt::Long::Configure(). This subroutine takes a list of quoted
strings, each specifying a configuration option to be enabled, e.g.
C<ignore_case>, or disabled, e.g. C<no_ignore_case>. Case does not
matter. Multiple calls to Configure() are possible.

Alternatively, as of version 2.24, the configuration options may be
passed together with the C<use> statement:

    use Getopt::Long qw(:config no_ignore_case bundling);

The following options are available:

=over 12

=item default

This option causes all configuration options to be reset to their
default values.

=item posix_default

This option causes all configuration options to be reset to their
default values as if the environment variable POSIXLY_CORRECT had
been set.

=item auto_abbrev

Allow option names to be abbreviated to uniqueness.
Default is enabled unless environment variable
POSIXLY_CORRECT has been set, in which case C<auto_abbrev> is disabled.

=item getopt_compat

Allow C<+> to start options.
Default is enabled unless environment variable
POSIXLY_CORRECT has been set, in which case C<getopt_compat> is disabled.

=item gnu_compat

C<gnu_compat> controls whether C<--opt=> is allowed, and what it should
do. Without C<gnu_compat>, C<--opt=> gives an error. With C<gnu_compat>,
C<--opt=> will give option C<opt> and empty value.
This is the way GNU getopt_long() does it.

=item gnu_getopt

This is a short way of setting C<gnu_compat> C<bundling> C<permute>
C<no_getopt_compat>. With C<gnu_getopt>, command line handling should be
fully compatible with GNU getopt_long().

=item require_order

Whether command line arguments are allowed to be mixed with options.
Default is disabled unless environment variable
POSIXLY_CORRECT has been set, in which case C<require_order> is enabled.

See also C<permute>, which is the opposite of C<require_order>.

=item permute

Whether command line arguments are allowed to be mixed with options.
Default is enabled unless environment variable
POSIXLY_CORRECT has been set, in which case C<permute> is disabled.
Note that C<permute> is the opposite of C<require_order>.

If C<permute> is enabled, this means that

    --foo arg1 --bar arg2 arg3

is equivalent to

    --foo --bar arg1 arg2 arg3

If an argument callback routine is specified, C<@ARGV> will always be
empty upon successful return of GetOptions() since all options have been
processed. The only exception is when C<--> is used:

    --foo arg1 --bar arg2 -- arg3

This will call the callback routine for arg1 and arg2, and then
terminate GetOptions() leaving C<"arg3"> in C<@ARGV>.

If C<require_order> is enabled, options processing
terminates when the first non-option is encountered.

    --foo arg1 --bar arg2 arg3

is equivalent to

    --foo -- arg1 --bar arg2 arg3

If C<pass_through> is also enabled, options processing will terminate
at the first unrecognized option, or non-option, whichever comes
first.

=item bundling (default: disabled)

Enabling this option will allow single-character options to be
bundled. To distinguish bundles from long option names, long options
I<must> be introduced with C<--> and bundles with C<->.

Note that, if you have options C<a>, C<l> and C<all>, and
auto_abbrev enabled, possible arguments and option settings are:

    using argument               sets option(s)
    ------------------------------------------
    -a, --a                      a
    -l, --l                      l
    -al, -la, -ala, -all,...     a, l
    --al, --all                  all

The surprising part is that C<--a> sets option C<a> (due to auto
completion), not C<all>.

Note: disabling C<bundling> also disables C<bundling_override>.

=item bundling_override (default: disabled)

If C<bundling_override> is enabled, bundling is enabled as with
C<bundling> but now long option names override option bundles.

Note: disabling C<bundling_override> also disables C<bundling>.

B<Note:> Using option bundling can easily lead to unexpected results,
especially when mixing long options and bundles. Caveat emptor.

=item ignore_case  (default: enabled)

If enabled, case is ignored when matching long option names. If,
however, bundling is enabled as well, single character options will be
treated case-sensitive.

With C<ignore_case>, option specifications for options that only
differ in case, e.g., C<"foo"> and C<"Foo">, will be flagged as
duplicates.

Note: disabling C<ignore_case> also disables C<ignore_case_always>.

=item ignore_case_always (default: disabled)

When bundling is in effect, case is ignored on single-character
options also.

Note: disabling C<ignore_case_always> also disables C<ignore_case>.

=item auto_version (default:disabled)

Automatically provide support for the B<--version> option if
the application did not specify a handler for this option itself.

Getopt::Long will provide a standard version message that includes the
program name, its version (if $main::VERSION is defined), and the
versions of Getopt::Long and Perl. The message will be written to
standard output and processing will terminate.

C<auto_version> will be enabled if the calling program explicitly
specified a version number higher than 2.32 in the C<use> or
C<require> statement.

=item auto_help (default:disabled)

Automatically provide support for the B<--help> and B<-?> options if
the application did not specify a handler for this option itself.

Getopt::Long will provide a help message using module L<Pod::Usage>. The
message, derived from the SYNOPSIS POD section, will be written to
standard output and processing will terminate.

C<auto_help> will be enabled if the calling program explicitly
specified a version number higher than 2.32 in the C<use> or
C<require> statement.

=item pass_through (default: disabled)

Options that are unknown, ambiguous or supplied with an invalid option
value are passed through in C<@ARGV> instead of being flagged as
errors. This makes it possible to write wrapper scripts that process
only part of the user supplied command line arguments, and pass the
remaining options to some other program.

If C<require_order> is enabled, options processing will terminate at
the first unrecognized option, or non-option, whichever comes first.
However, if C<permute> is enabled instead, results can become confusing.

Note that the options terminator (default C<-->), if present, will
also be passed through in C<@ARGV>.

=item prefix

The string that starts options. If a constant string is not
sufficient, see C<prefix_pattern>.

=item prefix_pattern

A Perl pattern that identifies the strings that introduce options.
Default is C<--|-|\+> unless environment variable
POSIXLY_CORRECT has been set, in which case it is C<--|->.

=item long_prefix_pattern

A Perl pattern that allows the disambiguation of long and short
prefixes. Default is C<-->.

Typically you only need to set this if you are using nonstandard
prefixes and want some or all of them to have the same semantics as
'--' does under normal circumstances.

For example, setting prefix_pattern to C<--|-|\+|\/> and
long_prefix_pattern to C<--|\/> would add Win32 style argument
handling.

=item debug (default: disabled)

Enable debugging output.

=back

=head1 Exportable Methods

=over

=item VersionMessage

This subroutine provides a standard version message. Its argument can be:

=over 4

=item *

A string containing the text of a message to print I<before> printing
the standard message.

=item *

A numeric value corresponding to the desired exit status.

=item *

A reference to a hash.

=back

If more than one argument is given then the entire argument list is
assumed to be a hash.  If a hash is supplied (either as a reference or
as a list) it should contain one or more elements with the following
keys:

=over 4

=item C<-message>

=item C<-msg>

The text of a message to print immediately prior to printing the
program's usage message.

=item C<-exitval>

The desired exit status to pass to the B<exit()> function.
This should be an integer, or else the string "NOEXIT" to
indicate that control should simply be returned without
terminating the invoking process.

=item C<-output>

A reference to a filehandle, or the pathname of a file to which the
usage message should be written. The default is C<\*STDERR> unless the
exit value is less than 2 (in which case the default is C<\*STDOUT>).

=back

You cannot tie this routine directly to an option, e.g.:

    GetOptions("version" => \&VersionMessage);

Use this instead:

    GetOptions("version" => sub { VersionMessage() });

=item HelpMessage

This subroutine produces a standard help message, derived from the
program's POD section SYNOPSIS using L<Pod::Usage>. It takes the same
arguments as VersionMessage(). In particular, you cannot tie it
directly to an option, e.g.:

    GetOptions("help" => \&HelpMessage);

Use this instead:

    GetOptions("help" => sub { HelpMessage() });

=back

=head1 Return values and Errors

Configuration errors and errors in the option definitions are
signalled using die() and will terminate the calling program unless
the call to Getopt::Long::GetOptions() was embedded in C<eval { ...
}>, or die() was trapped using C<$SIG{__DIE__}>.

GetOptions returns true to indicate success.
It returns false when the function detected one or more errors during
option parsing. These errors are signalled using warn() and can be
trapped with C<$SIG{__WARN__}>.

=head1 Legacy

The earliest development of C<newgetopt.pl> started in 1990, with Perl
version 4. As a result, its development, and the development of
Getopt::Long, has gone through several stages. Since backward
compatibility has always been extremely important, the current version
of Getopt::Long still supports a lot of constructs that nowadays are
no longer necessary or otherwise unwanted. This section describes
briefly some of these 'features'.

=head2 Default destinations

When no destination is specified for an option, GetOptions will store
the resultant value in a global variable named C<opt_>I<XXX>, where
I<XXX> is the primary name of this option. When a progam executes
under C<use strict> (recommended), these variables must be
pre-declared with our() or C<use vars>.

    our $opt_length = 0;
    GetOptions ('length=i');	# will store in $opt_length

To yield a usable Perl variable, characters that are not part of the
syntax for variables are translated to underscores. For example,
C<--fpp-struct-return> will set the variable
C<$opt_fpp_struct_return>. Note that this variable resides in the
namespace of the calling program, not necessarily C<main>. For
example:

    GetOptions ("size=i", "sizes=i@");

with command line "-size 10 -sizes 24 -sizes 48" will perform the
equivalent of the assignments

    $opt_size = 10;
    @opt_sizes = (24, 48);

=head2 Alternative option starters

A string of alternative option starter characters may be passed as the
first argument (or the first argument after a leading hash reference
argument).

    my $len = 0;
    GetOptions ('/', 'length=i' => $len);

Now the command line may look like:

    /length 24 -- arg

Note that to terminate options processing still requires a double dash
C<-->.

GetOptions() will not interpret a leading C<< "<>" >> as option starters
if the next argument is a reference. To force C<< "<" >> and C<< ">" >> as
option starters, use C<< "><" >>. Confusing? Well, B<using a starter
argument is strongly deprecated> anyway.

=head2 Configuration variables

Previous versions of Getopt::Long used variables for the purpose of
configuring. Although manipulating these variables still work, it is
strongly encouraged to use the C<Configure> routine that was introduced
in version 2.17. Besides, it is much easier.

=head1 Tips and Techniques

=head2 Pushing multiple values in a hash option

Sometimes you want to combine the best of hashes and arrays. For
example, the command line:

  --list add=first --list add=second --list add=third

where each successive 'list add' option will push the value of add
into array ref $list->{'add'}. The result would be like

  $list->{add} = [qw(first second third)];

This can be accomplished with a destination routine:

  GetOptions('list=s%' =>
               sub { push(@{$list{$_[1]}}, $_[2]) });

=head1 Trouble Shooting

=head2 GetOptions does not return a false result when an option is not supplied

That's why they're called 'options'.

=head2 GetOptions does not split the command line correctly

The command line is not split by GetOptions, but by the command line
interpreter (CLI). On Unix, this is the shell. On Windows, it is
COMMAND.COM or CMD.EXE. Other operating systems have other CLIs.

It is important to know that these CLIs may behave different when the
command line contains special characters, in particular quotes or
backslashes. For example, with Unix shells you can use single quotes
(C<'>) and double quotes (C<">) to group words together. The following
alternatives are equivalent on Unix:

    "two words"
    'two words'
    two\ words

In case of doubt, insert the following statement in front of your Perl
program:

    print STDERR (join("|",@ARGV),"\n");

to verify how your CLI passes the arguments to the program.

=head2 Undefined subroutine &main::GetOptions called

Are you running Windows, and did you write

    use GetOpt::Long;

(note the capital 'O')?

=head2 How do I put a "-?" option into a Getopt::Long?

You can only obtain this using an alias, and Getopt::Long of at least
version 2.13.

    use Getopt::Long;
    GetOptions ("help|?");    # -help and -? will both set $opt_help

=head1 AUTHOR

Johan Vromans <jvromans@squirrel.nl>

=head1 COPYRIGHT AND DISCLAIMER

This program is Copyright 1990,2007 by Johan Vromans.
This program is free software; you can redistribute it and/or
modify it under the terms of the Perl Artistic License or the
GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

If you do not have a copy of the GNU General Public License write to
the Free Software Foundation, Inc., 675 Mass Ave, Cambridge,
MA 02139, USA.

=cut

