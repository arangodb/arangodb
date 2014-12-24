package Math::BigFloat;

# 
# Mike grinned. 'Two down, infinity to go' - Mike Nostrus in 'Before and After'
#

# The following hash values are internally used:
#   _e	: exponent (ref to $CALC object)
#   _m	: mantissa (ref to $CALC object)
#   _es	: sign of _e
# sign	: +,-,+inf,-inf, or "NaN" if not a number
#   _a	: accuracy
#   _p	: precision

$VERSION = '1.60';
require 5.006;

require Exporter;
@ISA		= qw/Math::BigInt/;
@EXPORT_OK	= qw/bpi/;

use strict;
# $_trap_inf/$_trap_nan are internal and should never be accessed from outside
use vars qw/$AUTOLOAD $accuracy $precision $div_scale $round_mode $rnd_mode
	    $upgrade $downgrade $_trap_nan $_trap_inf/;
my $class = "Math::BigFloat";

use overload
'<=>'	=>	sub { my $rc = $_[2] ?
                      ref($_[0])->bcmp($_[1],$_[0]) : 
                      ref($_[0])->bcmp($_[0],$_[1]); 
		      $rc = 1 unless defined $rc;
		      $rc <=> 0;
		},
# we need '>=' to get things like "1 >= NaN" right:
'>='	=>	sub { my $rc = $_[2] ?
                      ref($_[0])->bcmp($_[1],$_[0]) : 
                      ref($_[0])->bcmp($_[0],$_[1]);
		      # if there was a NaN involved, return false
		      return '' unless defined $rc;
		      $rc >= 0;
		},
'int'	=>	sub { $_[0]->as_number() },		# 'trunc' to bigint
;

##############################################################################
# global constants, flags and assorted stuff

# the following are public, but their usage is not recommended. Use the
# accessor methods instead.

# class constants, use Class->constant_name() to access
# one of 'even', 'odd', '+inf', '-inf', 'zero', 'trunc' or 'common'
$round_mode = 'even';
$accuracy   = undef;
$precision  = undef;
$div_scale  = 40;

$upgrade = undef;
$downgrade = undef;
# the package we are using for our private parts, defaults to:
# Math::BigInt->config()->{lib}
my $MBI = 'Math::BigInt::FastCalc';

# are NaNs ok? (otherwise it dies when encountering an NaN) set w/ config()
$_trap_nan = 0;
# the same for infinity
$_trap_inf = 0;

# constant for easier life
my $nan = 'NaN'; 

my $IMPORT = 0;	# was import() called yet? used to make require work

# some digits of accuracy for blog(undef,10); which we use in blog() for speed
my $LOG_10 = 
 '2.3025850929940456840179914546843642076011014886287729760333279009675726097';
my $LOG_10_A = length($LOG_10)-1;
# ditto for log(2)
my $LOG_2 = 
 '0.6931471805599453094172321214581765680755001343602552541206800094933936220';
my $LOG_2_A = length($LOG_2)-1;
my $HALF = '0.5';			# made into an object if nec.

##############################################################################
# the old code had $rnd_mode, so we need to support it, too

sub TIESCALAR   { my ($class) = @_; bless \$round_mode, $class; }
sub FETCH       { return $round_mode; }
sub STORE       { $rnd_mode = $_[0]->round_mode($_[1]); }

BEGIN
  {
  # when someone sets $rnd_mode, we catch this and check the value to see
  # whether it is valid or not. 
  $rnd_mode   = 'even'; tie $rnd_mode, 'Math::BigFloat';

  # we need both of them in this package:
  *as_int = \&as_number;
  }
 
##############################################################################

{
  # valid method aliases for AUTOLOAD
  my %methods = map { $_ => 1 }  
   qw / fadd fsub fmul fdiv fround ffround fsqrt fmod fstr fsstr fpow fnorm
        fint facmp fcmp fzero fnan finf finc fdec ffac fneg
	fceil ffloor frsft flsft fone flog froot fexp
      /;
  # valid methods that can be handed up (for AUTOLOAD)
  my %hand_ups = map { $_ => 1 }  
   qw / is_nan is_inf is_negative is_positive is_pos is_neg
        accuracy precision div_scale round_mode fabs fnot
        objectify upgrade downgrade
	bone binf bnan bzero
	bsub
      /;

  sub _method_alias { exists $methods{$_[0]||''}; } 
  sub _method_hand_up { exists $hand_ups{$_[0]||''}; } 
}

##############################################################################
# constructors

sub new 
  {
  # create a new BigFloat object from a string or another bigfloat object. 
  # _e: exponent
  # _m: mantissa
  # sign  => sign (+/-), or "NaN"

  my ($class,$wanted,@r) = @_;

  # avoid numify-calls by not using || on $wanted!
  return $class->bzero() if !defined $wanted;	# default to 0
  return $wanted->copy() if UNIVERSAL::isa($wanted,'Math::BigFloat');

  $class->import() if $IMPORT == 0;             # make require work

  my $self = {}; bless $self, $class;
  # shortcut for bigints and its subclasses
  if ((ref($wanted)) && UNIVERSAL::can( $wanted, "as_number"))
    {
    $self->{_m} = $wanted->as_number()->{value}; # get us a bigint copy
    $self->{_e} = $MBI->_zero();
    $self->{_es} = '+';
    $self->{sign} = $wanted->sign();
    return $self->bnorm();
    }
  # else: got a string or something maskerading as number (with overload)

  # handle '+inf', '-inf' first
  if ($wanted =~ /^[+-]?inf\z/)
    {
    return $downgrade->new($wanted) if $downgrade;

    $self->{sign} = $wanted;		# set a default sign for bstr()
    return $self->binf($wanted);
    }

  # shortcut for simple forms like '12' that neither have trailing nor leading
  # zeros
  if ($wanted =~ /^([+-]?)([1-9][0-9]*[1-9])$/)
    {
    $self->{_e} = $MBI->_zero();
    $self->{_es} = '+';
    $self->{sign} = $1 || '+';
    $self->{_m} = $MBI->_new($2);
    return $self->round(@r) if !$downgrade;
    }

  my ($mis,$miv,$mfv,$es,$ev) = Math::BigInt::_split($wanted);
  if (!ref $mis)
    {
    if ($_trap_nan)
      {
      require Carp;
      Carp::croak ("$wanted is not a number initialized to $class");
      }
    
    return $downgrade->bnan() if $downgrade;
    
    $self->{_e} = $MBI->_zero();
    $self->{_es} = '+';
    $self->{_m} = $MBI->_zero();
    $self->{sign} = $nan;
    }
  else
    {
    # make integer from mantissa by adjusting exp, then convert to int
    $self->{_e} = $MBI->_new($$ev);		# exponent
    $self->{_es} = $$es || '+';
    my $mantissa = "$$miv$$mfv"; 		# create mant.
    $mantissa =~ s/^0+(\d)/$1/;			# strip leading zeros
    $self->{_m} = $MBI->_new($mantissa); 	# create mant.

    # 3.123E0 = 3123E-3, and 3.123E-2 => 3123E-5
    if (CORE::length($$mfv) != 0)
      {
      my $len = $MBI->_new( CORE::length($$mfv));
      ($self->{_e}, $self->{_es}) =
	_e_sub ($self->{_e}, $len, $self->{_es}, '+');
      }
    # we can only have trailing zeros on the mantissa if $$mfv eq ''
    else
      {
      # Use a regexp to count the trailing zeros in $$miv instead of _zeros()
      # because that is faster, especially when _m is not stored in base 10.
      my $zeros = 0; $zeros = CORE::length($1) if $$miv =~ /[1-9](0*)$/; 
      if ($zeros != 0)
        {
        my $z = $MBI->_new($zeros);
        # turn '120e2' into '12e3'
        $MBI->_rsft ( $self->{_m}, $z, 10);
        ($self->{_e}, $self->{_es}) =
	  _e_add ( $self->{_e}, $z, $self->{_es}, '+');
        }
      }
    $self->{sign} = $$mis;

    # for something like 0Ey, set y to 1, and -0 => +0
    # Check $$miv for being '0' and $$mfv eq '', because otherwise _m could not
    # have become 0. That's faster than to call $MBI->_is_zero().
    $self->{sign} = '+', $self->{_e} = $MBI->_one()
     if $$miv eq '0' and $$mfv eq '';

    return $self->round(@r) if !$downgrade;
    }
  # if downgrade, inf, NaN or integers go down

  if ($downgrade && $self->{_es} eq '+')
    {
    if ($MBI->_is_zero( $self->{_e} ))
      {
      return $downgrade->new($$mis . $MBI->_str( $self->{_m} ));
      }
    return $downgrade->new($self->bsstr()); 
    }
  $self->bnorm()->round(@r);			# first normalize, then round
  }

sub copy
  {
  # if two arguments, the first one is the class to "swallow" subclasses
  if (@_ > 1)
    {
    my  $self = bless {
	sign => $_[1]->{sign}, 
	_es => $_[1]->{_es}, 
	_m => $MBI->_copy($_[1]->{_m}),
	_e => $MBI->_copy($_[1]->{_e}),
    }, $_[0] if @_ > 1;

    $self->{_a} = $_[1]->{_a} if defined $_[1]->{_a};
    $self->{_p} = $_[1]->{_p} if defined $_[1]->{_p};
    return $self;
    }

  my $self = bless {
	sign => $_[0]->{sign}, 
	_es => $_[0]->{_es}, 
	_m => $MBI->_copy($_[0]->{_m}),
	_e => $MBI->_copy($_[0]->{_e}),
	}, ref($_[0]);

  $self->{_a} = $_[0]->{_a} if defined $_[0]->{_a};
  $self->{_p} = $_[0]->{_p} if defined $_[0]->{_p};
  $self;
  }

sub _bnan
  {
  # used by parent class bone() to initialize number to NaN
  my $self = shift;
  
  if ($_trap_nan)
    {
    require Carp;
    my $class = ref($self);
    Carp::croak ("Tried to set $self to NaN in $class\::_bnan()");
    }

  $IMPORT=1;					# call our import only once
  $self->{_m} = $MBI->_zero();
  $self->{_e} = $MBI->_zero();
  $self->{_es} = '+';
  }

sub _binf
  {
  # used by parent class bone() to initialize number to +-inf
  my $self = shift;
  
  if ($_trap_inf)
    {
    require Carp;
    my $class = ref($self);
    Carp::croak ("Tried to set $self to +-inf in $class\::_binf()");
    }

  $IMPORT=1;					# call our import only once
  $self->{_m} = $MBI->_zero();
  $self->{_e} = $MBI->_zero();
  $self->{_es} = '+';
  }

sub _bone
  {
  # used by parent class bone() to initialize number to 1
  my $self = shift;
  $IMPORT=1;					# call our import only once
  $self->{_m} = $MBI->_one();
  $self->{_e} = $MBI->_zero();
  $self->{_es} = '+';
  }

sub _bzero
  {
  # used by parent class bone() to initialize number to 0
  my $self = shift;
  $IMPORT=1;					# call our import only once
  $self->{_m} = $MBI->_zero();
  $self->{_e} = $MBI->_one();
  $self->{_es} = '+';
  }

sub isa
  {
  my ($self,$class) = @_;
  return if $class =~ /^Math::BigInt/;		# we aren't one of these
  UNIVERSAL::isa($self,$class);
  }

sub config
  {
  # return (later set?) configuration data as hash ref
  my $class = shift || 'Math::BigFloat';

  if (@_ == 1 && ref($_[0]) ne 'HASH')
    {
    my $cfg = $class->SUPER::config();
    return $cfg->{$_[0]};
    }

  my $cfg = $class->SUPER::config(@_);

  # now we need only to override the ones that are different from our parent
  $cfg->{class} = $class;
  $cfg->{with} = $MBI;
  $cfg;
  }

##############################################################################
# string conversation

sub bstr 
  {
  # (ref to BFLOAT or num_str ) return num_str
  # Convert number from internal format to (non-scientific) string format.
  # internal format is always normalized (no leading zeros, "-0" => "+0")
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)
    {
    return $x->{sign} unless $x->{sign} eq '+inf';      # -inf, NaN
    return 'inf';                                       # +inf
    }

  my $es = '0'; my $len = 1; my $cad = 0; my $dot = '.';

  # $x is zero?
  my $not_zero = !($x->{sign} eq '+' && $MBI->_is_zero($x->{_m}));
  if ($not_zero)
    {
    $es = $MBI->_str($x->{_m});
    $len = CORE::length($es);
    my $e = $MBI->_num($x->{_e});	
    $e = -$e if $x->{_es} eq '-';
    if ($e < 0)
      {
      $dot = '';
      # if _e is bigger than a scalar, the following will blow your memory
      if ($e <= -$len)
        {
        my $r = abs($e) - $len;
        $es = '0.'. ('0' x $r) . $es; $cad = -($len+$r);
        }
      else
        {
        substr($es,$e,0) = '.'; $cad = $MBI->_num($x->{_e});
        $cad = -$cad if $x->{_es} eq '-';
        }
      }
    elsif ($e > 0)
      {
      # expand with zeros
      $es .= '0' x $e; $len += $e; $cad = 0;
      }
    } # if not zero

  $es = '-'.$es if $x->{sign} eq '-';
  # if set accuracy or precision, pad with zeros on the right side
  if ((defined $x->{_a}) && ($not_zero))
    {
    # 123400 => 6, 0.1234 => 4, 0.001234 => 4
    my $zeros = $x->{_a} - $cad;		# cad == 0 => 12340
    $zeros = $x->{_a} - $len if $cad != $len;
    $es .= $dot.'0' x $zeros if $zeros > 0;
    }
  elsif ((($x->{_p} || 0) < 0))
    {
    # 123400 => 6, 0.1234 => 4, 0.001234 => 6
    my $zeros = -$x->{_p} + $cad;
    $es .= $dot.'0' x $zeros if $zeros > 0;
    }
  $es;
  }

sub bsstr
  {
  # (ref to BFLOAT or num_str ) return num_str
  # Convert number from internal format to scientific string format.
  # internal format is always normalized (no leading zeros, "-0E0" => "+0E0")
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)
    {
    return $x->{sign} unless $x->{sign} eq '+inf';      # -inf, NaN
    return 'inf';                                       # +inf
    }
  my $sep = 'e'.$x->{_es};
  my $sign = $x->{sign}; $sign = '' if $sign eq '+';
  $sign . $MBI->_str($x->{_m}) . $sep . $MBI->_str($x->{_e});
  }
    
sub numify 
  {
  # Make a number from a BigFloat object
  # simple return a string and let Perl's atoi()/atof() handle the rest
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);
  $x->bsstr(); 
  }

##############################################################################
# public stuff (usually prefixed with "b")

sub bneg
  {
  # (BINT or num_str) return BINT
  # negate number or make a negated number from string
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return $x if $x->modify('bneg');

  # for +0 dont negate (to have always normalized +0). Does nothing for 'NaN'
  $x->{sign} =~ tr/+-/-+/ unless ($x->{sign} eq '+' && $MBI->_is_zero($x->{_m}));
  $x;
  }

# tels 2001-08-04 
# XXX TODO this must be overwritten and return NaN for non-integer values
# band(), bior(), bxor(), too
#sub bnot
#  {
#  $class->SUPER::bnot($class,@_);
#  }

sub bcmp 
  {
  # Compares 2 values.  Returns one of undef, <0, =0, >0. (suitable for sort)

  # set up parameters
  my ($self,$x,$y) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y) = objectify(2,@_);
    }

  return $upgrade->bcmp($x,$y) if defined $upgrade &&
    ((!$x->isa($self)) || (!$y->isa($self)));

  if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/))
    {
    # handle +-inf and NaN
    return undef if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));
    return 0 if ($x->{sign} eq $y->{sign}) && ($x->{sign} =~ /^[+-]inf$/);
    return +1 if $x->{sign} eq '+inf';
    return -1 if $x->{sign} eq '-inf';
    return -1 if $y->{sign} eq '+inf';
    return +1;
    }

  # check sign for speed first
  return 1 if $x->{sign} eq '+' && $y->{sign} eq '-';	# does also 0 <=> -y
  return -1 if $x->{sign} eq '-' && $y->{sign} eq '+';	# does also -x <=> 0

  # shortcut 
  my $xz = $x->is_zero();
  my $yz = $y->is_zero();
  return 0 if $xz && $yz;				# 0 <=> 0
  return -1 if $xz && $y->{sign} eq '+';		# 0 <=> +y
  return 1 if $yz && $x->{sign} eq '+';			# +x <=> 0

  # adjust so that exponents are equal
  my $lxm = $MBI->_len($x->{_m});
  my $lym = $MBI->_len($y->{_m});
  # the numify somewhat limits our length, but makes it much faster
  my ($xes,$yes) = (1,1);
  $xes = -1 if $x->{_es} ne '+';
  $yes = -1 if $y->{_es} ne '+';
  my $lx = $lxm + $xes * $MBI->_num($x->{_e});
  my $ly = $lym + $yes * $MBI->_num($y->{_e});
  my $l = $lx - $ly; $l = -$l if $x->{sign} eq '-';
  return $l <=> 0 if $l != 0;
  
  # lengths (corrected by exponent) are equal
  # so make mantissa equal length by padding with zero (shift left)
  my $diff = $lxm - $lym;
  my $xm = $x->{_m};		# not yet copy it
  my $ym = $y->{_m};
  if ($diff > 0)
    {
    $ym = $MBI->_copy($y->{_m});
    $ym = $MBI->_lsft($ym, $MBI->_new($diff), 10);
    }
  elsif ($diff < 0)
    {
    $xm = $MBI->_copy($x->{_m});
    $xm = $MBI->_lsft($xm, $MBI->_new(-$diff), 10);
    }
  my $rc = $MBI->_acmp($xm,$ym);
  $rc = -$rc if $x->{sign} eq '-';		# -124 < -123
  $rc <=> 0;
  }

