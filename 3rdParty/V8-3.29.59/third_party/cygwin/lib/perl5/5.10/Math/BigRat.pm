
#
# "Tax the rat farms." - Lord Vetinari
#

# The following hash values are used:
#   sign : +,-,NaN,+inf,-inf
#   _d   : denominator
#   _n   : numeraotr (value = _n/_d)
#   _a   : accuracy
#   _p   : precision
# You should not look at the innards of a BigRat - use the methods for this.

package Math::BigRat;

# anythig older is untested, and unlikely to work
use 5.006;
use strict;

use Math::BigFloat;
use vars qw($VERSION @ISA $upgrade $downgrade
            $accuracy $precision $round_mode $div_scale $_trap_nan $_trap_inf);

@ISA = qw(Math::BigFloat);

$VERSION = '0.22';

use overload;			# inherit overload from Math::BigFloat

BEGIN
  { 
  *objectify = \&Math::BigInt::objectify; 	# inherit this from BigInt
  *AUTOLOAD = \&Math::BigFloat::AUTOLOAD;	# can't inherit AUTOLOAD
  # we inherit these from BigFloat because currently it is not possible
  # that MBF has a different $MBI variable than we, because MBF also uses
  # Math::BigInt::config->('lib'); (there is always only one library loaded)
  *_e_add = \&Math::BigFloat::_e_add;
  *_e_sub = \&Math::BigFloat::_e_sub;
  *as_int = \&as_number;
  *is_pos = \&is_positive;
  *is_neg = \&is_negative;
  }

##############################################################################
# Global constants and flags. Access these only via the accessor methods!

$accuracy = $precision = undef;
$round_mode = 'even';
$div_scale = 40;
$upgrade = undef;
$downgrade = undef;

# These are internally, and not to be used from the outside at all!

$_trap_nan = 0;                         # are NaNs ok? set w/ config()
$_trap_inf = 0;                         # are infs ok? set w/ config()

# the package we are using for our private parts, defaults to:
# Math::BigInt->config()->{lib}
my $MBI = 'Math::BigInt::Calc';

my $nan = 'NaN';
my $class = 'Math::BigRat';

sub isa
  {
  return 0 if $_[1] =~ /^Math::Big(Int|Float)/;		# we aren't
  UNIVERSAL::isa(@_);
  }

##############################################################################

sub _new_from_float
  {
  # turn a single float input into a rational number (like '0.1')
  my ($self,$f) = @_;

  return $self->bnan() if $f->is_nan();
  return $self->binf($f->{sign}) if $f->{sign} =~ /^[+-]inf$/;

  $self->{_n} = $MBI->_copy( $f->{_m} );	# mantissa
  $self->{_d} = $MBI->_one();
  $self->{sign} = $f->{sign} || '+';
  if ($f->{_es} eq '-')
    {
    # something like Math::BigRat->new('0.1');
    # 1 / 1 => 1/10
    $MBI->_lsft ( $self->{_d}, $f->{_e} ,10);	
    }
  else
    {
    # something like Math::BigRat->new('10');
    # 1 / 1 => 10/1
    $MBI->_lsft ( $self->{_n}, $f->{_e} ,10) unless 
      $MBI->_is_zero($f->{_e});	
    }
  $self;
  }

