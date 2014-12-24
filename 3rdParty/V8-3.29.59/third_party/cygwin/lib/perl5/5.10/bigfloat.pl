package bigfloat;
require "bigint.pl";
#
# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
# Suggested alternative: Math::BigFloat
#
# Arbitrary length float math package
#
# by Mark Biggar
#
# number format
#   canonical strings have the form /[+-]\d+E[+-]\d+/
#   Input values can have embedded whitespace
# Error returns
#   'NaN'           An input parameter was "Not a Number" or 
#                       divide by zero or sqrt of negative number
# Division is computed to 
#   max($div_scale,length(dividend)+length(divisor)) 
#   digits by default.
# Also used for default sqrt scale

$div_scale = 40;

# Rounding modes one of 'even', 'odd', '+inf', '-inf', 'zero' or 'trunc'.

$rnd_mode = 'even';

#   bigfloat routines
#
#   fadd(NSTR, NSTR) return NSTR            addition
#   fsub(NSTR, NSTR) return NSTR            subtraction
#   fmul(NSTR, NSTR) return NSTR            multiplication
#   fdiv(NSTR, NSTR[,SCALE]) returns NSTR   division to SCALE places
#   fneg(NSTR) return NSTR                  negation
#   fabs(NSTR) return NSTR                  absolute value
#   fcmp(NSTR,NSTR) return CODE             compare undef,<0,=0,>0
#   fround(NSTR, SCALE) return NSTR         round to SCALE digits
#   ffround(NSTR, SCALE) return NSTR        round at SCALEth place
#   fnorm(NSTR) return (NSTR)               normalize
#   fsqrt(NSTR[, SCALE]) return NSTR        sqrt to SCALE places

# Convert a number to canonical string form.
#   Takes something that looks like a number and converts it to
#   the form /^[+-]\d+E[+-]\d+$/.
sub main'fnorm { #(string) return fnum_str
    local($_) = @_;
    s/\s+//g;                               # strip white space
    if (/^([+-]?)(\d*)(\.(\d*))?([Ee]([+-]?\d+))?$/
	  && ($2 ne '' || defined($4))) {
	my $x = defined($4) ? $4 : '';
	&norm(($1 ? "$1$2$x" : "+$2$x"), (($x ne '') ? $6-length($x) : $6));
    } else {
	'NaN';
    }
}

# normalize number -- for internal use
sub norm { #(mantissa, exponent) return fnum_str
    local($_, $exp) = @_;
    if ($_ eq 'NaN') {
	'NaN';
    } else {
	s/^([+-])0+/$1/;                        # strip leading zeros
	if (length($_) == 1) {
	    '+0E+0';
	} else {
	    $exp += length($1) if (s/(0+)$//);  # strip trailing zeros
	    sprintf("%sE%+ld", $_, $exp);
	}
    }
}

# negation
sub main'fneg { #(fnum_str) return fnum_str
    local($_) = &'fnorm($_[$[]);
    vec($_,0,8) ^= ord('+') ^ ord('-') unless $_ eq '+0E+0'; # flip sign
    if ( ord("\t") == 9 ) { # ascii
        s/^H/N/;
    }
    else { # ebcdic character set
        s/\373/N/;
    }
    $_;
}

# absolute value
sub main'fabs { #(fnum_str) return fnum_str
    local($_) = &'fnorm($_[$[]);
    s/^-/+/;		                       # mash sign
    $_;
}