sub bacmp 
  {
  # Compares 2 values, ignoring their signs. 
  # Returns one of undef, <0, =0, >0. (suitable for sort)
  
  # set up parameters
  my ($self,$x,$y) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y) = objectify(2,@_);
    }

  return $upgrade->bacmp($x,$y) if defined $upgrade &&
    ((!$x->isa($self)) || (!$y->isa($self)));

  # handle +-inf and NaN's
  if ($x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/)
    {
    return undef if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));
    return 0 if ($x->is_inf() && $y->is_inf());
    return 1 if ($x->is_inf() && !$y->is_inf());
    return -1;
    }

  # shortcut 
  my $xz = $x->is_zero();
  my $yz = $y->is_zero();
  return 0 if $xz && $yz;				# 0 <=> 0
  return -1 if $xz && !$yz;				# 0 <=> +y
  return 1 if $yz && !$xz;				# +x <=> 0

  # adjust so that exponents are equal
  my $lxm = $MBI->_len($x->{_m});
  my $lym = $MBI->_len($y->{_m});
  my ($xes,$yes) = (1,1);
  $xes = -1 if $x->{_es} ne '+';
  $yes = -1 if $y->{_es} ne '+';
  # the numify somewhat limits our length, but makes it much faster
  my $lx = $lxm + $xes * $MBI->_num($x->{_e});
  my $ly = $lym + $yes * $MBI->_num($y->{_e});
  my $l = $lx - $ly;
  return $l <=> 0 if $l != 0;
  
  # lengths (corrected by exponent) are equal
  # so make mantissa equal-length by padding with zero (shift left)
  my $diff = $lxm - $lym;
  my $xm = $x->{_m};		# not yet copy it
  my $ym = $y->{_m};
  if ($diff > 0)
    {
    $ym = $MBI->_copy($y->{_m});
    $ym = $MBI->_lsft($ym, $MBI->_new($diff), 10);
    }
  elsif ($diff < 0)
    {
    $xm = $MBI->_copy($x->{_m});
    $xm = $MBI->_lsft($xm, $MBI->_new(-$diff), 10);
    }
  $MBI->_acmp($xm,$ym);
  }

sub badd 
  {
  # add second arg (BFLOAT or string) to first (BFLOAT) (modifies first)
  # return result as BFLOAT

  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }
 
  return $x if $x->modify('badd');

  # inf and NaN handling
  if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/))
    {
    # NaN first
    return $x->bnan() if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));
    # inf handling
    if (($x->{sign} =~ /^[+-]inf$/) && ($y->{sign} =~ /^[+-]inf$/))
      {
      # +inf++inf or -inf+-inf => same, rest is NaN
      return $x if $x->{sign} eq $y->{sign};
      return $x->bnan();
      }
    # +-inf + something => +inf; something +-inf => +-inf
    $x->{sign} = $y->{sign}, return $x if $y->{sign} =~ /^[+-]inf$/;
    return $x;
    }

  return $upgrade->badd($x,$y,@r) if defined $upgrade &&
   ((!$x->isa($self)) || (!$y->isa($self)));

  $r[3] = $y;						# no push!

  # speed: no add for 0+y or x+0
  return $x->bround(@r) if $y->is_zero();		# x+0
  if ($x->is_zero())					# 0+y
    {
    # make copy, clobbering up x (modify in place!)
    $x->{_e} = $MBI->_copy($y->{_e});
    $x->{_es} = $y->{_es};
    $x->{_m} = $MBI->_copy($y->{_m});
    $x->{sign} = $y->{sign} || $nan;
    return $x->round(@r);
    }
 
  # take lower of the two e's and adapt m1 to it to match m2
  my $e = $y->{_e};
  $e = $MBI->_zero() if !defined $e;		# if no BFLOAT?
  $e = $MBI->_copy($e);				# make copy (didn't do it yet)

  my $es;

  ($e,$es) = _e_sub($e, $x->{_e}, $y->{_es} || '+', $x->{_es});

  my $add = $MBI->_copy($y->{_m});

  if ($es eq '-')				# < 0
    {
    $MBI->_lsft( $x->{_m}, $e, 10);
    ($x->{_e},$x->{_es}) = _e_add($x->{_e}, $e, $x->{_es}, $es);
    }
  elsif (!$MBI->_is_zero($e))			# > 0
    {
    $MBI->_lsft($add, $e, 10);
    }
  # else: both e are the same, so just leave them

  if ($x->{sign} eq $y->{sign})
    {
    # add
    $x->{_m} = $MBI->_add($x->{_m}, $add);
    }
  else
    {
    ($x->{_m}, $x->{sign}) = 
     _e_add($x->{_m}, $add, $x->{sign}, $y->{sign});
    }

  # delete trailing zeros, then round
  $x->bnorm()->round(@r);
  }

# sub bsub is inherited from Math::BigInt!

sub binc
  {
  # increment arg by one
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x if $x->modify('binc');

  if ($x->{_es} eq '-')
    {
    return $x->badd($self->bone(),@r);	#  digits after dot
    }

  if (!$MBI->_is_zero($x->{_e}))		# _e == 0 for NaN, inf, -inf
    {
    # 1e2 => 100, so after the shift below _m has a '0' as last digit
    $x->{_m} = $MBI->_lsft($x->{_m}, $x->{_e},10);	# 1e2 => 100
    $x->{_e} = $MBI->_zero();				# normalize
    $x->{_es} = '+';
    # we know that the last digit of $x will be '1' or '9', depending on the
    # sign
    }
  # now $x->{_e} == 0
  if ($x->{sign} eq '+')
    {
    $MBI->_inc($x->{_m});
    return $x->bnorm()->bround(@r);
    }
  elsif ($x->{sign} eq '-')
    {
    $MBI->_dec($x->{_m});
    $x->{sign} = '+' if $MBI->_is_zero($x->{_m}); # -1 +1 => -0 => +0
    return $x->bnorm()->bround(@r);
    }
  # inf, nan handling etc
  $x->badd($self->bone(),@r);			# badd() does round 
  }

sub bdec
  {
  # decrement arg by one
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x if $x->modify('bdec');

  if ($x->{_es} eq '-')
    {
    return $x->badd($self->bone('-'),@r);	#  digits after dot
    }

  if (!$MBI->_is_zero($x->{_e}))
    {
    $x->{_m} = $MBI->_lsft($x->{_m}, $x->{_e},10);	# 1e2 => 100
    $x->{_e} = $MBI->_zero();				# normalize
    $x->{_es} = '+';
    }
  # now $x->{_e} == 0
  my $zero = $x->is_zero();
  # <= 0
  if (($x->{sign} eq '-') || $zero)
    {
    $MBI->_inc($x->{_m});
    $x->{sign} = '-' if $zero;				# 0 => 1 => -1
    $x->{sign} = '+' if $MBI->_is_zero($x->{_m});	# -1 +1 => -0 => +0
    return $x->bnorm()->round(@r);
    }
  # > 0
  elsif ($x->{sign} eq '+')
    {
    $MBI->_dec($x->{_m});
    return $x->bnorm()->round(@r);
    }
  # inf, nan handling etc
  $x->badd($self->bone('-'),@r);		# does round
  } 

sub DEBUG () { 0; }

sub blog
  {
  my ($self,$x,$base,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x if $x->modify('blog');

  # $base > 0, $base != 1; if $base == undef default to $base == e
  # $x >= 0

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my ($scale,@params);
  ($x,@params) = $x->_find_round_parameters($a,$p,$r);

  # also takes care of the "error in _find_round_parameters?" case
  return $x->bnan() if $x->{sign} ne '+' || $x->is_zero();

  # no rounding at all, so must use fallback
  if (scalar @params == 0)
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $params[1] = undef;			# P = undef
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4;	# take whatever is defined
    }

  return $x->bzero(@params) if $x->is_one();
  # base not defined => base == Euler's number e
  if (defined $base)
    {
    # make object, since we don't feed it through objectify() to still get the
    # case of $base == undef
    $base = $self->new($base) unless ref($base);
    # $base > 0; $base != 1
    return $x->bnan() if $base->is_zero() || $base->is_one() ||
      $base->{sign} ne '+';
    # if $x == $base, we know the result must be 1.0
    if ($x->bcmp($base) == 0)
      {
      $x->bone('+',@params);
      if ($fallback)
        {
        # clear a/p after round, since user did not request it
        delete $x->{_a}; delete $x->{_p};
        }
      return $x;
      }
    }

  # when user set globals, they would interfere with our calculation, so
  # disable them and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;
  local $Math::BigFloat::downgrade = undef;

  # upgrade $x if $x is not a BigFloat (handle BigInt input)
  # XXX TODO: rebless!
  if (!$x->isa('Math::BigFloat'))
    {
    $x = Math::BigFloat->new($x);
    $self = ref($x);
    }
  
  my $done = 0;

  # If the base is defined and an integer, try to calculate integer result
  # first. This is very fast, and in case the real result was found, we can
  # stop right here.
  if (defined $base && $base->is_int() && $x->is_int())
    {
    my $i = $MBI->_copy( $x->{_m} );
    $MBI->_lsft( $i, $x->{_e}, 10 ) unless $MBI->_is_zero($x->{_e});
    my $int = Math::BigInt->bzero();
    $int->{value} = $i;
    $int->blog($base->as_number());
    # if ($exact)
    if ($base->as_number()->bpow($int) == $x)
      {
      # found result, return it
      $x->{_m} = $int->{value};
      $x->{_e} = $MBI->_zero();
      $x->{_es} = '+';
      $x->bnorm();
      $done = 1;
      }
    }

  if ($done == 0)
    {
    # base is undef, so base should be e (Euler's number), so first calculate the
    # log to base e (using reduction by 10 (and probably 2)):
    $self->_log_10($x,$scale);

    # and if a different base was requested, convert it
    if (defined $base)
      {
      $base = Math::BigFloat->new($base) unless $base->isa('Math::BigFloat');
      # not ln, but some other base (don't modify $base)
      $x->bdiv( $base->copy()->blog(undef,$scale), $scale );
      }
    }
 
  # shortcut to not run through _find_round_parameters again
  if (defined $params[0])
    {
    $x->bround($params[0],$params[2]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[1],$params[2]);		# then round accordingly
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    delete $x->{_a}; delete $x->{_p};
    }
  # restore globals
  $$abr = $ab; $$pbr = $pb;

  $x;
  }

sub _len_to_steps
  {
  # Given D (digits in decimal), compute N so that N! (N factorial) is
  # at least D digits long. D should be at least 50.
  my $d = shift;

  # two constants for the Ramanujan estimate of ln(N!)
  my $lg2 = log(2 * 3.14159265) / 2;
  my $lg10 = log(10);

  # D = 50 => N => 42, so L = 40 and R = 50
  my $l = 40; my $r = $d;

  # Otherwise this does not work under -Mbignum and we do not yet have "no bignum;" :(
  $l = $l->numify if ref($l);
  $r = $r->numify if ref($r);
  $lg2 = $lg2->numify if ref($lg2);
  $lg10 = $lg10->numify if ref($lg10);

  # binary search for the right value (could this be written as the reverse of lg(n!)?)
  while ($r - $l > 1)
    {
    my $n = int(($r - $l) / 2) + $l;
    my $ramanujan = 
      int(($n * log($n) - $n + log( $n * (1 + 4*$n*(1+2*$n)) ) / 6 + $lg2) / $lg10);
    $ramanujan > $d ? $r = $n : $l = $n;
    }
  $l;
  }

sub bnok
  {
  # Calculate n over k (binomial coefficient or "choose" function) as integer.
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);

  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $x if $x->modify('bnok');

  return $x->bnan() if $x->is_nan() || $y->is_nan();
  return $x->binf() if $x->is_inf();

  my $u = $x->as_int();
  $u->bnok($y->as_int());

  $x->{_m} = $u->{value};
  $x->{_e} = $MBI->_zero();
  $x->{_es} = '+';
  $x->{sign} = '+';
  $x->bnorm(@r);
  }

sub bexp
  {
  # Calculate e ** X (Euler's number to the power of X)
  my ($self,$x,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x if $x->modify('bexp');

  return $x->binf() if $x->{sign} eq '+inf';
  return $x->bzero() if $x->{sign} eq '-inf';

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my ($scale,@params);
  ($x,@params) = $x->_find_round_parameters($a,$p,$r);

  # also takes care of the "error in _find_round_parameters?" case
  return $x if $x->{sign} eq 'NaN';

  # no rounding at all, so must use fallback
  if (scalar @params == 0)
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $params[1] = undef;			# P = undef
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it's not enough...
    $scale = abs($params[0] || $params[1]) + 4;	# take whatever is defined
    }

  return $x->bone(@params) if $x->is_zero();

  if (!$x->isa('Math::BigFloat'))
    {
    $x = Math::BigFloat->new($x);
    $self = ref($x);
    }
  
  # when user set globals, they would interfere with our calculation, so
  # disable them and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;
  local $Math::BigFloat::downgrade = undef;

  my $x_org = $x->copy();

  # We use the following Taylor series:

  #           x    x^2   x^3   x^4
  #  e = 1 + --- + --- + --- + --- ...
  #           1!    2!    3!    4!

  # The difference for each term is X and N, which would result in:
  # 2 copy, 2 mul, 2 add, 1 inc, 1 div operations per term

  # But it is faster to compute exp(1) and then raising it to the
  # given power, esp. if $x is really big and an integer because:

  #  * The numerator is always 1, making the computation faster
  #  * the series converges faster in the case of x == 1
  #  * We can also easily check when we have reached our limit: when the
  #    term to be added is smaller than "1E$scale", we can stop - f.i.
  #    scale == 5, and we have 1/40320, then we stop since 1/40320 < 1E-5.
  #  * we can compute the *exact* result by simulating bigrat math:

  #  1   1    gcd(3,4) = 1    1*24 + 1*6    5
  #  - + -                  = ---------- =  --                 
  #  6   24                      6*24       24

  # We do not compute the gcd() here, but simple do:
  #  1   1    1*24 + 1*6   30
  #  - + -  = --------- =  --                 
  #  6   24       6*24     144

  # In general:
  #  a   c    a*d + c*b 	and note that c is always 1 and d = (b*f)
  #  - + -  = ---------
  #  b   d       b*d

  # This leads to:         which can be reduced by b to:
  #  a   1     a*b*f + b    a*f + 1
  #  - + -   = --------- =  -------
  #  b   b*f     b*b*f        b*f

  # The first terms in the series are:

  # 1     1    1    1    1    1     1     1     13700
  # -- + -- + -- + -- + -- + --- + --- + ---- = -----
  # 1     1    2    6   24   120   720   5040   5040

  # Note that we cannot simple reduce 13700/5040 to 685/252, but must keep A and B!

  if ($scale <= 75)
    {
    # set $x directly from a cached string form
    $x->{_m} = $MBI->_new(
    "27182818284590452353602874713526624977572470936999595749669676277240766303535476");
    $x->{sign} = '+';
    $x->{_es} = '-';
    $x->{_e} = $MBI->_new(79);
    }
  else
    {
    # compute A and B so that e = A / B.
 
    # After some terms we end up with this, so we use it as a starting point:
    my $A = $MBI->_new("90933395208605785401971970164779391644753259799242");
    my $F = $MBI->_new(42); my $step = 42;

    # Compute how many steps we need to take to get $A and $B sufficiently big
    my $steps = _len_to_steps($scale - 4);
#    print STDERR "# Doing $steps steps for ", $scale-4, " digits\n";
    while ($step++ <= $steps)
      {
      # calculate $a * $f + 1
      $A = $MBI->_mul($A, $F);
      $A = $MBI->_inc($A);
      # increment f
      $F = $MBI->_inc($F);
      }
    # compute $B as factorial of $steps (this is faster than doing it manually)
    my $B = $MBI->_fac($MBI->_new($steps));
    
#  print "A ", $MBI->_str($A), "\nB ", $MBI->_str($B), "\n";

    # compute A/B with $scale digits in the result (truncate, not round)
    $A = $MBI->_lsft( $A, $MBI->_new($scale), 10);
    $A = $MBI->_div( $A, $B );

    $x->{_m} = $A;
    $x->{sign} = '+';
    $x->{_es} = '-';
    $x->{_e} = $MBI->_new($scale);
    }

  # $x contains now an estimate of e, with some surplus digits, so we can round
  if (!$x_org->is_one())
    {
    # raise $x to the wanted power and round it in one step:
    $x->bpow($x_org, @params);
    }
  else
    {
    # else just round the already computed result
    delete $x->{_a}; delete $x->{_p};
    # shortcut to not run through _find_round_parameters again
    if (defined $params[0])
      {
      $x->bround($params[0],$params[2]);		# then round accordingly
      }
    else
      {
      $x->bfround($params[1],$params[2]);		# then round accordingly
      }
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    delete $x->{_a}; delete $x->{_p};
    }
  # restore globals
  $$abr = $ab; $$pbr = $pb;

  $x;						# return modified $x
  }