sub new
  {
  # create a Math::BigRat
  my $class = shift;

  my ($n,$d) = @_;

  my $self = { }; bless $self,$class;
 
  # input like (BigInt) or (BigFloat):
  if ((!defined $d) && (ref $n) && (!$n->isa('Math::BigRat')))
    {
    if ($n->isa('Math::BigFloat'))
      {
      $self->_new_from_float($n);
      }
    if ($n->isa('Math::BigInt'))
      {
      # TODO: trap NaN, inf
      $self->{_n} = $MBI->_copy($n->{value});		# "mantissa" = N
      $self->{_d} = $MBI->_one();			# d => 1
      $self->{sign} = $n->{sign};
      }
    if ($n->isa('Math::BigInt::Lite'))
      {
      # TODO: trap NaN, inf
      $self->{sign} = '+'; $self->{sign} = '-' if $$n < 0;
      $self->{_n} = $MBI->_new(abs($$n));		# "mantissa" = N
      $self->{_d} = $MBI->_one();			# d => 1
      }
    return $self->bnorm();				# normalize (120/1 => 12/10)
    }

  # input like (BigInt,BigInt) or (BigLite,BigLite):
  if (ref($d) && ref($n))
    {
    # do N first (for $self->{sign}):
    if ($n->isa('Math::BigInt'))
      {
      # TODO: trap NaN, inf
      $self->{_n} = $MBI->_copy($n->{value});		# "mantissa" = N
      $self->{sign} = $n->{sign};
      }
    elsif ($n->isa('Math::BigInt::Lite'))
      {
      # TODO: trap NaN, inf
      $self->{sign} = '+'; $self->{sign} = '-' if $$n < 0;
      $self->{_n} = $MBI->_new(abs($$n));		# "mantissa" = $n
      }
    else
      {
      require Carp;
      Carp::croak(ref($n) . " is not a recognized object format for Math::BigRat->new");
      }
    # now D:
    if ($d->isa('Math::BigInt'))
      {
      # TODO: trap NaN, inf
      $self->{_d} = $MBI->_copy($d->{value});		# "mantissa" = D
      # +/+ or -/- => +, +/- or -/+ => -
      $self->{sign} = $d->{sign} ne $self->{sign} ? '-' : '+';
      }
    elsif ($d->isa('Math::BigInt::Lite'))
      {
      # TODO: trap NaN, inf
      $self->{_d} = $MBI->_new(abs($$d));		# "mantissa" = D
      my $ds = '+'; $ds = '-' if $$d < 0;
      # +/+ or -/- => +, +/- or -/+ => -
      $self->{sign} = $ds ne $self->{sign} ? '-' : '+';
      }
    else
      {
      require Carp;
      Carp::croak(ref($d) . " is not a recognized object format for Math::BigRat->new");
      }
    return $self->bnorm();				# normalize (120/1 => 12/10)
    }
  return $n->copy() if ref $n;				# already a BigRat

  if (!defined $n)
    {
    $self->{_n} = $MBI->_zero();			# undef => 0
    $self->{_d} = $MBI->_one();
    $self->{sign} = '+';
    return $self;
    }

  # string input with / delimiter
  if ($n =~ /\s*\/\s*/)
    {
    return $class->bnan() if $n =~ /\/.*\//;	# 1/2/3 isn't valid
    return $class->bnan() if $n =~ /\/\s*$/;	# 1/ isn't valid
    ($n,$d) = split (/\//,$n);
    # try as BigFloats first
    if (($n =~ /[\.eE]/) || ($d =~ /[\.eE]/))
      {
      local $Math::BigFloat::accuracy = undef;
      local $Math::BigFloat::precision = undef;

      # one of them looks like a float 
      my $nf = Math::BigFloat->new($n,undef,undef);
      $self->{sign} = '+';
      return $self->bnan() if $nf->is_nan();

      $self->{_n} = $MBI->_copy( $nf->{_m} );	# get mantissa

      # now correct $self->{_n} due to $n
      my $f = Math::BigFloat->new($d,undef,undef);
      return $self->bnan() if $f->is_nan();
      $self->{_d} = $MBI->_copy( $f->{_m} );

      # calculate the difference between nE and dE
      my $diff_e = $nf->exponent()->bsub( $f->exponent);
      if ($diff_e->is_negative())
	{
        # < 0: mul d with it
        $MBI->_lsft( $self->{_d}, $MBI->_new( $diff_e->babs()), 10);
	}
      elsif (!$diff_e->is_zero())
        {
        # > 0: mul n with it
        $MBI->_lsft( $self->{_n}, $MBI->_new( $diff_e), 10);
        }
      }
    else
      {
      # both d and n look like (big)ints

      $self->{sign} = '+';					# no sign => '+'
      $self->{_n} = undef;
      $self->{_d} = undef;
      if ($n =~ /^([+-]?)0*([0-9]+)\z/)				# first part ok?
	{
	$self->{sign} = $1 || '+';				# no sign => '+'
	$self->{_n} = $MBI->_new($2 || 0);
        }

      if ($d =~ /^([+-]?)0*([0-9]+)\z/)				# second part ok?
	{
	$self->{sign} =~ tr/+-/-+/ if ($1 || '') eq '-';	# negate if second part neg.
	$self->{_d} = $MBI->_new($2 || 0);
        }

      if (!defined $self->{_n} || !defined $self->{_d})
	{
        $d = Math::BigInt->new($d,undef,undef) unless ref $d;
        $n = Math::BigInt->new($n,undef,undef) unless ref $n;

        if ($n->{sign} =~ /^[+-]$/ && $d->{sign} =~ /^[+-]$/)
	  { 
	  # both parts are ok as integers (wierd things like ' 1e0'
          $self->{_n} = $MBI->_copy($n->{value});
          $self->{_d} = $MBI->_copy($d->{value});
          $self->{sign} = $n->{sign};
          $self->{sign} =~ tr/+-/-+/ if $d->{sign} eq '-';	# -1/-2 => 1/2
          return $self->bnorm();
	  }

        $self->{sign} = '+';					# a default sign
        return $self->bnan() if $n->is_nan() || $d->is_nan();

	# handle inf cases:
        if ($n->is_inf() || $d->is_inf())
	  {
	  if ($n->is_inf())
	    {
	    return $self->bnan() if $d->is_inf();		# both are inf => NaN
	    my $s = '+'; 		# '+inf/+123' or '-inf/-123'
	    $s = '-' if substr($n->{sign},0,1) ne $d->{sign};
	    # +-inf/123 => +-inf
	    return $self->binf($s);
	    }
          # 123/inf => 0
          return $self->bzero();
	  }
	}
      }

    return $self->bnorm();
    }

  # simple string input
  if (($n =~ /[\.eE]/))
    {
    # looks like a float, quacks like a float, so probably is a float
    $self->{sign} = 'NaN';
    local $Math::BigFloat::accuracy = undef;
    local $Math::BigFloat::precision = undef;
    $self->_new_from_float(Math::BigFloat->new($n,undef,undef));
    }
  else
    {
    # for simple forms, use $MBI directly
    if ($n =~ /^([+-]?)0*([0-9]+)\z/)
      {
      $self->{sign} = $1 || '+';
      $self->{_n} = $MBI->_new($2 || 0);
      $self->{_d} = $MBI->_one();
      }
    else
      {
      my $n = Math::BigInt->new($n,undef,undef);
      $self->{_n} = $MBI->_copy($n->{value});
      $self->{_d} = $MBI->_one();
      $self->{sign} = $n->{sign};
      return $self->bnan() if $self->{sign} eq 'NaN';
      return $self->binf($self->{sign}) if $self->{sign} =~ /^[+-]inf$/;
      }
    }
  $self->bnorm();
  }

sub copy
  {
  # if two arguments, the first one is the class to "swallow" subclasses
  my ($c,$x) = @_;

  if (scalar @_ == 1)
    {
    $x = $_[0];
    $c = ref($x);
    }
  return unless ref($x); # only for objects

  my $self = bless {}, $c;

  $self->{sign} = $x->{sign};
  $self->{_d} = $MBI->_copy($x->{_d});
  $self->{_n} = $MBI->_copy($x->{_n});
  $self->{_a} = $x->{_a} if defined $x->{_a};
  $self->{_p} = $x->{_p} if defined $x->{_p};
  $self;
  }

##############################################################################

sub config
  {
  # return (later set?) configuration data as hash ref
  my $class = shift || 'Math::BigRat';

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

sub bstr
  {
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)		# inf, NaN etc
    {
    my $s = $x->{sign}; $s =~ s/^\+//; 	# +inf => inf
    return $s;
    }

  my $s = ''; $s = $x->{sign} if $x->{sign} ne '+';	# '+3/2' => '3/2'

  return $s . $MBI->_str($x->{_n}) if $MBI->_is_one($x->{_d});
  $s . $MBI->_str($x->{_n}) . '/' . $MBI->_str($x->{_d});
  }

sub bsstr
  {
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)		# inf, NaN etc
    {
    my $s = $x->{sign}; $s =~ s/^\+//; 	# +inf => inf
    return $s;
    }
  
  my $s = ''; $s = $x->{sign} if $x->{sign} ne '+';	# +3 vs 3
  $s . $MBI->_str($x->{_n}) . '/' . $MBI->_str($x->{_d});
  }

sub bnorm
  {
  # reduce the number to the shortest form
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  # Both parts must be objects of whatever we are using today.
  if ( my $c = $MBI->_check($x->{_n}) )
    {
    require Carp; Carp::croak ("n did not pass the self-check ($c) in bnorm()");
    }
  if ( my $c = $MBI->_check($x->{_d}) )
    {
    require Carp; Carp::croak ("d did not pass the self-check ($c) in bnorm()");
    }

  # no normalize for NaN, inf etc.
  return $x if $x->{sign} !~ /^[+-]$/;

  # normalize zeros to 0/1
  if ($MBI->_is_zero($x->{_n}))
    {
    $x->{sign} = '+';					# never leave a -0
    $x->{_d} = $MBI->_one() unless $MBI->_is_one($x->{_d});
    return $x;
    }

  return $x if $MBI->_is_one($x->{_d});			# no need to reduce

  # reduce other numbers
  my $gcd = $MBI->_copy($x->{_n});
  $gcd = $MBI->_gcd($gcd,$x->{_d});
  
  if (!$MBI->_is_one($gcd))
    {
    $x->{_n} = $MBI->_div($x->{_n},$gcd);
    $x->{_d} = $MBI->_div($x->{_d},$gcd);
    }
  $x;
  }

##############################################################################
# sign manipulation

sub bneg
  {
  # (BRAT or num_str) return BRAT
  # negate number or make a negated number from string
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return $x if $x->modify('bneg');

  # for +0 dont negate (to have always normalized +0). Does nothing for 'NaN'
  $x->{sign} =~ tr/+-/-+/ unless ($x->{sign} eq '+' && $MBI->_is_zero($x->{_n}));
  $x;
  }

