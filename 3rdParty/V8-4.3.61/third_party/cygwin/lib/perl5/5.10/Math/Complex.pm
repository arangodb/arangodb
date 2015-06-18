#
# Complex numbers and associated mathematical functions
# -- Raphael Manfredi	Since Sep 1996
# -- Jarkko Hietaniemi	Since Mar 1997
# -- Daniel S. Lewart	Since Sep 1997
#

package Math::Complex;

use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $Inf $ExpInf);

$VERSION = 1.54;

use Config;

BEGIN {
    my %DBL_MAX =
	(
	  4  => '1.70141183460469229e+38',
	  8  => '1.7976931348623157e+308',
	 # AFAICT the 10, 12, and 16-byte long doubles
	 # all have the same maximum.
	 10 => '1.1897314953572317650857593266280070162E+4932',
	 12 => '1.1897314953572317650857593266280070162E+4932',
	 16 => '1.1897314953572317650857593266280070162E+4932',
	);
    my $nvsize = $Config{nvsize} ||
	        ($Config{uselongdouble} && $Config{longdblsize}) ||
                 $Config{doublesize};
    die "Math::Complex: Could not figure out nvsize\n"
	unless defined $nvsize;
    die "Math::Complex: Cannot not figure out max nv (nvsize = $nvsize)\n"
	unless defined $DBL_MAX{$nvsize};
    my $DBL_MAX = eval $DBL_MAX{$nvsize};
    die "Math::Complex: Could not figure out max nv (nvsize = $nvsize)\n"
	unless defined $DBL_MAX;
    my $BIGGER_THAN_THIS = 1e30;  # Must find something bigger than this.
    if ($^O eq 'unicosmk') {
	$Inf = $DBL_MAX;
    } else {
	local $SIG{FPE} = { };
        local $!;
	# We do want an arithmetic overflow, Inf INF inf Infinity.
	for my $t (
	    'exp(99999)',  # Enough even with 128-bit long doubles.
	    'inf',
	    'Inf',
	    'INF',
	    'infinity',
	    'Infinity',
	    'INFINITY',
	    '1e99999',
	    ) {
	    local $^W = 0;
	    my $i = eval "$t+1.0";
	    if (defined $i && $i > $BIGGER_THAN_THIS) {
		$Inf = $i;
		last;
	    }
	}
	$Inf = $DBL_MAX unless defined $Inf;  # Oh well, close enough.
	die "Math::Complex: Could not get Infinity"
	    unless $Inf > $BIGGER_THAN_THIS;
	$ExpInf = exp(99999);
    }
    # print "# On this machine, Inf = '$Inf'\n";
}

use strict;

my $i;
my %LOGN;

# Regular expression for floating point numbers.
# These days we could use Scalar::Util::lln(), I guess.
my $gre = qr'\s*([\+\-]?(?:(?:(?:\d+(?:_\d+)*(?:\.\d*(?:_\d+)*)?|\.\d+(?:_\d+)*)(?:[eE][\+\-]?\d+(?:_\d+)*)?))|inf)'i;

require Exporter;

@ISA = qw(Exporter);

my @trig = qw(
	      pi
	      tan
	      csc cosec sec cot cotan
	      asin acos atan
	      acsc acosec asec acot acotan
	      sinh cosh tanh
	      csch cosech sech coth cotanh
	      asinh acosh atanh
	      acsch acosech asech acoth acotanh
	     );

@EXPORT = (qw(
	     i Re Im rho theta arg
	     sqrt log ln
	     log10 logn cbrt root
	     cplx cplxe
	     atan2
	     ),
	   @trig);

my @pi = qw(pi pi2 pi4 pip2 pip4 Inf);

@EXPORT_OK = @pi;

%EXPORT_TAGS = (
    'trig' => [@trig],
    'pi' => [@pi],
);

use overload
	'+'	=> \&_plus,
	'-'	=> \&_minus,
	'*'	=> \&_multiply,
	'/'	=> \&_divide,
	'**'	=> \&_power,
	'=='	=> \&_numeq,
	'<=>'	=> \&_spaceship,
	'neg'	=> \&_negate,
	'~'	=> \&_conjugate,
	'abs'	=> \&abs,
	'sqrt'	=> \&sqrt,
	'exp'	=> \&exp,
	'log'	=> \&log,
	'sin'	=> \&sin,
	'cos'	=> \&cos,
	'tan'	=> \&tan,
	'atan2'	=> \&atan2,
        '""'    => \&_stringify;

#
# Package "privates"
#

my %DISPLAY_FORMAT = ('style' => 'cartesian',
		      'polar_pretty_print' => 1);
my $eps            = 1e-14;		# Epsilon

#
# Object attributes (internal):
#	cartesian	[real, imaginary] -- cartesian form
#	polar		[rho, theta] -- polar form
#	c_dirty		cartesian form not up-to-date
#	p_dirty		polar form not up-to-date
#	display		display format (package's global when not set)
#

# Die on bad *make() arguments.

sub _cannot_make {
    die "@{[(caller(1))[3]]}: Cannot take $_[0] of '$_[1]'.\n";
}

sub _make {
    my $arg = shift;
    my ($p, $q);

    if ($arg =~ /^$gre$/) {
	($p, $q) = ($1, 0);
    } elsif ($arg =~ /^(?:$gre)?$gre\s*i\s*$/) {
	($p, $q) = ($1 || 0, $2);
    } elsif ($arg =~ /^\s*\(\s*$gre\s*(?:,\s*$gre\s*)?\)\s*$/) {
	($p, $q) = ($1, $2 || 0);
    }

    if (defined $p) {
	$p =~ s/^\+//;
	$p =~ s/^(-?)inf$/"${1}9**9**9"/e;
	$q =~ s/^\+//;
	$q =~ s/^(-?)inf$/"${1}9**9**9"/e;
    }

    return ($p, $q);
}

sub _emake {
    my $arg = shift;
    my ($p, $q);

    if ($arg =~ /^\s*\[\s*$gre\s*(?:,\s*$gre\s*)?\]\s*$/) {
	($p, $q) = ($1, $2 || 0);
    } elsif ($arg =~ m!^\s*\[\s*$gre\s*(?:,\s*([-+]?\d*\s*)?pi(?:/\s*(\d+))?\s*)?\]\s*$!) {
	($p, $q) = ($1, ($2 eq '-' ? -1 : ($2 || 1)) * pi() / ($3 || 1));
    } elsif ($arg =~ /^\s*\[\s*$gre\s*\]\s*$/) {
	($p, $q) = ($1, 0);
    } elsif ($arg =~ /^\s*$gre\s*$/) {
	($p, $q) = ($1, 0);
    }

    if (defined $p) {
	$p =~ s/^\+//;
	$q =~ s/^\+//;
	$p =~ s/^(-?)inf$/"${1}9**9**9"/e;
	$q =~ s/^(-?)inf$/"${1}9**9**9"/e;
    }

    return ($p, $q);
}