sub _log
  {
  # internal log function to calculate ln() based on Taylor series.
  # Modifies $x in place.
  my ($self,$x,$scale) = @_;

  # in case of $x == 1, result is 0
  return $x->bzero() if $x->is_one();

  # XXX TODO: rewrite this in a similiar manner to bexp()

  # http://www.efunda.com/math/taylor_series/logarithmic.cfm?search_string=log

  # u = x-1, v = x+1
  #              _                               _
  # Taylor:     |    u    1   u^3   1   u^5       |
  # ln (x)  = 2 |   --- + - * --- + - * --- + ... |  x > 0
  #             |_   v    3   v^3   5   v^5      _|

  # This takes much more steps to calculate the result and is thus not used
  # u = x-1
  #              _                               _
  # Taylor:     |    u    1   u^2   1   u^3       |
  # ln (x)  = 2 |   --- + - * --- + - * --- + ... |  x > 1/2
  #             |_   x    2   x^2   3   x^3      _|

  my ($limit,$v,$u,$below,$factor,$two,$next,$over,$f);

  $v = $x->copy(); $v->binc();		# v = x+1
  $x->bdec(); $u = $x->copy();		# u = x-1; x = x-1
  $x->bdiv($v,$scale);			# first term: u/v
  $below = $v->copy();
  $over = $u->copy();
  $u *= $u; $v *= $v;				# u^2, v^2
  $below->bmul($v);				# u^3, v^3
  $over->bmul($u);
  $factor = $self->new(3); $f = $self->new(2);

  my $steps = 0 if DEBUG;  
  $limit = $self->new("1E-". ($scale-1));
  while (3 < 5)
    {
    # we calculate the next term, and add it to the last
    # when the next term is below our limit, it won't affect the outcome
    # anymore, so we stop

    # calculating the next term simple from over/below will result in quite
    # a time hog if the input has many digits, since over and below will
    # accumulate more and more digits, and the result will also have many
    # digits, but in the end it is rounded to $scale digits anyway. So if we
    # round $over and $below first, we save a lot of time for the division
    # (not with log(1.2345), but try log (123**123) to see what I mean. This
    # can introduce a rounding error if the division result would be f.i.
    # 0.1234500000001 and we round it to 5 digits it would become 0.12346, but
    # if we truncated $over and $below we might get 0.12345. Does this matter
    # for the end result? So we give $over and $below 4 more digits to be
    # on the safe side (unscientific error handling as usual... :+D

    $next = $over->copy->bround($scale+4)->bdiv(
      $below->copy->bmul($factor)->bround($scale+4), 
      $scale);

## old version:    
##    $next = $over->copy()->bdiv($below->copy()->bmul($factor),$scale);

    last if $next->bacmp($limit) <= 0;

    delete $next->{_a}; delete $next->{_p};
    $x->badd($next);
    # calculate things for the next term
    $over *= $u; $below *= $v; $factor->badd($f);
    if (DEBUG)
      {
      $steps++; print "step $steps = $x\n" if $steps % 10 == 0;
      }
    }
  print "took $steps steps\n" if DEBUG;
  $x->bmul($f);					# $x *= 2
  }

sub _log_10
  {
  # Internal log function based on reducing input to the range of 0.1 .. 9.99
  # and then "correcting" the result to the proper one. Modifies $x in place.
  my ($self,$x,$scale) = @_;

  # Taking blog() from numbers greater than 10 takes a *very long* time, so we
  # break the computation down into parts based on the observation that:
  #  blog(X*Y) = blog(X) + blog(Y)
  # We set Y here to multiples of 10 so that $x becomes below 1 - the smaller
  # $x is the faster it gets. Since 2*$x takes about 10 times as
  # long, we make it faster by about a factor of 100 by dividing $x by 10.

  # The same observation is valid for numbers smaller than 0.1, e.g. computing
  # log(1) is fastest, and the further away we get from 1, the longer it takes.
  # So we also 'break' this down by multiplying $x with 10 and subtract the
  # log(10) afterwards to get the correct result.

  # To get $x even closer to 1, we also divide by 2 and then use log(2) to
  # correct for this. For instance if $x is 2.4, we use the formula:
  #  blog(2.4 * 2) == blog (1.2) + blog(2)
  # and thus calculate only blog(1.2) and blog(2), which is faster in total
  # than calculating blog(2.4).

  # In addition, the values for blog(2) and blog(10) are cached.

  # Calculate nr of digits before dot:
  my $dbd = $MBI->_num($x->{_e});
  $dbd = -$dbd if $x->{_es} eq '-';
  $dbd += $MBI->_len($x->{_m});

  # more than one digit (e.g. at least 10), but *not* exactly 10 to avoid
  # infinite recursion

  my $calc = 1;					# do some calculation?

  # disable the shortcut for 10, since we need log(10) and this would recurse
  # infinitely deep
  if ($x->{_es} eq '+' && $MBI->_is_one($x->{_e}) && $MBI->_is_one($x->{_m}))
    {
    $dbd = 0;					# disable shortcut
    # we can use the cached value in these cases
    if ($scale <= $LOG_10_A)
      {
      $x->bzero(); $x->badd($LOG_10);		# modify $x in place
      $calc = 0; 				# no need to calc, but round
      }
    # if we can't use the shortcut, we continue normally
    }
  else
    {
    # disable the shortcut for 2, since we maybe have it cached
    if (($MBI->_is_zero($x->{_e}) && $MBI->_is_two($x->{_m})))
      {
      $dbd = 0;					# disable shortcut
      # we can use the cached value in these cases
      if ($scale <= $LOG_2_A)
        {
        $x->bzero(); $x->badd($LOG_2);		# modify $x in place
        $calc = 0; 				# no need to calc, but round
        }
      # if we can't use the shortcut, we continue normally
      }
    }

  # if $x = 0.1, we know the result must be 0-log(10)
  if ($calc != 0 && $x->{_es} eq '-' && $MBI->_is_one($x->{_e}) &&
      $MBI->_is_one($x->{_m}))
    {
    $dbd = 0;					# disable shortcut
    # we can use the cached value in these cases
    if ($scale <= $LOG_10_A)
      {
      $x->bzero(); $x->bsub($LOG_10);
      $calc = 0; 				# no need to calc, but round
      }
    }

  return if $calc == 0;				# already have the result

  # default: these correction factors are undef and thus not used
  my $l_10;				# value of ln(10) to A of $scale
  my $l_2;				# value of ln(2) to A of $scale

  my $two = $self->new(2);

  # $x == 2 => 1, $x == 13 => 2, $x == 0.1 => 0, $x == 0.01 => -1
  # so don't do this shortcut for 1 or 0
  if (($dbd > 1) || ($dbd < 0))
    {
    # convert our cached value to an object if not already (avoid doing this
    # at import() time, since not everybody needs this)
    $LOG_10 = $self->new($LOG_10,undef,undef) unless ref $LOG_10;

    #print "x = $x, dbd = $dbd, calc = $calc\n";
    # got more than one digit before the dot, or more than one zero after the
    # dot, so do:
    #  log(123)    == log(1.23) + log(10) * 2
    #  log(0.0123) == log(1.23) - log(10) * 2
  
    if ($scale <= $LOG_10_A)
      {
      # use cached value
      $l_10 = $LOG_10->copy();		# copy for mul
      }
    else
      {
      # else: slower, compute and cache result
      # also disable downgrade for this code path
      local $Math::BigFloat::downgrade = undef;

      # shorten the time to calculate log(10) based on the following:
      # log(1.25 * 8) = log(1.25) + log(8)
      #               = log(1.25) + log(2) + log(2) + log(2)

      # first get $l_2 (and possible compute and cache log(2))
      $LOG_2 = $self->new($LOG_2,undef,undef) unless ref $LOG_2;
      if ($scale <= $LOG_2_A)
        {
        # use cached value
        $l_2 = $LOG_2->copy();			# copy() for the mul below
        }
      else
        {
        # else: slower, compute and cache result
        $l_2 = $two->copy(); $self->_log($l_2, $scale); # scale+4, actually
        $LOG_2 = $l_2->copy();			# cache the result for later
						# the copy() is for mul below
        $LOG_2_A = $scale;
        }

      # now calculate log(1.25):
      $l_10 = $self->new('1.25'); $self->_log($l_10, $scale); # scale+4, actually

      # log(1.25) + log(2) + log(2) + log(2):
      $l_10->badd($l_2);
      $l_10->badd($l_2);
      $l_10->badd($l_2);
      $LOG_10 = $l_10->copy();		# cache the result for later
					# the copy() is for mul below
      $LOG_10_A = $scale;
      }
    $dbd-- if ($dbd > 1); 		# 20 => dbd=2, so make it dbd=1	
    $l_10->bmul( $self->new($dbd));	# log(10) * (digits_before_dot-1)
    my $dbd_sign = '+';
    if ($dbd < 0)
      {
      $dbd = -$dbd;
      $dbd_sign = '-';
      }
    ($x->{_e}, $x->{_es}) = 
	_e_sub( $x->{_e}, $MBI->_new($dbd), $x->{_es}, $dbd_sign); # 123 => 1.23
 
    }

  # Now: 0.1 <= $x < 10 (and possible correction in l_10)

  ### Since $x in the range 0.5 .. 1.5 is MUCH faster, we do a repeated div
  ### or mul by 2 (maximum times 3, since x < 10 and x > 0.1)

  $HALF = $self->new($HALF) unless ref($HALF);

  my $twos = 0;				# default: none (0 times)	
  while ($x->bacmp($HALF) <= 0)		# X <= 0.5
    {
    $twos--; $x->bmul($two);
    }
  while ($x->bacmp($two) >= 0)		# X >= 2
    {
    $twos++; $x->bdiv($two,$scale+4);		# keep all digits
    }
  # $twos > 0 => did mul 2, < 0 => did div 2 (but we never did both)
  # So calculate correction factor based on ln(2):
  if ($twos != 0)
    {
    $LOG_2 = $self->new($LOG_2,undef,undef) unless ref $LOG_2;
    if ($scale <= $LOG_2_A)
      {
      # use cached value
      $l_2 = $LOG_2->copy();			# copy() for the mul below
      }
    else
      {
      # else: slower, compute and cache result
      # also disable downgrade for this code path
      local $Math::BigFloat::downgrade = undef;
      $l_2 = $two->copy(); $self->_log($l_2, $scale); # scale+4, actually
      $LOG_2 = $l_2->copy();			# cache the result for later
						# the copy() is for mul below
      $LOG_2_A = $scale;
      }
    $l_2->bmul($twos);		# * -2 => subtract, * 2 => add
    }
  
  $self->_log($x,$scale);			# need to do the "normal" way
  $x->badd($l_10) if defined $l_10; 		# correct it by ln(10)
  $x->badd($l_2) if defined $l_2;		# and maybe by ln(2)

  # all done, $x contains now the result
  $x;
  }

sub blcm 
  { 
  # (BFLOAT or num_str, BFLOAT or num_str) return BFLOAT
  # does not modify arguments, but returns new object
  # Lowest Common Multiplicator

  my ($self,@arg) = objectify(0,@_);
  my $x = $self->new(shift @arg);
  while (@arg) { $x = Math::BigInt::__lcm($x,shift @arg); } 
  $x;
  }

sub bgcd
  {
  # (BINT or num_str, BINT or num_str) return BINT
  # does not modify arguments, but returns new object

  my $y = shift;
  $y = __PACKAGE__->new($y) if !ref($y);
  my $self = ref($y);
  my $x = $y->copy()->babs();			# keep arguments

  return $x->bnan() if $x->{sign} !~ /^[+-]$/	# x NaN?
	|| !$x->is_int();			# only for integers now

  while (@_)
    {
    my $t = shift; $t = $self->new($t) if !ref($t);
    $y = $t->copy()->babs();
    
    return $x->bnan() if $y->{sign} !~ /^[+-]$/	# y NaN?
     	|| !$y->is_int();			# only for integers now

    # greatest common divisor
    while (! $y->is_zero())
      {
      ($x,$y) = ($y->copy(), $x->copy()->bmod($y));
      }

    last if $x->is_one();
    }
  $x;
  }

##############################################################################

sub _e_add
  {
  # Internal helper sub to take two positive integers and their signs and
  # then add them. Input ($CALC,$CALC,('+'|'-'),('+'|'-')), 
  # output ($CALC,('+'|'-'))
  my ($x,$y,$xs,$ys) = @_;

  # if the signs are equal we can add them (-5 + -3 => -(5 + 3) => -8)
  if ($xs eq $ys)
    {
    $x = $MBI->_add ($x, $y );		# a+b
    # the sign follows $xs
    return ($x, $xs);
    }

  my $a = $MBI->_acmp($x,$y);
  if ($a > 0)
    {
    $x = $MBI->_sub ($x , $y);				# abs sub
    }
  elsif ($a == 0)
    {
    $x = $MBI->_zero();					# result is 0
    $xs = '+';
    }
  else # a < 0
    {
    $x = $MBI->_sub ( $y, $x, 1 );			# abs sub
    $xs = $ys;
    }
  ($x,$xs);
  }

sub _e_sub
  {
  # Internal helper sub to take two positive integers and their signs and
  # then subtract them. Input ($CALC,$CALC,('+'|'-'),('+'|'-')), 
  # output ($CALC,('+'|'-'))
  my ($x,$y,$xs,$ys) = @_;

  # flip sign
  $ys =~ tr/+-/-+/;
  _e_add($x,$y,$xs,$ys);		# call add (does subtract now)
  }

###############################################################################
# is_foo methods (is_negative, is_positive are inherited from BigInt)

sub is_int
  {
  # return true if arg (BFLOAT or num_str) is an integer
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  (($x->{sign} =~ /^[+-]$/) &&			# NaN and +-inf aren't
   ($x->{_es} eq '+')) ? 1 : 0;			# 1e-1 => no integer
  }

sub is_zero
  {
  # return true if arg (BFLOAT or num_str) is zero
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  ($x->{sign} eq '+' && $MBI->_is_zero($x->{_m})) ? 1 : 0;
  }

sub is_one
  {
  # return true if arg (BFLOAT or num_str) is +1 or -1 if signis given
  my ($self,$x,$sign) = ref($_[0]) ? (undef,@_) : objectify(1,@_);

  $sign = '+' if !defined $sign || $sign ne '-';

  ($x->{sign} eq $sign && 
   $MBI->_is_zero($x->{_e}) &&
   $MBI->_is_one($x->{_m}) ) ? 1 : 0; 
  }

sub is_odd
  {
  # return true if arg (BFLOAT or num_str) is odd or false if even
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);
  
  (($x->{sign} =~ /^[+-]$/) &&		# NaN & +-inf aren't
   ($MBI->_is_zero($x->{_e})) &&
   ($MBI->_is_odd($x->{_m}))) ? 1 : 0; 
  }

sub is_even
  {
  # return true if arg (BINT or num_str) is even or false if odd
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  (($x->{sign} =~ /^[+-]$/) &&			# NaN & +-inf aren't
   ($x->{_es} eq '+') &&	 		# 123.45 isn't
   ($MBI->_is_even($x->{_m}))) ? 1 : 0;		# but 1200 is
  }

sub bmul
  { 
  # multiply two numbers
  
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $x if $x->modify('bmul');

  return $x->bnan() if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));

  # inf handling
  if (($x->{sign} =~ /^[+-]inf$/) || ($y->{sign} =~ /^[+-]inf$/))
    {
    return $x->bnan() if $x->is_zero() || $y->is_zero(); 
    # result will always be +-inf:
    # +inf * +/+inf => +inf, -inf * -/-inf => +inf
    # +inf * -/-inf => -inf, -inf * +/+inf => -inf
    return $x->binf() if ($x->{sign} =~ /^\+/ && $y->{sign} =~ /^\+/);
    return $x->binf() if ($x->{sign} =~ /^-/ && $y->{sign} =~ /^-/);
    return $x->binf('-');
    }
  
  return $upgrade->bmul($x,$y,@r) if defined $upgrade &&
   ((!$x->isa($self)) || (!$y->isa($self)));

  # aEb * cEd = (a*c)E(b+d)
  $MBI->_mul($x->{_m},$y->{_m});
  ($x->{_e}, $x->{_es}) = _e_add($x->{_e}, $y->{_e}, $x->{_es}, $y->{_es});

  $r[3] = $y;				# no push!

  # adjust sign:
  $x->{sign} = $x->{sign} ne $y->{sign} ? '-' : '+';
  $x->bnorm->round(@r);
  }