##############################################################################
# special values

sub _bnan
  {
  # used by parent class bnan() to initialize number to NaN
  my $self = shift;

  if ($_trap_nan)
    {
    require Carp;
    my $class = ref($self);
    # "$self" below will stringify the object, this blows up if $self is a
    # partial object (happens under trap_nan), so fix it beforehand
    $self->{_d} = $MBI->_zero() unless defined $self->{_d};
    $self->{_n} = $MBI->_zero() unless defined $self->{_n};
    Carp::croak ("Tried to set $self to NaN in $class\::_bnan()");
    }
  $self->{_n} = $MBI->_zero();
  $self->{_d} = $MBI->_zero();
  }

sub _binf
  {
  # used by parent class bone() to initialize number to +inf/-inf
  my $self = shift;

  if ($_trap_inf)
    {
    require Carp;
    my $class = ref($self);
    # "$self" below will stringify the object, this blows up if $self is a
    # partial object (happens under trap_nan), so fix it beforehand
    $self->{_d} = $MBI->_zero() unless defined $self->{_d};
    $self->{_n} = $MBI->_zero() unless defined $self->{_n};
    Carp::croak ("Tried to set $self to inf in $class\::_binf()");
    }
  $self->{_n} = $MBI->_zero();
  $self->{_d} = $MBI->_zero();
  }

sub _bone
  {
  # used by parent class bone() to initialize number to +1/-1
  my $self = shift;
  $self->{_n} = $MBI->_one();
  $self->{_d} = $MBI->_one();
  }

sub _bzero
  {
  # used by parent class bzero() to initialize number to 0
  my $self = shift;
  $self->{_n} = $MBI->_zero();
  $self->{_d} = $MBI->_one();
  }

##############################################################################
# mul/add/div etc

sub badd
  {
  # add two rational numbers

  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  # +inf + +inf => +inf,  -inf + -inf => -inf
  return $x->binf(substr($x->{sign},0,1))
    if $x->{sign} eq $y->{sign} && $x->{sign} =~ /^[+-]inf$/;

  # +inf + -inf or -inf + +inf => NaN
  return $x->bnan() if ($x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/);

  #  1   1    gcd(3,4) = 1    1*3 + 1*4    7
  #  - + -                  = --------- = --                 
  #  4   3                      4*3       12

  # we do not compute the gcd() here, but simple do:
  #  5   7    5*3 + 7*4   43
  #  - + -  = --------- = --                 
  #  4   3       4*3      12
 
  # and bnorm() will then take care of the rest

  # 5 * 3
  $x->{_n} = $MBI->_mul( $x->{_n}, $y->{_d});

  # 7 * 4
  my $m = $MBI->_mul( $MBI->_copy( $y->{_n} ), $x->{_d} );

  # 5 * 3 + 7 * 4
  ($x->{_n}, $x->{sign}) = _e_add( $x->{_n}, $m, $x->{sign}, $y->{sign});

  # 4 * 3
  $x->{_d} = $MBI->_mul( $x->{_d}, $y->{_d});

  # normalize result, and possible round
  $x->bnorm()->round(@r);
  }

sub bsub
  {
  # subtract two rational numbers

  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  # flip sign of $x, call badd(), then flip sign of result
  $x->{sign} =~ tr/+-/-+/
    unless $x->{sign} eq '+' && $MBI->_is_zero($x->{_n});	# not -0
  $x->badd($y,@r);				# does norm and round
  $x->{sign} =~ tr/+-/-+/ 
    unless $x->{sign} eq '+' && $MBI->_is_zero($x->{_n});	# not -0
  $x;
  }

sub bmul
  {
  # multiply two rational numbers
  
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $x->bnan() if ($x->{sign} eq 'NaN' || $y->{sign} eq 'NaN');

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

  # x== 0 # also: or y == 1 or y == -1
  return wantarray ? ($x,$self->bzero()) : $x if $x->is_zero();

  # XXX TODO:
  # According to Knuth, this can be optimized by doing gcd twice (for d and n)
  # and reducing in one step. This would save us the bnorm() at the end.

  #  1   2    1 * 2    2    1
  #  - * - =  -----  = -  = -
  #  4   3    4 * 3    12   6
  
  $x->{_n} = $MBI->_mul( $x->{_n}, $y->{_n});
  $x->{_d} = $MBI->_mul( $x->{_d}, $y->{_d});

  # compute new sign
  $x->{sign} = $x->{sign} eq $y->{sign} ? '+' : '-';

  $x->bnorm()->round(@r);
  }

sub bdiv
  {
  # (dividend: BRAT or num_str, divisor: BRAT or num_str) return
  # (BRAT,BRAT) (quo,rem) or BRAT (only rem)

  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $self->_div_inf($x,$y)
   if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/) || $y->is_zero());

  # x== 0 # also: or y == 1 or y == -1
  return wantarray ? ($x,$self->bzero()) : $x if $x->is_zero();

  # XXX TODO: list context, upgrade
  # According to Knuth, this can be optimized by doing gcd twice (for d and n)
  # and reducing in one step. This would save us the bnorm() at the end.

  # 1     1    1   3
  # -  /  - == - * -
  # 4     3    4   1
  
  $x->{_n} = $MBI->_mul( $x->{_n}, $y->{_d});
  $x->{_d} = $MBI->_mul( $x->{_d}, $y->{_n});

  # compute new sign 
  $x->{sign} = $x->{sign} eq $y->{sign} ? '+' : '-';

  $x->bnorm()->round(@r);
  $x;
  }

sub bmod
  {
  # compute "remainder" (in Perl way) of $x / $y

  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $self->_div_inf($x,$y)
   if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/) || $y->is_zero());

  return $x if $x->is_zero();           # 0 / 7 = 0, mod 0

  # compute $x - $y * floor($x/$y), keeping the sign of $x

  # copy x to u, make it positive and then do a normal division ($u/$y)
  my $u = bless { sign => '+' }, $self;
  $u->{_n} = $MBI->_mul( $MBI->_copy($x->{_n}), $y->{_d} );
  $u->{_d} = $MBI->_mul( $MBI->_copy($x->{_d}), $y->{_n} );
  
  # compute floor(u)
  if (! $MBI->_is_one($u->{_d}))
    {
    $u->{_n} = $MBI->_div($u->{_n},$u->{_d});	# 22/7 => 3/1 w/ truncate
    # no need to set $u->{_d} to 1, since below we set it to $y->{_d} anyway
    }
  
  # now compute $y * $u
  $u->{_d} = $MBI->_copy($y->{_d});		# 1 * $y->{_d}, see floor above
  $u->{_n} = $MBI->_mul($u->{_n},$y->{_n});

  my $xsign = $x->{sign}; $x->{sign} = '+';	# remember sign and make x positive
  # compute $x - $u
  $x->bsub($u);
  $x->{sign} = $xsign;				# put sign back

  $x->bnorm()->round(@r);
  }