#
# ->make
#
# Create a new complex number (cartesian form)
#
sub make {
    my $self = bless {}, shift;
    my ($re, $im);
    if (@_ == 0) {
	($re, $im) = (0, 0);
    } elsif (@_ == 1) {
	return (ref $self)->emake($_[0])
	    if ($_[0] =~ /^\s*\[/);
	($re, $im) = _make($_[0]);
    } elsif (@_ == 2) {
	($re, $im) = @_;
    }
    if (defined $re) {
	_cannot_make("real part",      $re) unless $re =~ /^$gre$/;
    }
    $im ||= 0;
    _cannot_make("imaginary part", $im) unless $im =~ /^$gre$/;
    $self->_set_cartesian([$re, $im ]);
    $self->display_format('cartesian');

    return $self;
}

#
# ->emake
#
# Create a new complex number (exponential form)
#
sub emake {
    my $self = bless {}, shift;
    my ($rho, $theta);
    if (@_ == 0) {
	($rho, $theta) = (0, 0);
    } elsif (@_ == 1) {
	return (ref $self)->make($_[0])
	    if ($_[0] =~ /^\s*\(/ || $_[0] =~ /i\s*$/);
	($rho, $theta) = _emake($_[0]);
    } elsif (@_ == 2) {
	($rho, $theta) = @_;
    }
    if (defined $rho && defined $theta) {
	if ($rho < 0) {
	    $rho   = -$rho;
	    $theta = ($theta <= 0) ? $theta + pi() : $theta - pi();
	}
    }
    if (defined $rho) {
	_cannot_make("rho",   $rho)   unless $rho   =~ /^$gre$/;
    }
    $theta ||= 0;
    _cannot_make("theta", $theta) unless $theta =~ /^$gre$/;
    $self->_set_polar([$rho, $theta]);
    $self->display_format('polar');

    return $self;
}

sub new { &make }		# For backward compatibility only.

#
# cplx
#
# Creates a complex number from a (re, im) tuple.
# This avoids the burden of writing Math::Complex->make(re, im).
#
sub cplx {
	return __PACKAGE__->make(@_);
}

#
# cplxe
#
# Creates a complex number from a (rho, theta) tuple.
# This avoids the burden of writing Math::Complex->emake(rho, theta).
#
sub cplxe {
	return __PACKAGE__->emake(@_);
}

#
# pi
#
# The number defined as pi = 180 degrees
#
sub pi () { 4 * CORE::atan2(1, 1) }

#
# pi2
#
# The full circle
#
sub pi2 () { 2 * pi }

#
# pi4
#
# The full circle twice.
#
sub pi4 () { 4 * pi }

#
# pip2
#
# The quarter circle
#
sub pip2 () { pi / 2 }

#
# pip4
#
# The eighth circle.
#
sub pip4 () { pi / 4 }

#
# _uplog10
#
# Used in log10().
#
sub _uplog10 () { 1 / CORE::log(10) }

#
# i
#
# The number defined as i*i = -1;
#
sub i () {
        return $i if ($i);
	$i = bless {};
	$i->{'cartesian'} = [0, 1];
	$i->{'polar'}     = [1, pip2];
	$i->{c_dirty} = 0;
	$i->{p_dirty} = 0;
	return $i;
}

#
# _ip2
#
# Half of i.
#
sub _ip2 () { i / 2 }

#
# Attribute access/set routines
#

sub _cartesian {$_[0]->{c_dirty} ?
		   $_[0]->_update_cartesian : $_[0]->{'cartesian'}}
sub _polar     {$_[0]->{p_dirty} ?
		   $_[0]->_update_polar : $_[0]->{'polar'}}

sub _set_cartesian { $_[0]->{p_dirty}++; $_[0]->{c_dirty} = 0;
		     $_[0]->{'cartesian'} = $_[1] }
sub _set_polar     { $_[0]->{c_dirty}++; $_[0]->{p_dirty} = 0;
		     $_[0]->{'polar'} = $_[1] }

#
# ->_update_cartesian
#
# Recompute and return the cartesian form, given accurate polar form.
#
sub _update_cartesian {
	my $self = shift;
	my ($r, $t) = @{$self->{'polar'}};
	$self->{c_dirty} = 0;
	return $self->{'cartesian'} = [$r * CORE::cos($t), $r * CORE::sin($t)];
}

#
#
# ->_update_polar
#
# Recompute and return the polar form, given accurate cartesian form.
#
sub _update_polar {
	my $self = shift;
	my ($x, $y) = @{$self->{'cartesian'}};
	$self->{p_dirty} = 0;
	return $self->{'polar'} = [0, 0] if $x == 0 && $y == 0;
	return $self->{'polar'} = [CORE::sqrt($x*$x + $y*$y),
				   CORE::atan2($y, $x)];
}

#
# (_plus)
#
# Computes z1+z2.
#
sub _plus {
	my ($z1, $z2, $regular) = @_;
	my ($re1, $im1) = @{$z1->_cartesian};
	$z2 = cplx($z2) unless ref $z2;
	my ($re2, $im2) = ref $z2 ? @{$z2->_cartesian} : ($z2, 0);
	unless (defined $regular) {
		$z1->_set_cartesian([$re1 + $re2, $im1 + $im2]);
		return $z1;
	}
	return (ref $z1)->make($re1 + $re2, $im1 + $im2);
}

#
# (_minus)
#
# Computes z1-z2.
#
sub _minus {
	my ($z1, $z2, $inverted) = @_;
	my ($re1, $im1) = @{$z1->_cartesian};
	$z2 = cplx($z2) unless ref $z2;
	my ($re2, $im2) = @{$z2->_cartesian};
	unless (defined $inverted) {
		$z1->_set_cartesian([$re1 - $re2, $im1 - $im2]);
		return $z1;
	}
	return $inverted ?
		(ref $z1)->make($re2 - $re1, $im2 - $im1) :
		(ref $z1)->make($re1 - $re2, $im1 - $im2);

}

#
# (_multiply)
#
# Computes z1*z2.
#
sub _multiply {
        my ($z1, $z2, $regular) = @_;
	if ($z1->{p_dirty} == 0 and ref $z2 and $z2->{p_dirty} == 0) {
	    # if both polar better use polar to avoid rounding errors
	    my ($r1, $t1) = @{$z1->_polar};
	    my ($r2, $t2) = @{$z2->_polar};
	    my $t = $t1 + $t2;
	    if    ($t >   pi()) { $t -= pi2 }
	    elsif ($t <= -pi()) { $t += pi2 }
	    unless (defined $regular) {
		$z1->_set_polar([$r1 * $r2, $t]);
		return $z1;
	    }
	    return (ref $z1)->emake($r1 * $r2, $t);
	} else {
	    my ($x1, $y1) = @{$z1->_cartesian};
	    if (ref $z2) {
		my ($x2, $y2) = @{$z2->_cartesian};
		return (ref $z1)->make($x1*$x2-$y1*$y2, $x1*$y2+$y1*$x2);
	    } else {
		return (ref $z1)->make($x1*$z2, $y1*$z2);
	    }
	}
}

#
# _divbyzero
#
# Die on division by zero.
#
sub _divbyzero {
    my $mess = "$_[0]: Division by zero.\n";

    if (defined $_[1]) {
	$mess .= "(Because in the definition of $_[0], the divisor ";
	$mess .= "$_[1] " unless ("$_[1]" eq '0');
	$mess .= "is 0)\n";
    }

    my @up = caller(1);

    $mess .= "Died at $up[1] line $up[2].\n";

    die $mess;
}

#
# (_divide)
#
# Computes z1/z2.
#
sub _divide {
	my ($z1, $z2, $inverted) = @_;
	if ($z1->{p_dirty} == 0 and ref $z2 and $z2->{p_dirty} == 0) {
	    # if both polar better use polar to avoid rounding errors
	    my ($r1, $t1) = @{$z1->_polar};
	    my ($r2, $t2) = @{$z2->_polar};
	    my $t;
	    if ($inverted) {
		_divbyzero "$z2/0" if ($r1 == 0);
		$t = $t2 - $t1;
		if    ($t >   pi()) { $t -= pi2 }
		elsif ($t <= -pi()) { $t += pi2 }
		return (ref $z1)->emake($r2 / $r1, $t);
	    } else {
		_divbyzero "$z1/0" if ($r2 == 0);
		$t = $t1 - $t2;
		if    ($t >   pi()) { $t -= pi2 }
		elsif ($t <= -pi()) { $t += pi2 }
		return (ref $z1)->emake($r1 / $r2, $t);
	    }
	} else {
	    my ($d, $x2, $y2);
	    if ($inverted) {
		($x2, $y2) = @{$z1->_cartesian};
		$d = $x2*$x2 + $y2*$y2;
		_divbyzero "$z2/0" if $d == 0;
		return (ref $z1)->make(($x2*$z2)/$d, -($y2*$z2)/$d);
	    } else {
		my ($x1, $y1) = @{$z1->_cartesian};
		if (ref $z2) {
		    ($x2, $y2) = @{$z2->_cartesian};
		    $d = $x2*$x2 + $y2*$y2;
		    _divbyzero "$z1/0" if $d == 0;
		    my $u = ($x1*$x2 + $y1*$y2)/$d;
		    my $v = ($y1*$x2 - $x1*$y2)/$d;
		    return (ref $z1)->make($u, $v);
		} else {
		    _divbyzero "$z1/0" if $z2 == 0;
		    return (ref $z1)->make($x1/$z2, $y1/$z2);
		}
	    }
	}
}

#
# (_power)
#
# Computes z1**z2 = exp(z2 * log z1)).
#
sub _power {
	my ($z1, $z2, $inverted) = @_;
	if ($inverted) {
	    return 1 if $z1 == 0 || $z2 == 1;
	    return 0 if $z2 == 0 && Re($z1) > 0;
	} else {
	    return 1 if $z2 == 0 || $z1 == 1;
	    return 0 if $z1 == 0 && Re($z2) > 0;
	}
	my $w = $inverted ? &exp($z1 * &log($z2))
	                  : &exp($z2 * &log($z1));
	# If both arguments cartesian, return cartesian, else polar.
	return $z1->{c_dirty} == 0 &&
	       (not ref $z2 or $z2->{c_dirty} == 0) ?
	       cplx(@{$w->_cartesian}) : $w;
}

#
# (_spaceship)
#
# Computes z1 <=> z2.
# Sorts on the real part first, then on the imaginary part. Thus 2-4i < 3+8i.
#
sub _spaceship {
	my ($z1, $z2, $inverted) = @_;
	my ($re1, $im1) = ref $z1 ? @{$z1->_cartesian} : ($z1, 0);
	my ($re2, $im2) = ref $z2 ? @{$z2->_cartesian} : ($z2, 0);
	my $sgn = $inverted ? -1 : 1;
	return $sgn * ($re1 <=> $re2) if $re1 != $re2;
	return $sgn * ($im1 <=> $im2);
}

#
# (_numeq)
#
# Computes z1 == z2.
#
# (Required in addition to _spaceship() because of NaNs.)
sub _numeq {
	my ($z1, $z2, $inverted) = @_;
	my ($re1, $im1) = ref $z1 ? @{$z1->_cartesian} : ($z1, 0);
	my ($re2, $im2) = ref $z2 ? @{$z2->_cartesian} : ($z2, 0);
	return $re1 == $re2 && $im1 == $im2 ? 1 : 0;
}

#
# (_negate)
#
# Computes -z.
#
sub _negate {
	my ($z) = @_;
	if ($z->{c_dirty}) {
		my ($r, $t) = @{$z->_polar};
		$t = ($t <= 0) ? $t + pi : $t - pi;
		return (ref $z)->emake($r, $t);
	}
	my ($re, $im) = @{$z->_cartesian};
	return (ref $z)->make(-$re, -$im);
}

#
# (_conjugate)
#
# Compute complex's _conjugate.
#
sub _conjugate {
	my ($z) = @_;
	if ($z->{c_dirty}) {
		my ($r, $t) = @{$z->_polar};
		return (ref $z)->emake($r, -$t);
	}
	my ($re, $im) = @{$z->_cartesian};
	return (ref $z)->make($re, -$im);
}

#
# (abs)
#
# Compute or set complex's norm (rho).
#
sub abs {
	my ($z, $rho) = @_;
	unless (ref $z) {
	    if (@_ == 2) {
		$_[0] = $_[1];
	    } else {
		return CORE::abs($z);
	    }
	}
	if (defined $rho) {
	    $z->{'polar'} = [ $rho, ${$z->_polar}[1] ];
	    $z->{p_dirty} = 0;
	    $z->{c_dirty} = 1;
	    return $rho;
	} else {
	    return ${$z->_polar}[0];
	}
}

sub _theta {
    my $theta = $_[0];

    if    ($$theta >   pi()) { $$theta -= pi2 }
    elsif ($$theta <= -pi()) { $$theta += pi2 }
}

#
# arg
#
# Compute or set complex's argument (theta).
#
sub arg {
	my ($z, $theta) = @_;
	return $z unless ref $z;
	if (defined $theta) {
	    _theta(\$theta);
	    $z->{'polar'} = [ ${$z->_polar}[0], $theta ];
	    $z->{p_dirty} = 0;
	    $z->{c_dirty} = 1;
	} else {
	    $theta = ${$z->_polar}[1];
	    _theta(\$theta);
	}
	return $theta;
}

#
# (sqrt)
#
# Compute sqrt(z).
#
# It is quite tempting to use wantarray here so that in list context
# sqrt() would return the two solutions.  This, however, would
# break things like
#
#	print "sqrt(z) = ", sqrt($z), "\n";
#
# The two values would be printed side by side without no intervening
# whitespace, quite confusing.
# Therefore if you want the two solutions use the root().
#
sub sqrt {
	my ($z) = @_;
	my ($re, $im) = ref $z ? @{$z->_cartesian} : ($z, 0);
	return $re < 0 ? cplx(0, CORE::sqrt(-$re)) : CORE::sqrt($re)
	    if $im == 0;
	my ($r, $t) = @{$z->_polar};
	return (ref $z)->emake(CORE::sqrt($r), $t/2);
}

#
# cbrt
#
# Compute cbrt(z) (cubic root).
#
# Why are we not returning three values?  The same answer as for sqrt().
#
sub cbrt {
	my ($z) = @_;
	return $z < 0 ?
	    -CORE::exp(CORE::log(-$z)/3) :
		($z > 0 ? CORE::exp(CORE::log($z)/3): 0)
	    unless ref $z;
	my ($r, $t) = @{$z->_polar};
	return 0 if $r == 0;
	return (ref $z)->emake(CORE::exp(CORE::log($r)/3), $t/3);
}

#
# _rootbad
#
# Die on bad root.
#
sub _rootbad {
    my $mess = "Root '$_[0]' illegal, root rank must be positive integer.\n";

    my @up = caller(1);

    $mess .= "Died at $up[1] line $up[2].\n";

    die $mess;
}

#
# root
#
# Computes all nth root for z, returning an array whose size is n.
# `n' must be a positive integer.
#
# The roots are given by (for k = 0..n-1):
#
# z^(1/n) = r^(1/n) (cos ((t+2 k pi)/n) + i sin ((t+2 k pi)/n))
#
sub root {
	my ($z, $n, $k) = @_;
	_rootbad($n) if ($n < 1 or int($n) != $n);
	my ($r, $t) = ref $z ?
	    @{$z->_polar} : (CORE::abs($z), $z >= 0 ? 0 : pi);
	my $theta_inc = pi2 / $n;
	my $rho = $r ** (1/$n);
	my $cartesian = ref $z && $z->{c_dirty} == 0;
	if (@_ == 2) {
	    my @root;
	    for (my $i = 0, my $theta = $t / $n;
		 $i < $n;
		 $i++, $theta += $theta_inc) {
		my $w = cplxe($rho, $theta);
		# Yes, $cartesian is loop invariant.
		push @root, $cartesian ? cplx(@{$w->_cartesian}) : $w;
	    }
	    return @root;
	} elsif (@_ == 3) {
	    my $w = cplxe($rho, $t / $n + $k * $theta_inc);
	    return $cartesian ? cplx(@{$w->_cartesian}) : $w;
	}
}

#
# Re
#
# Return or set Re(z).
#
sub Re {
	my ($z, $Re) = @_;
	return $z unless ref $z;
	if (defined $Re) {
	    $z->{'cartesian'} = [ $Re, ${$z->_cartesian}[1] ];
	    $z->{c_dirty} = 0;
	    $z->{p_dirty} = 1;
	} else {
	    return ${$z->_cartesian}[0];
	}
}

#
# Im
#
# Return or set Im(z).
#
sub Im {
	my ($z, $Im) = @_;
	return 0 unless ref $z;
	if (defined $Im) {
	    $z->{'cartesian'} = [ ${$z->_cartesian}[0], $Im ];
	    $z->{c_dirty} = 0;
	    $z->{p_dirty} = 1;
	} else {
	    return ${$z->_cartesian}[1];
	}
}

#
# rho
#
# Return or set rho(w).
#
sub rho {
    Math::Complex::abs(@_);
}

#
# theta
#
# Return or set theta(w).
#
sub theta {
    Math::Complex::arg(@_);
}

#
# (exp)
#
# Computes exp(z).
#
sub exp {
	my ($z) = @_;
	my ($x, $y) = @{$z->_cartesian};
	return (ref $z)->emake(CORE::exp($x), $y);
}

#
# _logofzero
#
# Die on logarithm of zero.
#
sub _logofzero {
    my $mess = "$_[0]: Logarithm of zero.\n";

    if (defined $_[1]) {
	$mess .= "(Because in the definition of $_[0], the argument ";
	$mess .= "$_[1] " unless ($_[1] eq '0');
	$mess .= "is 0)\n";
    }

    my @up = caller(1);

    $mess .= "Died at $up[1] line $up[2].\n";

    die $mess;
}

#
# (log)
#
# Compute log(z).
#
sub log {
	my ($z) = @_;
	unless (ref $z) {
	    _logofzero("log") if $z == 0;
	    return $z > 0 ? CORE::log($z) : cplx(CORE::log(-$z), pi);
	}
	my ($r, $t) = @{$z->_polar};
	_logofzero("log") if $r == 0;
	if    ($t >   pi()) { $t -= pi2 }
	elsif ($t <= -pi()) { $t += pi2 }
	return (ref $z)->make(CORE::log($r), $t);
}

#
# ln
#
# Alias for log().
#
sub ln { Math::Complex::log(@_) }

#
# log10
#
# Compute log10(z).
#

sub log10 {
	return Math::Complex::log($_[0]) * _uplog10;
}

#
# logn
#
# Compute logn(z,n) = log(z) / log(n)
#
sub logn {
	my ($z, $n) = @_;
	$z = cplx($z, 0) unless ref $z;
	my $logn = $LOGN{$n};
	$logn = $LOGN{$n} = CORE::log($n) unless defined $logn;	# Cache log(n)
	return &log($z) / $logn;
}

#
# (cos)
#
# Compute cos(z) = (exp(iz) + exp(-iz))/2.
#
sub cos {
	my ($z) = @_;
	return CORE::cos($z) unless ref $z;
	my ($x, $y) = @{$z->_cartesian};
	my $ey = CORE::exp($y);
	my $sx = CORE::sin($x);
	my $cx = CORE::cos($x);
	my $ey_1 = $ey ? 1 / $ey : Inf();
	return (ref $z)->make($cx * ($ey + $ey_1)/2,
			      $sx * ($ey_1 - $ey)/2);
}

#
# (sin)
#
# Compute sin(z) = (exp(iz) - exp(-iz))/2.
#
sub sin {
	my ($z) = @_;
	return CORE::sin($z) unless ref $z;
	my ($x, $y) = @{$z->_cartesian};
	my $ey = CORE::exp($y);
	my $sx = CORE::sin($x);
	my $cx = CORE::cos($x);
	my $ey_1 = $ey ? 1 / $ey : Inf();
	return (ref $z)->make($sx * ($ey + $ey_1)/2,
			      $cx * ($ey - $ey_1)/2);
}

#
# tan
#
# Compute tan(z) = sin(z) / cos(z).
#
sub tan {
	my ($z) = @_;
	my $cz = &cos($z);
	_divbyzero "tan($z)", "cos($z)" if $cz == 0;
	return &sin($z) / $cz;
}

#
# sec
#
# Computes the secant sec(z) = 1 / cos(z).
#
sub sec {
	my ($z) = @_;
	my $cz = &cos($z);
	_divbyzero "sec($z)", "cos($z)" if ($cz == 0);
	return 1 / $cz;
}

#
# csc
#
# Computes the cosecant csc(z) = 1 / sin(z).
#
sub csc {
	my ($z) = @_;
	my $sz = &sin($z);
	_divbyzero "csc($z)", "sin($z)" if ($sz == 0);
	return 1 / $sz;
}

#
# cosec
#
# Alias for csc().
#
sub cosec { Math::Complex::csc(@_) }

#
# cot
#
# Computes cot(z) = cos(z) / sin(z).
#
sub cot {
	my ($z) = @_;
	my $sz = &sin($z);
	_divbyzero "cot($z)", "sin($z)" if ($sz == 0);
	return &cos($z) / $sz;
}

#
# cotan
#
# Alias for cot().
#
sub cotan { Math::Complex::cot(@_) }

#
# acos
#
# Computes the arc cosine acos(z) = -i log(z + sqrt(z*z-1)).
#
sub acos {
	my $z = $_[0];
	return CORE::atan2(CORE::sqrt(1-$z*$z), $z)
	    if (! ref $z) && CORE::abs($z) <= 1;
	$z = cplx($z, 0) unless ref $z;
	my ($x, $y) = @{$z->_cartesian};
	return 0 if $x == 1 && $y == 0;
	my $t1 = CORE::sqrt(($x+1)*($x+1) + $y*$y);
	my $t2 = CORE::sqrt(($x-1)*($x-1) + $y*$y);
	my $alpha = ($t1 + $t2)/2;
	my $beta  = ($t1 - $t2)/2;
	$alpha = 1 if $alpha < 1;
	if    ($beta >  1) { $beta =  1 }
	elsif ($beta < -1) { $beta = -1 }
	my $u = CORE::atan2(CORE::sqrt(1-$beta*$beta), $beta);
	my $v = CORE::log($alpha + CORE::sqrt($alpha*$alpha-1));
	$v = -$v if $y > 0 || ($y == 0 && $x < -1);
	return (ref $z)->make($u, $v);
}

#
# asin
#
# Computes the arc sine asin(z) = -i log(iz + sqrt(1-z*z)).
#
sub asin {
	my $z = $_[0];
	return CORE::atan2($z, CORE::sqrt(1-$z*$z))
	    if (! ref $z) && CORE::abs($z) <= 1;
	$z = cplx($z, 0) unless ref $z;
	my ($x, $y) = @{$z->_cartesian};
	return 0 if $x == 0 && $y == 0;
	my $t1 = CORE::sqrt(($x+1)*($x+1) + $y*$y);
	my $t2 = CORE::sqrt(($x-1)*($x-1) + $y*$y);
	my $alpha = ($t1 + $t2)/2;
	my $beta  = ($t1 - $t2)/2;
	$alpha = 1 if $alpha < 1;
	if    ($beta >  1) { $beta =  1 }
	elsif ($beta < -1) { $beta = -1 }
	my $u =  CORE::atan2($beta, CORE::sqrt(1-$beta*$beta));
	my $v = -CORE::log($alpha + CORE::sqrt($alpha*$alpha-1));
	$v = -$v if $y > 0 || ($y == 0 && $x < -1);
	return (ref $z)->make($u, $v);
}

#
# atan
#
# Computes the arc tangent atan(z) = i/2 log((i+z) / (i-z)).
#
sub atan {
	my ($z) = @_;
	return CORE::atan2($z, 1) unless ref $z;
	my ($x, $y) = ref $z ? @{$z->_cartesian} : ($z, 0);
	return 0 if $x == 0 && $y == 0;
	_divbyzero "atan(i)"  if ( $z == i);
	_logofzero "atan(-i)" if (-$z == i); # -i is a bad file test...
	my $log = &log((i + $z) / (i - $z));
	return _ip2 * $log;
}

#
# asec
#
# Computes the arc secant asec(z) = acos(1 / z).
#
sub asec {
	my ($z) = @_;
	_divbyzero "asec($z)", $z if ($z == 0);
	return acos(1 / $z);
}

#
# acsc
#
# Computes the arc cosecant acsc(z) = asin(1 / z).
#
sub acsc {
	my ($z) = @_;
	_divbyzero "acsc($z)", $z if ($z == 0);
	return asin(1 / $z);
}

#
# acosec
#
# Alias for acsc().
#
sub acosec { Math::Complex::acsc(@_) }

#
# acot
#
# Computes the arc cotangent acot(z) = atan(1 / z)
#
sub acot {
	my ($z) = @_;
	_divbyzero "acot(0)"  if $z == 0;
	return ($z >= 0) ? CORE::atan2(1, $z) : CORE::atan2(-1, -$z)
	    unless ref $z;
	_divbyzero "acot(i)"  if ($z - i == 0);
	_logofzero "acot(-i)" if ($z + i == 0);
	return atan(1 / $z);
}

#
# acotan
#
# Alias for acot().
#
sub acotan { Math::Complex::acot(@_) }

#
# cosh
#
# Computes the hyperbolic cosine cosh(z) = (exp(z) + exp(-z))/2.
#
sub cosh {
	my ($z) = @_;
	my $ex;
	unless (ref $z) {
	    $ex = CORE::exp($z);
            return $ex ? ($ex == $ExpInf ? Inf() : ($ex + 1/$ex)/2) : Inf();
	}
	my ($x, $y) = @{$z->_cartesian};
	$ex = CORE::exp($x);
	my $ex_1 = $ex ? 1 / $ex : Inf();
	return (ref $z)->make(CORE::cos($y) * ($ex + $ex_1)/2,
			      CORE::sin($y) * ($ex - $ex_1)/2);
}

#
# sinh
#
# Computes the hyperbolic sine sinh(z) = (exp(z) - exp(-z))/2.
#
sub sinh {
	my ($z) = @_;
	my $ex;
	unless (ref $z) {
	    return 0 if $z == 0;
	    $ex = CORE::exp($z);
            return $ex ? ($ex == $ExpInf ? Inf() : ($ex - 1/$ex)/2) : -Inf();
	}
	my ($x, $y) = @{$z->_cartesian};
	my $cy = CORE::cos($y);
	my $sy = CORE::sin($y);
	$ex = CORE::exp($x);
	my $ex_1 = $ex ? 1 / $ex : Inf();
	return (ref $z)->make(CORE::cos($y) * ($ex - $ex_1)/2,
			      CORE::sin($y) * ($ex + $ex_1)/2);
}

#
# tanh
#
# Computes the hyperbolic tangent tanh(z) = sinh(z) / cosh(z).
#
sub tanh {
	my ($z) = @_;
	my $cz = cosh($z);
	_divbyzero "tanh($z)", "cosh($z)" if ($cz == 0);
	my $sz = sinh($z);
	return  1 if $cz ==  $sz;
	return -1 if $cz == -$sz;
	return $sz / $cz;
}

#
# sech
#
# Computes the hyperbolic secant sech(z) = 1 / cosh(z).
#
sub sech {
	my ($z) = @_;
	my $cz = cosh($z);
	_divbyzero "sech($z)", "cosh($z)" if ($cz == 0);
	return 1 / $cz;
}

#
# csch
#
# Computes the hyperbolic cosecant csch(z) = 1 / sinh(z).
#
sub csch {
	my ($z) = @_;
	my $sz = sinh($z);
	_divbyzero "csch($z)", "sinh($z)" if ($sz == 0);
	return 1 / $sz;
}

#
# cosech
#
# Alias for csch().
#
sub cosech { Math::Complex::csch(@_) }

#
# coth
#
# Computes the hyperbolic cotangent coth(z) = cosh(z) / sinh(z).
#
sub coth {
	my ($z) = @_;
	my $sz = sinh($z);
	_divbyzero "coth($z)", "sinh($z)" if $sz == 0;
	my $cz = cosh($z);
	return  1 if $cz ==  $sz;
	return -1 if $cz == -$sz;
	return $cz / $sz;
}

#
# cotanh
#
# Alias for coth().
#
sub cotanh { Math::Complex::coth(@_) }

#
# acosh
#
# Computes the area/inverse hyperbolic cosine acosh(z) = log(z + sqrt(z*z-1)).
#
sub acosh {
	my ($z) = @_;
	unless (ref $z) {
	    $z = cplx($z, 0);
	}
	my ($re, $im) = @{$z->_cartesian};
	if ($im == 0) {
	    return CORE::log($re + CORE::sqrt($re*$re - 1))
		if $re >= 1;
	    return cplx(0, CORE::atan2(CORE::sqrt(1 - $re*$re), $re))
		if CORE::abs($re) < 1;
	}
	my $t = &sqrt($z * $z - 1) + $z;
	# Try Taylor if looking bad (this usually means that
	# $z was large negative, therefore the sqrt is really
	# close to abs(z), summing that with z...)
	$t = 1/(2 * $z) - 1/(8 * $z**3) + 1/(16 * $z**5) - 5/(128 * $z**7)
	    if $t == 0;
	my $u = &log($t);
	$u->Im(-$u->Im) if $re < 0 && $im == 0;
	return $re < 0 ? -$u : $u;
}

#
# asinh
#
# Computes the area/inverse hyperbolic sine asinh(z) = log(z + sqrt(z*z+1))
#
sub asinh {
	my ($z) = @_;
	unless (ref $z) {
	    my $t = $z + CORE::sqrt($z*$z + 1);
	    return CORE::log($t) if $t;
	}
	my $t = &sqrt($z * $z + 1) + $z;
	# Try Taylor if looking bad (this usually means that
	# $z was large negative, therefore the sqrt is really
	# close to abs(z), summing that with z...)
	$t = 1/(2 * $z) - 1/(8 * $z**3) + 1/(16 * $z**5) - 5/(128 * $z**7)
	    if $t == 0;
	return &log($t);
}

#
# atanh
#
# Computes the area/inverse hyperbolic tangent atanh(z) = 1/2 log((1+z) / (1-z)).
#
sub atanh {
	my ($z) = @_;
	unless (ref $z) {
	    return CORE::log((1 + $z)/(1 - $z))/2 if CORE::abs($z) < 1;
	    $z = cplx($z, 0);
	}
	_divbyzero 'atanh(1)',  "1 - $z" if (1 - $z == 0);
	_logofzero 'atanh(-1)'           if (1 + $z == 0);
	return 0.5 * &log((1 + $z) / (1 - $z));
}

#
# asech
#
# Computes the area/inverse hyperbolic secant asech(z) = acosh(1 / z).
#
sub asech {
	my ($z) = @_;
	_divbyzero 'asech(0)', "$z" if ($z == 0);
	return acosh(1 / $z);
}

#
# acsch
#
# Computes the area/inverse hyperbolic cosecant acsch(z) = asinh(1 / z).
#
sub acsch {
	my ($z) = @_;
	_divbyzero 'acsch(0)', $z if ($z == 0);
	return asinh(1 / $z);
}

#
# acosech
#
# Alias for acosh().
#
sub acosech { Math::Complex::acsch(@_) }

#
# acoth
#
# Computes the area/inverse hyperbolic cotangent acoth(z) = 1/2 log((1+z) / (z-1)).
#
sub acoth {
	my ($z) = @_;
	_divbyzero 'acoth(0)'            if ($z == 0);
	unless (ref $z) {
	    return CORE::log(($z + 1)/($z - 1))/2 if CORE::abs($z) > 1;
	    $z = cplx($z, 0);
	}
	_divbyzero 'acoth(1)',  "$z - 1" if ($z - 1 == 0);
	_logofzero 'acoth(-1)', "1 + $z" if (1 + $z == 0);
	return &log((1 + $z) / ($z - 1)) / 2;
}

#
# acotanh
#
# Alias for acot().
#
sub acotanh { Math::Complex::acoth(@_) }

#
# (atan2)
#
# Compute atan(z1/z2), minding the right quadrant.
#
sub atan2 {
	my ($z1, $z2, $inverted) = @_;
	my ($re1, $im1, $re2, $im2);
	if ($inverted) {
	    ($re1, $im1) = ref $z2 ? @{$z2->_cartesian} : ($z2, 0);
	    ($re2, $im2) = ref $z1 ? @{$z1->_cartesian} : ($z1, 0);
	} else {
	    ($re1, $im1) = ref $z1 ? @{$z1->_cartesian} : ($z1, 0);
	    ($re2, $im2) = ref $z2 ? @{$z2->_cartesian} : ($z2, 0);
	}
	if ($im1 || $im2) {
	    # In MATLAB the imaginary parts are ignored.
	    # warn "atan2: Imaginary parts ignored";
	    # http://documents.wolfram.com/mathematica/functions/ArcTan
	    # NOTE: Mathematica ArcTan[x,y] while atan2(y,x)
	    my $s = $z1 * $z1 + $z2 * $z2;
	    _divbyzero("atan2") if $s == 0;
	    my $i = &i;
	    my $r = $z2 + $z1 * $i;
	    return -$i * &log($r / &sqrt( $s ));
	}
	return CORE::atan2($re1, $re2);
}

#
# display_format
# ->display_format
#
# Set (get if no argument) the display format for all complex numbers that
# don't happen to have overridden it via ->display_format
#
# When called as an object method, this actually sets the display format for
# the current object.
#
# Valid object formats are 'c' and 'p' for cartesian and polar. The first
# letter is used actually, so the type can be fully spelled out for clarity.
#
sub display_format {
	my $self  = shift;
	my %display_format = %DISPLAY_FORMAT;

	if (ref $self) {			# Called as an object method
	    if (exists $self->{display_format}) {
		my %obj = %{$self->{display_format}};
		@display_format{keys %obj} = values %obj;
	    }
	}
	if (@_ == 1) {
	    $display_format{style} = shift;
	} else {
	    my %new = @_;
	    @display_format{keys %new} = values %new;
	}

	if (ref $self) { # Called as an object method
	    $self->{display_format} = { %display_format };
	    return
		wantarray ?
		    %{$self->{display_format}} :
		    $self->{display_format}->{style};
	}

        # Called as a class method
	%DISPLAY_FORMAT = %display_format;
	return
	    wantarray ?
		%DISPLAY_FORMAT :
		    $DISPLAY_FORMAT{style};
}

#
# (_stringify)
#
# Show nicely formatted complex number under its cartesian or polar form,
# depending on the current display format:
#
# . If a specific display format has been recorded for this object, use it.
# . Otherwise, use the generic current default for all complex numbers,
#   which is a package global variable.
#
sub _stringify {
	my ($z) = shift;

	my $style = $z->display_format;

	$style = $DISPLAY_FORMAT{style} unless defined $style;

	return $z->_stringify_polar if $style =~ /^p/i;
	return $z->_stringify_cartesian;
}

#
# ->_stringify_cartesian
#
# Stringify as a cartesian representation 'a+bi'.
#
sub _stringify_cartesian {
	my $z  = shift;
	my ($x, $y) = @{$z->_cartesian};
	my ($re, $im);

	my %format = $z->display_format;
	my $format = $format{format};

	if ($x) {
	    if ($x =~ /^NaN[QS]?$/i) {
		$re = $x;
	    } else {
		if ($x =~ /^-?\Q$Inf\E$/oi) {
		    $re = $x;
		} else {
		    $re = defined $format ? sprintf($format, $x) : $x;
		}
	    }
	} else {
	    undef $re;
	}

	if ($y) {
	    if ($y =~ /^(NaN[QS]?)$/i) {
		$im = $y;
	    } else {
		if ($y =~ /^-?\Q$Inf\E$/oi) {
		    $im = $y;
		} else {
		    $im =
			defined $format ?
			    sprintf($format, $y) :
			    ($y == 1 ? "" : ($y == -1 ? "-" : $y));
		}
	    }
	    $im .= "i";
	} else {
	    undef $im;
	}

	my $str = $re;

	if (defined $im) {
	    if ($y < 0) {
		$str .= $im;
	    } elsif ($y > 0 || $im =~ /^NaN[QS]?i$/i)  {
		$str .= "+" if defined $re;
		$str .= $im;
	    }
	} elsif (!defined $re) {
	    $str = "0";
	}

	return $str;
}


#
# ->_stringify_polar
#
# Stringify as a polar representation '[r,t]'.
#
sub _stringify_polar {
	my $z  = shift;
	my ($r, $t) = @{$z->_polar};
	my $theta;

	my %format = $z->display_format;
	my $format = $format{format};

	if ($t =~ /^NaN[QS]?$/i || $t =~ /^-?\Q$Inf\E$/oi) {
	    $theta = $t; 
	} elsif ($t == pi) {
	    $theta = "pi";
	} elsif ($r == 0 || $t == 0) {
	    $theta = defined $format ? sprintf($format, $t) : $t;
	}

	return "[$r,$theta]" if defined $theta;

	#
	# Try to identify pi/n and friends.
	#

	$t -= int(CORE::abs($t) / pi2) * pi2;

	if ($format{polar_pretty_print} && $t) {
	    my ($a, $b);
	    for $a (2..9) {
		$b = $t * $a / pi;
		if ($b =~ /^-?\d+$/) {
		    $b = $b < 0 ? "-" : "" if CORE::abs($b) == 1;
		    $theta = "${b}pi/$a";
		    last;
		}
	    }
	}

        if (defined $format) {
	    $r     = sprintf($format, $r);
	    $theta = sprintf($format, $theta) unless defined $theta;
	} else {
	    $theta = $t unless defined $theta;
	}

	return "[$r,$theta]";
}

sub Inf {
    return $Inf;
}

1;
__END__

=pod

=head1 NAME

Math::Complex - complex numbers and associated mathematical functions

=head1 SYNOPSIS

	use Math::Complex;

	$z = Math::Complex->make(5, 6);
	$t = 4 - 3*i + $z;
	$j = cplxe(1, 2*pi/3);

=head1 DESCRIPTION

This package lets you create and manipulate complex numbers. By default,
I<Perl> limits itself to real numbers, but an extra C<use> statement brings
full complex support, along with a full set of mathematical functions
typically associated with and/or extended to complex numbers.

If you wonder what complex numbers are, they were invented to be able to solve
the following equation:

	x*x = -1

and by definition, the solution is noted I<i> (engineers use I<j> instead since
I<i> usually denotes an intensity, but the name does not matter). The number
I<i> is a pure I<imaginary> number.

The arithmetics with pure imaginary numbers works just like you would expect
it with real numbers... you just have to remember that

	i*i = -1

so you have:

	5i + 7i = i * (5 + 7) = 12i
	4i - 3i = i * (4 - 3) = i
	4i * 2i = -8
	6i / 2i = 3
	1 / i = -i

Complex numbers are numbers that have both a real part and an imaginary
part, and are usually noted:

	a + bi

where C<a> is the I<real> part and C<b> is the I<imaginary> part. The
arithmetic with complex numbers is straightforward. You have to
keep track of the real and the imaginary parts, but otherwise the
rules used for real numbers just apply:

	(4 + 3i) + (5 - 2i) = (4 + 5) + i(3 - 2) = 9 + i
	(2 + i) * (4 - i) = 2*4 + 4i -2i -i*i = 8 + 2i + 1 = 9 + 2i

A graphical representation of complex numbers is possible in a plane
(also called the I<complex plane>, but it's really a 2D plane).
The number

	z = a + bi

is the point whose coordinates are (a, b). Actually, it would
be the vector originating from (0, 0) to (a, b). It follows that the addition
of two complex numbers is a vectorial addition.

Since there is a bijection between a point in the 2D plane and a complex
number (i.e. the mapping is unique and reciprocal), a complex number
can also be uniquely identified with polar coordinates:

	[rho, theta]

where C<rho> is the distance to the origin, and C<theta> the angle between
the vector and the I<x> axis. There is a notation for this using the
exponential form, which is:

	rho * exp(i * theta)

where I<i> is the famous imaginary number introduced above. Conversion
between this form and the cartesian form C<a + bi> is immediate:

	a = rho * cos(theta)
	b = rho * sin(theta)

which is also expressed by this formula:

	z = rho * exp(i * theta) = rho * (cos theta + i * sin theta)

In other words, it's the projection of the vector onto the I<x> and I<y>
axes. Mathematicians call I<rho> the I<norm> or I<modulus> and I<theta>
the I<argument> of the complex number. The I<norm> of C<z> is
marked here as C<abs(z)>.

The polar notation (also known as the trigonometric representation) is
much more handy for performing multiplications and divisions of
complex numbers, whilst the cartesian notation is better suited for
additions and subtractions. Real numbers are on the I<x> axis, and
therefore I<y> or I<theta> is zero or I<pi>.

All the common operations that can be performed on a real number have
been defined to work on complex numbers as well, and are merely
I<extensions> of the operations defined on real numbers. This means
they keep their natural meaning when there is no imaginary part, provided
the number is within their definition set.

For instance, the C<sqrt> routine which computes the square root of
its argument is only defined for non-negative real numbers and yields a
non-negative real number (it is an application from B<R+> to B<R+>).
If we allow it to return a complex number, then it can be extended to
negative real numbers to become an application from B<R> to B<C> (the
set of complex numbers):

	sqrt(x) = x >= 0 ? sqrt(x) : sqrt(-x)*i

It can also be extended to be an application from B<C> to B<C>,
whilst its restriction to B<R> behaves as defined above by using
the following definition:

	sqrt(z = [r,t]) = sqrt(r) * exp(i * t/2)

Indeed, a negative real number can be noted C<[x,pi]> (the modulus
I<x> is always non-negative, so C<[x,pi]> is really C<-x>, a negative
number) and the above definition states that

	sqrt([x,pi]) = sqrt(x) * exp(i*pi/2) = [sqrt(x),pi/2] = sqrt(x)*i

which is exactly what we had defined for negative real numbers above.
The C<sqrt> returns only one of the solutions: if you want the both,
use the C<root> function.

All the common mathematical functions defined on real numbers that
are extended to complex numbers share that same property of working
I<as usual> when the imaginary part is zero (otherwise, it would not
be called an extension, would it?).

A I<new> operation possible on a complex number that is
the identity for real numbers is called the I<conjugate>, and is noted
with a horizontal bar above the number, or C<~z> here.

	 z = a + bi
	~z = a - bi

Simple... Now look:

	z * ~z = (a + bi) * (a - bi) = a*a + b*b

We saw that the norm of C<z> was noted C<abs(z)> and was defined as the
distance to the origin, also known as:

	rho = abs(z) = sqrt(a*a + b*b)

so

	z * ~z = abs(z) ** 2

If z is a pure real number (i.e. C<b == 0>), then the above yields:

	a * a = abs(a) ** 2

which is true (C<abs> has the regular meaning for real number, i.e. stands
for the absolute value). This example explains why the norm of C<z> is
noted C<abs(z)>: it extends the C<abs> function to complex numbers, yet
is the regular C<abs> we know when the complex number actually has no
imaginary part... This justifies I<a posteriori> our use of the C<abs>
notation for the norm.

=head1 OPERATIONS

Given the following notations:

	z1 = a + bi = r1 * exp(i * t1)
	z2 = c + di = r2 * exp(i * t2)
	z = <any complex or real number>

the following (overloaded) operations are supported on complex numbers:

	z1 + z2 = (a + c) + i(b + d)
	z1 - z2 = (a - c) + i(b - d)
	z1 * z2 = (r1 * r2) * exp(i * (t1 + t2))
	z1 / z2 = (r1 / r2) * exp(i * (t1 - t2))
	z1 ** z2 = exp(z2 * log z1)
	~z = a - bi
	abs(z) = r1 = sqrt(a*a + b*b)
	sqrt(z) = sqrt(r1) * exp(i * t/2)
	exp(z) = exp(a) * exp(i * b)
	log(z) = log(r1) + i*t
	sin(z) = 1/2i (exp(i * z1) - exp(-i * z))
	cos(z) = 1/2 (exp(i * z1) + exp(-i * z))
	atan2(y, x) = atan(y / x) # Minding the right quadrant, note the order.

The definition used for complex arguments of atan2() is

       -i log((x + iy)/sqrt(x*x+y*y))

Note that atan2(0, 0) is not well-defined.

The following extra operations are supported on both real and complex
numbers:

	Re(z) = a
	Im(z) = b
	arg(z) = t
	abs(z) = r

	cbrt(z) = z ** (1/3)
	log10(z) = log(z) / log(10)
	logn(z, n) = log(z) / log(n)

	tan(z) = sin(z) / cos(z)

	csc(z) = 1 / sin(z)
	sec(z) = 1 / cos(z)
	cot(z) = 1 / tan(z)

	asin(z) = -i * log(i*z + sqrt(1-z*z))
	acos(z) = -i * log(z + i*sqrt(1-z*z))
	atan(z) = i/2 * log((i+z) / (i-z))

	acsc(z) = asin(1 / z)
	asec(z) = acos(1 / z)
	acot(z) = atan(1 / z) = -i/2 * log((i+z) / (z-i))

	sinh(z) = 1/2 (exp(z) - exp(-z))
	cosh(z) = 1/2 (exp(z) + exp(-z))
	tanh(z) = sinh(z) / cosh(z) = (exp(z) - exp(-z)) / (exp(z) + exp(-z))

	csch(z) = 1 / sinh(z)
	sech(z) = 1 / cosh(z)
	coth(z) = 1 / tanh(z)

	asinh(z) = log(z + sqrt(z*z+1))
	acosh(z) = log(z + sqrt(z*z-1))
	atanh(z) = 1/2 * log((1+z) / (1-z))

	acsch(z) = asinh(1 / z)
	asech(z) = acosh(1 / z)
	acoth(z) = atanh(1 / z) = 1/2 * log((1+z) / (z-1))

I<arg>, I<abs>, I<log>, I<csc>, I<cot>, I<acsc>, I<acot>, I<csch>,
I<coth>, I<acosech>, I<acotanh>, have aliases I<rho>, I<theta>, I<ln>,
I<cosec>, I<cotan>, I<acosec>, I<acotan>, I<cosech>, I<cotanh>,
I<acosech>, I<acotanh>, respectively.  C<Re>, C<Im>, C<arg>, C<abs>,
C<rho>, and C<theta> can be used also as mutators.  The C<cbrt>
returns only one of the solutions: if you want all three, use the
C<root> function.

The I<root> function is available to compute all the I<n>
roots of some complex, where I<n> is a strictly positive integer.
There are exactly I<n> such roots, returned as a list. Getting the
number mathematicians call C<j> such that:

	1 + j + j*j = 0;

is a simple matter of writing:

	$j = ((root(1, 3))[1];

The I<k>th root for C<z = [r,t]> is given by:

	(root(z, n))[k] = r**(1/n) * exp(i * (t + 2*k*pi)/n)

You can return the I<k>th root directly by C<root(z, n, k)>,
indexing starting from I<zero> and ending at I<n - 1>.

The I<spaceship> numeric comparison operator, E<lt>=E<gt>, is also
defined. In order to ensure its restriction to real numbers is conform
to what you would expect, the comparison is run on the real part of
the complex number first, and imaginary parts are compared only when
the real parts match.

=head1 CREATION

To create a complex number, use either:

	$z = Math::Complex->make(3, 4);
	$z = cplx(3, 4);

if you know the cartesian form of the number, or

	$z = 3 + 4*i;

if you like. To create a number using the polar form, use either:

	$z = Math::Complex->emake(5, pi/3);
	$x = cplxe(5, pi/3);

instead. The first argument is the modulus, the second is the angle
(in radians, the full circle is 2*pi).  (Mnemonic: C<e> is used as a
notation for complex numbers in the polar form).

It is possible to write:

	$x = cplxe(-3, pi/4);

but that will be silently converted into C<[3,-3pi/4]>, since the
modulus must be non-negative (it represents the distance to the origin
in the complex plane).

It is also possible to have a complex number as either argument of the
C<make>, C<emake>, C<cplx>, and C<cplxe>: the appropriate component of
the argument will be used.

	$z1 = cplx(-2,  1);
	$z2 = cplx($z1, 4);

The C<new>, C<make>, C<emake>, C<cplx>, and C<cplxe> will also
understand a single (string) argument of the forms

    	2-3i
    	-3i
	[2,3]
	[2,-3pi/4]
	[2]

in which case the appropriate cartesian and exponential components
will be parsed from the string and used to create new complex numbers.
The imaginary component and the theta, respectively, will default to zero.

The C<new>, C<make>, C<emake>, C<cplx>, and C<cplxe> will also
understand the case of no arguments: this means plain zero or (0, 0).

=head1 DISPLAYING

When printed, a complex number is usually shown under its cartesian
style I<a+bi>, but there are legitimate cases where the polar style
I<[r,t]> is more appropriate.  The process of converting the complex
number into a string that can be displayed is known as I<stringification>.

By calling the class method C<Math::Complex::display_format> and
supplying either C<"polar"> or C<"cartesian"> as an argument, you
override the default display style, which is C<"cartesian">. Not
supplying any argument returns the current settings.

This default can be overridden on a per-number basis by calling the
C<display_format> method instead. As before, not supplying any argument
returns the current display style for this number. Otherwise whatever you
specify will be the new display style for I<this> particular number.

For instance:

	use Math::Complex;

	Math::Complex::display_format('polar');
	$j = (root(1, 3))[1];
	print "j = $j\n";		# Prints "j = [1,2pi/3]"
	$j->display_format('cartesian');
	print "j = $j\n";		# Prints "j = -0.5+0.866025403784439i"

The polar style attempts to emphasize arguments like I<k*pi/n>
(where I<n> is a positive integer and I<k> an integer within [-9, +9]),
this is called I<polar pretty-printing>.

For the reverse of stringifying, see the C<make> and C<emake>.

=head2 CHANGED IN PERL 5.6

The C<display_format> class method and the corresponding
C<display_format> object method can now be called using
a parameter hash instead of just a one parameter.

The old display format style, which can have values C<"cartesian"> or
C<"polar">, can be changed using the C<"style"> parameter.

	$j->display_format(style => "polar");

The one parameter calling convention also still works.

	$j->display_format("polar");

There are two new display parameters.

The first one is C<"format">, which is a sprintf()-style format string
to be used for both numeric parts of the complex number(s).  The is
somewhat system-dependent but most often it corresponds to C<"%.15g">.
You can revert to the default by setting the C<format> to C<undef>.

	# the $j from the above example

	$j->display_format('format' => '%.5f');
	print "j = $j\n";		# Prints "j = -0.50000+0.86603i"
	$j->display_format('format' => undef);
	print "j = $j\n";		# Prints "j = -0.5+0.86603i"

Notice that this affects also the return values of the
C<display_format> methods: in list context the whole parameter hash
will be returned, as opposed to only the style parameter value.
This is a potential incompatibility with earlier versions if you
have been calling the C<display_format> method in list context.

The second new display parameter is C<"polar_pretty_print">, which can
be set to true or false, the default being true.  See the previous
section for what this means.

=head1 USAGE

Thanks to overloading, the handling of arithmetics with complex numbers
is simple and almost transparent.

Here are some examples:

	use Math::Complex;

	$j = cplxe(1, 2*pi/3);	# $j ** 3 == 1
	print "j = $j, j**3 = ", $j ** 3, "\n";
	print "1 + j + j**2 = ", 1 + $j + $j**2, "\n";

	$z = -16 + 0*i;			# Force it to be a complex
	print "sqrt($z) = ", sqrt($z), "\n";

	$k = exp(i * 2*pi/3);
	print "$j - $k = ", $j - $k, "\n";

	$z->Re(3);			# Re, Im, arg, abs,
	$j->arg(2);			# (the last two aka rho, theta)
					# can be used also as mutators.

=head1 CONSTANTS

=head2 PI

The constant C<pi> and some handy multiples of it (pi2, pi4,
and pip2 (pi/2) and pip4 (pi/4)) are also available if separately
exported:

    use Math::Complex ':pi'; 
    $third_of_circle = pi2 / 3;

=head2 Inf

The floating point infinity can be exported as a subroutine Inf():

    use Math::Complex qw(Inf sinh);
    my $AlsoInf = Inf() + 42;
    my $AnotherInf = sinh(1e42);
    print "$AlsoInf is $AnotherInf\n" if $AlsoInf == $AnotherInf;

Note that the stringified form of infinity varies between platforms:
it can be for example any of

   inf
   infinity
   INF
   1.#INF

or it can be something else. 

Also note that in some platforms trying to use the infinity in
arithmetic operations may result in Perl crashing because using
an infinity causes SIGFPE or its moral equivalent to be sent.
The way to ignore this is

  local $SIG{FPE} = sub { };

=head1 ERRORS DUE TO DIVISION BY ZERO OR LOGARITHM OF ZERO

The division (/) and the following functions

	log	ln	log10	logn
	tan	sec	csc	cot
	atan	asec	acsc	acot
	tanh	sech	csch	coth
	atanh	asech	acsch	acoth

cannot be computed for all arguments because that would mean dividing
by zero or taking logarithm of zero. These situations cause fatal
runtime errors looking like this

	cot(0): Division by zero.
	(Because in the definition of cot(0), the divisor sin(0) is 0)
	Died at ...

or

	atanh(-1): Logarithm of zero.
	Died at...

For the C<csc>, C<cot>, C<asec>, C<acsc>, C<acot>, C<csch>, C<coth>,
C<asech>, C<acsch>, the argument cannot be C<0> (zero).  For the
logarithmic functions and the C<atanh>, C<acoth>, the argument cannot
be C<1> (one).  For the C<atanh>, C<acoth>, the argument cannot be
C<-1> (minus one).  For the C<atan>, C<acot>, the argument cannot be
C<i> (the imaginary unit).  For the C<atan>, C<acoth>, the argument
cannot be C<-i> (the negative imaginary unit).  For the C<tan>,
C<sec>, C<tanh>, the argument cannot be I<pi/2 + k * pi>, where I<k>
is any integer.  atan2(0, 0) is undefined, and if the complex arguments
are used for atan2(), a division by zero will happen if z1**2+z2**2 == 0.

Note that because we are operating on approximations of real numbers,
these errors can happen when merely `too close' to the singularities
listed above.

=head1 ERRORS DUE TO INDIGESTIBLE ARGUMENTS

The C<make> and C<emake> accept both real and complex arguments.
When they cannot recognize the arguments they will die with error
messages like the following

    Math::Complex::make: Cannot take real part of ...
    Math::Complex::make: Cannot take real part of ...
    Math::Complex::emake: Cannot take rho of ...
    Math::Complex::emake: Cannot take theta of ...

=head1 BUGS

Saying C<use Math::Complex;> exports many mathematical routines in the
caller environment and even overrides some (C<sqrt>, C<log>, C<atan2>).
This is construed as a feature by the Authors, actually... ;-)

All routines expect to be given real or complex numbers. Don't attempt to
use BigFloat, since Perl has currently no rule to disambiguate a '+'
operation (for instance) between two overloaded entities.

In Cray UNICOS there is some strange numerical instability that results
in root(), cos(), sin(), cosh(), sinh(), losing accuracy fast.  Beware.
The bug may be in UNICOS math libs, in UNICOS C compiler, in Math::Complex.
Whatever it is, it does not manifest itself anywhere else where Perl runs.

=head1 SEE ALSO

L<Math::Trig>

=head1 AUTHORS

Daniel S. Lewart <F<lewart!at!uiuc.edu>>
Jarkko Hietaniemi <F<jhi!at!iki.fi>>
Raphael Manfredi <F<Raphael_Manfredi!at!pobox.com>>

=head1 LICENSE

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut

1;

# eof