sub bmuladd
  { 
  # multiply two numbers and add the third to the result
  
  # set up parameters
  my ($self,$x,$y,$z,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$z,@r) = objectify(3,@_);
    }

  return $x if $x->modify('bmuladd');

  return $x->bnan() if (($x->{sign} eq $nan) ||
			($y->{sign} eq $nan) ||
			($z->{sign} eq $nan));

  # inf handling
  if (($x->{sign} =~ /^[+-]inf$/) || ($y->{sign} =~ /^[+-]inf$/))
    {
    return $x->bnan() if $x->is_zero() || $y->is_zero(); 
    # result will always be +-inf:
    # +inf * +/+inf => +inf, -inf * -/-inf => +inf
    # +inf * -/-inf => -inf, -inf * +/+inf => -inf
    return $x->binf() if ($x->{sign} =~ /^\+/ && $y->{sign} =~ /^\+/);
    return $x->binf() if ($x->{sign} =~ /^-/ && $y->{sign} =~ /^-/);
    return $x->binf('-');
    }

  return $upgrade->bmul($x,$y,@r) if defined $upgrade &&
   ((!$x->isa($self)) || (!$y->isa($self)));

  # aEb * cEd = (a*c)E(b+d)
  $MBI->_mul($x->{_m},$y->{_m});
  ($x->{_e}, $x->{_es}) = _e_add($x->{_e}, $y->{_e}, $x->{_es}, $y->{_es});

  $r[3] = $y;				# no push!

  # adjust sign:
  $x->{sign} = $x->{sign} ne $y->{sign} ? '-' : '+';

  # z=inf handling (z=NaN handled above)
  $x->{sign} = $z->{sign}, return $x if $z->{sign} =~ /^[+-]inf$/;

  # take lower of the two e's and adapt m1 to it to match m2
  my $e = $z->{_e};
  $e = $MBI->_zero() if !defined $e;		# if no BFLOAT?
  $e = $MBI->_copy($e);				# make copy (didn't do it yet)

  my $es;

  ($e,$es) = _e_sub($e, $x->{_e}, $z->{_es} || '+', $x->{_es});

  my $add = $MBI->_copy($z->{_m});

  if ($es eq '-')				# < 0
    {
    $MBI->_lsft( $x->{_m}, $e, 10);
    ($x->{_e},$x->{_es}) = _e_add($x->{_e}, $e, $x->{_es}, $es);
    }
  elsif (!$MBI->_is_zero($e))			# > 0
    {
    $MBI->_lsft($add, $e, 10);
    }
  # else: both e are the same, so just leave them

  if ($x->{sign} eq $z->{sign})
    {
    # add
    $x->{_m} = $MBI->_add($x->{_m}, $add);
    }
  else
    {
    ($x->{_m}, $x->{sign}) = 
     _e_add($x->{_m}, $add, $x->{sign}, $z->{sign});
    }

  # delete trailing zeros, then round
  $x->bnorm()->round(@r);
  }

sub bdiv 
  {
  # (dividend: BFLOAT or num_str, divisor: BFLOAT or num_str) return 
  # (BFLOAT,BFLOAT) (quo,rem) or BFLOAT (only rem)

  # set up parameters
  my ($self,$x,$y,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$a,$p,$r) = objectify(2,@_);
    }

  return $x if $x->modify('bdiv');

  return $self->_div_inf($x,$y)
   if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/) || $y->is_zero());

  # x== 0 # also: or y == 1 or y == -1
  return wantarray ? ($x,$self->bzero()) : $x if $x->is_zero();

  # upgrade ?
  return $upgrade->bdiv($upgrade->new($x),$y,$a,$p,$r) if defined $upgrade;

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my (@params,$scale);
  ($x,@params) = $x->_find_round_parameters($a,$p,$r,$y);

  return $x if $x->is_nan();		# error in _find_round_parameters?

  # no rounding at all, so must use fallback
  if (scalar @params == 0)
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4;	# take whatever is defined
    }

  my $rem; $rem = $self->bzero() if wantarray;

  $y = $self->new($y) unless $y->isa('Math::BigFloat');

  my $lx = $MBI->_len($x->{_m}); my $ly = $MBI->_len($y->{_m});
  $scale = $lx if $lx > $scale;
  $scale = $ly if $ly > $scale;
  my $diff = $ly - $lx;
  $scale += $diff if $diff > 0;		# if lx << ly, but not if ly << lx!

  # already handled inf/NaN/-inf above:

  # check that $y is not 1 nor -1 and cache the result:
  my $y_not_one = !($MBI->_is_zero($y->{_e}) && $MBI->_is_one($y->{_m}));

  # flipping the sign of $y will also flip the sign of $x for the special
  # case of $x->bsub($x); so we can catch it below:
  my $xsign = $x->{sign};
  $y->{sign} =~ tr/+-/-+/;

  if ($xsign ne $x->{sign})
    {
    # special case of $x /= $x results in 1
    $x->bone();			# "fixes" also sign of $y, since $x is $y
    }
  else
    {
    # correct $y's sign again
    $y->{sign} =~ tr/+-/-+/;
    # continue with normal div code:

    # make copy of $x in case of list context for later reminder calculation
    if (wantarray && $y_not_one)
      {
      $rem = $x->copy();
      }

    $x->{sign} = $x->{sign} ne $y->sign() ? '-' : '+'; 

    # check for / +-1 ( +/- 1E0)
    if ($y_not_one)
      {
      # promote BigInts and it's subclasses (except when already a BigFloat)
      $y = $self->new($y) unless $y->isa('Math::BigFloat'); 

      # calculate the result to $scale digits and then round it
      # a * 10 ** b / c * 10 ** d => a/c * 10 ** (b-d)
      $MBI->_lsft($x->{_m},$MBI->_new($scale),10);
      $MBI->_div ($x->{_m},$y->{_m});	# a/c

      # correct exponent of $x
      ($x->{_e},$x->{_es}) = _e_sub($x->{_e}, $y->{_e}, $x->{_es}, $y->{_es});
      # correct for 10**scale
      ($x->{_e},$x->{_es}) = _e_sub($x->{_e}, $MBI->_new($scale), $x->{_es}, '+');
      $x->bnorm();		# remove trailing 0's
      }
    } # ende else $x != $y

  # shortcut to not run through _find_round_parameters again
  if (defined $params[0])
    {
    delete $x->{_a}; 				# clear before round
    $x->bround($params[0],$params[2]);		# then round accordingly
    }
  else
    {
    delete $x->{_p}; 				# clear before round
    $x->bfround($params[1],$params[2]);		# then round accordingly
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    delete $x->{_a}; delete $x->{_p};
    }

  if (wantarray)
    {
    if ($y_not_one)
      {
      $rem->bmod($y,@params);			# copy already done
      }
    if ($fallback)
      {
      # clear a/p after round, since user did not request it
      delete $rem->{_a}; delete $rem->{_p};
      }
    return ($x,$rem);
    }
  $x;
  }

sub bmod 
  {
  # (dividend: BFLOAT or num_str, divisor: BFLOAT or num_str) return reminder 

  # set up parameters
  my ($self,$x,$y,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$a,$p,$r) = objectify(2,@_);
    }

  return $x if $x->modify('bmod');

  # handle NaN, inf, -inf
  if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/))
    {
    my ($d,$re) = $self->SUPER::_div_inf($x,$y);
    $x->{sign} = $re->{sign};
    $x->{_e} = $re->{_e};
    $x->{_m} = $re->{_m};
    return $x->round($a,$p,$r,$y);
    } 
  if ($y->is_zero())
    {
    return $x->bnan() if $x->is_zero();
    return $x;
    }

  return $x->bzero() if $x->is_zero()
 || ($x->is_int() &&
  # check that $y == +1 or $y == -1:
    ($MBI->_is_zero($y->{_e}) && $MBI->_is_one($y->{_m})));

  my $cmp = $x->bacmp($y);			# equal or $x < $y?
  return $x->bzero($a,$p) if $cmp == 0;		# $x == $y => result 0

  # only $y of the operands negative? 
  my $neg = 0; $neg = 1 if $x->{sign} ne $y->{sign};

  $x->{sign} = $y->{sign};				# calc sign first
  return $x->round($a,$p,$r) if $cmp < 0 && $neg == 0;	# $x < $y => result $x
  
  my $ym = $MBI->_copy($y->{_m});
  
  # 2e1 => 20
  $MBI->_lsft( $ym, $y->{_e}, 10) 
   if $y->{_es} eq '+' && !$MBI->_is_zero($y->{_e});
 
  # if $y has digits after dot
  my $shifty = 0;			# correct _e of $x by this
  if ($y->{_es} eq '-')			# has digits after dot
    {
    # 123 % 2.5 => 1230 % 25 => 5 => 0.5
    $shifty = $MBI->_num($y->{_e}); 	# no more digits after dot
    $MBI->_lsft($x->{_m}, $y->{_e}, 10);# 123 => 1230, $y->{_m} is already 25
    }
  # $ym is now mantissa of $y based on exponent 0

  my $shiftx = 0;			# correct _e of $x by this
  if ($x->{_es} eq '-')			# has digits after dot
    {
    # 123.4 % 20 => 1234 % 200
    $shiftx = $MBI->_num($x->{_e});	# no more digits after dot
    $MBI->_lsft($ym, $x->{_e}, 10);	# 123 => 1230
    }
  # 123e1 % 20 => 1230 % 20
  if ($x->{_es} eq '+' && !$MBI->_is_zero($x->{_e}))
    {
    $MBI->_lsft( $x->{_m}, $x->{_e},10);	# es => '+' here
    }

  $x->{_e} = $MBI->_new($shiftx);
  $x->{_es} = '+'; 
  $x->{_es} = '-' if $shiftx != 0 || $shifty != 0;
  $MBI->_add( $x->{_e}, $MBI->_new($shifty)) if $shifty != 0;
  
  # now mantissas are equalized, exponent of $x is adjusted, so calc result

  $x->{_m} = $MBI->_mod( $x->{_m}, $ym);

  $x->{sign} = '+' if $MBI->_is_zero($x->{_m});		# fix sign for -0
  $x->bnorm();

  if ($neg != 0)	# one of them negative => correct in place
    {
    my $r = $y - $x;
    $x->{_m} = $r->{_m};
    $x->{_e} = $r->{_e};
    $x->{_es} = $r->{_es};
    $x->{sign} = '+' if $MBI->_is_zero($x->{_m});	# fix sign for -0
    $x->bnorm();
    }

  $x->round($a,$p,$r,$y);	# round and return
  }

sub broot
  {
  # calculate $y'th root of $x
  
  # set up parameters
  my ($self,$x,$y,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$a,$p,$r) = objectify(2,@_);
    }

  return $x if $x->modify('broot');

  # NaN handling: $x ** 1/0, x or y NaN, or y inf/-inf or y == 0
  return $x->bnan() if $x->{sign} !~ /^\+/ || $y->is_zero() ||
         $y->{sign} !~ /^\+$/;

  return $x if $x->is_zero() || $x->is_one() || $x->is_inf() || $y->is_one();
  
  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my (@params,$scale);
  ($x,@params) = $x->_find_round_parameters($a,$p,$r);

  return $x if $x->is_nan();		# error in _find_round_parameters?

  # no rounding at all, so must use fallback
  if (scalar @params == 0) 
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r;			# iound mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4; # take whatever is defined
    }

  # when user set globals, they would interfere with our calculation, so
  # disable them and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;	# should be really parent class vs MBI

  # remember sign and make $x positive, since -4 ** (1/2) => -2
  my $sign = 0; $sign = 1 if $x->{sign} eq '-'; $x->{sign} = '+';

  my $is_two = 0;
  if ($y->isa('Math::BigFloat'))
    {
    $is_two = ($y->{sign} eq '+' && $MBI->_is_two($y->{_m}) && $MBI->_is_zero($y->{_e}));
    }
  else
    {
    $is_two = ($y == 2);
    }

  # normal square root if $y == 2:
  if ($is_two)
    {
    $x->bsqrt($scale+4);
    }
  elsif ($y->is_one('-'))
    {
    # $x ** -1 => 1/$x
    my $u = $self->bone()->bdiv($x,$scale);
    # copy private parts over
    $x->{_m} = $u->{_m};
    $x->{_e} = $u->{_e};
    $x->{_es} = $u->{_es};
    }
  else
    {
    # calculate the broot() as integer result first, and if it fits, return
    # it rightaway (but only if $x and $y are integer):

    my $done = 0;				# not yet
    if ($y->is_int() && $x->is_int())
      {
      my $i = $MBI->_copy( $x->{_m} );
      $MBI->_lsft( $i, $x->{_e}, 10 ) unless $MBI->_is_zero($x->{_e});
      my $int = Math::BigInt->bzero();
      $int->{value} = $i;
      $int->broot($y->as_number());
      # if ($exact)
      if ($int->copy()->bpow($y) == $x)
        {
        # found result, return it
        $x->{_m} = $int->{value};
        $x->{_e} = $MBI->_zero();
        $x->{_es} = '+';
        $x->bnorm();
        $done = 1;
        }
      }
    if ($done == 0)
      {
      my $u = $self->bone()->bdiv($y,$scale+4);
      delete $u->{_a}; delete $u->{_p};         # otherwise it conflicts
      $x->bpow($u,$scale+4);                    # el cheapo
      }
    }
  $x->bneg() if $sign == 1;
  
  # shortcut to not run through _find_round_parameters again
  if (defined $params[0])
    {
    $x->bround($params[0],$params[2]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[1],$params[2]);		# then round accordingly
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    delete $x->{_a}; delete $x->{_p};
    }
  # restore globals
  $$abr = $ab; $$pbr = $pb;
  $x;
  }

sub bsqrt
  { 
  # calculate square root
  my ($self,$x,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x if $x->modify('bsqrt');

  return $x->bnan() if $x->{sign} !~ /^[+]/;	# NaN, -inf or < 0
  return $x if $x->{sign} eq '+inf';		# sqrt(inf) == inf
  return $x->round($a,$p,$r) if $x->is_zero() || $x->is_one();

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my (@params,$scale);
  ($x,@params) = $x->_find_round_parameters($a,$p,$r);

  return $x if $x->is_nan();		# error in _find_round_parameters?

  # no rounding at all, so must use fallback
  if (scalar @params == 0) 
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4; # take whatever is defined
    }

  # when user set globals, they would interfere with our calculation, so
  # disable them and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;	# should be really parent class vs MBI

  my $i = $MBI->_copy( $x->{_m} );
  $MBI->_lsft( $i, $x->{_e}, 10 ) unless $MBI->_is_zero($x->{_e});
  my $xas = Math::BigInt->bzero();
  $xas->{value} = $i;

  my $gs = $xas->copy()->bsqrt();	# some guess

  if (($x->{_es} ne '-')		# guess can't be accurate if there are
					# digits after the dot
   && ($xas->bacmp($gs * $gs) == 0))	# guess hit the nail on the head?
    {
    # exact result, copy result over to keep $x
    $x->{_m} = $gs->{value}; $x->{_e} = $MBI->_zero(); $x->{_es} = '+';
    $x->bnorm();
    # shortcut to not run through _find_round_parameters again
    if (defined $params[0])
      {
      $x->bround($params[0],$params[2]);	# then round accordingly
      }
    else
      {
      $x->bfround($params[1],$params[2]);	# then round accordingly
      }
    if ($fallback)
      {
      # clear a/p after round, since user did not request it
      delete $x->{_a}; delete $x->{_p};
      }
    # re-enable A and P, upgrade is taken care of by "local"
    ${"$self\::accuracy"} = $ab; ${"$self\::precision"} = $pb;
    return $x;
    }
 
  # sqrt(2) = 1.4 because sqrt(2*100) = 1.4*10; so we can increase the accuracy
  # of the result by multipyling the input by 100 and then divide the integer
  # result of sqrt(input) by 10. Rounding afterwards returns the real result.

  # The following steps will transform 123.456 (in $x) into 123456 (in $y1)
  my $y1 = $MBI->_copy($x->{_m});

  my $length = $MBI->_len($y1);
  
  # Now calculate how many digits the result of sqrt(y1) would have
  my $digits = int($length / 2);

  # But we need at least $scale digits, so calculate how many are missing
  my $shift = $scale - $digits;

  # This happens if the input had enough digits
  # (we take care of integer guesses above)
  $shift = 0 if $shift < 0; 

  # Multiply in steps of 100, by shifting left two times the "missing" digits
  my $s2 = $shift * 2;

  # We now make sure that $y1 has the same odd or even number of digits than
  # $x had. So when _e of $x is odd, we must shift $y1 by one digit left,
  # because we always must multiply by steps of 100 (sqrt(100) is 10) and not
  # steps of 10. The length of $x does not count, since an even or odd number
  # of digits before the dot is not changed by adding an even number of digits
  # after the dot (the result is still odd or even digits long).
  $s2++ if $MBI->_is_odd($x->{_e});

  $MBI->_lsft( $y1, $MBI->_new($s2), 10);

  # now take the square root and truncate to integer
  $y1 = $MBI->_sqrt($y1);

  # By "shifting" $y1 right (by creating a negative _e) we calculate the final
  # result, which is than later rounded to the desired scale.

  # calculate how many zeros $x had after the '.' (or before it, depending
  # on sign of $dat, the result should have half as many:
  my $dat = $MBI->_num($x->{_e});
  $dat = -$dat if $x->{_es} eq '-';
  $dat += $length;

  if ($dat > 0)
    {
    # no zeros after the dot (e.g. 1.23, 0.49 etc)
    # preserve half as many digits before the dot than the input had 
    # (but round this "up")
    $dat = int(($dat+1)/2);
    }
  else
    {
    $dat = int(($dat)/2);
    }
  $dat -= $MBI->_len($y1);
  if ($dat < 0)
    {
    $dat = abs($dat);
    $x->{_e} = $MBI->_new( $dat );
    $x->{_es} = '-';
    }
  else
    {    
    $x->{_e} = $MBI->_new( $dat );
    $x->{_es} = '+';
    }
  $x->{_m} = $y1;
  $x->bnorm();

  # shortcut to not run through _find_round_parameters again
  if (defined $params[0])
    {
    $x->bround($params[0],$params[2]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[1],$params[2]);		# then round accordingly
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    delete $x->{_a}; delete $x->{_p};
    }
  # restore globals
  $$abr = $ab; $$pbr = $pb;
  $x;
  }