# multiplication
sub main'fmul { #(fnum_str, fnum_str) return fnum_str
    local($x,$y) = (&'fnorm($_[$[]),&'fnorm($_[$[+1]));
    if ($x eq 'NaN' || $y eq 'NaN') {
	'NaN';
    } else {
	local($xm,$xe) = split('E',$x);
	local($ym,$ye) = split('E',$y);
	&norm(&'bmul($xm,$ym),$xe+$ye);
    }
}

# addition
sub main'fadd { #(fnum_str, fnum_str) return fnum_str
    local($x,$y) = (&'fnorm($_[$[]),&'fnorm($_[$[+1]));
    if ($x eq 'NaN' || $y eq 'NaN') {
	'NaN';
    } else {
	local($xm,$xe) = split('E',$x);
	local($ym,$ye) = split('E',$y);
	($xm,$xe,$ym,$ye) = ($ym,$ye,$xm,$xe) if ($xe < $ye);
	&norm(&'badd($ym,$xm.('0' x ($xe-$ye))),$ye);
    }
}

# subtraction
sub main'fsub { #(fnum_str, fnum_str) return fnum_str
    &'fadd($_[$[],&'fneg($_[$[+1]));    
}

# division
#   args are dividend, divisor, scale (optional)
#   result has at most max(scale, length(dividend), length(divisor)) digits
sub main'fdiv #(fnum_str, fnum_str[,scale]) return fnum_str
{
    local($x,$y,$scale) = (&'fnorm($_[$[]),&'fnorm($_[$[+1]),$_[$[+2]);
    if ($x eq 'NaN' || $y eq 'NaN' || $y eq '+0E+0') {
	'NaN';
    } else {
	local($xm,$xe) = split('E',$x);
	local($ym,$ye) = split('E',$y);
	$scale = $div_scale if (!$scale);
	$scale = length($xm)-1 if (length($xm)-1 > $scale);
	$scale = length($ym)-1 if (length($ym)-1 > $scale);
	$scale = $scale + length($ym) - length($xm);
	&norm(&round(&'bdiv($xm.('0' x $scale),$ym),&'babs($ym)),
	    $xe-$ye-$scale);
    }
}

# round int $q based on fraction $r/$base using $rnd_mode
sub round { #(int_str, int_str, int_str) return int_str
    local($q,$r,$base) = @_;
    if ($q eq 'NaN' || $r eq 'NaN') {
	'NaN';
    } elsif ($rnd_mode eq 'trunc') {
	$q;                         # just truncate
    } else {
	local($cmp) = &'bcmp(&'bmul($r,'+2'),$base);
	if ( $cmp < 0 ||
		 ($cmp == 0 &&
		  ( $rnd_mode eq 'zero'                             ||
		   ($rnd_mode eq '-inf' && (substr($q,$[,1) eq '+')) ||
		   ($rnd_mode eq '+inf' && (substr($q,$[,1) eq '-')) ||
		   ($rnd_mode eq 'even' && $q =~ /[24680]$/)        ||
		   ($rnd_mode eq 'odd'  && $q =~ /[13579]$/)        )) ) {
	    $q;                     # round down
	} else {
	    &'badd($q, ((substr($q,$[,1) eq '-') ? '-1' : '+1'));
				    # round up
	}
    }
}

# round the mantissa of $x to $scale digits
sub main'fround { #(fnum_str, scale) return fnum_str
    local($x,$scale) = (&'fnorm($_[$[]),$_[$[+1]);
    if ($x eq 'NaN' || $scale <= 0) {
	$x;
    } else {
	local($xm,$xe) = split('E',$x);
	if (length($xm)-1 <= $scale) {
	    $x;
	} else {
	    &norm(&round(substr($xm,$[,$scale+1),
			 "+0".substr($xm,$[+$scale+1,1),"+10"),
		  $xe+length($xm)-$scale-1);
	}
    }
}

# round $x at the 10 to the $scale digit place
sub main'ffround { #(fnum_str, scale) return fnum_str
    local($x,$scale) = (&'fnorm($_[$[]),$_[$[+1]);
    if ($x eq 'NaN') {
	'NaN';
    } else {
	local($xm,$xe) = split('E',$x);
	if ($xe >= $scale) {
	    $x;
	} else {
	    $xe = length($xm)+$xe-$scale;
	    if ($xe < 1) {
		'+0E+0';
	    } elsif ($xe == 1) {
		# The first substr preserves the sign, which means that
		# we'll pass a non-normalized "-0" to &round when rounding
		# -0.006 (for example), purely so that &round won't lose
		# the sign.
		&norm(&round(substr($xm,$[,1).'0',
		      "+0".substr($xm,$[+1,1),"+10"), $scale);
	    } else {
		&norm(&round(substr($xm,$[,$xe),
		      "+0".substr($xm,$[+$xe,1),"+10"), $scale);
	    }
	}
    }
}
    
# compare 2 values returns one of undef, <0, =0, >0
#   returns undef if either or both input value are not numbers
sub main'fcmp #(fnum_str, fnum_str) return cond_code
{
    local($x, $y) = (&'fnorm($_[$[]),&'fnorm($_[$[+1]));
    if ($x eq "NaN" || $y eq "NaN") {
	undef;
    } else {
	ord($y) <=> ord($x)
	||
	(  local($xm,$xe,$ym,$ye) = split('E', $x."E$y"),
	     (($xe <=> $ye) * (substr($x,$[,1).'1')
             || &bigint'cmp($xm,$ym))
	);
    }
}

# square root by Newtons method.
sub main'fsqrt { #(fnum_str[, scale]) return fnum_str
    local($x, $scale) = (&'fnorm($_[$[]), $_[$[+1]);
    if ($x eq 'NaN' || $x =~ /^-/) {
	'NaN';
    } elsif ($x eq '+0E+0') {
	'+0E+0';
    } else {
	local($xm, $xe) = split('E',$x);
	$scale = $div_scale if (!$scale);
	$scale = length($xm)-1 if ($scale < length($xm)-1);
	local($gs, $guess) = (1, sprintf("1E%+d", (length($xm)+$xe-1)/2));
	while ($gs < 2*$scale) {
	    $guess = &'fmul(&'fadd($guess,&'fdiv($x,$guess,$gs*2)),".5");
	    $gs *= 2;
	}
	&'fround($guess, $scale);
    }
}

1;
