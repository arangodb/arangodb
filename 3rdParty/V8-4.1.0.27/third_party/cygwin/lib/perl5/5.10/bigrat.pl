package bigrat;
require "bigint.pl";
#
# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
# Arbitrary size rational math package
#
# by Mark Biggar
#
# Input values to these routines consist of strings of the form 
#   m|^\s*[+-]?[\d\s]+(/[\d\s]+)?$|.
# Examples:
#   "+0/1"                          canonical zero value
#   "3"                             canonical value "+3/1"
#   "   -123/123 123"               canonical value "-1/1001"
#   "123 456/7890"                  canonical value "+20576/1315"
# Output values always include a sign and no leading zeros or
#   white space.
# This package makes use of the bigint package.
# The string 'NaN' is used to represent the result when input arguments 
#   that are not numbers, as well as the result of dividing by zero and
#       the sqrt of a negative number.
# Extreamly naive algorthims are used.
#
# Routines provided are:
#
#   rneg(RAT) return RAT                negation
#   rabs(RAT) return RAT                absolute value
#   rcmp(RAT,RAT) return CODE           compare numbers (undef,<0,=0,>0)
#   radd(RAT,RAT) return RAT            addition
#   rsub(RAT,RAT) return RAT            subtraction
#   rmul(RAT,RAT) return RAT            multiplication
#   rdiv(RAT,RAT) return RAT            division
#   rmod(RAT) return (RAT,RAT)          integer and fractional parts
#   rnorm(RAT) return RAT               normalization
#   rsqrt(RAT, cycles) return RAT       square root

# Convert a number to the canonical string form m|^[+-]\d+/\d+|.
sub main'rnorm { #(string) return rat_num
    local($_) = @_;
    s/\s+//g;
    if (m#^([+-]?\d+)(/(\d*[1-9]0*))?$#) {
	&norm($1, $3 ? $3 : '+1');
    } else {
	'NaN';
    }
}

# Normalize by reducing to lowest terms
sub norm { #(bint, bint) return rat_num
    local($num,$dom) = @_;
    if ($num eq 'NaN') {
	'NaN';
    } elsif ($dom eq 'NaN') {
	'NaN';
    } elsif ($dom =~ /^[+-]?0+$/) {
	'NaN';
    } else {
	local($gcd) = &'bgcd($num,$dom);
	$gcd =~ s/^-/+/;
	if ($gcd ne '+1') { 
	    $num = &'bdiv($num,$gcd);
	    $dom = &'bdiv($dom,$gcd);
	} else {
	    $num = &'bnorm($num);
	    $dom = &'bnorm($dom);
	}
	substr($dom,$[,1) = '';
	"$num/$dom";
    }
}

# negation
sub main'rneg { #(rat_num) return rat_num
    local($_) = &'rnorm(@_);
    tr/-+/+-/ if ($_ ne '+0/1');
    $_;
}

# absolute value
sub main'rabs { #(rat_num) return $rat_num
    local($_) = &'rnorm(@_);
    substr($_,$[,1) = '+' unless $_ eq 'NaN';
    $_;
}

# multipication
sub main'rmul { #(rat_num, rat_num) return rat_num
    local($xn,$xd) = split('/',&'rnorm($_[$[]));
    local($yn,$yd) = split('/',&'rnorm($_[$[+1]));
    &norm(&'bmul($xn,$yn),&'bmul($xd,$yd));
}

# division
sub main'rdiv { #(rat_num, rat_num) return rat_num
    local($xn,$xd) = split('/',&'rnorm($_[$[]));
    local($yn,$yd) = split('/',&'rnorm($_[$[+1]));
    &norm(&'bmul($xn,$yd),&'bmul($xd,$yn));
}

# addition
sub main'radd { #(rat_num, rat_num) return rat_num
    local($xn,$xd) = split('/',&'rnorm($_[$[]));
    local($yn,$yd) = split('/',&'rnorm($_[$[+1]));
    &norm(&'badd(&'bmul($xn,$yd),&'bmul($yn,$xd)),&'bmul($xd,$yd));
}

# subtraction
sub main'rsub { #(rat_num, rat_num) return rat_num
    local($xn,$xd) = split('/',&'rnorm($_[$[]));
    local($yn,$yd) = split('/',&'rnorm($_[$[+1]));
    &norm(&'bsub(&'bmul($xn,$yd),&'bmul($yn,$xd)),&'bmul($xd,$yd));
}

# comparison
sub main'rcmp { #(rat_num, rat_num) return cond_code
    local($xn,$xd) = split('/',&'rnorm($_[$[]));
    local($yn,$yd) = split('/',&'rnorm($_[$[+1]));
    &bigint'cmp(&'bmul($xn,$yd),&'bmul($yn,$xd));
}

# int and frac parts
sub main'rmod { #(rat_num) return (rat_num,rat_num)
    local($xn,$xd) = split('/',&'rnorm(@_));
    local($i,$f) = &'bdiv($xn,$xd);
    if (wantarray) {
	("$i/1", "$f/$xd");
    } else {
	"$i/1";
    }   
}

# square root by Newtons method.
#   cycles specifies the number of iterations default: 5
sub main'rsqrt { #(fnum_str[, cycles]) return fnum_str
    local($x, $scale) = (&'rnorm($_[$[]), $_[$[+1]);
    if ($x eq 'NaN') {
	'NaN';
    } elsif ($x =~ /^-/) {
	'NaN';
    } else {
	local($gscale, $guess) = (0, '+1/1');
	$scale = 5 if (!$scale);
	while ($gscale++ < $scale) {
	    $guess = &'rmul(&'radd($guess,&'rdiv($x,$guess)),"+1/2");
	}
	"$guess";          # quotes necessary due to perl bug
    }
}

1;