sub bfac
  {
  # (BFLOAT or num_str, BFLOAT or num_str) return BFLOAT
  # compute factorial number, modifies first argument

  # set up parameters
  my ($self,$x,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  ($self,$x,@r) = objectify(1,@_) if !ref($x);

  # inf => inf
  return $x if $x->modify('bfac') || $x->{sign} eq '+inf';	

  return $x->bnan() 
    if (($x->{sign} ne '+') ||		# inf, NaN, <0 etc => NaN
     ($x->{_es} ne '+'));		# digits after dot?

  # use BigInt's bfac() for faster calc
  if (! $MBI->_is_zero($x->{_e}))
    {
    $MBI->_lsft($x->{_m}, $x->{_e},10);	# change 12e1 to 120e0
    $x->{_e} = $MBI->_zero();		# normalize
    $x->{_es} = '+';
    }
  $MBI->_fac($x->{_m});			# calculate factorial
  $x->bnorm()->round(@r); 		# norm again and round result
  }

sub _pow
  {
  # Calculate a power where $y is a non-integer, like 2 ** 0.3
  my ($x,$y,@r) = @_;
  my $self = ref($x);

  # if $y == 0.5, it is sqrt($x)
  $HALF = $self->new($HALF) unless ref($HALF);
  return $x->bsqrt(@r,$y) if $y->bcmp($HALF) == 0;

  # Using:
  # a ** x == e ** (x * ln a)

  # u = y * ln x
  #                _                         _
  # Taylor:       |   u    u^2    u^3         |
  # x ** y  = 1 + |  --- + --- + ----- + ...  |
  #               |_  1    1*2   1*2*3       _|

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my ($scale,@params);
  ($x,@params) = $x->_find_round_parameters(@r);
    
  return $x if $x->is_nan();		# error in _find_round_parameters?

  # no rounding at all, so must use fallback
  if (scalar @params == 0)
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $params[1] = undef;			# disable P
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r[2];			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4; # take whatever is defined
    }

  # when user set globals, they would interfere with our calculation, so
  # disable them and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;
 
  my ($limit,$v,$u,$below,$factor,$next,$over);

  $u = $x->copy()->blog(undef,$scale)->bmul($y);
  $v = $self->bone();				# 1
  $factor = $self->new(2);			# 2
  $x->bone();					# first term: 1

  $below = $v->copy();
  $over = $u->copy();

  $limit = $self->new("1E-". ($scale-1));
  #my $steps = 0;
  while (3 < 5)
    {
    # we calculate the next term, and add it to the last
    # when the next term is below our limit, it won't affect the outcome
    # anymore, so we stop:
    $next = $over->copy()->bdiv($below,$scale);
    last if $next->bacmp($limit) <= 0;
    $x->badd($next);
    # calculate things for the next term
    $over *= $u; $below *= $factor; $factor->binc();

    last if $x->{sign} !~ /^[-+]$/;

    #$steps++;
    }
  
  # shortcut to not run through _find_round_parameters again
  if (defined $params[0])
    {
    $x->bround($params[0],$params[2]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[1],$params[2]);		# then round accordingly
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    delete $x->{_a}; delete $x->{_p};
    }
  # restore globals
  $$abr = $ab; $$pbr = $pb;
  $x;
  }

sub bpow 
  {
  # (BFLOAT or num_str, BFLOAT or num_str) return BFLOAT
  # compute power of two numbers, second arg is used as integer
  # modifies first argument

  # set up parameters
  my ($self,$x,$y,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$a,$p,$r) = objectify(2,@_);
    }

  return $x if $x->modify('bpow');

  return $x->bnan() if $x->{sign} eq $nan || $y->{sign} eq $nan;
  return $x if $x->{sign} =~ /^[+-]inf$/;
  
  # cache the result of is_zero
  my $y_is_zero = $y->is_zero();
  return $x->bone() if $y_is_zero;
  return $x         if $x->is_one() || $y->is_one();

  my $x_is_zero = $x->is_zero();
  return $x->_pow($y,$a,$p,$r) if !$x_is_zero && !$y->is_int();		# non-integer power

  my $y1 = $y->as_number()->{value};			# make MBI part

  # if ($x == -1)
  if ($x->{sign} eq '-' && $MBI->_is_one($x->{_m}) && $MBI->_is_zero($x->{_e}))
    {
    # if $x == -1 and odd/even y => +1/-1  because +-1 ^ (+-1) => +-1
    return $MBI->_is_odd($y1) ? $x : $x->babs(1);
    }
  if ($x_is_zero)
    {
    return $x if $y->{sign} eq '+'; 	# 0**y => 0 (if not y <= 0)
    # 0 ** -y => 1 / (0 ** y) => 1 / 0! (1 / 0 => +inf)
    return $x->binf();
    }

  my $new_sign = '+';
  $new_sign = $MBI->_is_odd($y1) ? '-' : '+' if $x->{sign} ne '+';

  # calculate $x->{_m} ** $y and $x->{_e} * $y separately (faster)
  $x->{_m} = $MBI->_pow( $x->{_m}, $y1);
  $x->{_e} = $MBI->_mul ($x->{_e}, $y1);

  $x->{sign} = $new_sign;
  $x->bnorm();
  if ($y->{sign} eq '-')
    {
    # modify $x in place!
    my $z = $x->copy(); $x->bone();
    return scalar $x->bdiv($z,$a,$p,$r);	# round in one go (might ignore y's A!)
    }
  $x->round($a,$p,$r,$y);
  }

sub bmodpow
  {
  # takes a very large number to a very large exponent in a given very
  # large modulus, quickly, thanks to binary exponentation. Supports
  # negative exponents.
  my ($self,$num,$exp,$mod,@r) = objectify(3,@_);

  return $num if $num->modify('bmodpow');

  # check modulus for valid values
  return $num->bnan() if ($mod->{sign} ne '+'           # NaN, - , -inf, +inf
                       || $mod->is_zero());

  # check exponent for valid values
  if ($exp->{sign} =~ /\w/)
    {
    # i.e., if it's NaN, +inf, or -inf...
    return $num->bnan();
    }

  $num->bmodinv ($mod) if ($exp->{sign} eq '-');

  # check num for valid values (also NaN if there was no inverse but $exp < 0)
  return $num->bnan() if $num->{sign} !~ /^[+-]$/;

  # $mod is positive, sign on $exp is ignored, result also positive

  # XXX TODO: speed it up when all three numbers are integers
  $num->bpow($exp)->bmod($mod);
  }

###############################################################################
# trigonometric functions

# helper function for bpi() and batan2(), calculates arcus tanges (1/x)

sub _atan_inv
  {
  # return a/b so that a/b approximates atan(1/x) to at least limit digits
  my ($self, $x, $limit) = @_;

  # Taylor:       x^3   x^5   x^7   x^9
  #    atan = x - --- + --- - --- + --- - ...
  #                3     5     7     9 

  #               1      1         1        1
  #    atan 1/x = - - ------- + ------- - ------- + ...
  #               x   x^3 * 3   x^5 * 5   x^7 * 7 

  #               1      1         1            1
  #    atan 1/x = - - --------- + ---------- - ----------- + ... 
  #               5    3 * 125     5 * 3125     7 * 78125

  # Subtraction/addition of a rational:

  #  5    7    5*3 +- 7*4
  #  - +- -  = ----------
  #  4    3       4*3

  # Term:  N        N+1
  #
  #        a             1                  a * d * c +- b
  #        ----- +- ------------------  =  ----------------
  #        b           d * c                b * d * c

  #  since b1 = b0 * (d-2) * c

  #        a             1                  a * d +- b / c
  #        ----- +- ------------------  =  ----------------
  #        b           d * c                b * d 

  # and  d = d + 2
  # and  c = c * x * x

  #        u = d * c
  #        stop if length($u) > limit 
  #        a = a * u +- b
  #        b = b * u
  #        d = d + 2
  #        c = c * x * x
  #        sign = 1 - sign

  my $a = $MBI->_one();
  my $b = $MBI->_copy($x);
 
  my $x2  = $MBI->_mul( $MBI->_copy($x), $b);		# x2 = x * x
  my $d   = $MBI->_new( 3 );				# d = 3
  my $c   = $MBI->_mul( $MBI->_copy($x), $x2);		# c = x ^ 3
  my $two = $MBI->_new( 2 );

  # run the first step unconditionally
  my $u = $MBI->_mul( $MBI->_copy($d), $c);
  $a = $MBI->_mul($a, $u);
  $a = $MBI->_sub($a, $b);
  $b = $MBI->_mul($b, $u);
  $d = $MBI->_add($d, $two);
  $c = $MBI->_mul($c, $x2);

  # a is now a * (d-3) * c
  # b is now b * (d-2) * c

  # run the second step unconditionally
  $u = $MBI->_mul( $MBI->_copy($d), $c);
  $a = $MBI->_mul($a, $u);
  $a = $MBI->_add($a, $b);
  $b = $MBI->_mul($b, $u);
  $d = $MBI->_add($d, $two);
  $c = $MBI->_mul($c, $x2);

  # a is now a * (d-3) * (d-5) * c * c  
  # b is now b * (d-2) * (d-4) * c * c

  # so we can remove c * c from both a and b to shorten the numbers involved:
  $a = $MBI->_div($a, $x2);
  $b = $MBI->_div($b, $x2);
  $a = $MBI->_div($a, $x2);
  $b = $MBI->_div($b, $x2);

#  my $step = 0; 
  my $sign = 0;						# 0 => -, 1 => +
  while (3 < 5)
    {
#    $step++;
#    if (($i++ % 100) == 0)
#      {
#    print "a=",$MBI->_str($a),"\n";
#    print "b=",$MBI->_str($b),"\n";
#      }
#    print "d=",$MBI->_str($d),"\n";
#    print "x2=",$MBI->_str($x2),"\n";
#    print "c=",$MBI->_str($c),"\n";

    my $u = $MBI->_mul( $MBI->_copy($d), $c);
    # use _alen() for libs like GMP where _len() would be O(N^2)
    last if $MBI->_alen($u) > $limit;
    my ($bc,$r) = $MBI->_div( $MBI->_copy($b), $c);
    if ($MBI->_is_zero($r))
      {
      # b / c is an integer, so we can remove c from all terms
      # this happens almost every time:
      $a = $MBI->_mul($a, $d);
      $a = $MBI->_sub($a, $bc) if $sign == 0;
      $a = $MBI->_add($a, $bc) if $sign == 1;
      $b = $MBI->_mul($b, $d);
      }
    else
      {
      # b / c is not an integer, so we keep c in the terms
      # this happens very rarely, for instance for x = 5, this happens only
      # at the following steps:
      # 1, 5, 14, 32, 72, 157, 340, ...
      $a = $MBI->_mul($a, $u);
      $a = $MBI->_sub($a, $b) if $sign == 0;
      $a = $MBI->_add($a, $b) if $sign == 1;
      $b = $MBI->_mul($b, $u);
      }
    $d = $MBI->_add($d, $two);
    $c = $MBI->_mul($c, $x2);
    $sign = 1 - $sign;

    }

#  print "Took $step steps for ", $MBI->_str($x),"\n";
#  print "a=",$MBI->_str($a),"\n"; print "b=",$MBI->_str($b),"\n";
  # return a/b so that a/b approximates atan(1/x)
  ($a,$b);
  }

sub bpi
  {
  my ($self,$n) = @_;
  if (@_ == 0)
    {
    $self = $class;
    }
  if (@_ == 1)
    {
    # called like Math::BigFloat::bpi(10);
    $n = $self; $self = $class;
    # called like Math::BigFloat->bpi();
    $n = undef if $n eq 'Math::BigFloat';
    }
  $self = ref($self) if ref($self);
  my $fallback = defined $n ? 0 : 1;
  $n = 40 if !defined $n || $n < 1;

  # after 黃見利 (Hwang Chien-Lih) (1997)
  # pi/4 = 183 * atan(1/239) + 32 * atan(1/1023) – 68 * atan(1/5832)
  #	 + 12 * atan(1/110443) - 12 * atan(1/4841182) - 100 * atan(1/6826318)

  # a few more to prevent rounding errors
  $n += 4;

  my ($a,$b) = $self->_atan_inv( $MBI->_new(239),$n);
  my ($c,$d) = $self->_atan_inv( $MBI->_new(1023),$n);
  my ($e,$f) = $self->_atan_inv( $MBI->_new(5832),$n);
  my ($g,$h) = $self->_atan_inv( $MBI->_new(110443),$n);
  my ($i,$j) = $self->_atan_inv( $MBI->_new(4841182),$n);
  my ($k,$l) = $self->_atan_inv( $MBI->_new(6826318),$n);

  $MBI->_mul($a, $MBI->_new(732));
  $MBI->_mul($c, $MBI->_new(128));
  $MBI->_mul($e, $MBI->_new(272));
  $MBI->_mul($g, $MBI->_new(48));
  $MBI->_mul($i, $MBI->_new(48));
  $MBI->_mul($k, $MBI->_new(400));

  my $x = $self->bone(); $x->{_m} = $a; my $x_d = $self->bone(); $x_d->{_m} = $b;
  my $y = $self->bone(); $y->{_m} = $c; my $y_d = $self->bone(); $y_d->{_m} = $d;
  my $z = $self->bone(); $z->{_m} = $e; my $z_d = $self->bone(); $z_d->{_m} = $f;
  my $u = $self->bone(); $u->{_m} = $g; my $u_d = $self->bone(); $u_d->{_m} = $h;
  my $v = $self->bone(); $v->{_m} = $i; my $v_d = $self->bone(); $v_d->{_m} = $j;
  my $w = $self->bone(); $w->{_m} = $k; my $w_d = $self->bone(); $w_d->{_m} = $l;
  $x->bdiv($x_d, $n);
  $y->bdiv($y_d, $n);
  $z->bdiv($z_d, $n);
  $u->bdiv($u_d, $n);
  $v->bdiv($v_d, $n);
  $w->bdiv($w_d, $n);

  delete $x->{_a}; delete $y->{_a}; delete $z->{_a};
  delete $u->{_a}; delete $v->{_a}; delete $w->{_a};
  $x->badd($y)->bsub($z)->badd($u)->bsub($v)->bsub($w);

  $x->bround($n-4);
  delete $x->{_a} if $fallback == 1;
  $x;
  }

sub bcos
  {
  # Calculate a cosinus of x.
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  # Taylor:      x^2   x^4   x^6   x^8
  #    cos = 1 - --- + --- - --- + --- ...
  #               2!    4!    6!    8!

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my ($scale,@params);
  ($x,@params) = $x->_find_round_parameters(@r);
    
  #         constant object       or error in _find_round_parameters?
  return $x if $x->modify('bcos') || $x->is_nan();

  return $x->bone(@r) if $x->is_zero();

  # no rounding at all, so must use fallback
  if (scalar @params == 0)
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $params[1] = undef;			# disable P
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r[2];			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4; # take whatever is defined
    }

  # when user set globals, they would interfere with our calculation, so
  # disable them and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;
 
  my $last = 0;
  my $over = $x * $x;                   # X ^ 2
  my $x2 = $over->copy();               # X ^ 2; difference between terms
  my $sign = 1;                         # start with -=
  my $below = $self->new(2); my $factorial = $self->new(3);
  $x->bone(); delete $x->{_a}; delete $x->{_p};

  my $limit = $self->new("1E-". ($scale-1));
  #my $steps = 0;
  while (3 < 5)
    {
    # we calculate the next term, and add it to the last
    # when the next term is below our limit, it won't affect the outcome
    # anymore, so we stop:
    my $next = $over->copy()->bdiv($below,$scale);
    last if $next->bacmp($limit) <= 0;

    if ($sign == 0)
      {
      $x->badd($next);
      }
    else
      {
      $x->bsub($next);
      }
    $sign = 1-$sign;					# alternate
    # calculate things for the next term
    $over->bmul($x2);					# $x*$x
    $below->bmul($factorial); $factorial->binc();	# n*(n+1)
    $below->bmul($factorial); $factorial->binc();	# n*(n+1)
    }

  # shortcut to not run through _find_round_parameters again
  if (defined $params[0])
    {
    $x->bround($params[0],$params[2]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[1],$params[2]);		# then round accordingly
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    delete $x->{_a}; delete $x->{_p};
    }
  # restore globals
  $$abr = $ab; $$pbr = $pb;
  $x;
  }