##############################################################################
# bdec/binc

sub bdec
  {
  # decrement value (subtract 1)
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x if $x->{sign} !~ /^[+-]$/;	# NaN, inf, -inf

  if ($x->{sign} eq '-')
    {
    $x->{_n} = $MBI->_add( $x->{_n}, $x->{_d});		# -5/2 => -7/2
    }
  else
    {
    if ($MBI->_acmp($x->{_n},$x->{_d}) < 0)		# n < d?
      {
      # 1/3 -- => -2/3
      $x->{_n} = $MBI->_sub( $MBI->_copy($x->{_d}), $x->{_n});
      $x->{sign} = '-';
      }
    else
      {
      $x->{_n} = $MBI->_sub($x->{_n}, $x->{_d}); 	# 5/2 => 3/2
      }
    }
  $x->bnorm()->round(@r);
  }

sub binc
  {
  # increment value (add 1)
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);
  
  return $x if $x->{sign} !~ /^[+-]$/;	# NaN, inf, -inf

  if ($x->{sign} eq '-')
    {
    if ($MBI->_acmp($x->{_n},$x->{_d}) < 0)
      {
      # -1/3 ++ => 2/3 (overflow at 0)
      $x->{_n} = $MBI->_sub( $MBI->_copy($x->{_d}), $x->{_n});
      $x->{sign} = '+';
      }
    else
      {
      $x->{_n} = $MBI->_sub($x->{_n}, $x->{_d}); 	# -5/2 => -3/2
      }
    }
  else
    {
    $x->{_n} = $MBI->_add($x->{_n},$x->{_d});		# 5/2 => 7/2
    }
  $x->bnorm()->round(@r);
  }

##############################################################################
# is_foo methods (the rest is inherited)

sub is_int
  {
  # return true if arg (BRAT or num_str) is an integer
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 1 if ($x->{sign} =~ /^[+-]$/) &&	# NaN and +-inf aren't
    $MBI->_is_one($x->{_d});			# x/y && y != 1 => no integer
  0;
  }

sub is_zero
  {
  # return true if arg (BRAT or num_str) is zero
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 1 if $x->{sign} eq '+' && $MBI->_is_zero($x->{_n});
  0;
  }

sub is_one
  {
  # return true if arg (BRAT or num_str) is +1 or -1 if signis given
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  my $sign = $_[2] || ''; $sign = '+' if $sign ne '-';
  return 1
   if ($x->{sign} eq $sign && $MBI->_is_one($x->{_n}) && $MBI->_is_one($x->{_d}));
  0;
  }

sub is_odd
  {
  # return true if arg (BFLOAT or num_str) is odd or false if even
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 1 if ($x->{sign} =~ /^[+-]$/) &&		# NaN & +-inf aren't
    ($MBI->_is_one($x->{_d}) && $MBI->_is_odd($x->{_n})); # x/2 is not, but 3/1
  0;
  }

sub is_even
  {
  # return true if arg (BINT or num_str) is even or false if odd
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 0 if $x->{sign} !~ /^[+-]$/;			# NaN & +-inf aren't
  return 1 if ($MBI->_is_one($x->{_d})			# x/3 is never
     && $MBI->_is_even($x->{_n}));			# but 4/1 is
  0;
  }

##############################################################################
# parts() and friends

sub numerator
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  # NaN, inf, -inf
  return Math::BigInt->new($x->{sign}) if ($x->{sign} !~ /^[+-]$/);

  my $n = Math::BigInt->new($MBI->_str($x->{_n})); $n->{sign} = $x->{sign};
  $n;
  }

sub denominator
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  # NaN
  return Math::BigInt->new($x->{sign}) if $x->{sign} eq 'NaN';
  # inf, -inf
  return Math::BigInt->bone() if $x->{sign} !~ /^[+-]$/;
  
  Math::BigInt->new($MBI->_str($x->{_d}));
  }

sub parts
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  my $c = 'Math::BigInt';

  return ($c->bnan(),$c->bnan()) if $x->{sign} eq 'NaN';
  return ($c->binf(),$c->binf()) if $x->{sign} eq '+inf';
  return ($c->binf('-'),$c->binf()) if $x->{sign} eq '-inf';

  my $n = $c->new( $MBI->_str($x->{_n}));
  $n->{sign} = $x->{sign};
  my $d = $c->new( $MBI->_str($x->{_d}));
  ($n,$d);
  }

sub length
  {
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return $nan unless $x->is_int();
  $MBI->_len($x->{_n});				# length(-123/1) => length(123)
  }

sub digit
  {
  my ($self,$x,$n) = ref($_[0]) ? (undef,$_[0],$_[1]) : objectify(1,@_);

  return $nan unless $x->is_int();
  $MBI->_digit($x->{_n},$n || 0);		# digit(-123/1,2) => digit(123,2)
  }

##############################################################################
# special calc routines

sub bceil
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x if $x->{sign} !~ /^[+-]$/ ||	# not for NaN, inf
            $MBI->_is_one($x->{_d});		# 22/1 => 22, 0/1 => 0

  $x->{_n} = $MBI->_div($x->{_n},$x->{_d});	# 22/7 => 3/1 w/ truncate
  $x->{_d} = $MBI->_one();			# d => 1
  $x->{_n} = $MBI->_inc($x->{_n})
    if $x->{sign} eq '+';			# +22/7 => 4/1
  $x->{sign} = '+' if $MBI->_is_zero($x->{_n});	# -0 => 0
  $x;
  }

sub bfloor
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x if $x->{sign} !~ /^[+-]$/ ||	# not for NaN, inf
            $MBI->_is_one($x->{_d});		# 22/1 => 22, 0/1 => 0

  $x->{_n} = $MBI->_div($x->{_n},$x->{_d});	# 22/7 => 3/1 w/ truncate
  $x->{_d} = $MBI->_one();			# d => 1
  $x->{_n} = $MBI->_inc($x->{_n})
    if $x->{sign} eq '-';			# -22/7 => -4/1
  $x;
  }

sub bfac
  {
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  # if $x is not an integer
  if (($x->{sign} ne '+') || (!$MBI->_is_one($x->{_d})))
    {
    return $x->bnan();
    }

  $x->{_n} = $MBI->_fac($x->{_n});
  # since _d is 1, we don't need to reduce/norm the result
  $x->round(@r);
  }

