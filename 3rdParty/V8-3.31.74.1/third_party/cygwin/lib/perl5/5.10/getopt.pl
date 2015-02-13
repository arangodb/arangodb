;# $RCSfile: getopt.pl,v $$Revision: 4.1 $$Date: 92/08/07 18:23:58 $
#
# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
# Suggested alternatives: Getopt::Long or Getopt::Std
#
;# Process single-character switches with switch clustering.  Pass one argument
;# which is a string containing all switches that take an argument.  For each
;# switch found, sets $opt_x (where x is the switch name) to the value of the
;# argument, or 1 if no argument.  Switches which take an argument don't care
;# whether there is a space between the switch and the argument.

;# Usage:
;#	do Getopt('oDI');  # -o, -D & -I take arg.  Sets opt_* as a side effect.

sub Getopt {
    local($argumentative) = @_;
    local($_,$first,$rest);
    local($[) = 0;

    while (@ARGV && ($_ = $ARGV[0]) =~ /^-(.)(.*)/) {
	($first,$rest) = ($1,$2);
	if (index($argumentative,$first) >= $[) {
	    if ($rest ne '') {
		shift(@ARGV);
	    }
	    else {
		shift(@ARGV);
		$rest = shift(@ARGV);
	    }
	    ${"opt_$first"} = $rest;
	}
	else {
	    ${"opt_$first"} = 1;
	    if ($rest ne '') {
		$ARGV[0] = "-$rest";
	    }
	    else {
		shift(@ARGV);
	    }
	}
    }
}

1;