sub bsin
  {
  # Calculate a sinus of x.
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  # taylor:      x^3   x^5   x^7   x^9
  #    sin = x - --- + --- - --- + --- ...
  #               3!    5!    7!    9!

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my ($scale,@params);
  ($x,@params) = $x->_find_round_parameters(@r);
    
  #         constant object       or error in _find_round_parameters?
  return $x if $x->modify('bsin') || $x->is_nan();

  return $x->bzero(@r) if $x->is_zero();

  # no rounding at all, so must use fallback
  if (scalar @params == 0)
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $params[1] = undef;			# disable P
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r[2];			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4; # take whatever is defined
    }

  # when user set globals, they would interfere with our calculation, so
  # disable them and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;
 
  my $last = 0;
  my $over = $x * $x;			# X ^ 2
  my $x2 = $over->copy();		# X ^ 2; difference between terms
  $over->bmul($x);			# X ^ 3 as starting value
  my $sign = 1;				# start with -=
  my $below = $self->new(6); my $factorial = $self->new(4);
  delete $x->{_a}; delete $x->{_p};

  my $limit = $self->new("1E-". ($scale-1));
  #my $steps = 0;
  while (3 < 5)
    {
    # we calculate the next term, and add it to the last
    # when the next term is below our limit, it won't affect the outcome
    # anymore, so we stop:
    my $next = $over->copy()->bdiv($below,$scale);
    last if $next->bacmp($limit) <= 0;

    if ($sign == 0)
      {
      $x->badd($next);
      }
    else
      {
      $x->bsub($next);
      }
    $sign = 1-$sign;					# alternate
    # calculate things for the next term
    $over->bmul($x2);					# $x*$x
    $below->bmul($factorial); $factorial->binc();	# n*(n+1)
    $below->bmul($factorial); $factorial->binc();	# n*(n+1)
    }

  # shortcut to not run through _find_round_parameters again
  if (defined $params[0])
    {
    $x->bround($params[0],$params[2]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[1],$params[2]);		# then round accordingly
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    delete $x->{_a}; delete $x->{_p};
    }
  # restore globals
  $$abr = $ab; $$pbr = $pb;
  $x;
  }

sub batan2
  { 
  # calculate arcus tangens of ($y/$x)
  
  # set up parameters
  my ($self,$y,$x,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$y,$x,@r) = objectify(2,@_);
    }

  return $y if $y->modify('batan2');

  return $y->bnan() if ($y->{sign} eq $nan) || ($x->{sign} eq $nan);

  # Y X
  # 0 0 result is 0
  # 0 +x result is 0
  # ? inf result is 0
  return $y->bzero(@r) if ($x->is_inf('+') && !$y->is_inf()) || ($y->is_zero() && $x->{sign} eq '+');

  # Y    X
  # != 0 -inf result is +- pi
  if ($x->is_inf() || $y->is_inf())
    {
    # calculate PI
    my $pi = $self->bpi(@r);
    if ($y->is_inf())
      {
      # upgrade to BigRat etc. 
      return $upgrade->new($y)->batan2($upgrade->new($x),@r) if defined $upgrade;
      if ($x->{sign} eq '-inf')
        {
        # calculate 3 pi/4
        $MBI->_mul($pi->{_m}, $MBI->_new(3));
        $MBI->_div($pi->{_m}, $MBI->_new(4));
        }
      elsif ($x->{sign} eq '+inf')
	{
        # calculate pi/4
        $MBI->_div($pi->{_m}, $MBI->_new(4));
	}
      else
        {
        # calculate pi/2
        $MBI->_div($pi->{_m}, $MBI->_new(2));
        }
      $y->{sign} = substr($y->{sign},0,1); # keep +/-
      }
    # modify $y in place
    $y->{_m} = $pi->{_m};
    $y->{_e} = $pi->{_e};
    $y->{_es} = $pi->{_es};
    # keep the sign of $y
    return $y;
    }

  return $upgrade->new($y)->batan2($upgrade->new($x),@r) if defined $upgrade;

  # Y X
  # 0 -x result is PI
  if ($y->is_zero())
    {
    # calculate PI
    my $pi = $self->bpi(@r);
    # modify $y in place
    $y->{_m} = $pi->{_m};
    $y->{_e} = $pi->{_e};
    $y->{_es} = $pi->{_es};
    $y->{sign} = '+';
    return $y;
    }

  # Y X
  # +y 0 result is PI/2
  # -y 0 result is -PI/2
  if ($x->is_zero())
    {
    # calculate PI/2
    my $pi = $self->bpi(@r);
    # modify $y in place
    $y->{_m} = $pi->{_m};
    $y->{_e} = $pi->{_e};
    $y->{_es} = $pi->{_es};
    # -y => -PI/2, +y => PI/2
    $MBI->_div($y->{_m}, $MBI->_new(2));
    return $y;
    }

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my ($scale,@params);
  ($y,@params) = $y->_find_round_parameters(@r);
    
  # error in _find_round_parameters?
  return $y if $y->is_nan();

  # no rounding at all, so must use fallback
  if (scalar @params == 0)
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $params[1] = undef;			# disable P
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r[2];			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4; # take whatever is defined
    }

  # inlined is_one() && is_one('-')
  if ($MBI->_is_one($y->{_m}) && $MBI->_is_zero($y->{_e}))
    {
    # shortcut: 1 1 result is PI/4
    # inlined is_one() && is_one('-')
    if ($MBI->_is_one($x->{_m}) && $MBI->_is_zero($x->{_e}))
      {
      # 1,1 => PI/4
      my $pi_4 = $self->bpi( $scale - 3);
      # modify $y in place
      $y->{_m} = $pi_4->{_m};
      $y->{_e} = $pi_4->{_e};
      $y->{_es} = $pi_4->{_es};
      # 1 1 => +
      # -1 1 => -
      # 1 -1 => -
      # -1 -1 => +
      $y->{sign} = $x->{sign} eq $y->{sign} ? '+' : '-';
      $MBI->_div($y->{_m}, $MBI->_new(4));
      return $y;
      }
    # shortcut: 1 int(X) result is _atan_inv(X)

    # is integer
    if ($x->{_es} eq '+')
      {
      my $x1 = $MBI->_copy($x->{_m});
      $MBI->_lsft($x1, $x->{_e},10) unless $MBI->_is_zero($x->{_e});

      my ($a,$b) = $self->_atan_inv($x1, $scale);
      my $y_sign = $y->{sign};
      # calculate A/B
      $y->bone(); $y->{_m} = $a; my $y_d = $self->bone(); $y_d->{_m} = $b;
      $y->bdiv($y_d, @r);
      $y->{sign} = $y_sign;
      return $y;
      }
    }

  # handle all other cases
  #  X  Y
  # +x +y 0 to PI/2
  # -x +y PI/2 to PI
  # +x -y 0 to -PI/2
  # -x -y -PI/2 to -PI 

  my $y_sign = $y->{sign};

  # divide $x by $y
  $y->bdiv($x, $scale) unless $x->is_one();
  $y->batan(@r);

  # restore sign
  $y->{sign} = $y_sign;

  $y;
  }

sub batan
  {
  # Calculate a arcus tangens of x.
  my ($x,@r) = @_;
  my $self = ref($x);

  # taylor:       x^3   x^5   x^7   x^9
  #    atan = x - --- + --- - --- + --- ...
  #                3     5     7     9 

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my ($scale,@params);
  ($x,@params) = $x->_find_round_parameters(@r);
    
  #         constant object       or error in _find_round_parameters?
  return $x if $x->modify('batan') || $x->is_nan();

  if ($x->{sign} =~ /^[+-]inf\z/)
    {
    # +inf result is PI/2
    # -inf result is -PI/2
    # calculate PI/2
    my $pi = $self->bpi(@r);
    # modify $x in place
    $x->{_m} = $pi->{_m};
    $x->{_e} = $pi->{_e};
    $x->{_es} = $pi->{_es};
    # -y => -PI/2, +y => PI/2
    $x->{sign} = substr($x->{sign},0,1);		# +inf => +
    $MBI->_div($x->{_m}, $MBI->_new(2));
    return $x;
    }

  return $x->bzero(@r) if $x->is_zero();

  # no rounding at all, so must use fallback
  if (scalar @params == 0)
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $params[1] = undef;			# disable P
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r[2];			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4; # take whatever is defined
    }

  # 1 or -1 => PI/4
  # inlined is_one() && is_one('-')
  if ($MBI->_is_one($x->{_m}) && $MBI->_is_zero($x->{_e}))
    {
    my $pi = $self->bpi($scale - 3);
    # modify $x in place
    $x->{_m} = $pi->{_m};
    $x->{_e} = $pi->{_e};
    $x->{_es} = $pi->{_es};
    # leave the sign of $x alone (+1 => +PI/4, -1 => -PI/4)
    $MBI->_div($x->{_m}, $MBI->_new(4));
    return $x;
    }
  
  # This series is only valid if -1 < x < 1, so for other x we need to
  # to calculate PI/2 - atan(1/x):
  my $one = $MBI->_new(1);
  my $pi = undef;
  if ($x->{_es} eq '+' && ($MBI->_acmp($x->{_m},$one) >= 0))
    {
    # calculate PI/2
    $pi = $self->bpi($scale - 3);
    $MBI->_div($pi->{_m}, $MBI->_new(2));
    # calculate 1/$x:
    my $x_copy = $x->copy();
    # modify $x in place
    $x->bone(); $x->bdiv($x_copy,$scale);
    }

  # when user set globals, they would interfere with our calculation, so
  # disable them and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;
 
  my $last = 0;
  my $over = $x * $x;			# X ^ 2
  my $x2 = $over->copy();		# X ^ 2; difference between terms
  $over->bmul($x);			# X ^ 3 as starting value
  my $sign = 1;				# start with -=
  my $below = $self->new(3);
  my $two = $self->new(2);
  delete $x->{_a}; delete $x->{_p};

  my $limit = $self->new("1E-". ($scale-1));
  #my $steps = 0;
  while (3 < 5)
    {
    # we calculate the next term, and add it to the last
    # when the next term is below our limit, it won't affect the outcome
    # anymore, so we stop:
    my $next = $over->copy()->bdiv($below,$scale);
    last if $next->bacmp($limit) <= 0;

    if ($sign == 0)
      {
      $x->badd($next);
      }
    else
      {
      $x->bsub($next);
      }
    $sign = 1-$sign;					# alternate
    # calculate things for the next term
    $over->bmul($x2);					# $x*$x
    $below->badd($two);					# n += 2
    }

  if (defined $pi)
    {
    my $x_copy = $x->copy();
    # modify $x in place
    $x->{_m} = $pi->{_m};
    $x->{_e} = $pi->{_e};
    $x->{_es} = $pi->{_es};
    # PI/2 - $x
    $x->bsub($x_copy);
    }

  # shortcut to not run through _find_round_parameters again
  if (defined $params[0])
    {
    $x->bround($params[0],$params[2]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[1],$params[2]);		# then round accordingly
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    delete $x->{_a}; delete $x->{_p};
    }
  # restore globals
  $$abr = $ab; $$pbr = $pb;
  $x;
  }

###############################################################################
# rounding functions

sub bfround
  {
  # precision: round to the $Nth digit left (+$n) or right (-$n) from the '.'
  # $n == 0 means round to integer
  # expects and returns normalized numbers!
  my $x = shift; my $self = ref($x) || $x; $x = $self->new(shift) if !ref($x);

  my ($scale,$mode) = $x->_scale_p(@_);
  return $x if !defined $scale || $x->modify('bfround'); # no-op

  # never round a 0, +-inf, NaN
  if ($x->is_zero())
    {
    $x->{_p} = $scale if !defined $x->{_p} || $x->{_p} < $scale; # -3 < -2
    return $x; 
    }
  return $x if $x->{sign} !~ /^[+-]$/;

  # don't round if x already has lower precision
  return $x if (defined $x->{_p} && $x->{_p} < 0 && $scale < $x->{_p});

  $x->{_p} = $scale;			# remember round in any case
  delete $x->{_a};			# and clear A
  if ($scale < 0)
    {
    # round right from the '.'

    return $x if $x->{_es} eq '+';		# e >= 0 => nothing to round

    $scale = -$scale;				# positive for simplicity
    my $len = $MBI->_len($x->{_m});		# length of mantissa

    # the following poses a restriction on _e, but if _e is bigger than a
    # scalar, you got other problems (memory etc) anyway
    my $dad = -(0+ ($x->{_es}.$MBI->_num($x->{_e})));	# digits after dot
    my $zad = 0;				# zeros after dot
    $zad = $dad - $len if (-$dad < -$len);	# for 0.00..00xxx style
   
    # p rint "scale $scale dad $dad zad $zad len $len\n";
    # number  bsstr   len zad dad	
    # 0.123   123e-3	3   0 3
    # 0.0123  123e-4	3   1 4
    # 0.001   1e-3      1   2 3
    # 1.23    123e-2	3   0 2
    # 1.2345  12345e-4	5   0 4

    # do not round after/right of the $dad
    return $x if $scale > $dad;			# 0.123, scale >= 3 => exit

    # round to zero if rounding inside the $zad, but not for last zero like:
    # 0.0065, scale -2, round last '0' with following '65' (scale == zad case)
    return $x->bzero() if $scale < $zad;
    if ($scale == $zad)			# for 0.006, scale -3 and trunc
      {
      $scale = -$len;
      }
    else
      {
      # adjust round-point to be inside mantissa
      if ($zad != 0)
        {
	$scale = $scale-$zad;
        }
      else
        {
        my $dbd = $len - $dad; $dbd = 0 if $dbd < 0;	# digits before dot
	$scale = $dbd+$scale;
        }
      }
    }
  else
    {
    # round left from the '.'

    # 123 => 100 means length(123) = 3 - $scale (2) => 1

    my $dbt = $MBI->_len($x->{_m}); 
    # digits before dot 
    my $dbd = $dbt + ($x->{_es} . $MBI->_num($x->{_e}));
    # should be the same, so treat it as this 
    $scale = 1 if $scale == 0; 
    # shortcut if already integer 
    return $x if $scale == 1 && $dbt <= $dbd; 
    # maximum digits before dot 
    ++$dbd;

    if ($scale > $dbd) 
       { 
       # not enough digits before dot, so round to zero 
       return $x->bzero; 
       }
    elsif ( $scale == $dbd )
       { 
       # maximum 
       $scale = -$dbt; 
       } 
    else
       { 
       $scale = $dbd - $scale; 
       }
    }
  # pass sign to bround for rounding modes '+inf' and '-inf'
  my $m = bless { sign => $x->{sign}, value => $x->{_m} }, 'Math::BigInt';
  $m->bround($scale,$mode);
  $x->{_m} = $m->{value};			# get our mantissa back
  $x->bnorm();
  }

sub bround
  {
  # accuracy: preserve $N digits, and overwrite the rest with 0's
  my $x = shift; my $self = ref($x) || $x; $x = $self->new(shift) if !ref($x);

  if (($_[0] || 0) < 0)
    {
    require Carp; Carp::croak ('bround() needs positive accuracy');
    }

  my ($scale,$mode) = $x->_scale_a(@_);
  return $x if !defined $scale || $x->modify('bround');	# no-op

  # scale is now either $x->{_a}, $accuracy, or the user parameter
  # test whether $x already has lower accuracy, do nothing in this case 
  # but do round if the accuracy is the same, since a math operation might
  # want to round a number with A=5 to 5 digits afterwards again
  return $x if defined $x->{_a} && $x->{_a} < $scale;

  # scale < 0 makes no sense
  # scale == 0 => keep all digits
  # never round a +-inf, NaN
  return $x if ($scale <= 0) || $x->{sign} !~ /^[+-]$/;

  # 1: never round a 0
  # 2: if we should keep more digits than the mantissa has, do nothing
  if ($x->is_zero() || $MBI->_len($x->{_m}) <= $scale)
    {
    $x->{_a} = $scale if !defined $x->{_a} || $x->{_a} > $scale;
    return $x; 
    }

  # pass sign to bround for '+inf' and '-inf' rounding modes
  my $m = bless { sign => $x->{sign}, value => $x->{_m} }, 'Math::BigInt';

  $m->bround($scale,$mode);		# round mantissa
  $x->{_m} = $m->{value};		# get our mantissa back
  $x->{_a} = $scale;			# remember rounding
  delete $x->{_p};			# and clear P
  $x->bnorm();				# del trailing zeros gen. by bround()
  }