sub bpow
  {
  # power ($x ** $y)

  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $x if $x->{sign} =~ /^[+-]inf$/;       # -inf/+inf ** x
  return $x->bnan() if $x->{sign} eq $nan || $y->{sign} eq $nan;
  return $x->bone(@r) if $y->is_zero();
  return $x->round(@r) if $x->is_one() || $y->is_one();

  if ($x->{sign} eq '-' && $MBI->_is_one($x->{_n}) && $MBI->_is_one($x->{_d}))
    {
    # if $x == -1 and odd/even y => +1/-1
    return $y->is_odd() ? $x->round(@r) : $x->babs()->round(@r);
    # my Casio FX-5500L has a bug here: -1 ** 2 is -1, but -1 * -1 is 1;
    }
  # 1 ** -y => 1 / (1 ** |y|)
  # so do test for negative $y after above's clause

  return $x->round(@r) if $x->is_zero();  # 0**y => 0 (if not y <= 0)

  # shortcut if y == 1/N (is then sqrt() respective broot())
  if ($MBI->_is_one($y->{_n}))
    {
    return $x->bsqrt(@r) if $MBI->_is_two($y->{_d});	# 1/2 => sqrt
    return $x->broot($MBI->_str($y->{_d}),@r);		# 1/N => root(N)
    }

  # shortcut y/1 (and/or x/1)
  if ($MBI->_is_one($y->{_d}))
    {
    # shortcut for x/1 and y/1
    if ($MBI->_is_one($x->{_d}))
      {
      $x->{_n} = $MBI->_pow($x->{_n},$y->{_n});		# x/1 ** y/1 => (x ** y)/1
      if ($y->{sign} eq '-')
        {
        # 0.2 ** -3 => 1/(0.2 ** 3)
        ($x->{_n},$x->{_d}) = ($x->{_d},$x->{_n});	# swap
        }
      # correct sign; + ** + => +
      if ($x->{sign} eq '-')
        {
        # - * - => +, - * - * - => -
        $x->{sign} = '+' if $MBI->_is_even($y->{_n});	
        }
      return $x->round(@r);
      }
    # x/z ** y/1
    $x->{_n} = $MBI->_pow($x->{_n},$y->{_n});		# 5/2 ** y/1 => 5 ** y / 2 ** y
    $x->{_d} = $MBI->_pow($x->{_d},$y->{_n});
    if ($y->{sign} eq '-')
      {
      # 0.2 ** -3 => 1/(0.2 ** 3)
      ($x->{_n},$x->{_d}) = ($x->{_d},$x->{_n});	# swap
      }
    # correct sign; + ** + => +
    if ($x->{sign} eq '-')
      {
      # - * - => +, - * - * - => -
      $x->{sign} = '+' if $MBI->_is_even($y->{_n});	
      }
    return $x->round(@r);
    }

#  print STDERR "# $x $y\n";

  # otherwise:

  #      n/d     n  ______________
  # a/b       =  -\/  (a/b) ** d

  # (a/b) ** n == (a ** n) / (b ** n)
  $MBI->_pow($x->{_n}, $y->{_n} );
  $MBI->_pow($x->{_d}, $y->{_n} );

  return $x->broot($MBI->_str($y->{_d}),@r);		# n/d => root(n)
  }

sub blog
  {
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);

  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,$class,@_);
    }

  # blog(1,Y) => 0
  return $x->bzero() if $x->is_one() && $y->{sign} eq '+';

  # $x <= 0 => NaN
  return $x->bnan() if $x->is_zero() || $x->{sign} ne '+' || $y->{sign} ne '+';

  if ($x->is_int() && $y->is_int())
    {
    return $self->new($x->as_number()->blog($y->as_number(),@r));
    }

  # do it with floats
  $x->_new_from_float( $x->_as_float()->blog(Math::BigFloat->new("$y"),@r) );
  }

sub bexp
  {
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);

  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,$class,@_);
    }

  return $x->binf(@r) if $x->{sign} eq '+inf';
  return $x->bzero(@r) if $x->{sign} eq '-inf';

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my ($scale,@params);
  ($x,@params) = $x->_find_round_parameters(@r);

  # also takes care of the "error in _find_round_parameters?" case
  return $x if $x->{sign} eq 'NaN';

  # no rounding at all, so must use fallback
  if (scalar @params == 0)
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $params[1] = undef;			# P = undef
    $scale = $params[0]+4;		# at least four more for proper round
    $params[2] = $r[2];			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it's not enough...
    $scale = abs($params[0] || $params[1]) + 4; # take whatever is defined
    }

  return $x->bone(@params) if $x->is_zero();

  # See the comments in Math::BigFloat on how this algorithm works.
  # Basically we calculate A and B (where B is faculty(N)) so that A/B = e

  my $x_org = $x->copy();
  if ($scale <= 75)
    {
    # set $x directly from a cached string form
    $x->{_n} = $MBI->_new("90933395208605785401971970164779391644753259799242");
    $x->{_d} = $MBI->_new("33452526613163807108170062053440751665152000000000");
    $x->{sign} = '+';
    }
  else
    {
    # compute A and B so that e = A / B.

    # After some terms we end up with this, so we use it as a starting point:
    my $A = $MBI->_new("90933395208605785401971970164779391644753259799242");
    my $F = $MBI->_new(42); my $step = 42;

    # Compute how many steps we need to take to get $A and $B sufficiently big
    my $steps = Math::BigFloat::_len_to_steps($scale - 4);
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

    $x->{_n} = $A;
    $x->{_d} = $B;
    $x->{sign} = '+';
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
      $x->bround($params[0],$params[2]);                # then round accordingly
      }
    else
      {
      $x->bfround($params[1],$params[2]);               # then round accordingly
      }
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    delete $x->{_a}; delete $x->{_p};
    }

  $x;
  }

sub bnok
  {
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);

  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,$class,@_);
    }

  # do it with floats
  $x->_new_from_float( $x->_as_float()->bnok(Math::BigFloat->new("$y"),@r) );
  }

sub _float_from_part
  {
  my $x = shift;

  my $f = Math::BigFloat->bzero();
  $f->{_m} = $MBI->_copy($x);
  $f->{_e} = $MBI->_zero();

  $f;
  }

sub _as_float
  {
  my $x = shift;

  local $Math::BigFloat::upgrade = undef;
  local $Math::BigFloat::accuracy = undef;
  local $Math::BigFloat::precision = undef;
  # 22/7 => 3.142857143..

  my $a = $x->accuracy() || 0;
  if ($a != 0 || !$MBI->_is_one($x->{_d}))
    {
    # n/d
    return scalar Math::BigFloat->new($x->{sign} . $MBI->_str($x->{_n}))->bdiv( $MBI->_str($x->{_d}), $x->accuracy());
    }
  # just n
  Math::BigFloat->new($x->{sign} . $MBI->_str($x->{_n}));
  }

sub broot
  {
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  if ($x->is_int() && $y->is_int())
    {
    return $self->new($x->as_number()->broot($y->as_number(),@r));
    }

  # do it with floats
  $x->_new_from_float( $x->_as_float()->broot($y->_as_float(),@r) )->bnorm()->bround(@r);
  }

sub bmodpow
  {
  # set up parameters
  my ($self,$x,$y,$m,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$m,@r) = objectify(3,@_);
    }

  # $x or $y or $m are NaN or +-inf => NaN
  return $x->bnan()
   if $x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/ ||
   $m->{sign} !~ /^[+-]$/;

  if ($x->is_int() && $y->is_int() && $m->is_int())
    {
    return $self->new($x->as_number()->bmodpow($y->as_number(),$m,@r));
    }

  warn ("bmodpow() not fully implemented");
  $x->bnan();
  }