sub bfloor
  {
  # return integer less or equal then $x
  my ($self,$x,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x if $x->modify('bfloor');
   
  return $x if $x->{sign} !~ /^[+-]$/;	# nan, +inf, -inf

  # if $x has digits after dot
  if ($x->{_es} eq '-')
    {
    $x->{_m} = $MBI->_rsft($x->{_m},$x->{_e},10); # cut off digits after dot
    $x->{_e} = $MBI->_zero();			# trunc/norm	
    $x->{_es} = '+';				# abs e
    $MBI->_inc($x->{_m}) if $x->{sign} eq '-';	# increment if negative
    }
  $x->round($a,$p,$r);
  }

sub bceil
  {
  # return integer greater or equal then $x
  my ($self,$x,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x if $x->modify('bceil');
  return $x if $x->{sign} !~ /^[+-]$/;	# nan, +inf, -inf

  # if $x has digits after dot
  if ($x->{_es} eq '-')
    {
    $x->{_m} = $MBI->_rsft($x->{_m},$x->{_e},10); # cut off digits after dot
    $x->{_e} = $MBI->_zero();			# trunc/norm	
    $x->{_es} = '+';				# abs e
    $MBI->_inc($x->{_m}) if $x->{sign} eq '+';	# increment if positive
    }
  $x->round($a,$p,$r);
  }

sub brsft
  {
  # shift right by $y (divide by power of $n)
  
  # set up parameters
  my ($self,$x,$y,$n,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$n,$a,$p,$r) = objectify(2,@_);
    }

  return $x if $x->modify('brsft');
  return $x if $x->{sign} !~ /^[+-]$/;	# nan, +inf, -inf

  $n = 2 if !defined $n; $n = $self->new($n);

  # negative amount?
  return $x->blsft($y->copy()->babs(),$n) if $y->{sign} =~ /^-/;

  # the following call to bdiv() will return either quo or (quo,reminder):
  $x->bdiv($n->bpow($y),$a,$p,$r,$y);
  }

sub blsft
  {
  # shift left by $y (multiply by power of $n)
  
  # set up parameters
  my ($self,$x,$y,$n,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$n,$a,$p,$r) = objectify(2,@_);
    }

  return $x if $x->modify('blsft');
  return $x if $x->{sign} !~ /^[+-]$/;	# nan, +inf, -inf

  $n = 2 if !defined $n; $n = $self->new($n);

  # negative amount?
  return $x->brsft($y->copy()->babs(),$n) if $y->{sign} =~ /^-/;

  $x->bmul($n->bpow($y),$a,$p,$r,$y);
  }

###############################################################################

sub DESTROY
  {
  # going through AUTOLOAD for every DESTROY is costly, avoid it by empty sub
  }

sub AUTOLOAD
  {
  # make fxxx and bxxx both work by selectively mapping fxxx() to MBF::bxxx()
  # or falling back to MBI::bxxx()
  my $name = $AUTOLOAD;

  $name =~ s/(.*):://;	# split package
  my $c = $1 || $class;
  no strict 'refs';
  $c->import() if $IMPORT == 0;
  if (!_method_alias($name))
    {
    if (!defined $name)
      {
      # delayed load of Carp and avoid recursion	
      require Carp;
      Carp::croak ("$c: Can't call a method without name");
      }
    if (!_method_hand_up($name))
      {
      # delayed load of Carp and avoid recursion	
      require Carp;
      Carp::croak ("Can't call $c\-\>$name, not a valid method");
      }
    # try one level up, but subst. bxxx() for fxxx() since MBI only got bxxx()
    $name =~ s/^f/b/;
    return &{"Math::BigInt"."::$name"}(@_);
    }
  my $bname = $name; $bname =~ s/^f/b/;
  $c .= "::$name";
  *{$c} = \&{$bname};
  &{$c};	# uses @_
  }

sub exponent
  {
  # return a copy of the exponent
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)
    {
    my $s = $x->{sign}; $s =~ s/^[+-]//;
    return Math::BigInt->new($s); 		# -inf, +inf => +inf
    }
  Math::BigInt->new( $x->{_es} . $MBI->_str($x->{_e}));
  }

sub mantissa
  {
  # return a copy of the mantissa
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);
 
  if ($x->{sign} !~ /^[+-]$/)
    {
    my $s = $x->{sign}; $s =~ s/^[+]//;
    return Math::BigInt->new($s);		# -inf, +inf => +inf
    }
  my $m = Math::BigInt->new( $MBI->_str($x->{_m}));
  $m->bneg() if $x->{sign} eq '-';

  $m;
  }

sub parts
  {
  # return a copy of both the exponent and the mantissa
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)
    {
    my $s = $x->{sign}; $s =~ s/^[+]//; my $se = $s; $se =~ s/^[-]//;
    return ($self->new($s),$self->new($se)); # +inf => inf and -inf,+inf => inf
    }
  my $m = Math::BigInt->bzero();
  $m->{value} = $MBI->_copy($x->{_m});
  $m->bneg() if $x->{sign} eq '-';
  ($m, Math::BigInt->new( $x->{_es} . $MBI->_num($x->{_e}) ));
  }

##############################################################################
# private stuff (internal use only)

sub import
  {
  my $self = shift;
  my $l = scalar @_;
  my $lib = ''; my @a;
  my $lib_kind = 'try';
  $IMPORT=1;
  for ( my $i = 0; $i < $l ; $i++)
    {
    if ( $_[$i] eq ':constant' )
      {
      # This causes overlord er load to step in. 'binary' and 'integer'
      # are handled by BigInt.
      overload::constant float => sub { $self->new(shift); }; 
      }
    elsif ($_[$i] eq 'upgrade')
      {
      # this causes upgrading
      $upgrade = $_[$i+1];		# or undef to disable
      $i++;
      }
    elsif ($_[$i] eq 'downgrade')
      {
      # this causes downgrading
      $downgrade = $_[$i+1];		# or undef to disable
      $i++;
      }
    elsif ($_[$i] =~ /^(lib|try|only)\z/)
      {
      # alternative library
      $lib = $_[$i+1] || '';		# default Calc
      $lib_kind = $1;			# lib, try or only
      $i++;
      }
    elsif ($_[$i] eq 'with')
      {
      # alternative class for our private parts()
      # XXX: no longer supported
      # $MBI = $_[$i+1] || 'Math::BigInt';
      $i++;
      }
    else
      {
      push @a, $_[$i];
      }
    }

  $lib =~ tr/a-zA-Z0-9,://cd;		# restrict to sane characters
  # let use Math::BigInt lib => 'GMP'; use Math::BigFloat; still work
  my $mbilib = eval { Math::BigInt->config()->{lib} };
  if ((defined $mbilib) && ($MBI eq 'Math::BigInt::Calc'))
    {
    # MBI already loaded
    Math::BigInt->import( $lib_kind, "$lib,$mbilib", 'objectify');
    }
  else
    {
    # MBI not loaded, or with ne "Math::BigInt::Calc"
    $lib .= ",$mbilib" if defined $mbilib;
    $lib =~ s/^,//;				# don't leave empty 
    
    # replacement library can handle lib statement, but also could ignore it
    
    # Perl < 5.6.0 dies with "out of memory!" when eval() and ':constant' is
    # used in the same script, or eval inside import(). So we require MBI:
    require Math::BigInt;
    Math::BigInt->import( $lib_kind => $lib, 'objectify' );
    }
  if ($@)
    {
    require Carp; Carp::croak ("Couldn't load $lib: $! $@");
    }
  # find out which one was actually loaded
  $MBI = Math::BigInt->config()->{lib};

  # register us with MBI to get notified of future lib changes
  Math::BigInt::_register_callback( $self, sub { $MBI = $_[0]; } );

  $self->export_to_level(1,$self,@a);		# export wanted functions
  }

sub bnorm
  {
  # adjust m and e so that m is smallest possible
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return $x if $x->{sign} !~ /^[+-]$/;		# inf, nan etc

  my $zeros = $MBI->_zeros($x->{_m});		# correct for trailing zeros
  if ($zeros != 0)
    {
    my $z = $MBI->_new($zeros);
    $x->{_m} = $MBI->_rsft ($x->{_m}, $z, 10);
    if ($x->{_es} eq '-')
      {
      if ($MBI->_acmp($x->{_e},$z) >= 0)
        {
        $x->{_e} = $MBI->_sub ($x->{_e}, $z);
        $x->{_es} = '+' if $MBI->_is_zero($x->{_e});
        }
      else
        {
        $x->{_e} = $MBI->_sub ( $MBI->_copy($z), $x->{_e});
        $x->{_es} = '+';
        }
      }
    else
      {
      $x->{_e} = $MBI->_add ($x->{_e}, $z);
      }
    }
  else
    {
    # $x can only be 0Ey if there are no trailing zeros ('0' has 0 trailing
    # zeros). So, for something like 0Ey, set y to 1, and -0 => +0
    $x->{sign} = '+', $x->{_es} = '+', $x->{_e} = $MBI->_one()
     if $MBI->_is_zero($x->{_m});
    }

  $x;					# MBI bnorm is no-op, so dont call it
  } 
 
##############################################################################

sub as_hex
  {
  # return number as hexadecimal string (only for integers defined)
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x->bstr() if $x->{sign} !~ /^[+-]$/;  # inf, nan etc
  return '0x0' if $x->is_zero();

  return $nan if $x->{_es} ne '+';		# how to do 1e-1 in hex!?

  my $z = $MBI->_copy($x->{_m});
  if (! $MBI->_is_zero($x->{_e}))		# > 0 
    {
    $MBI->_lsft( $z, $x->{_e},10);
    }
  $z = Math::BigInt->new( $x->{sign} . $MBI->_num($z));
  $z->as_hex();
  }

sub as_bin
  {
  # return number as binary digit string (only for integers defined)
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x->bstr() if $x->{sign} !~ /^[+-]$/;  # inf, nan etc
  return '0b0' if $x->is_zero();

  return $nan if $x->{_es} ne '+';		# how to do 1e-1 in hex!?

  my $z = $MBI->_copy($x->{_m});
  if (! $MBI->_is_zero($x->{_e}))		# > 0 
    {
    $MBI->_lsft( $z, $x->{_e},10);
    }
  $z = Math::BigInt->new( $x->{sign} . $MBI->_num($z));
  $z->as_bin();
  }

sub as_oct
  {
  # return number as octal digit string (only for integers defined)
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x->bstr() if $x->{sign} !~ /^[+-]$/;  # inf, nan etc
  return '0' if $x->is_zero();

  return $nan if $x->{_es} ne '+';		# how to do 1e-1 in hex!?

  my $z = $MBI->_copy($x->{_m});
  if (! $MBI->_is_zero($x->{_e}))		# > 0 
    {
    $MBI->_lsft( $z, $x->{_e},10);
    }
  $z = Math::BigInt->new( $x->{sign} . $MBI->_num($z));
  $z->as_oct();
  }

sub as_number
  {
  # return copy as a bigint representation of this BigFloat number
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x if $x->modify('as_number');

  if (!$x->isa('Math::BigFloat'))
    {
    # if the object can as_number(), use it
    return $x->as_number() if $x->can('as_number');
    # otherwise, get us a float and then a number
    $x = $x->can('as_float') ? $x->as_float() : $self->new(0+"$x");
    }

  my $z = $MBI->_copy($x->{_m});
  if ($x->{_es} eq '-')			# < 0
    {
    $MBI->_rsft( $z, $x->{_e},10);
    } 
  elsif (! $MBI->_is_zero($x->{_e}))	# > 0 
    {
    $MBI->_lsft( $z, $x->{_e},10);
    }
  $z = Math::BigInt->new( $x->{sign} . $MBI->_num($z));
  $z;
  }

sub length
  {
  my $x = shift;
  my $class = ref($x) || $x;
  $x = $class->new(shift) unless ref($x);

  return 1 if $MBI->_is_zero($x->{_m});

  my $len = $MBI->_len($x->{_m});
  $len += $MBI->_num($x->{_e}) if $x->{_es} eq '+';
  if (wantarray())
    {
    my $t = 0;
    $t = $MBI->_num($x->{_e}) if $x->{_es} eq '-';
    return ($len, $t);
    }
  $len;
  }

1;
__END__

=head1 NAME

Math::BigFloat - Arbitrary size floating point math package

=head1 SYNOPSIS

  use Math::BigFloat;

  # Number creation
  my $x = Math::BigFloat->new($str);	# defaults to 0
  my $y = $x->copy();			# make a true copy
  my $nan  = Math::BigFloat->bnan();	# create a NotANumber
  my $zero = Math::BigFloat->bzero();	# create a +0
  my $inf = Math::BigFloat->binf();	# create a +inf
  my $inf = Math::BigFloat->binf('-');	# create a -inf
  my $one = Math::BigFloat->bone();	# create a +1
  my $mone = Math::BigFloat->bone('-');	# create a -1

  my $pi = Math::BigFloat->bpi(100);	# PI to 100 digits

  # the following examples compute their result to 100 digits accuracy:
  my $cos  = Math::BigFloat->new(1)->bcos(100);		# cosinus(1)
  my $sin  = Math::BigFloat->new(1)->bsin(100);		# sinus(1)
  my $atan = Math::BigFloat->new(1)->batan(100);	# arcus tangens(1)

  my $atan2 = Math::BigFloat->new(  1 )->batan2( 1 ,100); # batan(1)
  my $atan2 = Math::BigFloat->new(  1 )->batan2( 8 ,100); # batan(1/8)
  my $atan2 = Math::BigFloat->new( -2 )->batan2( 1 ,100); # batan(-2)

  # Testing
  $x->is_zero();		# true if arg is +0
  $x->is_nan();			# true if arg is NaN
  $x->is_one();			# true if arg is +1
  $x->is_one('-');		# true if arg is -1
  $x->is_odd();			# true if odd, false for even
  $x->is_even();		# true if even, false for odd
  $x->is_pos();			# true if >= 0
  $x->is_neg();			# true if <  0
  $x->is_inf(sign);		# true if +inf, or -inf (default is '+')

  $x->bcmp($y);			# compare numbers (undef,<0,=0,>0)
  $x->bacmp($y);		# compare absolutely (undef,<0,=0,>0)
  $x->sign();			# return the sign, either +,- or NaN
  $x->digit($n);		# return the nth digit, counting from right
  $x->digit(-$n);		# return the nth digit, counting from left 

  # The following all modify their first argument. If you want to preserve
  # $x, use $z = $x->copy()->bXXX($y); See under L<CAVEATS> for why this is
  # necessary when mixing $a = $b assignments with non-overloaded math.
 
  # set 
  $x->bzero();			# set $i to 0
  $x->bnan();			# set $i to NaN
  $x->bone();                   # set $x to +1
  $x->bone('-');                # set $x to -1
  $x->binf();                   # set $x to inf
  $x->binf('-');                # set $x to -inf

  $x->bneg();			# negation
  $x->babs();			# absolute value
  $x->bnorm();			# normalize (no-op)
  $x->bnot();			# two's complement (bit wise not)
  $x->binc();			# increment x by 1
  $x->bdec();			# decrement x by 1
  
  $x->badd($y);			# addition (add $y to $x)
  $x->bsub($y);			# subtraction (subtract $y from $x)
  $x->bmul($y);			# multiplication (multiply $x by $y)
  $x->bdiv($y);			# divide, set $x to quotient
				# return (quo,rem) or quo if scalar

  $x->bmod($y);			# modulus ($x % $y)
  $x->bpow($y);			# power of arguments ($x ** $y)
  $x->bmodpow($exp,$mod);	# modular exponentation (($num**$exp) % $mod))
  $x->blsft($y, $n);		# left shift by $y places in base $n
  $x->brsft($y, $n);		# right shift by $y places in base $n
				# returns (quo,rem) or quo if in scalar context
  
  $x->blog();			# logarithm of $x to base e (Euler's number)
  $x->blog($base);		# logarithm of $x to base $base (f.i. 2)
  $x->bexp();			# calculate e ** $x where e is Euler's number
  
  $x->band($y);			# bit-wise and
  $x->bior($y);			# bit-wise inclusive or
  $x->bxor($y);			# bit-wise exclusive or
  $x->bnot();			# bit-wise not (two's complement)
 
  $x->bsqrt();			# calculate square-root
  $x->broot($y);		# $y'th root of $x (e.g. $y == 3 => cubic root)
  $x->bfac();			# factorial of $x (1*2*3*4*..$x)
 
  $x->bround($N); 		# accuracy: preserve $N digits
  $x->bfround($N);		# precision: round to the $Nth digit

  $x->bfloor();			# return integer less or equal than $x
  $x->bceil();			# return integer greater or equal than $x

  # The following do not modify their arguments:

  bgcd(@values);		# greatest common divisor
  blcm(@values);		# lowest common multiplicator
  
  $x->bstr();			# return string
  $x->bsstr();			# return string in scientific notation

  $x->as_int();			# return $x as BigInt 
  $x->exponent();		# return exponent as BigInt
  $x->mantissa();		# return mantissa as BigInt
  $x->parts();			# return (mantissa,exponent) as BigInt

  $x->length();			# number of digits (w/o sign and '.')
  ($l,$f) = $x->length();	# number of digits, and length of fraction	

  $x->precision();		# return P of $x (or global, if P of $x undef)
  $x->precision($n);		# set P of $x to $n
  $x->accuracy();		# return A of $x (or global, if A of $x undef)
  $x->accuracy($n);		# set A $x to $n

  # these get/set the appropriate global value for all BigFloat objects
  Math::BigFloat->precision();	# Precision
  Math::BigFloat->accuracy();	# Accuracy
  Math::BigFloat->round_mode();	# rounding mode

=head1 DESCRIPTION

All operators (including basic math operations) are overloaded if you
declare your big floating point numbers as

  $i = new Math::BigFloat '12_3.456_789_123_456_789E-2';

Operations with overloaded operators preserve the arguments, which is
exactly what you expect.

=head2 Canonical notation

Input to these routines are either BigFloat objects, or strings of the
following four forms:

=over 2

=item *

C</^[+-]\d+$/>

=item *

C</^[+-]\d+\.\d*$/>

=item *

C</^[+-]\d+E[+-]?\d+$/>

=item *

C</^[+-]\d*\.\d+E[+-]?\d+$/>

=back

all with optional leading and trailing zeros and/or spaces. Additionally,
numbers are allowed to have an underscore between any two digits.

Empty strings as well as other illegal numbers results in 'NaN'.

bnorm() on a BigFloat object is now effectively a no-op, since the numbers 
are always stored in normalized form. On a string, it creates a BigFloat 
object.

=head2 Output

Output values are BigFloat objects (normalized), except for bstr() and bsstr().

The string output will always have leading and trailing zeros stripped and drop
a plus sign. C<bstr()> will give you always the form with a decimal point,
while C<bsstr()> (s for scientific) gives you the scientific notation.

	Input			bstr()		bsstr()
	'-0'			'0'		'0E1'
   	'  -123 123 123'	'-123123123'	'-123123123E0'
	'00.0123'		'0.0123'	'123E-4'
	'123.45E-2'		'1.2345'	'12345E-4'
	'10E+3'			'10000'		'1E4'

Some routines (C<is_odd()>, C<is_even()>, C<is_zero()>, C<is_one()>,
C<is_nan()>) return true or false, while others (C<bcmp()>, C<bacmp()>)
return either undef, <0, 0 or >0 and are suited for sort.

Actual math is done by using the class defined with C<with => Class;> (which
defaults to BigInts) to represent the mantissa and exponent.

The sign C</^[+-]$/> is stored separately. The string 'NaN' is used to 
represent the result when input arguments are not numbers, as well as 
the result of dividing by zero.

=head2 C<mantissa()>, C<exponent()> and C<parts()>

C<mantissa()> and C<exponent()> return the said parts of the BigFloat 
as BigInts such that:

	$m = $x->mantissa();
	$e = $x->exponent();
	$y = $m * ( 10 ** $e );
	print "ok\n" if $x == $y;

C<< ($m,$e) = $x->parts(); >> is just a shortcut giving you both of them.

A zero is represented and returned as C<0E1>, B<not> C<0E0> (after Knuth).

Currently the mantissa is reduced as much as possible, favouring higher
exponents over lower ones (e.g. returning 1e7 instead of 10e6 or 10000000e0).
This might change in the future, so do not depend on it.

=head2 Accuracy vs. Precision

See also: L<Rounding|Rounding>.

Math::BigFloat supports both precision (rounding to a certain place before or
after the dot) and accuracy (rounding to a certain number of digits). For a
full documentation, examples and tips on these topics please see the large
section about rounding in L<Math::BigInt>.

Since things like C<sqrt(2)> or C<1 / 3> must presented with a limited
accuracy lest a operation consumes all resources, each operation produces
no more than the requested number of digits.

If there is no gloabl precision or accuracy set, B<and> the operation in
question was not called with a requested precision or accuracy, B<and> the
input $x has no accuracy or precision set, then a fallback parameter will
be used. For historical reasons, it is called C<div_scale> and can be accessed
via:

	$d = Math::BigFloat->div_scale();		# query
	Math::BigFloat->div_scale($n);			# set to $n digits

The default value for C<div_scale> is 40.

In case the result of one operation has more digits than specified,
it is rounded. The rounding mode taken is either the default mode, or the one
supplied to the operation after the I<scale>:

	$x = Math::BigFloat->new(2);
	Math::BigFloat->accuracy(5);		# 5 digits max
	$y = $x->copy()->bdiv(3);		# will give 0.66667
	$y = $x->copy()->bdiv(3,6);		# will give 0.666667
	$y = $x->copy()->bdiv(3,6,undef,'odd');	# will give 0.666667
	Math::BigFloat->round_mode('zero');
	$y = $x->copy()->bdiv(3,6);		# will also give 0.666667

Note that C<< Math::BigFloat->accuracy() >> and C<< Math::BigFloat->precision() >>
set the global variables, and thus B<any> newly created number will be subject
to the global rounding B<immediately>. This means that in the examples above, the
C<3> as argument to C<bdiv()> will also get an accuracy of B<5>.

It is less confusing to either calculate the result fully, and afterwards
round it explicitly, or use the additional parameters to the math
functions like so:

	use Math::BigFloat;	
	$x = Math::BigFloat->new(2);
	$y = $x->copy()->bdiv(3);
	print $y->bround(5),"\n";		# will give 0.66667

	or

	use Math::BigFloat;	
	$x = Math::BigFloat->new(2);
	$y = $x->copy()->bdiv(3,5);		# will give 0.66667
	print "$y\n";

=head2 Rounding

=over 2

=item ffround ( +$scale )

Rounds to the $scale'th place left from the '.', counting from the dot.
The first digit is numbered 1. 

=item ffround ( -$scale )

Rounds to the $scale'th place right from the '.', counting from the dot.

=item ffround ( 0 )

Rounds to an integer.

=item fround  ( +$scale )

Preserves accuracy to $scale digits from the left (aka significant digits)
and pads the rest with zeros. If the number is between 1 and -1, the
significant digits count from the first non-zero after the '.'

=item fround  ( -$scale ) and fround ( 0 )

These are effectively no-ops.

=back

All rounding functions take as a second parameter a rounding mode from one of
the following: 'even', 'odd', '+inf', '-inf', 'zero', 'trunc' or 'common'.

The default rounding mode is 'even'. By using
C<< Math::BigFloat->round_mode($round_mode); >> you can get and set the default
mode for subsequent rounding. The usage of C<$Math::BigFloat::$round_mode> is
no longer supported.
The second parameter to the round functions then overrides the default
temporarily. 

The C<as_number()> function returns a BigInt from a Math::BigFloat. It uses
'trunc' as rounding mode to make it equivalent to:

	$x = 2.5;
	$y = int($x) + 2;

You can override this by passing the desired rounding mode as parameter to
C<as_number()>:

	$x = Math::BigFloat->new(2.5);
	$y = $x->as_number('odd');	# $y = 3

=head1 METHODS

Math::BigFloat supports all methods that Math::BigInt supports, except it
calculates non-integer results when possible. Please see L<Math::BigInt>
for a full description of each method. Below are just the most important
differences:

=head2 accuracy

        $x->accuracy(5);                # local for $x
        CLASS->accuracy(5);             # global for all members of CLASS
                                        # Note: This also applies to new()!

        $A = $x->accuracy();            # read out accuracy that affects $x
        $A = CLASS->accuracy();         # read out global accuracy

Set or get the global or local accuracy, aka how many significant digits the
results have. If you set a global accuracy, then this also applies to new()!

Warning! The accuracy I<sticks>, e.g. once you created a number under the
influence of C<< CLASS->accuracy($A) >>, all results from math operations with
that number will also be rounded.

In most cases, you should probably round the results explicitly using one of
L<round()>, L<bround()> or L<bfround()> or by passing the desired accuracy
to the math operation as additional parameter:

        my $x = Math::BigInt->new(30000);
        my $y = Math::BigInt->new(7);
        print scalar $x->copy()->bdiv($y, 2);           # print 4300
        print scalar $x->copy()->bdiv($y)->bround(2);   # print 4300

=head2 precision()

        $x->precision(-2);      # local for $x, round at the second digit right of the dot
        $x->precision(2);       # ditto, round at the second digit left of the dot

        CLASS->precision(5);    # Global for all members of CLASS
                                # This also applies to new()!
        CLASS->precision(-5);   # ditto

        $P = CLASS->precision();        # read out global precision
        $P = $x->precision();           # read out precision that affects $x

Note: You probably want to use L<accuracy()> instead. With L<accuracy> you
set the number of digits each result should have, with L<precision> you
set the place where to round!

=head2 bexp()

	$x->bexp($accuracy);		# calculate e ** X

Calculates the expression C<e ** $x> where C<e> is Euler's number.

This method was added in v1.82 of Math::BigInt (April 2007).

=head2 bnok()

	$x->bnok($y);		   # x over y (binomial coefficient n over k)

Calculates the binomial coefficient n over k, also called the "choose"
function. The result is equivalent to:

	( n )      n!
	| - |  = -------
	( k )    k!(n-k)!

This method was added in v1.84 of Math::BigInt (April 2007).

=head2 bpi()

	print Math::BigFloat->bpi(100), "\n";

Calculate PI to N digits (including the 3 before the dot). The result is
rounded according to the current rounding mode, which defaults to "even".

This method was added in v1.87 of Math::BigInt (June 2007).

=head2 bcos()

	my $x = Math::BigFloat->new(1);
	print $x->bcos(100), "\n";

Calculate the cosinus of $x, modifying $x in place.

This method was added in v1.87 of Math::BigInt (June 2007).

=head2 bsin()

	my $x = Math::BigFloat->new(1);
	print $x->bsin(100), "\n";

Calculate the sinus of $x, modifying $x in place.

This method was added in v1.87 of Math::BigInt (June 2007).

=head2 batan2()

	my $y = Math::BigFloat->new(2);
	my $x = Math::BigFloat->new(3);
	print $y->batan2($x), "\n";

Calculate the arcus tanges of C<$y> divided by C<$x>, modifying $y in place.
See also L<batan()>.

This method was added in v1.87 of Math::BigInt (June 2007).

=head2 batan()

	my $x = Math::BigFloat->new(1);
	print $x->batan(100), "\n";

Calculate the arcus tanges of $x, modifying $x in place. See also L<batan2()>.

This method was added in v1.87 of Math::BigInt (June 2007).

=head2 bmuladd()

	$x->bmuladd($y,$z);		

Multiply $x by $y, and then add $z to the result.

This method was added in v1.87 of Math::BigInt (June 2007).

=head1 Autocreating constants

After C<use Math::BigFloat ':constant'> all the floating point constants
in the given scope are converted to C<Math::BigFloat>. This conversion
happens at compile time.

In particular

  perl -MMath::BigFloat=:constant -e 'print 2E-100,"\n"'

prints the value of C<2E-100>. Note that without conversion of 
constants the expression 2E-100 will be calculated as normal floating point 
number.

Please note that ':constant' does not affect integer constants, nor binary 
nor hexadecimal constants. Use L<bignum> or L<Math::BigInt> to get this to
work.

=head2 Math library

Math with the numbers is done (by default) by a module called
Math::BigInt::Calc. This is equivalent to saying:

	use Math::BigFloat lib => 'Calc';

You can change this by using:

	use Math::BigFloat lib => 'GMP';

B<Note>: General purpose packages should not be explicit about the library
to use; let the script author decide which is best.

Note: The keyword 'lib' will warn when the requested library could not be
loaded. To suppress the warning use 'try' instead:

	use Math::BigFloat try => 'GMP';

If your script works with huge numbers and Calc is too slow for them,
you can also for the loading of one of these libraries and if none
of them can be used, the code will die:

        use Math::BigFloat only => 'GMP,Pari';

The following would first try to find Math::BigInt::Foo, then
Math::BigInt::Bar, and when this also fails, revert to Math::BigInt::Calc:

	use Math::BigFloat lib => 'Foo,Math::BigInt::Bar';

See the respective low-level library documentation for further details.

Please note that Math::BigFloat does B<not> use the denoted library itself,
but it merely passes the lib argument to Math::BigInt. So, instead of the need
to do:

	use Math::BigInt lib => 'GMP';
	use Math::BigFloat;

you can roll it all into one line:

	use Math::BigFloat lib => 'GMP';

It is also possible to just require Math::BigFloat:

	require Math::BigFloat;

This will load the necessary things (like BigInt) when they are needed, and
automatically.

See L<Math::BigInt> for more details than you ever wanted to know about using
a different low-level library.

=head2 Using Math::BigInt::Lite

For backwards compatibility reasons it is still possible to
request a different storage class for use with Math::BigFloat:

        use Math::BigFloat with => 'Math::BigInt::Lite';

However, this request is ignored, as the current code now uses the low-level
math libary for directly storing the number parts.

=head1 EXPORTS

C<Math::BigFloat> exports nothing by default, but can export the C<bpi()> method:

	use Math::BigFloat qw/bpi/;

	print bpi(10), "\n";

=head1 BUGS

Please see the file BUGS in the CPAN distribution Math::BigInt for known bugs.

=head1 CAVEATS

Do not try to be clever to insert some operations in between switching
libraries:

	require Math::BigFloat;
	my $matter = Math::BigFloat->bone() + 4;	# load BigInt and Calc
	Math::BigFloat->import( lib => 'Pari' );	# load Pari, too
	my $anti_matter = Math::BigFloat->bone()+4;	# now use Pari

This will create objects with numbers stored in two different backend libraries,
and B<VERY BAD THINGS> will happen when you use these together:

	my $flash_and_bang = $matter + $anti_matter;	# Don't do this!

=over 1

=item stringify, bstr()

Both stringify and bstr() now drop the leading '+'. The old code would return
'+1.23', the new returns '1.23'. See the documentation in L<Math::BigInt> for
reasoning and details.

=item bdiv

The following will probably not print what you expect:

	print $c->bdiv(123.456),"\n";

It prints both quotient and reminder since print works in list context. Also,
bdiv() will modify $c, so be careful. You probably want to use
	
	print $c / 123.456,"\n";
	print scalar $c->bdiv(123.456),"\n";  # or if you want to modify $c

instead.

=item brsft

The following will probably not print what you expect:

	my $c = Math::BigFloat->new('3.14159');
        print $c->brsft(3,10),"\n";	# prints 0.00314153.1415

It prints both quotient and remainder, since print calls C<brsft()> in list
context. Also, C<< $c->brsft() >> will modify $c, so be careful.
You probably want to use

	print scalar $c->copy()->brsft(3,10),"\n";
	# or if you really want to modify $c
        print scalar $c->brsft(3,10),"\n";

instead.

=item Modifying and =

Beware of:

	$x = Math::BigFloat->new(5);
	$y = $x;

It will not do what you think, e.g. making a copy of $x. Instead it just makes
a second reference to the B<same> object and stores it in $y. Thus anything
that modifies $x will modify $y (except overloaded math operators), and vice
versa. See L<Math::BigInt> for details and how to avoid that.

=item bpow

C<bpow()> now modifies the first argument, unlike the old code which left
it alone and only returned the result. This is to be consistent with
C<badd()> etc. The first will modify $x, the second one won't:

	print bpow($x,$i),"\n"; 	# modify $x
	print $x->bpow($i),"\n"; 	# ditto
	print $x ** $i,"\n";		# leave $x alone 

=item precision() vs. accuracy()

A common pitfall is to use L<precision()> when you want to round a result to
a certain number of digits:

	use Math::BigFloat;

	Math::BigFloat->precision(4);		# does not do what you think it does
	my $x = Math::BigFloat->new(12345);	# rounds $x to "12000"!
	print "$x\n";				# print "12000"
	my $y = Math::BigFloat->new(3);		# rounds $y to "0"!
	print "$y\n";				# print "0"
	$z = $x / $y;				# 12000 / 0 => NaN!
	print "$z\n";
	print $z->precision(),"\n";		# 4

Replacing L<precision> with L<accuracy> is probably not what you want, either:

	use Math::BigFloat;

	Math::BigFloat->accuracy(4);		# enables global rounding:
	my $x = Math::BigFloat->new(123456);	# rounded immediately to "12350"
	print "$x\n";				# print "123500"
	my $y = Math::BigFloat->new(3);		# rounded to "3
	print "$y\n";				# print "3"
	print $z = $x->copy()->bdiv($y),"\n";	# 41170
	print $z->accuracy(),"\n";		# 4

What you want to use instead is:

	use Math::BigFloat;

	my $x = Math::BigFloat->new(123456);	# no rounding
	print "$x\n";				# print "123456"
	my $y = Math::BigFloat->new(3);		# no rounding
	print "$y\n";				# print "3"
	print $z = $x->copy()->bdiv($y,4),"\n";	# 41150
	print $z->accuracy(),"\n";		# undef

In addition to computing what you expected, the last example also does B<not>
"taint" the result with an accuracy or precision setting, which would
influence any further operation.

=back

=head1 SEE ALSO

L<Math::BigInt>, L<Math::BigRat> and L<Math::Big> as well as
L<Math::BigInt::BitVect>, L<Math::BigInt::Pari> and  L<Math::BigInt::GMP>.

The pragmas L<bignum>, L<bigint> and L<bigrat> might also be of interest
because they solve the autoupgrading/downgrading issue, at least partly.

The package at L<http://search.cpan.org/~tels/Math-BigInt> contains
more documentation including a full version history, testcases, empty
subclass files and benchmarks.

=head1 LICENSE

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=head1 AUTHORS

Mark Biggar, overloaded interface by Ilya Zakharevich.
Completely rewritten by Tels L<http://bloodgate.com> in 2001 - 2006, and still
at it in 2007.

=cut