sub bmodinv
  {
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  # $x or $y are NaN or +-inf => NaN
  return $x->bnan() 
   if $x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/;

  if ($x->is_int() && $y->is_int())
    {
    return $self->new($x->as_number()->bmodinv($y->as_number(),@r));
    }

  warn ("bmodinv() not fully implemented");
  $x->bnan();
  }

sub bsqrt
  {
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x->bnan() if $x->{sign} !~ /^[+]/;    # NaN, -inf or < 0
  return $x if $x->{sign} eq '+inf';            # sqrt(inf) == inf
  return $x->round(@r) if $x->is_zero() || $x->is_one();

  local $Math::BigFloat::upgrade = undef;
  local $Math::BigFloat::downgrade = undef;
  local $Math::BigFloat::precision = undef;
  local $Math::BigFloat::accuracy = undef;
  local $Math::BigInt::upgrade = undef;
  local $Math::BigInt::precision = undef;
  local $Math::BigInt::accuracy = undef;

  $x->{_n} = _float_from_part( $x->{_n} )->bsqrt();
  $x->{_d} = _float_from_part( $x->{_d} )->bsqrt();

  # XXX TODO: we probably can optimze this:

  # if sqrt(D) was not integer
  if ($x->{_d}->{_es} ne '+')
    {
    $x->{_n}->blsft($x->{_d}->exponent()->babs(),10);	# 7.1/4.51 => 7.1/45.1
    $x->{_d} = $MBI->_copy( $x->{_d}->{_m} );		# 7.1/45.1 => 71/45.1
    }
  # if sqrt(N) was not integer
  if ($x->{_n}->{_es} ne '+')
    {
    $x->{_d}->blsft($x->{_n}->exponent()->babs(),10);	# 71/45.1 => 710/45.1
    $x->{_n} = $MBI->_copy( $x->{_n}->{_m} );		# 710/45.1 => 710/451
    }

  # convert parts to $MBI again 
  $x->{_n} = $MBI->_lsft( $MBI->_copy( $x->{_n}->{_m} ), $x->{_n}->{_e}, 10)
    if ref($x->{_n}) ne $MBI && ref($x->{_n}) ne 'ARRAY';
  $x->{_d} = $MBI->_lsft( $MBI->_copy( $x->{_d}->{_m} ), $x->{_d}->{_e}, 10)
    if ref($x->{_d}) ne $MBI && ref($x->{_d}) ne 'ARRAY';

  $x->bnorm()->round(@r);
  }

sub blsft
  {
  my ($self,$x,$y,$b,@r) = objectify(3,@_);
 
  $b = 2 unless defined $b;
  $b = $self->new($b) unless ref ($b);
  $x->bmul( $b->copy()->bpow($y), @r);
  $x;
  }

sub brsft
  {
  my ($self,$x,$y,$b,@r) = objectify(3,@_);

  $b = 2 unless defined $b;
  $b = $self->new($b) unless ref ($b);
  $x->bdiv( $b->copy()->bpow($y), @r);
  $x;
  }

##############################################################################
# round

sub round
  {
  $_[0];
  }

sub bround
  {
  $_[0];
  }

sub bfround
  {
  $_[0];
  }

##############################################################################
# comparing

sub bcmp
  {
  # compare two signed numbers 
  
  # set up parameters
  my ($self,$x,$y) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y) = objectify(2,@_);
    }

  if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/))
    {
    # handle +-inf and NaN
    return undef if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));
    return 0 if $x->{sign} eq $y->{sign} && $x->{sign} =~ /^[+-]inf$/;
    return +1 if $x->{sign} eq '+inf';
    return -1 if $x->{sign} eq '-inf';
    return -1 if $y->{sign} eq '+inf';
    return +1;
    }
  # check sign for speed first
  return 1 if $x->{sign} eq '+' && $y->{sign} eq '-';   # does also 0 <=> -y
  return -1 if $x->{sign} eq '-' && $y->{sign} eq '+';  # does also -x <=> 0

  # shortcut
  my $xz = $MBI->_is_zero($x->{_n});
  my $yz = $MBI->_is_zero($y->{_n});
  return 0 if $xz && $yz;                               # 0 <=> 0
  return -1 if $xz && $y->{sign} eq '+';                # 0 <=> +y
  return 1 if $yz && $x->{sign} eq '+';                 # +x <=> 0
 
  my $t = $MBI->_mul( $MBI->_copy($x->{_n}), $y->{_d});
  my $u = $MBI->_mul( $MBI->_copy($y->{_n}), $x->{_d});

  my $cmp = $MBI->_acmp($t,$u);				# signs are equal
  $cmp = -$cmp if $x->{sign} eq '-';			# both are '-' => reverse
  $cmp;
  }

sub bacmp
  {
  # compare two numbers (as unsigned)
 
  # set up parameters
  my ($self,$x,$y) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y) = objectify(2,$class,@_);
    }

  if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/))
    {
    # handle +-inf and NaN
    return undef if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));
    return 0 if $x->{sign} =~ /^[+-]inf$/ && $y->{sign} =~ /^[+-]inf$/;
    return 1 if $x->{sign} =~ /^[+-]inf$/ && $y->{sign} !~ /^[+-]inf$/;
    return -1;
    }

  my $t = $MBI->_mul( $MBI->_copy($x->{_n}), $y->{_d});
  my $u = $MBI->_mul( $MBI->_copy($y->{_n}), $x->{_d});
  $MBI->_acmp($t,$u);					# ignore signs
  }

##############################################################################
# output conversation

sub numify
  {
  # convert 17/8 => float (aka 2.125)
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);
 
  return $x->bstr() if $x->{sign} !~ /^[+-]$/;	# inf, NaN, etc

  # N/1 => N
  my $neg = ''; $neg = '-' if $x->{sign} eq '-';
  return $neg . $MBI->_num($x->{_n}) if $MBI->_is_one($x->{_d});

  $x->_as_float()->numify() + 0.0;
  }

sub as_number
  {
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  # NaN, inf etc
  return Math::BigInt->new($x->{sign}) if $x->{sign} !~ /^[+-]$/;
 
  my $u = Math::BigInt->bzero();
  $u->{sign} = $x->{sign};
  $u->{value} = $MBI->_div( $MBI->_copy($x->{_n}), $x->{_d});	# 22/7 => 3
  $u;
  }

sub as_float
  {
  # return N/D as Math::BigFloat

  # set up parameters
  my ($self,$x,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  ($self,$x,@r) = objectify(1,$class,@_) unless ref $_[0];

  # NaN, inf etc
  return Math::BigFloat->new($x->{sign}) if $x->{sign} !~ /^[+-]$/;
 
  my $u = Math::BigFloat->bzero();
  $u->{sign} = $x->{sign};
  # n
  $u->{_m} = $MBI->_copy($x->{_n});
  $u->{_e} = $MBI->_zero();
  $u->bdiv( $MBI->_str($x->{_d}), @r);
  # return $u
  $u;
  }

sub as_bin
  {
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return $x unless $x->is_int();

  my $s = $x->{sign}; $s = '' if $s eq '+';
  $s . $MBI->_as_bin($x->{_n});
  }

sub as_hex
  {
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return $x unless $x->is_int();

  my $s = $x->{sign}; $s = '' if $s eq '+';
  $s . $MBI->_as_hex($x->{_n});
  }

sub as_oct
  {
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return $x unless $x->is_int();

  my $s = $x->{sign}; $s = '' if $s eq '+';
  $s . $MBI->_as_oct($x->{_n});
  }

##############################################################################

sub from_hex
  {
  my $class = shift;

  $class->new(@_);
  }

sub from_bin
  {
  my $class = shift;

  $class->new(@_);
  }

sub from_oct
  {
  my $class = shift;

  my @parts;
  for my $c (@_)
    {
    push @parts, Math::BigInt->from_oct($c);
    }
  $class->new ( @parts );
  }

##############################################################################
# import

sub import
  {
  my $self = shift;
  my $l = scalar @_;
  my $lib = ''; my @a;
  my $try = 'try';

  for ( my $i = 0; $i < $l ; $i++)
    {
    if ( $_[$i] eq ':constant' )
      {
      # this rest causes overlord er load to step in
      overload::constant float => sub { $self->new(shift); };
      }
#    elsif ($_[$i] eq 'upgrade')
#      {
#     # this causes upgrading
#      $upgrade = $_[$i+1];		# or undef to disable
#      $i++;
#      }
    elsif ($_[$i] eq 'downgrade')
      {
      # this causes downgrading
      $downgrade = $_[$i+1];		# or undef to disable
      $i++;
      }
    elsif ($_[$i] =~ /^(lib|try|only)\z/)
      {
      $lib = $_[$i+1] || '';		# default Calc
      $try = $1;			# lib, try or only
      $i++;
      }
    elsif ($_[$i] eq 'with')
      {
      # this argument is no longer used
      #$MBI = $_[$i+1] || 'Math::BigInt::Calc';	# default Math::BigInt::Calc
      $i++;
      }
    else
      {
      push @a, $_[$i];
      }
    }
  require Math::BigInt;

  # let use Math::BigInt lib => 'GMP'; use Math::BigRat; still have GMP
  if ($lib ne '')
    {
    my @c = split /\s*,\s*/, $lib;
    foreach (@c)
      {
      $_ =~ tr/a-zA-Z0-9://cd;                    # limit to sane characters
      }
    $lib = join(",", @c);
    }
  my @import = ('objectify');
  push @import, $try => $lib if $lib ne '';

  # MBI already loaded, so feed it our lib arguments
  Math::BigInt->import( @import );

  $MBI = Math::BigFloat->config()->{lib};

  # register us with MBI to get notified of future lib changes
  Math::BigInt::_register_callback( $self, sub { $MBI = $_[0]; } );
  
  # any non :constant stuff is handled by our parent, Exporter (loaded
  # by Math::BigFloat, even if @_ is empty, to give it a chance
  $self->SUPER::import(@a);             # for subclasses
  $self->export_to_level(1,$self,@a);   # need this, too
  }

1;

__END__

=head1 NAME

Math::BigRat - Arbitrary big rational numbers

=head1 SYNOPSIS

	use Math::BigRat;

	my $x = Math::BigRat->new('3/7'); $x += '5/9';

	print $x->bstr(),"\n";
  	print $x ** 2,"\n";

	my $y = Math::BigRat->new('inf');
	print "$y ", ($y->is_inf ? 'is' : 'is not') , " infinity\n";

	my $z = Math::BigRat->new(144); $z->bsqrt();

=head1 DESCRIPTION

Math::BigRat complements Math::BigInt and Math::BigFloat by providing support
for arbitrary big rational numbers.

=head2 MATH LIBRARY

You can change the underlying module that does the low-level
math operations by using:

	use Math::BigRat try => 'GMP';

Note: This needs Math::BigInt::GMP installed.

The following would first try to find Math::BigInt::Foo, then
Math::BigInt::Bar, and when this also fails, revert to Math::BigInt::Calc:

	use Math::BigRat try => 'Foo,Math::BigInt::Bar';

If you want to get warned when the fallback occurs, replace "try" with
"lib":

	use Math::BigRat lib => 'Foo,Math::BigInt::Bar';

If you want the code to die instead, replace "try" with
"only":

	use Math::BigRat only => 'Foo,Math::BigInt::Bar';

=head1 METHODS

Any methods not listed here are derived from Math::BigFloat (or
Math::BigInt), so make sure you check these two modules for further
information.

=head2 new()

	$x = Math::BigRat->new('1/3');

Create a new Math::BigRat object. Input can come in various forms:

	$x = Math::BigRat->new(123);				# scalars
	$x = Math::BigRat->new('inf');				# infinity
	$x = Math::BigRat->new('123.3');			# float
	$x = Math::BigRat->new('1/3');				# simple string
	$x = Math::BigRat->new('1 / 3');			# spaced
	$x = Math::BigRat->new('1 / 0.1');			# w/ floats
	$x = Math::BigRat->new(Math::BigInt->new(3));		# BigInt
	$x = Math::BigRat->new(Math::BigFloat->new('3.1'));	# BigFloat
	$x = Math::BigRat->new(Math::BigInt::Lite->new('2'));	# BigLite

	# You can also give D and N as different objects:
	$x = Math::BigRat->new(
		Math::BigInt->new(-123),
		Math::BigInt->new(7),
		);			# => -123/7

=head2 numerator()

	$n = $x->numerator();

Returns a copy of the numerator (the part above the line) as signed BigInt.

=head2 denominator()
	
	$d = $x->denominator();

Returns a copy of the denominator (the part under the line) as positive BigInt.

=head2 parts()

	($n,$d) = $x->parts();

Return a list consisting of (signed) numerator and (unsigned) denominator as
BigInts.

=head2 numify()

	my $y = $x->numify();

Returns the object as a scalar. This will lose some data if the object
cannot be represented by a normal Perl scalar (integer or float), so
use L<as_int()> or L<as_float()> instead.

This routine is automatically used whenever a scalar is required:

	my $x = Math::BigRat->new('3/1');
	@array = (1,2,3);
	$y = $array[$x];		# set $y to 3

=head2 as_int()/as_number()

	$x = Math::BigRat->new('13/7');
	print $x->as_int(),"\n";		# '1'

Returns a copy of the object as BigInt, truncated to an integer.

C<as_number()> is an alias for C<as_int()>.

=head2 as_float()

	$x = Math::BigRat->new('13/7');
	print $x->as_float(),"\n";		# '1'

	$x = Math::BigRat->new('2/3');
	print $x->as_float(5),"\n";		# '0.66667'

Returns a copy of the object as BigFloat, preserving the
accuracy as wanted, or the default of 40 digits.

This method was added in v0.22 of Math::BigRat (April 2008).

=head2 as_hex()

	$x = Math::BigRat->new('13');
	print $x->as_hex(),"\n";		# '0xd'

Returns the BigRat as hexadecimal string. Works only for integers. 

=head2 as_bin()

	$x = Math::BigRat->new('13');
	print $x->as_bin(),"\n";		# '0x1101'

Returns the BigRat as binary string. Works only for integers. 

=head2 as_oct()

	$x = Math::BigRat->new('13');
	print $x->as_oct(),"\n";		# '015'

Returns the BigRat as octal string. Works only for integers. 

=head2 from_hex()/from_bin()/from_oct()

	my $h = Math::BigRat->from_hex('0x10');
	my $b = Math::BigRat->from_bin('0b10000000');
	my $o = Math::BigRat->from_oct('020');

Create a BigRat from an hexadecimal, binary or octal number
in string form.

=head2 length()

	$len = $x->length();

Return the length of $x in digitis for integer values.

=head2 digit()

	print Math::BigRat->new('123/1')->digit(1);	# 1
	print Math::BigRat->new('123/1')->digit(-1);	# 3

Return the N'ths digit from X when X is an integer value.

=head2 bnorm()

	$x->bnorm();

Reduce the number to the shortest form. This routine is called
automatically whenever it is needed.

=head2 bfac()

	$x->bfac();

Calculates the factorial of $x. For instance:

	print Math::BigRat->new('3/1')->bfac(),"\n";	# 1*2*3
	print Math::BigRat->new('5/1')->bfac(),"\n";	# 1*2*3*4*5

Works currently only for integers.

=head2 bround()/round()/bfround()

Are not yet implemented.

=head2 bmod()

	use Math::BigRat;
	my $x = Math::BigRat->new('7/4');
	my $y = Math::BigRat->new('4/3');
	print $x->bmod($y);

Set $x to the remainder of the division of $x by $y.

=head2 bneg()

	$x->bneg();

Used to negate the object in-place.

=head2 is_one()

	print "$x is 1\n" if $x->is_one();

Return true if $x is exactly one, otherwise false.

=head2 is_zero()

	print "$x is 0\n" if $x->is_zero();

Return true if $x is exactly zero, otherwise false.

=head2 is_pos()/is_positive()

	print "$x is >= 0\n" if $x->is_positive();

Return true if $x is positive (greater than or equal to zero), otherwise
false. Please note that '+inf' is also positive, while 'NaN' and '-inf' aren't.

C<is_positive()> is an alias for C<is_pos()>.

=head2 is_neg()/is_negative()

	print "$x is < 0\n" if $x->is_negative();

Return true if $x is negative (smaller than zero), otherwise false. Please
note that '-inf' is also negative, while 'NaN' and '+inf' aren't.

C<is_negative()> is an alias for C<is_neg()>.

=head2 is_int()

	print "$x is an integer\n" if $x->is_int();

Return true if $x has a denominator of 1 (e.g. no fraction parts), otherwise
false. Please note that '-inf', 'inf' and 'NaN' aren't integer.

=head2 is_odd()

	print "$x is odd\n" if $x->is_odd();

Return true if $x is odd, otherwise false.

=head2 is_even()

	print "$x is even\n" if $x->is_even();

Return true if $x is even, otherwise false.

=head2 bceil()

	$x->bceil();

Set $x to the next bigger integer value (e.g. truncate the number to integer
and then increment it by one).

=head2 bfloor()
	
	$x->bfloor();

Truncate $x to an integer value.

=head2 bsqrt()
	
	$x->bsqrt();

Calculate the square root of $x.

=head2 broot()
	
	$x->broot($n);

Calculate the N'th root of $x.

=head2 badd()/bmul()/bsub()/bdiv()/bdec()/binc()

Please see the documentation in L<Math::BigInt>.

=head2 copy()

	my $z = $x->copy();

Makes a deep copy of the object.

Please see the documentation in L<Math::BigInt> for further details.

=head2 bstr()/bsstr()

	my $x = Math::BigInt->new('8/4');
	print $x->bstr(),"\n";			# prints 1/2
	print $x->bsstr(),"\n";			# prints 1/2

Return a string representating this object.

=head2 bacmp()/bcmp()

Used to compare numbers.

Please see the documentation in L<Math::BigInt> for further details.

=head2 blsft()/brsft()

Used to shift numbers left/right.

Please see the documentation in L<Math::BigInt> for further details.

=head2 bpow()

	$x->bpow($y);

Compute $x ** $y.

Please see the documentation in L<Math::BigInt> for further details.

=head2 bexp()

	$x->bexp($accuracy);		# calculate e ** X

Calculates two integers A and B so that A/B is equal to C<e ** $x>, where C<e> is
Euler's number.

This method was added in v0.20 of Math::BigRat (May 2007).

See also L<blog()>.

=head2 bnok()

	$x->bnok($y);		   # x over y (binomial coefficient n over k)

Calculates the binomial coefficient n over k, also called the "choose"
function. The result is equivalent to:

	( n )      n!
	| - |  = -------
	( k )    k!(n-k)!

This method was added in v0.20 of Math::BigRat (May 2007).

=head2 config()

        use Data::Dumper;

        print Dumper ( Math::BigRat->config() );
        print Math::BigRat->config()->{lib},"\n";

Returns a hash containing the configuration, e.g. the version number, lib
loaded etc. The following hash keys are currently filled in with the
appropriate information.

        key             RO/RW   Description
                                Example
        ============================================================
        lib             RO      Name of the Math library
                                Math::BigInt::Calc
        lib_version     RO      Version of 'lib'
                                0.30
        class           RO      The class of config you just called
                                Math::BigRat
        version         RO      version number of the class you used
                                0.10
        upgrade         RW      To which class numbers are upgraded
                                undef
        downgrade       RW      To which class numbers are downgraded
                                undef
        precision       RW      Global precision
                                undef
        accuracy        RW      Global accuracy
                                undef
        round_mode      RW      Global round mode
                                even
        div_scale       RW      Fallback accuracy for div
                                40
        trap_nan        RW      Trap creation of NaN (undef = no)
                                undef
        trap_inf        RW      Trap creation of +inf/-inf (undef = no)
                                undef

By passing a reference to a hash you may set the configuration values. This
works only for values that a marked with a C<RW> above, anything else is
read-only.

=head2 objectify()

This is an internal routine that turns scalars into objects.

=head1 BUGS

Some things are not yet implemented, or only implemented half-way:

=over 2

=item inf handling (partial)

=item NaN handling (partial)

=item rounding (not implemented except for bceil/bfloor)

=item $x ** $y where $y is not an integer

=item bmod(), blog(), bmodinv() and bmodpow() (partial)

=back

=head1 LICENSE

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=head1 SEE ALSO

L<Math::BigFloat> and L<Math::Big> as well as L<Math::BigInt::BitVect>,
L<Math::BigInt::Pari> and  L<Math::BigInt::GMP>.

See L<http://search.cpan.org/search?dist=bignum> for a way to use
Math::BigRat.

The package at L<http://search.cpan.org/search?dist=Math%3A%3ABigRat>
may contain more documentation and examples as well as testcases.

=head1 AUTHORS

(C) by Tels L<http://bloodgate.com/> 2001 - 2008.

=cut
