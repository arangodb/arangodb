package Math::BigInt;

#
# "Mike had an infinite amount to do and a negative amount of time in which
# to do it." - Before and After
#

# The following hash values are used:
#   value: unsigned int with actual value (as a Math::BigInt::Calc or similiar)
#   sign : +,-,NaN,+inf,-inf
#   _a   : accuracy
#   _p   : precision
#   _f   : flags, used by MBF to flag parts of a float as untouchable

# Remember not to take shortcuts ala $xs = $x->{value}; $CALC->foo($xs); since
# underlying lib might change the reference!

my $class = "Math::BigInt";
use 5.006;

$VERSION = '1.89';

@ISA = qw(Exporter);
@EXPORT_OK = qw(objectify bgcd blcm); 

# _trap_inf and _trap_nan are internal and should never be accessed from the
# outside
use vars qw/$round_mode $accuracy $precision $div_scale $rnd_mode 
	    $upgrade $downgrade $_trap_nan $_trap_inf/;
use strict;

# Inside overload, the first arg is always an object. If the original code had
# it reversed (like $x = 2 * $y), then the third paramater is true.
# In some cases (like add, $x = $x + 2 is the same as $x = 2 + $x) this makes
# no difference, but in some cases it does.

# For overloaded ops with only one argument we simple use $_[0]->copy() to
# preserve the argument.

# Thus inheritance of overload operators becomes possible and transparent for
# our subclasses without the need to repeat the entire overload section there.

use overload
'='     =>      sub { $_[0]->copy(); },

# some shortcuts for speed (assumes that reversed order of arguments is routed
# to normal '+' and we thus can always modify first arg. If this is changed,
# this breaks and must be adjusted.)
'+='	=>	sub { $_[0]->badd($_[1]); },
'-='	=>	sub { $_[0]->bsub($_[1]); },
'*='	=>	sub { $_[0]->bmul($_[1]); },
'/='	=>	sub { scalar $_[0]->bdiv($_[1]); },
'%='	=>	sub { $_[0]->bmod($_[1]); },
'^='	=>	sub { $_[0]->bxor($_[1]); },
'&='	=>	sub { $_[0]->band($_[1]); },
'|='	=>	sub { $_[0]->bior($_[1]); },

'**='	=>	sub { $_[0]->bpow($_[1]); },
'<<='	=>	sub { $_[0]->blsft($_[1]); },
'>>='	=>	sub { $_[0]->brsft($_[1]); },

# not supported by Perl yet
'..'	=>	\&_pointpoint,

'<=>'	=>	sub { my $rc = $_[2] ?
                      ref($_[0])->bcmp($_[1],$_[0]) : 
                      $_[0]->bcmp($_[1]); 
		      $rc = 1 unless defined $rc;
		      $rc <=> 0;
		},
# we need '>=' to get things like "1 >= NaN" right:
'>='	=>	sub { my $rc = $_[2] ?
                      ref($_[0])->bcmp($_[1],$_[0]) : 
                      $_[0]->bcmp($_[1]);
		      # if there was a NaN involved, return false
		      return '' unless defined $rc;
		      $rc >= 0;
		},
'cmp'	=>	sub {
         $_[2] ? 
               "$_[1]" cmp $_[0]->bstr() :
               $_[0]->bstr() cmp "$_[1]" },

'cos'	=>	sub { $_[0]->copy->bcos(); }, 
'sin'	=>	sub { $_[0]->copy->bsin(); }, 
'atan2'	=>	sub { $_[2] ?
			ref($_[0])->new($_[1])->batan2($_[0]) :
			$_[0]->copy()->batan2($_[1]) },

# are not yet overloadable
#'hex'	=>	sub { print "hex"; $_[0]; }, 
#'oct'	=>	sub { print "oct"; $_[0]; }, 

# log(N) is log(N, e), where e is Euler's number
'log'	=>	sub { $_[0]->copy()->blog($_[1], undef); }, 
'exp'	=>	sub { $_[0]->copy()->bexp($_[1]); }, 
'int'	=>	sub { $_[0]->copy(); }, 
'neg'	=>	sub { $_[0]->copy()->bneg(); }, 
'abs'	=>	sub { $_[0]->copy()->babs(); },
'sqrt'  =>	sub { $_[0]->copy()->bsqrt(); },
'~'	=>	sub { $_[0]->copy()->bnot(); },

# for subtract it's a bit tricky to not modify b: b-a => -a+b
'-'	=>	sub { my $c = $_[0]->copy; $_[2] ?
			$c->bneg()->badd( $_[1]) :
			$c->bsub( $_[1]) },
'+'	=>	sub { $_[0]->copy()->badd($_[1]); },
'*'	=>	sub { $_[0]->copy()->bmul($_[1]); },

'/'	=>	sub { 
   $_[2] ? ref($_[0])->new($_[1])->bdiv($_[0]) : $_[0]->copy->bdiv($_[1]);
  }, 
'%'	=>	sub { 
   $_[2] ? ref($_[0])->new($_[1])->bmod($_[0]) : $_[0]->copy->bmod($_[1]);
  }, 
'**'	=>	sub { 
   $_[2] ? ref($_[0])->new($_[1])->bpow($_[0]) : $_[0]->copy->bpow($_[1]);
  }, 
'<<'	=>	sub { 
   $_[2] ? ref($_[0])->new($_[1])->blsft($_[0]) : $_[0]->copy->blsft($_[1]);
  }, 
'>>'	=>	sub { 
   $_[2] ? ref($_[0])->new($_[1])->brsft($_[0]) : $_[0]->copy->brsft($_[1]);
  }, 
'&'	=>	sub { 
   $_[2] ? ref($_[0])->new($_[1])->band($_[0]) : $_[0]->copy->band($_[1]);
  }, 
'|'	=>	sub { 
   $_[2] ? ref($_[0])->new($_[1])->bior($_[0]) : $_[0]->copy->bior($_[1]);
  }, 
'^'	=>	sub { 
   $_[2] ? ref($_[0])->new($_[1])->bxor($_[0]) : $_[0]->copy->bxor($_[1]);
  }, 

# can modify arg of ++ and --, so avoid a copy() for speed, but don't
# use $_[0]->bone(), it would modify $_[0] to be 1!
'++'	=>	sub { $_[0]->binc() },
'--'	=>	sub { $_[0]->bdec() },

# if overloaded, O(1) instead of O(N) and twice as fast for small numbers
'bool'  =>	sub {
  # this kludge is needed for perl prior 5.6.0 since returning 0 here fails :-/
  # v5.6.1 dumps on this: return !$_[0]->is_zero() || undef;		    :-(
  my $t = undef;
  $t = 1 if !$_[0]->is_zero();
  $t;
  },

# the original qw() does not work with the TIESCALAR below, why?
# Order of arguments unsignificant
'""' => sub { $_[0]->bstr(); },
'0+' => sub { $_[0]->numify(); }
;

##############################################################################
# global constants, flags and accessory

# These vars are public, but their direct usage is not recommended, use the
# accessor methods instead

$round_mode = 'even'; # one of 'even', 'odd', '+inf', '-inf', 'zero', 'trunc' or 'common'
$accuracy   = undef;
$precision  = undef;
$div_scale  = 40;

$upgrade = undef;			# default is no upgrade
$downgrade = undef;			# default is no downgrade

# These are internally, and not to be used from the outside at all

$_trap_nan = 0;				# are NaNs ok? set w/ config()
$_trap_inf = 0;				# are infs ok? set w/ config()
my $nan = 'NaN'; 			# constants for easier life

my $CALC = 'Math::BigInt::FastCalc';	# module to do the low level math
					# default is FastCalc.pm
my $IMPORT = 0;				# was import() called yet?
					# used to make require work
my %WARN;				# warn only once for low-level libs
my %CAN;				# cache for $CALC->can(...)
my %CALLBACKS;				# callbacks to notify on lib loads
my $EMU_LIB = 'Math/BigInt/CalcEmu.pm';	# emulate low-level math

##############################################################################
# the old code had $rnd_mode, so we need to support it, too

$rnd_mode   = 'even';
sub TIESCALAR  { my ($class) = @_; bless \$round_mode, $class; }
sub FETCH      { return $round_mode; }
sub STORE      { $rnd_mode = $_[0]->round_mode($_[1]); }

BEGIN
  { 
  # tie to enable $rnd_mode to work transparently
  tie $rnd_mode, 'Math::BigInt'; 

  # set up some handy alias names
  *as_int = \&as_number;
  *is_pos = \&is_positive;
  *is_neg = \&is_negative;
  }

############################################################################## 

sub round_mode
  {
  no strict 'refs';
  # make Class->round_mode() work
  my $self = shift;
  my $class = ref($self) || $self || __PACKAGE__;
  if (defined $_[0])
    {
    my $m = shift;
    if ($m !~ /^(even|odd|\+inf|\-inf|zero|trunc|common)$/)
      {
      require Carp; Carp::croak ("Unknown round mode '$m'");
      }
    return ${"${class}::round_mode"} = $m;
    }
  ${"${class}::round_mode"};
  }

sub upgrade
  {
  no strict 'refs';
  # make Class->upgrade() work
  my $self = shift;
  my $class = ref($self) || $self || __PACKAGE__;
  # need to set new value?
  if (@_ > 0)
    {
    return ${"${class}::upgrade"} = $_[0];
    }
  ${"${class}::upgrade"};
  }

sub downgrade
  {
  no strict 'refs';
  # make Class->downgrade() work
  my $self = shift;
  my $class = ref($self) || $self || __PACKAGE__;
  # need to set new value?
  if (@_ > 0)
    {
    return ${"${class}::downgrade"} = $_[0];
    }
  ${"${class}::downgrade"};
  }

sub div_scale
  {
  no strict 'refs';
  # make Class->div_scale() work
  my $self = shift;
  my $class = ref($self) || $self || __PACKAGE__;
  if (defined $_[0])
    {
    if ($_[0] < 0)
      {
      require Carp; Carp::croak ('div_scale must be greater than zero');
      }
    ${"${class}::div_scale"} = $_[0];
    }
  ${"${class}::div_scale"};
  }

sub accuracy
  {
  # $x->accuracy($a);		ref($x)	$a
  # $x->accuracy();		ref($x)
  # Class->accuracy();		class
  # Class->accuracy($a);	class $a

  my $x = shift;
  my $class = ref($x) || $x || __PACKAGE__;

  no strict 'refs';
  # need to set new value?
  if (@_ > 0)
    {
    my $a = shift;
    # convert objects to scalars to avoid deep recursion. If object doesn't
    # have numify(), then hopefully it will have overloading for int() and
    # boolean test without wandering into a deep recursion path...
    $a = $a->numify() if ref($a) && $a->can('numify');

    if (defined $a)
      {
      # also croak on non-numerical
      if (!$a || $a <= 0)
        {
        require Carp;
	Carp::croak ('Argument to accuracy must be greater than zero');
        }
      if (int($a) != $a)
        {
        require Carp;
	Carp::croak ('Argument to accuracy must be an integer');
        }
      }
    if (ref($x))
      {
      # $object->accuracy() or fallback to global
      $x->bround($a) if $a;		# not for undef, 0
      $x->{_a} = $a;			# set/overwrite, even if not rounded
      delete $x->{_p};			# clear P
      $a = ${"${class}::accuracy"} unless defined $a;   # proper return value
      }
    else
      {
      ${"${class}::accuracy"} = $a;	# set global A
      ${"${class}::precision"} = undef;	# clear global P
      }
    return $a;				# shortcut
    }

  my $a;
  # $object->accuracy() or fallback to global
  $a = $x->{_a} if ref($x);
  # but don't return global undef, when $x's accuracy is 0!
  $a = ${"${class}::accuracy"} if !defined $a;
  $a;
  }

sub precision
  {
  # $x->precision($p);		ref($x)	$p
  # $x->precision();		ref($x)
  # Class->precision();		class
  # Class->precision($p);	class $p

  my $x = shift;
  my $class = ref($x) || $x || __PACKAGE__;

  no strict 'refs';
  if (@_ > 0)
    {
    my $p = shift;
    # convert objects to scalars to avoid deep recursion. If object doesn't
    # have numify(), then hopefully it will have overloading for int() and
    # boolean test without wandering into a deep recursion path...
    $p = $p->numify() if ref($p) && $p->can('numify');
    if ((defined $p) && (int($p) != $p))
      {
      require Carp; Carp::croak ('Argument to precision must be an integer');
      }
    if (ref($x))
      {
      # $object->precision() or fallback to global
      $x->bfround($p) if $p;		# not for undef, 0
      $x->{_p} = $p;			# set/overwrite, even if not rounded
      delete $x->{_a};			# clear A
      $p = ${"${class}::precision"} unless defined $p;  # proper return value
      }
    else
      {
      ${"${class}::precision"} = $p;	# set global P
      ${"${class}::accuracy"} = undef;	# clear global A
      }
    return $p;				# shortcut
    }

  my $p;
  # $object->precision() or fallback to global
  $p = $x->{_p} if ref($x);
  # but don't return global undef, when $x's precision is 0!
  $p = ${"${class}::precision"} if !defined $p;
  $p;
  }

sub config
  {
  # return (or set) configuration data as hash ref
  my $class = shift || 'Math::BigInt';

  no strict 'refs';
  if (@_ > 1 || (@_ == 1 && (ref($_[0]) eq 'HASH')))
    {
    # try to set given options as arguments from hash

    my $args = $_[0];
    if (ref($args) ne 'HASH')
      {
      $args = { @_ };
      }
    # these values can be "set"
    my $set_args = {};
    foreach my $key (
     qw/trap_inf trap_nan
        upgrade downgrade precision accuracy round_mode div_scale/
     )
      {
      $set_args->{$key} = $args->{$key} if exists $args->{$key};
      delete $args->{$key};
      }
    if (keys %$args > 0)
      {
      require Carp;
      Carp::croak ("Illegal key(s) '",
       join("','",keys %$args),"' passed to $class\->config()");
      }
    foreach my $key (keys %$set_args)
      {
      if ($key =~ /^trap_(inf|nan)\z/)
        {
        ${"${class}::_trap_$1"} = ($set_args->{"trap_$1"} ? 1 : 0);
        next;
        }
      # use a call instead of just setting the $variable to check argument
      $class->$key($set_args->{$key});
      }
    }

  # now return actual configuration

  my $cfg = {
    lib => $CALC,
    lib_version => ${"${CALC}::VERSION"},
    class => $class,
    trap_nan => ${"${class}::_trap_nan"},
    trap_inf => ${"${class}::_trap_inf"},
    version => ${"${class}::VERSION"},
    };
  foreach my $key (qw/
     upgrade downgrade precision accuracy round_mode div_scale
     /)
    {
    $cfg->{$key} = ${"${class}::$key"};
    };
  if (@_ == 1 && (ref($_[0]) ne 'HASH'))
    {
    # calls of the style config('lib') return just this value
    return $cfg->{$_[0]};
    }
  $cfg;
  }

sub _scale_a
  { 
  # select accuracy parameter based on precedence,
  # used by bround() and bfround(), may return undef for scale (means no op)
  my ($x,$scale,$mode) = @_;

  $scale = $x->{_a} unless defined $scale;

  no strict 'refs';
  my $class = ref($x);

  $scale = ${ $class . '::accuracy' } unless defined $scale;
  $mode = ${ $class . '::round_mode' } unless defined $mode;

  if (defined $scale)
    {
    $scale = $scale->can('numify') ? $scale->numify() : "$scale" if ref($scale);
    $scale = int($scale);
    }

  ($scale,$mode);
  }

sub _scale_p
  { 
  # select precision parameter based on precedence,
  # used by bround() and bfround(), may return undef for scale (means no op)
  my ($x,$scale,$mode) = @_;
  
  $scale = $x->{_p} unless defined $scale;

  no strict 'refs';
  my $class = ref($x);

  $scale = ${ $class . '::precision' } unless defined $scale;
  $mode = ${ $class . '::round_mode' } unless defined $mode;

  if (defined $scale)
    {
    $scale = $scale->can('numify') ? $scale->numify() : "$scale" if ref($scale);
    $scale = int($scale);
    }

  ($scale,$mode);
  }

##############################################################################
# constructors

sub copy
  {
  # if two arguments, the first one is the class to "swallow" subclasses
  if (@_ > 1)
    {
    my  $self = bless {
	sign => $_[1]->{sign}, 
	value => $CALC->_copy($_[1]->{value}),
    }, $_[0] if @_ > 1;

    $self->{_a} = $_[1]->{_a} if defined $_[1]->{_a};
    $self->{_p} = $_[1]->{_p} if defined $_[1]->{_p};
    return $self;
    }

  my $self = bless {
	sign => $_[0]->{sign}, 
	value => $CALC->_copy($_[0]->{value}),
	}, ref($_[0]);

  $self->{_a} = $_[0]->{_a} if defined $_[0]->{_a};
  $self->{_p} = $_[0]->{_p} if defined $_[0]->{_p};
  $self;
  }

sub new 
  {
  # create a new BigInt object from a string or another BigInt object. 
  # see hash keys documented at top

  # the argument could be an object, so avoid ||, && etc on it, this would
  # cause costly overloaded code to be called. The only allowed ops are
  # ref() and defined.

  my ($class,$wanted,$a,$p,$r) = @_;
 
  # avoid numify-calls by not using || on $wanted!
  return $class->bzero($a,$p) if !defined $wanted;	# default to 0
  return $class->copy($wanted,$a,$p,$r)
   if ref($wanted) && $wanted->isa($class);		# MBI or subclass

  $class->import() if $IMPORT == 0;		# make require work
  
  my $self = bless {}, $class;

  # shortcut for "normal" numbers
  if ((!ref $wanted) && ($wanted =~ /^([+-]?)[1-9][0-9]*\z/))
    {
    $self->{sign} = $1 || '+';

    if ($wanted =~ /^[+-]/)
     {
      # remove sign without touching wanted to make it work with constants
      my $t = $wanted; $t =~ s/^[+-]//;
      $self->{value} = $CALC->_new($t);
      }
    else
      {
      $self->{value} = $CALC->_new($wanted);
      }
    no strict 'refs';
    if ( (defined $a) || (defined $p) 
        || (defined ${"${class}::precision"})
        || (defined ${"${class}::accuracy"}) 
       )
      {
      $self->round($a,$p,$r) unless (@_ == 4 && !defined $a && !defined $p);
      }
    return $self;
    }

  # handle '+inf', '-inf' first
  if ($wanted =~ /^[+-]?inf\z/)
    {
    $self->{sign} = $wanted;		# set a default sign for bstr()
    return $self->binf($wanted);
    }
  # split str in m mantissa, e exponent, i integer, f fraction, v value, s sign
  my ($mis,$miv,$mfv,$es,$ev) = _split($wanted);
  if (!ref $mis)
    {
    if ($_trap_nan)
      {
      require Carp; Carp::croak("$wanted is not a number in $class");
      }
    $self->{value} = $CALC->_zero();
    $self->{sign} = $nan;
    return $self;
    }
  if (!ref $miv)
    {
    # _from_hex or _from_bin
    $self->{value} = $mis->{value};
    $self->{sign} = $mis->{sign};
    return $self;	# throw away $mis
    }
  # make integer from mantissa by adjusting exp, then convert to bigint
  $self->{sign} = $$mis;			# store sign
  $self->{value} = $CALC->_zero();		# for all the NaN cases
  my $e = int("$$es$$ev");			# exponent (avoid recursion)
  if ($e > 0)
    {
    my $diff = $e - CORE::length($$mfv);
    if ($diff < 0)				# Not integer
      {
      if ($_trap_nan)
        {
        require Carp; Carp::croak("$wanted not an integer in $class");
        }
      #print "NOI 1\n";
      return $upgrade->new($wanted,$a,$p,$r) if defined $upgrade;
      $self->{sign} = $nan;
      }
    else					# diff >= 0
      {
      # adjust fraction and add it to value
      #print "diff > 0 $$miv\n";
      $$miv = $$miv . ($$mfv . '0' x $diff);
      }
    }
  else
    {
    if ($$mfv ne '')				# e <= 0
      {
      # fraction and negative/zero E => NOI
      if ($_trap_nan)
        {
        require Carp; Carp::croak("$wanted not an integer in $class");
        }
      #print "NOI 2 \$\$mfv '$$mfv'\n";
      return $upgrade->new($wanted,$a,$p,$r) if defined $upgrade;
      $self->{sign} = $nan;
      }
    elsif ($e < 0)
      {
      # xE-y, and empty mfv
      #print "xE-y\n";
      $e = abs($e);
      if ($$miv !~ s/0{$e}$//)		# can strip so many zero's?
        {
        if ($_trap_nan)
          {
          require Carp; Carp::croak("$wanted not an integer in $class");
          }
        #print "NOI 3\n";
        return $upgrade->new($wanted,$a,$p,$r) if defined $upgrade;
        $self->{sign} = $nan;
        }
      }
    }
  $self->{sign} = '+' if $$miv eq '0';			# normalize -0 => +0
  $self->{value} = $CALC->_new($$miv) if $self->{sign} =~ /^[+-]$/;
  # if any of the globals is set, use them to round and store them inside $self
  # do not round for new($x,undef,undef) since that is used by MBF to signal
  # no rounding
  $self->round($a,$p,$r) unless @_ == 4 && !defined $a && !defined $p;
  $self;
  }

sub bnan
  {
  # create a bigint 'NaN', if given a BigInt, set it to 'NaN'
  my $self = shift;
  $self = $class if !defined $self;
  if (!ref($self))
    {
    my $c = $self; $self = {}; bless $self, $c;
    }
  no strict 'refs';
  if (${"${class}::_trap_nan"})
    {
    require Carp;
    Carp::croak ("Tried to set $self to NaN in $class\::bnan()");
    }
  $self->import() if $IMPORT == 0;		# make require work
  return if $self->modify('bnan');
  if ($self->can('_bnan'))
    {
    # use subclass to initialize
    $self->_bnan();
    }
  else
    {
    # otherwise do our own thing
    $self->{value} = $CALC->_zero();
    }
  $self->{sign} = $nan;
  delete $self->{_a}; delete $self->{_p};	# rounding NaN is silly
  $self;
  }

sub binf
  {
  # create a bigint '+-inf', if given a BigInt, set it to '+-inf'
  # the sign is either '+', or if given, used from there
  my $self = shift;
  my $sign = shift; $sign = '+' if !defined $sign || $sign !~ /^-(inf)?$/;
  $self = $class if !defined $self;
  if (!ref($self))
    {
    my $c = $self; $self = {}; bless $self, $c;
    }
  no strict 'refs';
  if (${"${class}::_trap_inf"})
    {
    require Carp;
    Carp::croak ("Tried to set $self to +-inf in $class\::binf()");
    }
  $self->import() if $IMPORT == 0;		# make require work
  return if $self->modify('binf');
  if ($self->can('_binf'))
    {
    # use subclass to initialize
    $self->_binf();
    }
  else
    {
    # otherwise do our own thing
    $self->{value} = $CALC->_zero();
    }
  $sign = $sign . 'inf' if $sign !~ /inf$/;	# - => -inf
  $self->{sign} = $sign;
  ($self->{_a},$self->{_p}) = @_;		# take over requested rounding
  $self;
  }

sub bzero
  {
  # create a bigint '+0', if given a BigInt, set it to 0
  my $self = shift;
  $self = __PACKAGE__ if !defined $self;
 
  if (!ref($self))
    {
    my $c = $self; $self = {}; bless $self, $c;
    }
  $self->import() if $IMPORT == 0;		# make require work
  return if $self->modify('bzero');
  
  if ($self->can('_bzero'))
    {
    # use subclass to initialize
    $self->_bzero();
    }
  else
    {
    # otherwise do our own thing
    $self->{value} = $CALC->_zero();
    }
  $self->{sign} = '+';
  if (@_ > 0)
    {
    if (@_ > 3)
      {
      # call like: $x->bzero($a,$p,$r,$y);
      ($self,$self->{_a},$self->{_p}) = $self->_find_round_parameters(@_);
      }
    else
      {
      $self->{_a} = $_[0]
       if ( (!defined $self->{_a}) || (defined $_[0] && $_[0] > $self->{_a}));
      $self->{_p} = $_[1]
       if ( (!defined $self->{_p}) || (defined $_[1] && $_[1] > $self->{_p}));
      }
    }
  $self;
  }

sub bone
  {
  # create a bigint '+1' (or -1 if given sign '-'),
  # if given a BigInt, set it to +1 or -1, respectively
  my $self = shift;
  my $sign = shift; $sign = '+' if !defined $sign || $sign ne '-';
  $self = $class if !defined $self;

  if (!ref($self))
    {
    my $c = $self; $self = {}; bless $self, $c;
    }
  $self->import() if $IMPORT == 0;		# make require work
  return if $self->modify('bone');

  if ($self->can('_bone'))
    {
    # use subclass to initialize
    $self->_bone();
    }
  else
    {
    # otherwise do our own thing
    $self->{value} = $CALC->_one();
    }
  $self->{sign} = $sign;
  if (@_ > 0)
    {
    if (@_ > 3)
      {
      # call like: $x->bone($sign,$a,$p,$r,$y);
      ($self,$self->{_a},$self->{_p}) = $self->_find_round_parameters(@_);
      }
    else
      {
      # call like: $x->bone($sign,$a,$p,$r);
      $self->{_a} = $_[0]
       if ( (!defined $self->{_a}) || (defined $_[0] && $_[0] > $self->{_a}));
      $self->{_p} = $_[1]
       if ( (!defined $self->{_p}) || (defined $_[1] && $_[1] > $self->{_p}));
      }
    }
  $self;
  }

##############################################################################
# string conversation

sub bsstr
  {
  # (ref to BFLOAT or num_str ) return num_str
  # Convert number from internal format to scientific string format.
  # internal format is always normalized (no leading zeros, "-0E0" => "+0E0")
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_); 

  if ($x->{sign} !~ /^[+-]$/)
    {
    return $x->{sign} unless $x->{sign} eq '+inf';	# -inf, NaN
    return 'inf';					# +inf
    }
  my ($m,$e) = $x->parts();
  #$m->bstr() . 'e+' . $e->bstr(); 	# e can only be positive in BigInt
  # 'e+' because E can only be positive in BigInt
  $m->bstr() . 'e+' . $CALC->_str($e->{value}); 
  }

sub bstr 
  {
  # make a string from bigint object
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_); 

  if ($x->{sign} !~ /^[+-]$/)
    {
    return $x->{sign} unless $x->{sign} eq '+inf';	# -inf, NaN
    return 'inf';					# +inf
    }
  my $es = ''; $es = $x->{sign} if $x->{sign} eq '-';
  $es.$CALC->_str($x->{value});
  }

sub numify 
  {
  # Make a "normal" scalar from a BigInt object
  my $x = shift; $x = $class->new($x) unless ref $x;

  return $x->bstr() if $x->{sign} !~ /^[+-]$/;
  my $num = $CALC->_num($x->{value});
  return -$num if $x->{sign} eq '-';
  $num;
  }

##############################################################################
# public stuff (usually prefixed with "b")

sub sign
  {
  # return the sign of the number: +/-/-inf/+inf/NaN
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_); 
  
  $x->{sign};
  }

sub _find_round_parameters
  {
  # After any operation or when calling round(), the result is rounded by
  # regarding the A & P from arguments, local parameters, or globals.

  # !!!!!!! If you change this, remember to change round(), too! !!!!!!!!!!

  # This procedure finds the round parameters, but it is for speed reasons
  # duplicated in round. Otherwise, it is tested by the testsuite and used
  # by fdiv().
 
  # returns ($self) or ($self,$a,$p,$r) - sets $self to NaN of both A and P
  # were requested/defined (locally or globally or both)
  
  my ($self,$a,$p,$r,@args) = @_;
  # $a accuracy, if given by caller
  # $p precision, if given by caller
  # $r round_mode, if given by caller
  # @args all 'other' arguments (0 for unary, 1 for binary ops)

  my $c = ref($self);				# find out class of argument(s)
  no strict 'refs';

  # convert to normal scalar for speed and correctness in inner parts
  $a = $a->can('numify') ? $a->numify() : "$a" if defined $a && ref($a);
  $p = $p->can('numify') ? $p->numify() : "$p" if defined $p && ref($p);

  # now pick $a or $p, but only if we have got "arguments"
  if (!defined $a)
    {
    foreach ($self,@args)
      {
      # take the defined one, or if both defined, the one that is smaller
      $a = $_->{_a} if (defined $_->{_a}) && (!defined $a || $_->{_a} < $a);
      }
    }
  if (!defined $p)
    {
    # even if $a is defined, take $p, to signal error for both defined
    foreach ($self,@args)
      {
      # take the defined one, or if both defined, the one that is bigger
      # -2 > -3, and 3 > 2
      $p = $_->{_p} if (defined $_->{_p}) && (!defined $p || $_->{_p} > $p);
      }
    }
  # if still none defined, use globals (#2)
  $a = ${"$c\::accuracy"} unless defined $a;
  $p = ${"$c\::precision"} unless defined $p;

  # A == 0 is useless, so undef it to signal no rounding
  $a = undef if defined $a && $a == 0;
 
  # no rounding today? 
  return ($self) unless defined $a || defined $p;		# early out

  # set A and set P is an fatal error
  return ($self->bnan()) if defined $a && defined $p;		# error

  $r = ${"$c\::round_mode"} unless defined $r;
  if ($r !~ /^(even|odd|\+inf|\-inf|zero|trunc|common)$/)
    {
    require Carp; Carp::croak ("Unknown round mode '$r'");
    }

  $a = int($a) if defined $a;
  $p = int($p) if defined $p;

  ($self,$a,$p,$r);
  }

sub round
  {
  # Round $self according to given parameters, or given second argument's
  # parameters or global defaults 

  # for speed reasons, _find_round_parameters is embeded here:

  my ($self,$a,$p,$r,@args) = @_;
  # $a accuracy, if given by caller
  # $p precision, if given by caller
  # $r round_mode, if given by caller
  # @args all 'other' arguments (0 for unary, 1 for binary ops)

  my $c = ref($self);				# find out class of argument(s)
  no strict 'refs';

  # now pick $a or $p, but only if we have got "arguments"
  if (!defined $a)
    {
    foreach ($self,@args)
      {
      # take the defined one, or if both defined, the one that is smaller
      $a = $_->{_a} if (defined $_->{_a}) && (!defined $a || $_->{_a} < $a);
      }
    }
  if (!defined $p)
    {
    # even if $a is defined, take $p, to signal error for both defined
    foreach ($self,@args)
      {
      # take the defined one, or if both defined, the one that is bigger
      # -2 > -3, and 3 > 2
      $p = $_->{_p} if (defined $_->{_p}) && (!defined $p || $_->{_p} > $p);
      }
    }
  # if still none defined, use globals (#2)
  $a = ${"$c\::accuracy"} unless defined $a;
  $p = ${"$c\::precision"} unless defined $p;
 
  # A == 0 is useless, so undef it to signal no rounding
  $a = undef if defined $a && $a == 0;
  
  # no rounding today? 
  return $self unless defined $a || defined $p;		# early out

  # set A and set P is an fatal error
  return $self->bnan() if defined $a && defined $p;

  $r = ${"$c\::round_mode"} unless defined $r;
  if ($r !~ /^(even|odd|\+inf|\-inf|zero|trunc|common)$/)
    {
    require Carp; Carp::croak ("Unknown round mode '$r'");
    }

  # now round, by calling either fround or ffround:
  if (defined $a)
    {
    $self->bround(int($a),$r) if !defined $self->{_a} || $self->{_a} >= $a;
    }
  else # both can't be undefined due to early out
    {
    $self->bfround(int($p),$r) if !defined $self->{_p} || $self->{_p} <= $p;
    }
  # bround() or bfround() already callled bnorm() if nec.
  $self;
  }

sub bnorm
  { 
  # (numstr or BINT) return BINT
  # Normalize number -- no-op here
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);
  $x;
  }

sub babs 
  {
  # (BINT or num_str) return BINT
  # make number absolute, or return absolute BINT from string
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return $x if $x->modify('babs');
  # post-normalized abs for internal use (does nothing for NaN)
  $x->{sign} =~ s/^-/+/;
  $x;
  }

sub bneg 
  { 
  # (BINT or num_str) return BINT
  # negate number or make a negated number from string
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);
  
  return $x if $x->modify('bneg');

  # for +0 dont negate (to have always normalized +0). Does nothing for 'NaN'
  $x->{sign} =~ tr/+-/-+/ unless ($x->{sign} eq '+' && $CALC->_is_zero($x->{value}));
  $x;
  }

sub bcmp 
  {
  # Compares 2 values.  Returns one of undef, <0, =0, >0. (suitable for sort)
  # (BINT or num_str, BINT or num_str) return cond_code
  
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
    return 0 if $x->{sign} eq $y->{sign} && $x->{sign} =~ /^[+-]inf$/;
    return +1 if $x->{sign} eq '+inf';
    return -1 if $x->{sign} eq '-inf';
    return -1 if $y->{sign} eq '+inf';
    return +1;
    }
  # check sign for speed first
  return 1 if $x->{sign} eq '+' && $y->{sign} eq '-';	# does also 0 <=> -y
  return -1 if $x->{sign} eq '-' && $y->{sign} eq '+';  # does also -x <=> 0 

  # have same sign, so compare absolute values. Don't make tests for zero here
  # because it's actually slower than testin in Calc (especially w/ Pari et al)

  # post-normalized compare for internal use (honors signs)
  if ($x->{sign} eq '+') 
    {
    # $x and $y both > 0
    return $CALC->_acmp($x->{value},$y->{value});
    }

  # $x && $y both < 0
  $CALC->_acmp($y->{value},$x->{value});	# swaped acmp (lib returns 0,1,-1)
  }

sub bacmp 
  {
  # Compares 2 values, ignoring their signs. 
  # Returns one of undef, <0, =0, >0. (suitable for sort)
  # (BINT, BINT) return cond_code
  
  # set up parameters
  my ($self,$x,$y) = (ref($_[0]),@_);
  # objectify is costly, so avoid it 
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y) = objectify(2,@_);
    }

  return $upgrade->bacmp($x,$y) if defined $upgrade &&
    ((!$x->isa($self)) || (!$y->isa($self)));

  if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/))
    {
    # handle +-inf and NaN
    return undef if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));
    return 0 if $x->{sign} =~ /^[+-]inf$/ && $y->{sign} =~ /^[+-]inf$/;
    return 1 if $x->{sign} =~ /^[+-]inf$/ && $y->{sign} !~ /^[+-]inf$/;
    return -1;
    }
  $CALC->_acmp($x->{value},$y->{value});	# lib does only 0,1,-1
  }

sub badd 
  {
  # add second arg (BINT or string) to first (BINT) (modifies first)
  # return result as BINT

  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it 
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $x if $x->modify('badd');
  return $upgrade->badd($upgrade->new($x),$upgrade->new($y),@r) if defined $upgrade &&
    ((!$x->isa($self)) || (!$y->isa($self)));

  $r[3] = $y;				# no push!
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
    # +-inf + something => +inf
    # something +-inf => +-inf
    $x->{sign} = $y->{sign}, return $x if $y->{sign} =~ /^[+-]inf$/;
    return $x;
    }
    
  my ($sx, $sy) = ( $x->{sign}, $y->{sign} ); 		# get signs

  if ($sx eq $sy)  
    {
    $x->{value} = $CALC->_add($x->{value},$y->{value});	# same sign, abs add
    }
  else 
    {
    my $a = $CALC->_acmp ($y->{value},$x->{value});	# absolute compare
    if ($a > 0)                           
      {
      $x->{value} = $CALC->_sub($y->{value},$x->{value},1); # abs sub w/ swap
      $x->{sign} = $sy;
      } 
    elsif ($a == 0)
      {
      # speedup, if equal, set result to 0
      $x->{value} = $CALC->_zero();
      $x->{sign} = '+';
      }
    else # a < 0
      {
      $x->{value} = $CALC->_sub($x->{value}, $y->{value}); # abs sub
      }
    }
  $x->round(@r);
  }

sub bsub 
  {
  # (BINT or num_str, BINT or num_str) return BINT
  # subtract second arg from first, modify first
  
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);

  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $x if $x->modify('bsub');

  return $upgrade->new($x)->bsub($upgrade->new($y),@r) if defined $upgrade &&
   ((!$x->isa($self)) || (!$y->isa($self)));

  return $x->round(@r) if $y->is_zero();

  # To correctly handle the lone special case $x->bsub($x), we note the sign
  # of $x, then flip the sign from $y, and if the sign of $x did change, too,
  # then we caught the special case:
  my $xsign = $x->{sign};
  $y->{sign} =~ tr/+\-/-+/; 	# does nothing for NaN
  if ($xsign ne $x->{sign})
    {
    # special case of $x->bsub($x) results in 0
    return $x->bzero(@r) if $xsign =~ /^[+-]$/;
    return $x->bnan();          # NaN, -inf, +inf
    }
  $x->badd($y,@r); 		# badd does not leave internal zeros
  $y->{sign} =~ tr/+\-/-+/; 	# refix $y (does nothing for NaN)
  $x;				# already rounded by badd() or no round nec.
  }

sub binc
  {
  # increment arg by one
  my ($self,$x,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);
  return $x if $x->modify('binc');

  if ($x->{sign} eq '+')
    {
    $x->{value} = $CALC->_inc($x->{value});
    return $x->round($a,$p,$r);
    }
  elsif ($x->{sign} eq '-')
    {
    $x->{value} = $CALC->_dec($x->{value});
    $x->{sign} = '+' if $CALC->_is_zero($x->{value}); # -1 +1 => -0 => +0
    return $x->round($a,$p,$r);
    }
  # inf, nan handling etc
  $x->badd($self->bone(),$a,$p,$r);		# badd does round
  }

sub bdec
  {
  # decrement arg by one
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);
  return $x if $x->modify('bdec');
  
  if ($x->{sign} eq '-')
    {
    # x already < 0
    $x->{value} = $CALC->_inc($x->{value});
    } 
  else
    {
    return $x->badd($self->bone('-'),@r) unless $x->{sign} eq '+'; 	# inf or NaN
    # >= 0
    if ($CALC->_is_zero($x->{value}))
      {
      # == 0
      $x->{value} = $CALC->_one(); $x->{sign} = '-';		# 0 => -1
      }
    else
      {
      # > 0
      $x->{value} = $CALC->_dec($x->{value});
      }
    }
  $x->round(@r);
  }

sub blog
  {
  # calculate $x = $a ** $base + $b and return $a (e.g. the log() to base
  # $base of $x)

  # set up parameters
  my ($self,$x,$base,@r) = (undef,@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$base,@r) = objectify(1,ref($x),@_);
    }

  return $x if $x->modify('blog');

  $base = $self->new($base) if defined $base && !ref $base;

  # inf, -inf, NaN, <0 => NaN
  return $x->bnan()
   if $x->{sign} ne '+' || (defined $base && $base->{sign} ne '+');

  return $upgrade->blog($upgrade->new($x),$base,@r) if 
    defined $upgrade;

  # fix for bug #24969:
  # the default base is e (Euler's number) which is not an integer
  if (!defined $base)
    {
    require Math::BigFloat;
    my $u = Math::BigFloat->blog(Math::BigFloat->new($x))->as_int();
    # modify $x in place
    $x->{value} = $u->{value};
    $x->{sign} = $u->{sign};
    return $x;
    }
  
  my ($rc,$exact) = $CALC->_log_int($x->{value},$base->{value});
  return $x->bnan() unless defined $rc;		# not possible to take log?
  $x->{value} = $rc;
  $x->round(@r);
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
  return $x->bnan() if $x->{sign} eq 'NaN' || $y->{sign} eq 'NaN';
  return $x->binf() if $x->{sign} eq '+inf';

  # k > n or k < 0 => 0
  my $cmp = $x->bacmp($y);
  return $x->bzero() if $cmp < 0 || $y->{sign} =~ /^-/;
  # k == n => 1
  return $x->bone(@r) if $cmp == 0;

  if ($CALC->can('_nok'))
    {
    $x->{value} = $CALC->_nok($x->{value},$y->{value});
    }
  else
    {
    # ( 7 )    7!          7*6*5 * 4*3*2*1   7 * 6 * 5
    # ( - ) = --------- =  --------------- = ---------
    # ( 3 )   3! (7-3)!    3*2*1 * 4*3*2*1   3 * 2 * 1 

    # compute n - k + 2 (so we start with 5 in the example above)
    my $z = $x - $y;
    if (!$z->is_one())
      {
      $z->binc();
      my $r = $z->copy(); $z->binc();
      my $d = $self->new(2);
      while ($z->bacmp($x) <= 0)		# f < x ?
        {
        $r->bmul($z); $r->bdiv($d);
        $z->binc(); $d->binc();
        }
      $x->{value} = $r->{value}; $x->{sign} = '+';
      }
    else { $x->bone(); }
    }
  $x->round(@r);
  }

sub bexp
  {
  # Calculate e ** $x (Euler's number to the power of X), truncated to
  # an integer value.
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);
  return $x if $x->modify('bexp');

  # inf, -inf, NaN, <0 => NaN
  return $x->bnan() if $x->{sign} eq 'NaN';
  return $x->bone() if $x->is_zero();
  return $x if $x->{sign} eq '+inf';
  return $x->bzero() if $x->{sign} eq '-inf';

  my $u;
  {
    # run through Math::BigFloat unless told otherwise
    require Math::BigFloat unless defined $upgrade;
    local $upgrade = 'Math::BigFloat' unless defined $upgrade;
    # calculate result, truncate it to integer
    $u = $upgrade->bexp($upgrade->new($x),@r);
  }

  if (!defined $upgrade)
    {
    $u = $u->as_int();
    # modify $x in place
    $x->{value} = $u->{value};
    $x->round(@r);
    }
  else { $x = $u; }
  }

sub blcm 
  { 
  # (BINT or num_str, BINT or num_str) return BINT
  # does not modify arguments, but returns new object
  # Lowest Common Multiplicator

  my $y = shift; my ($x);
  if (ref($y))
    {
    $x = $y->copy();
    }
  else
    {
    $x = $class->new($y);
    }
  my $self = ref($x);
  while (@_) 
    {
    my $y = shift; $y = $self->new($y) if !ref ($y);
    $x = __lcm($x,$y);
    } 
  $x;
  }

sub bgcd 
  { 
  # (BINT or num_str, BINT or num_str) return BINT
  # does not modify arguments, but returns new object
  # GCD -- Euclids algorithm, variant C (Knuth Vol 3, pg 341 ff)

  my $y = shift;
  $y = $class->new($y) if !ref($y);
  my $self = ref($y);
  my $x = $y->copy()->babs();			# keep arguments
  return $x->bnan() if $x->{sign} !~ /^[+-]$/;	# x NaN?

  while (@_)
    {
    $y = shift; $y = $self->new($y) if !ref($y);
    return $x->bnan() if $y->{sign} !~ /^[+-]$/;	# y NaN?
    $x->{value} = $CALC->_gcd($x->{value},$y->{value});
    last if $CALC->_is_one($x->{value});
    }
  $x;
  }

sub bnot 
  {
  # (num_str or BINT) return BINT
  # represent ~x as twos-complement number
  # we don't need $self, so undef instead of ref($_[0]) make it slightly faster
  my ($self,$x,$a,$p,$r) = ref($_[0]) ? (undef,@_) : objectify(1,@_);
 
  return $x if $x->modify('bnot');
  $x->binc()->bneg();			# binc already does round
  }

##############################################################################
# is_foo test routines
# we don't need $self, so undef instead of ref($_[0]) make it slightly faster

sub is_zero
  {
  # return true if arg (BINT or num_str) is zero (array '+', '0')
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);
  
  return 0 if $x->{sign} !~ /^\+$/;			# -, NaN & +-inf aren't
  $CALC->_is_zero($x->{value});
  }

sub is_nan
  {
  # return true if arg (BINT or num_str) is NaN
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  $x->{sign} eq $nan ? 1 : 0;
  }

sub is_inf
  {
  # return true if arg (BINT or num_str) is +-inf
  my ($self,$x,$sign) = ref($_[0]) ? (undef,@_) : objectify(1,@_);

  if (defined $sign)
    {
    $sign = '[+-]inf' if $sign eq '';	# +- doesn't matter, only that's inf
    $sign = "[$1]inf" if $sign =~ /^([+-])(inf)?$/;	# extract '+' or '-'
    return $x->{sign} =~ /^$sign$/ ? 1 : 0;
    }
  $x->{sign} =~ /^[+-]inf$/ ? 1 : 0;		# only +-inf is infinity
  }

sub is_one
  {
  # return true if arg (BINT or num_str) is +1, or -1 if sign is given
  my ($self,$x,$sign) = ref($_[0]) ? (undef,@_) : objectify(1,@_);
    
  $sign = '+' if !defined $sign || $sign ne '-';
 
  return 0 if $x->{sign} ne $sign; 	# -1 != +1, NaN, +-inf aren't either
  $CALC->_is_one($x->{value});
  }

sub is_odd
  {
  # return true when arg (BINT or num_str) is odd, false for even
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 0 if $x->{sign} !~ /^[+-]$/;			# NaN & +-inf aren't
  $CALC->_is_odd($x->{value});
  }

sub is_even
  {
  # return true when arg (BINT or num_str) is even, false for odd
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 0 if $x->{sign} !~ /^[+-]$/;			# NaN & +-inf aren't
  $CALC->_is_even($x->{value});
  }

sub is_positive
  {
  # return true when arg (BINT or num_str) is positive (>= 0)
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 1 if $x->{sign} eq '+inf';			# +inf is positive
 
  # 0+ is neither positive nor negative
  ($x->{sign} eq '+' && !$x->is_zero()) ? 1 : 0;	
  }

sub is_negative
  {
  # return true when arg (BINT or num_str) is negative (< 0)
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);
  
  $x->{sign} =~ /^-/ ? 1 : 0; 		# -inf is negative, but NaN is not
  }

sub is_int
  {
  # return true when arg (BINT or num_str) is an integer
  # always true for BigInt, but different for BigFloats
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);
  
  $x->{sign} =~ /^[+-]$/ ? 1 : 0;		# inf/-inf/NaN aren't
  }

###############################################################################

sub bmul 
  { 
  # multiply the first number by the second number
  # (BINT or num_str, BINT or num_str) return BINT

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

  return $upgrade->bmul($x,$upgrade->new($y),@r)
   if defined $upgrade && !$y->isa($self);
  
  $r[3] = $y;				# no push here

  $x->{sign} = $x->{sign} eq $y->{sign} ? '+' : '-'; # +1 * +1 or -1 * -1 => +

  $x->{value} = $CALC->_mul($x->{value},$y->{value});	# do actual math
  $x->{sign} = '+' if $CALC->_is_zero($x->{value}); 	# no -0

  $x->round(@r);
  }

sub bmuladd
  { 
  # multiply two numbers and then add the third to the result
  # (BINT or num_str, BINT or num_str, BINT or num_str) return BINT

  # set up parameters
  my ($self,$x,$y,$z,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$z,@r) = objectify(3,@_);
    }

  return $x if $x->modify('bmuladd');

  return $x->bnan() if  ($x->{sign} eq $nan) ||
			($y->{sign} eq $nan) ||
			($z->{sign} eq $nan);

  # inf handling of x and y
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
  # inf handling x*y and z
  if (($z->{sign} =~ /^[+-]inf$/))
    {
    # something +-inf => +-inf
    $x->{sign} = $z->{sign}, return $x if $z->{sign} =~ /^[+-]inf$/;
    }

  return $upgrade->bmuladd($x,$upgrade->new($y),$upgrade->new($z),@r)
   if defined $upgrade && (!$y->isa($self) || !$z->isa($self) || !$x->isa($self));
 
  # TODO: what if $y and $z have A or P set?
  $r[3] = $z;				# no push here

  $x->{sign} = $x->{sign} eq $y->{sign} ? '+' : '-'; # +1 * +1 or -1 * -1 => +

  $x->{value} = $CALC->_mul($x->{value},$y->{value});	# do actual math
  $x->{sign} = '+' if $CALC->_is_zero($x->{value}); 	# no -0

  my ($sx, $sz) = ( $x->{sign}, $z->{sign} ); 		# get signs

  if ($sx eq $sz)  
    {
    $x->{value} = $CALC->_add($x->{value},$z->{value});	# same sign, abs add
    }
  else 
    {
    my $a = $CALC->_acmp ($z->{value},$x->{value});	# absolute compare
    if ($a > 0)                           
      {
      $x->{value} = $CALC->_sub($z->{value},$x->{value},1); # abs sub w/ swap
      $x->{sign} = $sz;
      } 
    elsif ($a == 0)
      {
      # speedup, if equal, set result to 0
      $x->{value} = $CALC->_zero();
      $x->{sign} = '+';
      }
    else # a < 0
      {
      $x->{value} = $CALC->_sub($x->{value}, $z->{value}); # abs sub
      }
    }
  $x->round(@r);
  }

sub _div_inf
  {
  # helper function that handles +-inf cases for bdiv()/bmod() to reuse code
  my ($self,$x,$y) = @_;

  # NaN if x == NaN or y == NaN or x==y==0
  return wantarray ? ($x->bnan(),$self->bnan()) : $x->bnan()
   if (($x->is_nan() || $y->is_nan())   ||
       ($x->is_zero() && $y->is_zero()));
 
  # +-inf / +-inf == NaN, reminder also NaN
  if (($x->{sign} =~ /^[+-]inf$/) && ($y->{sign} =~ /^[+-]inf$/))
    {
    return wantarray ? ($x->bnan(),$self->bnan()) : $x->bnan();
    }
  # x / +-inf => 0, remainder x (works even if x == 0)
  if ($y->{sign} =~ /^[+-]inf$/)
    {
    my $t = $x->copy();		# bzero clobbers up $x
    return wantarray ? ($x->bzero(),$t) : $x->bzero()
    }
  
  # 5 / 0 => +inf, -6 / 0 => -inf
  # +inf / 0 = inf, inf,  and -inf / 0 => -inf, -inf 
  # exception:   -8 / 0 has remainder -8, not 8
  # exception: -inf / 0 has remainder -inf, not inf
  if ($y->is_zero())
    {
    # +-inf / 0 => special case for -inf
    return wantarray ?  ($x,$x->copy()) : $x if $x->is_inf();
    if (!$x->is_zero() && !$x->is_inf())
      {
      my $t = $x->copy();		# binf clobbers up $x
      return wantarray ?
       ($x->binf($x->{sign}),$t) : $x->binf($x->{sign})
      }
    }
  
  # last case: +-inf / ordinary number
  my $sign = '+inf';
  $sign = '-inf' if substr($x->{sign},0,1) ne $y->{sign};
  $x->{sign} = $sign;
  return wantarray ? ($x,$self->bzero()) : $x;
  }

sub bdiv 
  {
  # (dividend: BINT or num_str, divisor: BINT or num_str) return 
  # (BINT,BINT) (quo,rem) or BINT (only rem)
  
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it 
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    } 

  return $x if $x->modify('bdiv');

  return $self->_div_inf($x,$y)
   if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/) || $y->is_zero());

  return $upgrade->bdiv($upgrade->new($x),$upgrade->new($y),@r)
   if defined $upgrade;
   
  $r[3] = $y;					# no push!

  # calc new sign and in case $y == +/- 1, return $x
  my $xsign = $x->{sign};				# keep
  $x->{sign} = ($x->{sign} ne $y->{sign} ? '-' : '+'); 

  if (wantarray)
    {
    my $rem = $self->bzero(); 
    ($x->{value},$rem->{value}) = $CALC->_div($x->{value},$y->{value});
    $x->{sign} = '+' if $CALC->_is_zero($x->{value});
    $rem->{_a} = $x->{_a};
    $rem->{_p} = $x->{_p};
    $x->round(@r);
    if (! $CALC->_is_zero($rem->{value}))
      {
      $rem->{sign} = $y->{sign};
      $rem = $y->copy()->bsub($rem) if $xsign ne $y->{sign}; # one of them '-'
      }
    else
      {
      $rem->{sign} = '+';			# dont leave -0
      }
    $rem->round(@r);
    return ($x,$rem);
    }

  $x->{value} = $CALC->_div($x->{value},$y->{value});
  $x->{sign} = '+' if $CALC->_is_zero($x->{value});

  $x->round(@r);
  }

###############################################################################
# modulus functions

sub bmod 
  {
  # modulus (or remainder)
  # (BINT or num_str, BINT or num_str) return BINT
  
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $x if $x->modify('bmod');
  $r[3] = $y;					# no push!
  if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/) || $y->is_zero())
    {
    my ($d,$r) = $self->_div_inf($x,$y);
    $x->{sign} = $r->{sign};
    $x->{value} = $r->{value};
    return $x->round(@r);
    }

  # calc new sign and in case $y == +/- 1, return $x
  $x->{value} = $CALC->_mod($x->{value},$y->{value});
  if (!$CALC->_is_zero($x->{value}))
    {
    $x->{value} = $CALC->_sub($y->{value},$x->{value},1) 	# $y-$x
      if ($x->{sign} ne $y->{sign});
    $x->{sign} = $y->{sign};
    }
   else
    {
    $x->{sign} = '+';				# dont leave -0
    }
  $x->round(@r);
  }

sub bmodinv
  {
  # Modular inverse.  given a number which is (hopefully) relatively
  # prime to the modulus, calculate its inverse using Euclid's
  # alogrithm.  If the number is not relatively prime to the modulus
  # (i.e. their gcd is not one) then NaN is returned.

  # set up parameters
  my ($self,$x,$y,@r) = (undef,@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $x if $x->modify('bmodinv');

  return $x->bnan()
        if ($y->{sign} ne '+'                           # -, NaN, +inf, -inf
         || $x->is_zero()                               # or num == 0
         || $x->{sign} !~ /^[+-]$/                      # or num NaN, inf, -inf
        );

  # put least residue into $x if $x was negative, and thus make it positive
  $x->bmod($y) if $x->{sign} eq '-';

  my $sign;
  ($x->{value},$sign) = $CALC->_modinv($x->{value},$y->{value});
  return $x->bnan() if !defined $x->{value};		# in case no GCD found
  return $x if !defined $sign;			# already real result
  $x->{sign} = $sign;				# flip/flop see below
  $x->bmod($y);					# calc real result
  $x;
  }

sub bmodpow
  {
  # takes a very large number to a very large exponent in a given very
  # large modulus, quickly, thanks to binary exponentation. Supports
  # negative exponents.
  my ($self,$num,$exp,$mod,@r) = objectify(3,@_);

  return $num if $num->modify('bmodpow');

  # check modulus for valid values
  return $num->bnan() if ($mod->{sign} ne '+'		# NaN, - , -inf, +inf
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
  $num->{value} = $CALC->_modpow($num->{value},$exp->{value},$mod->{value});
  $num;
  }

###############################################################################

sub bfac
  {
  # (BINT or num_str, BINT or num_str) return BINT
  # compute factorial number from $x, modify $x in place
  my ($self,$x,@r) = ref($_[0]) ? (undef,@_) : objectify(1,@_);

  return $x if $x->modify('bfac') || $x->{sign} eq '+inf';	# inf => inf
  return $x->bnan() if $x->{sign} ne '+';			# NaN, <0 etc => NaN

  $x->{value} = $CALC->_fac($x->{value});
  $x->round(@r);
  }
 
sub bpow 
  {
  # (BINT or num_str, BINT or num_str) return BINT
  # compute power of two numbers -- stolen from Knuth Vol 2 pg 233
  # modifies first argument

  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $x if $x->modify('bpow');

  return $x->bnan() if $x->{sign} eq $nan || $y->{sign} eq $nan;

  # inf handling
  if (($x->{sign} =~ /^[+-]inf$/) || ($y->{sign} =~ /^[+-]inf$/))
    {
    if (($x->{sign} =~ /^[+-]inf$/) && ($y->{sign} =~ /^[+-]inf$/))
      {
      # +-inf ** +-inf
      return $x->bnan();
      }
    # +-inf ** Y
    if ($x->{sign} =~ /^[+-]inf/)
      {
      # +inf ** 0 => NaN
      return $x->bnan() if $y->is_zero();
      # -inf ** -1 => 1/inf => 0
      return $x->bzero() if $y->is_one('-') && $x->is_negative();

      # +inf ** Y => inf
      return $x if $x->{sign} eq '+inf';

      # -inf ** Y => -inf if Y is odd
      return $x if $y->is_odd();
      return $x->babs();
      }
    # X ** +-inf

    # 1 ** +inf => 1
    return $x if $x->is_one();
    
    # 0 ** inf => 0
    return $x if $x->is_zero() && $y->{sign} =~ /^[+]/;

    # 0 ** -inf => inf
    return $x->binf() if $x->is_zero();

    # -1 ** -inf => NaN
    return $x->bnan() if $x->is_one('-') && $y->{sign} =~ /^[-]/;

    # -X ** -inf => 0
    return $x->bzero() if $x->{sign} eq '-' && $y->{sign} =~ /^[-]/;

    # -1 ** inf => NaN
    return $x->bnan() if $x->{sign} eq '-';

    # X ** inf => inf
    return $x->binf() if $y->{sign} =~ /^[+]/;
    # X ** -inf => 0
    return $x->bzero();
    }

  return $upgrade->bpow($upgrade->new($x),$y,@r)
   if defined $upgrade && (!$y->isa($self) || $y->{sign} eq '-');

  $r[3] = $y;					# no push!

  # cases 0 ** Y, X ** 0, X ** 1, 1 ** Y are handled by Calc or Emu

  my $new_sign = '+';
  $new_sign = $y->is_odd() ? '-' : '+' if ($x->{sign} ne '+'); 

  # 0 ** -7 => ( 1 / (0 ** 7)) => 1 / 0 => +inf 
  return $x->binf() 
    if $y->{sign} eq '-' && $x->{sign} eq '+' && $CALC->_is_zero($x->{value});
  # 1 ** -y => 1 / (1 ** |y|)
  # so do test for negative $y after above's clause
  return $x->bnan() if $y->{sign} eq '-' && !$CALC->_is_one($x->{value});

  $x->{value} = $CALC->_pow($x->{value},$y->{value});
  $x->{sign} = $new_sign;
  $x->{sign} = '+' if $CALC->_is_zero($y->{value});
  $x->round(@r);
  }

sub blsft 
  {
  # (BINT or num_str, BINT or num_str) return BINT
  # compute x << y, base n, y >= 0
 
  # set up parameters
  my ($self,$x,$y,$n,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$n,@r) = objectify(2,@_);
    }

  return $x if $x->modify('blsft');
  return $x->bnan() if ($x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/);
  return $x->round(@r) if $y->is_zero();

  $n = 2 if !defined $n; return $x->bnan() if $n <= 0 || $y->{sign} eq '-';

  $x->{value} = $CALC->_lsft($x->{value},$y->{value},$n);
  $x->round(@r);
  }

sub brsft 
  {
  # (BINT or num_str, BINT or num_str) return BINT
  # compute x >> y, base n, y >= 0
  
  # set up parameters
  my ($self,$x,$y,$n,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$n,@r) = objectify(2,@_);
    }

  return $x if $x->modify('brsft');
  return $x->bnan() if ($x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/);
  return $x->round(@r) if $y->is_zero();
  return $x->bzero(@r) if $x->is_zero();		# 0 => 0

  $n = 2 if !defined $n; return $x->bnan() if $n <= 0 || $y->{sign} eq '-';

   # this only works for negative numbers when shifting in base 2
  if (($x->{sign} eq '-') && ($n == 2))
    {
    return $x->round(@r) if $x->is_one('-');	# -1 => -1
    if (!$y->is_one())
      {
      # although this is O(N*N) in calc (as_bin!) it is O(N) in Pari et al
      # but perhaps there is a better emulation for two's complement shift...
      # if $y != 1, we must simulate it by doing:
      # convert to bin, flip all bits, shift, and be done
      $x->binc();			# -3 => -2
      my $bin = $x->as_bin();
      $bin =~ s/^-0b//;			# strip '-0b' prefix
      $bin =~ tr/10/01/;		# flip bits
      # now shift
      if ($y >= CORE::length($bin))
        {
	$bin = '0'; 			# shifting to far right creates -1
					# 0, because later increment makes 
					# that 1, attached '-' makes it '-1'
					# because -1 >> x == -1 !
        } 
      else
	{
	$bin =~ s/.{$y}$//;		# cut off at the right side
        $bin = '1' . $bin;		# extend left side by one dummy '1'
        $bin =~ tr/10/01/;		# flip bits back
	}
      my $res = $self->new('0b'.$bin);	# add prefix and convert back
      $res->binc();			# remember to increment
      $x->{value} = $res->{value};	# take over value
      return $x->round(@r);		# we are done now, magic, isn't?
      }
    # x < 0, n == 2, y == 1
    $x->bdec();				# n == 2, but $y == 1: this fixes it
    }

  $x->{value} = $CALC->_rsft($x->{value},$y->{value},$n);
  $x->round(@r);
  }

sub band 
  {
  #(BINT or num_str, BINT or num_str) return BINT
  # compute x & y
 
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }
  
  return $x if $x->modify('band');

  $r[3] = $y;				# no push!

  return $x->bnan() if ($x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/);

  my $sx = $x->{sign} eq '+' ? 1 : -1;
  my $sy = $y->{sign} eq '+' ? 1 : -1;
  
  if ($sx == 1 && $sy == 1)
    {
    $x->{value} = $CALC->_and($x->{value},$y->{value});
    return $x->round(@r);
    }
  
  if ($CAN{signed_and})
    {
    $x->{value} = $CALC->_signed_and($x->{value},$y->{value},$sx,$sy);
    return $x->round(@r);
    }
 
  require $EMU_LIB;
  __emu_band($self,$x,$y,$sx,$sy,@r);
  }

sub bior 
  {
  #(BINT or num_str, BINT or num_str) return BINT
  # compute x | y
  
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $x if $x->modify('bior');
  $r[3] = $y;				# no push!

  return $x->bnan() if ($x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/);

  my $sx = $x->{sign} eq '+' ? 1 : -1;
  my $sy = $y->{sign} eq '+' ? 1 : -1;

  # the sign of X follows the sign of X, e.g. sign of Y irrelevant for bior()
  
  # don't use lib for negative values
  if ($sx == 1 && $sy == 1)
    {
    $x->{value} = $CALC->_or($x->{value},$y->{value});
    return $x->round(@r);
    }

  # if lib can do negative values, let it handle this
  if ($CAN{signed_or})
    {
    $x->{value} = $CALC->_signed_or($x->{value},$y->{value},$sx,$sy);
    return $x->round(@r);
    }

  require $EMU_LIB;
  __emu_bior($self,$x,$y,$sx,$sy,@r);
  }

sub bxor 
  {
  #(BINT or num_str, BINT or num_str) return BINT
  # compute x ^ y
  
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $x if $x->modify('bxor');
  $r[3] = $y;				# no push!

  return $x->bnan() if ($x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/);
  
  my $sx = $x->{sign} eq '+' ? 1 : -1;
  my $sy = $y->{sign} eq '+' ? 1 : -1;

  # don't use lib for negative values
  if ($sx == 1 && $sy == 1)
    {
    $x->{value} = $CALC->_xor($x->{value},$y->{value});
    return $x->round(@r);
    }
  
  # if lib can do negative values, let it handle this
  if ($CAN{signed_xor})
    {
    $x->{value} = $CALC->_signed_xor($x->{value},$y->{value},$sx,$sy);
    return $x->round(@r);
    }

  require $EMU_LIB;
  __emu_bxor($self,$x,$y,$sx,$sy,@r);
  }

sub length
  {
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  my $e = $CALC->_len($x->{value}); 
  wantarray ? ($e,0) : $e;
  }

sub digit
  {
  # return the nth decimal digit, negative values count backward, 0 is right
  my ($self,$x,$n) = ref($_[0]) ? (undef,@_) : objectify(1,@_);

  $n = $n->numify() if ref($n);
  $CALC->_digit($x->{value},$n||0);
  }

sub _trailing_zeros
  {
  # return the amount of trailing zeros in $x (as scalar)
  my $x = shift;
  $x = $class->new($x) unless ref $x;

  return 0 if $x->{sign} !~ /^[+-]$/;	# NaN, inf, -inf etc

  $CALC->_zeros($x->{value});		# must handle odd values, 0 etc
  }

sub bsqrt
  {
  # calculate square root of $x
  my ($self,$x,@r) = ref($_[0]) ? (undef,@_) : objectify(1,@_);

  return $x if $x->modify('bsqrt');

  return $x->bnan() if $x->{sign} !~ /^\+/;	# -x or -inf or NaN => NaN
  return $x if $x->{sign} eq '+inf';		# sqrt(+inf) == inf

  return $upgrade->bsqrt($x,@r) if defined $upgrade;

  $x->{value} = $CALC->_sqrt($x->{value});
  $x->round(@r);
  }

sub broot
  {
  # calculate $y'th root of $x
 
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);

  $y = $self->new(2) unless defined $y;

  # objectify is costly, so avoid it
  if ((!ref($x)) || (ref($x) ne ref($y)))
    {
    ($self,$x,$y,@r) = objectify(2,$self || $class,@_);
    }

  return $x if $x->modify('broot');

  # NaN handling: $x ** 1/0, x or y NaN, or y inf/-inf or y == 0
  return $x->bnan() if $x->{sign} !~ /^\+/ || $y->is_zero() ||
         $y->{sign} !~ /^\+$/;

  return $x->round(@r)
    if $x->is_zero() || $x->is_one() || $x->is_inf() || $y->is_one();

  return $upgrade->new($x)->broot($upgrade->new($y),@r) if defined $upgrade;

  $x->{value} = $CALC->_root($x->{value},$y->{value});
  $x->round(@r);
  }

sub exponent
  {
  # return a copy of the exponent (here always 0, NaN or 1 for $m == 0)
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);
 
  if ($x->{sign} !~ /^[+-]$/)
    {
    my $s = $x->{sign}; $s =~ s/^[+-]//;  # NaN, -inf,+inf => NaN or inf
    return $self->new($s);
    }
  return $self->bone() if $x->is_zero();

  # 12300 => 2 trailing zeros => exponent is 2
  $self->new( $CALC->_zeros($x->{value}) );
  }

sub mantissa
  {
  # return the mantissa (compatible to Math::BigFloat, e.g. reduced)
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)
    {
    # for NaN, +inf, -inf: keep the sign
    return $self->new($x->{sign});
    }
  my $m = $x->copy(); delete $m->{_p}; delete $m->{_a};

  # that's a bit inefficient:
  my $zeros = $CALC->_zeros($m->{value});
  $m->brsft($zeros,10) if $zeros != 0;
  $m;
  }

sub parts
  {
  # return a copy of both the exponent and the mantissa
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  ($x->mantissa(),$x->exponent());
  }
   
##############################################################################
# rounding functions

sub bfround
  {
  # precision: round to the $Nth digit left (+$n) or right (-$n) from the '.'
  # $n == 0 || $n == 1 => round to integer
  my $x = shift; my $self = ref($x) || $x; $x = $self->new($x) unless ref $x;

  my ($scale,$mode) = $x->_scale_p(@_);

  return $x if !defined $scale || $x->modify('bfround');	# no-op

  # no-op for BigInts if $n <= 0
  $x->bround( $x->length()-$scale, $mode) if $scale > 0;

  delete $x->{_a};	# delete to save memory
  $x->{_p} = $scale;	# store new _p
  $x;
  }

sub _scan_for_nonzero
  {
  # internal, used by bround() to scan for non-zeros after a '5'
  my ($x,$pad,$xs,$len) = @_;
 
  return 0 if $len == 1;		# "5" is trailed by invisible zeros
  my $follow = $pad - 1;
  return 0 if $follow > $len || $follow < 1;

  # use the string form to check whether only '0's follow or not
  substr ($xs,-$follow) =~ /[^0]/ ? 1 : 0;
  }

sub fround
  {
  # Exists to make life easier for switch between MBF and MBI (should we
  # autoload fxxx() like MBF does for bxxx()?)
  my $x = shift; $x = $class->new($x) unless ref $x;
  $x->bround(@_);
  }

sub bround
  {
  # accuracy: +$n preserve $n digits from left,
  #           -$n preserve $n digits from right (f.i. for 0.1234 style in MBF)
  # no-op for $n == 0
  # and overwrite the rest with 0's, return normalized number
  # do not return $x->bnorm(), but $x

  my $x = shift; $x = $class->new($x) unless ref $x;
  my ($scale,$mode) = $x->_scale_a(@_);
  return $x if !defined $scale || $x->modify('bround');	# no-op
  
  if ($x->is_zero() || $scale == 0)
    {
    $x->{_a} = $scale if !defined $x->{_a} || $x->{_a} > $scale; # 3 > 2
    return $x;
    }
  return $x if $x->{sign} !~ /^[+-]$/;		# inf, NaN

  # we have fewer digits than we want to scale to
  my $len = $x->length();
  # convert $scale to a scalar in case it is an object (put's a limit on the
  # number length, but this would already limited by memory constraints), makes
  # it faster
  $scale = $scale->numify() if ref ($scale);

  # scale < 0, but > -len (not >=!)
  if (($scale < 0 && $scale < -$len-1) || ($scale >= $len))
    {
    $x->{_a} = $scale if !defined $x->{_a} || $x->{_a} > $scale; # 3 > 2
    return $x; 
    }
   
  # count of 0's to pad, from left (+) or right (-): 9 - +6 => 3, or |-6| => 6
  my ($pad,$digit_round,$digit_after);
  $pad = $len - $scale;
  $pad = abs($scale-1) if $scale < 0;

  # do not use digit(), it is very costly for binary => decimal
  # getting the entire string is also costly, but we need to do it only once
  my $xs = $CALC->_str($x->{value});
  my $pl = -$pad-1;

  # pad:   123: 0 => -1, at 1 => -2, at 2 => -3, at 3 => -4
  # pad+1: 123: 0 => 0,  at 1 => -1, at 2 => -2, at 3 => -3
  $digit_round = '0'; $digit_round = substr($xs,$pl,1) if $pad <= $len;
  $pl++; $pl ++ if $pad >= $len;
  $digit_after = '0'; $digit_after = substr($xs,$pl,1) if $pad > 0;

  # in case of 01234 we round down, for 6789 up, and only in case 5 we look
  # closer at the remaining digits of the original $x, remember decision
  my $round_up = 1;					# default round up
  $round_up -- if
    ($mode eq 'trunc')				||	# trunc by round down
    ($digit_after =~ /[01234]/)			|| 	# round down anyway,
							# 6789 => round up
    ($digit_after eq '5')			&&	# not 5000...0000
    ($x->_scan_for_nonzero($pad,$xs,$len) == 0)		&&
    (
     ($mode eq 'even') && ($digit_round =~ /[24680]/) ||
     ($mode eq 'odd')  && ($digit_round =~ /[13579]/) ||
     ($mode eq '+inf') && ($x->{sign} eq '-')   ||
     ($mode eq '-inf') && ($x->{sign} eq '+')   ||
     ($mode eq 'zero')		# round down if zero, sign adjusted below
    );
  my $put_back = 0;					# not yet modified
	
  if (($pad > 0) && ($pad <= $len))
    {
    substr($xs,-$pad,$pad) = '0' x $pad;		# replace with '00...'
    $put_back = 1;					# need to put back
    }
  elsif ($pad > $len)
    {
    $x->bzero();					# round to '0'
    }

  if ($round_up)					# what gave test above?
    {
    $put_back = 1;					# need to put back
    $pad = $len, $xs = '0' x $pad if $scale < 0;	# tlr: whack 0.51=>1.0	

    # we modify directly the string variant instead of creating a number and
    # adding it, since that is faster (we already have the string)
    my $c = 0; $pad ++;				# for $pad == $len case
    while ($pad <= $len)
      {
      $c = substr($xs,-$pad,1) + 1; $c = '0' if $c eq '10';
      substr($xs,-$pad,1) = $c; $pad++;
      last if $c != 0;				# no overflow => early out
      }
    $xs = '1'.$xs if $c == 0;

    }
  $x->{value} = $CALC->_new($xs) if $put_back == 1;	# put back, if needed

  $x->{_a} = $scale if $scale >= 0;
  if ($scale < 0)
    {
    $x->{_a} = $len+$scale;
    $x->{_a} = 0 if $scale < -$len;
    }
  $x;
  }

sub bfloor
  {
  # return integer less or equal then number; no-op since it's already integer
  my ($self,$x,@r) = ref($_[0]) ? (undef,@_) : objectify(1,@_);

  $x->round(@r);
  }

sub bceil
  {
  # return integer greater or equal then number; no-op since it's already int
  my ($self,$x,@r) = ref($_[0]) ? (undef,@_) : objectify(1,@_);

  $x->round(@r);
  }

sub as_number
  {
  # An object might be asked to return itself as bigint on certain overloaded
  # operations. This does exactly this, so that sub classes can simple inherit
  # it or override with their own integer conversion routine.
  $_[0]->copy();
  }

sub as_hex
  {
  # return as hex string, with prefixed 0x
  my $x = shift; $x = $class->new($x) if !ref($x);

  return $x->bstr() if $x->{sign} !~ /^[+-]$/;	# inf, nan etc

  my $s = '';
  $s = $x->{sign} if $x->{sign} eq '-';
  $s . $CALC->_as_hex($x->{value});
  }

sub as_bin
  {
  # return as binary string, with prefixed 0b
  my $x = shift; $x = $class->new($x) if !ref($x);

  return $x->bstr() if $x->{sign} !~ /^[+-]$/;	# inf, nan etc

  my $s = ''; $s = $x->{sign} if $x->{sign} eq '-';
  return $s . $CALC->_as_bin($x->{value});
  }

sub as_oct
  {
  # return as octal string, with prefixed 0
  my $x = shift; $x = $class->new($x) if !ref($x);

  return $x->bstr() if $x->{sign} !~ /^[+-]$/;	# inf, nan etc

  my $s = ''; $s = $x->{sign} if $x->{sign} eq '-';
  return $s . $CALC->_as_oct($x->{value});
  }

##############################################################################
# private stuff (internal use only)

sub objectify
  {
  # check for strings, if yes, return objects instead
 
  # the first argument is number of args objectify() should look at it will
  # return $count+1 elements, the first will be a classname. This is because
  # overloaded '""' calls bstr($object,undef,undef) and this would result in
  # useless objects being created and thrown away. So we cannot simple loop
  # over @_. If the given count is 0, all arguments will be used.
 
  # If the second arg is a ref, use it as class.
  # If not, try to use it as classname, unless undef, then use $class 
  # (aka Math::BigInt). The latter shouldn't happen,though.

  # caller:			   gives us:
  # $x->badd(1);                => ref x, scalar y
  # Class->badd(1,2);           => classname x (scalar), scalar x, scalar y
  # Class->badd( Class->(1),2); => classname x (scalar), ref x, scalar y
  # Math::BigInt::badd(1,2);    => scalar x, scalar y
  # In the last case we check number of arguments to turn it silently into
  # $class,1,2. (We can not take '1' as class ;o)
  # badd($class,1) is not supported (it should, eventually, try to add undef)
  # currently it tries 'Math::BigInt' + 1, which will not work.

  # some shortcut for the common cases
  # $x->unary_op();
  return (ref($_[1]),$_[1]) if (@_ == 2) && ($_[0]||0 == 1) && ref($_[1]);

  my $count = abs(shift || 0);
  
  my (@a,$k,$d);		# resulting array, temp, and downgrade 
  if (ref $_[0])
    {
    # okay, got object as first
    $a[0] = ref $_[0];
    }
  else
    {
    # nope, got 1,2 (Class->xxx(1) => Class,1 and not supported)
    $a[0] = $class;
    $a[0] = shift if $_[0] =~ /^[A-Z].*::/;	# classname as first?
    }

  no strict 'refs';
  # disable downgrading, because Math::BigFLoat->foo('1.0','2.0') needs floats
  if (defined ${"$a[0]::downgrade"})
    {
    $d = ${"$a[0]::downgrade"};
    ${"$a[0]::downgrade"} = undef;
    }

  my $up = ${"$a[0]::upgrade"};
  # print STDERR "# Now in objectify, my class is today $a[0], count = $count\n";
  if ($count == 0)
    {
    while (@_)
      {
      $k = shift;
      if (!ref($k))
        {
        $k = $a[0]->new($k);
        }
      elsif (!defined $up && ref($k) ne $a[0])
	{
	# foreign object, try to convert to integer
        $k->can('as_number') ?  $k = $k->as_number() : $k = $a[0]->new($k);
	}
      push @a,$k;
      }
    }
  else
    {
    while ($count > 0)
      {
      $count--; 
      $k = shift;
      if (!ref($k))
        {
        $k = $a[0]->new($k);
        }
      elsif (!defined $up && ref($k) ne $a[0])
	{
	# foreign object, try to convert to integer
        $k->can('as_number') ? $k = $k->as_number() : $k = $a[0]->new($k);
	}
      push @a,$k;
      }
    push @a,@_;		# return other params, too
    }
  if (! wantarray)
    {
    require Carp; Carp::croak ("$class objectify needs list context");
    }
  ${"$a[0]::downgrade"} = $d;
  @a;
  }

sub _register_callback
  {
  my ($class,$callback) = @_;

  if (ref($callback) ne 'CODE')
    { 
    require Carp;
    Carp::croak ("$callback is not a coderef");
    }
  $CALLBACKS{$class} = $callback;
  }

sub import 
  {
  my $self = shift;

  $IMPORT++;				# remember we did import()
  my @a; my $l = scalar @_;
  my $warn_or_die = 0;			# 0 - no warn, 1 - warn, 2 - die
  for ( my $i = 0; $i < $l ; $i++ )
    {
    if ($_[$i] eq ':constant')
      {
      # this causes overlord er load to step in
      overload::constant 
	integer => sub { $self->new(shift) },
      	binary => sub { $self->new(shift) };
      }
    elsif ($_[$i] eq 'upgrade')
      {
      # this causes upgrading
      $upgrade = $_[$i+1];		# or undef to disable
      $i++;
      }
    elsif ($_[$i] =~ /^(lib|try|only)\z/)
      {
      # this causes a different low lib to take care...
      $CALC = $_[$i+1] || '';
      # lib => 1 (warn on fallback), try => 0 (no warn), only => 2 (die on fallback)
      $warn_or_die = 1 if $_[$i] eq 'lib';
      $warn_or_die = 2 if $_[$i] eq 'only';
      $i++;
      }
    else
      {
      push @a, $_[$i];
      }
    }
  # any non :constant stuff is handled by our parent, Exporter
  if (@a > 0)
    {
    require Exporter;
 
    $self->SUPER::import(@a);			# need it for subclasses
    $self->export_to_level(1,$self,@a);		# need it for MBF
    }

  # try to load core math lib
  my @c = split /\s*,\s*/,$CALC;
  foreach (@c)
    {
    $_ =~ tr/a-zA-Z0-9://cd;			# limit to sane characters
    }
  push @c, \'FastCalc', \'Calc'			# if all fail, try these
    if $warn_or_die < 2;			# but not for "only"
  $CALC = '';					# signal error
  foreach my $l (@c)
    {
    # fallback libraries are "marked" as \'string', extract string if nec.
    my $lib = $l; $lib = $$l if ref($l);

    next if ($lib || '') eq '';
    $lib = 'Math::BigInt::'.$lib if $lib !~ /^Math::BigInt/i;
    $lib =~ s/\.pm$//;
    if ($] < 5.006)
      {
      # Perl < 5.6.0 dies with "out of memory!" when eval("") and ':constant' is
      # used in the same script, or eval("") inside import().
      my @parts = split /::/, $lib;             # Math::BigInt => Math BigInt
      my $file = pop @parts; $file .= '.pm';    # BigInt => BigInt.pm
      require File::Spec;
      $file = File::Spec->catfile (@parts, $file);
      eval { require "$file"; $lib->import( @c ); }
      }
    else
      {
      eval "use $lib qw/@c/;";
      }
    if ($@ eq '')
      {
      my $ok = 1;
      # loaded it ok, see if the api_version() is high enough
      if ($lib->can('api_version') && $lib->api_version() >= 1.0)
	{
	$ok = 0;
	# api_version matches, check if it really provides anything we need
        for my $method (qw/
		one two ten
		str num
		add mul div sub dec inc
		acmp len digit is_one is_zero is_even is_odd
		is_two is_ten
		zeros new copy check
		from_hex from_oct from_bin as_hex as_bin as_oct
		rsft lsft xor and or
		mod sqrt root fac pow modinv modpow log_int gcd
	 /)
          {
	  if (!$lib->can("_$method"))
	    {
	    if (($WARN{$lib}||0) < 2)
	      {
	      require Carp;
	      Carp::carp ("$lib is missing method '_$method'");
	      $WARN{$lib} = 1;		# still warn about the lib
	      }
            $ok++; last; 
	    }
          }
	}
      if ($ok == 0)
	{
	$CALC = $lib;
	if ($warn_or_die > 0 && ref($l))
	  {
	  require Carp;
	  my $msg = "Math::BigInt: couldn't load specified math lib(s), fallback to $lib";
          Carp::carp ($msg) if $warn_or_die == 1;
          Carp::croak ($msg) if $warn_or_die == 2;
	  }
        last;			# found a usable one, break
	}
      else
	{
	if (($WARN{$lib}||0) < 2)
	  {
	  my $ver = eval "\$$lib\::VERSION" || 'unknown';
	  require Carp;
	  Carp::carp ("Cannot load outdated $lib v$ver, please upgrade");
	  $WARN{$lib} = 2;		# never warn again
	  }
        }
      }
    }
  if ($CALC eq '')
    {
    require Carp;
    if ($warn_or_die == 2)
      {
      Carp::croak ("Couldn't load specified math lib(s) and fallback disallowed");
      }
    else
      {
      Carp::croak ("Couldn't load any math lib(s), not even fallback to Calc.pm");
      }
    }

  # notify callbacks
  foreach my $class (keys %CALLBACKS)
    {
    &{$CALLBACKS{$class}}($CALC);
    }

  # Fill $CAN with the results of $CALC->can(...) for emulating lower math lib
  # functions

  %CAN = ();
  for my $method (qw/ signed_and signed_or signed_xor /)
    {
    $CAN{$method} = $CALC->can("_$method") ? 1 : 0;
    }

  # import done
  }

sub from_hex
  {
  # create a bigint from a hexadecimal string
  my ($self, $hs) = @_;

  my $rc = __from_hex($hs);

  return $self->bnan() unless defined $rc;

  $rc;
  }  

sub from_bin
  {
  # create a bigint from a hexadecimal string
  my ($self, $bs) = @_;

  my $rc = __from_bin($bs);

  return $self->bnan() unless defined $rc;

  $rc;
  }  

sub from_oct
  {
  # create a bigint from a hexadecimal string
  my ($self, $os) = @_;

  my $x = $self->bzero();
  
  # strip underscores
  $os =~ s/([0-7])_([0-7])/$1$2/g;	
  $os =~ s/([0-7])_([0-7])/$1$2/g;	
  
  return $x->bnan() if $os !~ /^[\-\+]?0[0-7]+\z/;

  my $sign = '+'; $sign = '-' if $os =~ /^-/;

  $os =~ s/^[+-]//;						# strip sign
  $x->{value} = $CALC->_from_oct($os);
  $x->{sign} = $sign unless $CALC->_is_zero($x->{value}); 	# no '-0'
  $x;
  }

sub __from_hex
  {
  # internal
  # convert a (ref to) big hex string to BigInt, return undef for error
  my $hs = shift;

  my $x = Math::BigInt->bzero();
  
  # strip underscores
  $hs =~ s/([0-9a-fA-F])_([0-9a-fA-F])/$1$2/g;	
  $hs =~ s/([0-9a-fA-F])_([0-9a-fA-F])/$1$2/g;	
  
  return $x->bnan() if $hs !~ /^[\-\+]?0x[0-9A-Fa-f]+$/;

  my $sign = '+'; $sign = '-' if $hs =~ /^-/;

  $hs =~ s/^[+-]//;						# strip sign
  $x->{value} = $CALC->_from_hex($hs);
  $x->{sign} = $sign unless $CALC->_is_zero($x->{value}); 	# no '-0'
  $x;
  }

sub __from_bin
  {
  # internal
  # convert a (ref to) big binary string to BigInt, return undef for error
  my $bs = shift;

  my $x = Math::BigInt->bzero();

  # strip underscores
  $bs =~ s/([01])_([01])/$1$2/g;	
  $bs =~ s/([01])_([01])/$1$2/g;	
  return $x->bnan() if $bs !~ /^[+-]?0b[01]+$/;

  my $sign = '+'; $sign = '-' if $bs =~ /^\-/;
  $bs =~ s/^[+-]//;						# strip sign

  $x->{value} = $CALC->_from_bin($bs);
  $x->{sign} = $sign unless $CALC->_is_zero($x->{value}); 	# no '-0'
  $x;
  }

sub _split
  {
  # input: num_str; output: undef for invalid or
  # (\$mantissa_sign,\$mantissa_value,\$mantissa_fraction,\$exp_sign,\$exp_value)
  # Internal, take apart a string and return the pieces.
  # Strip leading/trailing whitespace, leading zeros, underscore and reject
  # invalid input.
  my $x = shift;

  # strip white space at front, also extranous leading zeros
  $x =~ s/^\s*([-]?)0*([0-9])/$1$2/g;   # will not strip '  .2'
  $x =~ s/^\s+//;                       # but this will
  $x =~ s/\s+$//g;                      # strip white space at end

  # shortcut, if nothing to split, return early
  if ($x =~ /^[+-]?[0-9]+\z/)
    {
    $x =~ s/^([+-])0*([0-9])/$2/; my $sign = $1 || '+';
    return (\$sign, \$x, \'', \'', \0);
    }

  # invalid starting char?
  return if $x !~ /^[+-]?(\.?[0-9]|0b[0-1]|0x[0-9a-fA-F])/;

  return __from_hex($x) if $x =~ /^[\-\+]?0x/;		# hex string
  return __from_bin($x) if $x =~ /^[\-\+]?0b/;		# binary string
  
  # strip underscores between digits
  $x =~ s/([0-9])_([0-9])/$1$2/g;
  $x =~ s/([0-9])_([0-9])/$1$2/g;		# do twice for 1_2_3

  # some possible inputs: 
  # 2.1234 # 0.12        # 1 	      # 1E1 # 2.134E1 # 434E-10 # 1.02009E-2 
  # .2 	   # 1_2_3.4_5_6 # 1.4E1_2_3  # 1e3 # +.2     # 0e999	

  my ($m,$e,$last) = split /[Ee]/,$x;
  return if defined $last;		# last defined => 1e2E3 or others
  $e = '0' if !defined $e || $e eq "";

  # sign,value for exponent,mantint,mantfrac
  my ($es,$ev,$mis,$miv,$mfv);
  # valid exponent?
  if ($e =~ /^([+-]?)0*([0-9]+)$/)	# strip leading zeros
    {
    $es = $1; $ev = $2;
    # valid mantissa?
    return if $m eq '.' || $m eq '';
    my ($mi,$mf,$lastf) = split /\./,$m;
    return if defined $lastf;		# lastf defined => 1.2.3 or others
    $mi = '0' if !defined $mi;
    $mi .= '0' if $mi =~ /^[\-\+]?$/;
    $mf = '0' if !defined $mf || $mf eq '';
    if ($mi =~ /^([+-]?)0*([0-9]+)$/)		# strip leading zeros
      {
      $mis = $1||'+'; $miv = $2;
      return unless ($mf =~ /^([0-9]*?)0*$/);	# strip trailing zeros
      $mfv = $1;
      # handle the 0e999 case here
      $ev = 0 if $miv eq '0' && $mfv eq '';
      return (\$mis,\$miv,\$mfv,\$es,\$ev);
      }
    }
  return; # NaN, not a number
  }

##############################################################################
# internal calculation routines (others are in Math::BigInt::Calc etc)

sub __lcm 
  { 
  # (BINT or num_str, BINT or num_str) return BINT
  # does modify first argument
  # LCM
 
  my ($x,$ty) = @_;
  return $x->bnan() if ($x->{sign} eq $nan) || ($ty->{sign} eq $nan);
  my $method = ref($x) . '::bgcd';
  no strict 'refs';
  $x * $ty / &$method($x,$ty);
  }

###############################################################################
# trigonometric functions

sub bpi
  {
  # Calculate PI to N digits. Unless upgrading is in effect, returns the
  # result truncated to an integer, that is, always returns '3'.
  my ($self,$n) = @_;
  if (@_ == 1)
    {
    # called like Math::BigInt::bpi(10);
    $n = $self; $self = $class;
    }
  $self = ref($self) if ref($self);

  return $upgrade->new($n) if defined $upgrade;

  # hard-wired to "3"
  $self->new(3);
  }

sub bcos
  {
  # Calculate cosinus(x) to N digits. Unless upgrading is in effect, returns the
  # result truncated to an integer.
  my ($self,$x,@r) = ref($_[0]) ? (undef,@_) : objectify(1,@_);

  return $x if $x->modify('bcos');

  return $x->bnan() if $x->{sign} !~ /^[+-]\z/;	# -inf +inf or NaN => NaN

  return $upgrade->new($x)->bcos(@r) if defined $upgrade;

  require Math::BigFloat;
  # calculate the result and truncate it to integer
  my $t = Math::BigFloat->new($x)->bcos(@r)->as_int();

  $x->bone() if $t->is_one();
  $x->bzero() if $t->is_zero();
  $x->round(@r);
  }

sub bsin
  {
  # Calculate sinus(x) to N digits. Unless upgrading is in effect, returns the
  # result truncated to an integer.
  my ($self,$x,@r) = ref($_[0]) ? (undef,@_) : objectify(1,@_);

  return $x if $x->modify('bsin');

  return $x->bnan() if $x->{sign} !~ /^[+-]\z/;	# -inf +inf or NaN => NaN

  return $upgrade->new($x)->bsin(@r) if defined $upgrade;

  require Math::BigFloat;
  # calculate the result and truncate it to integer
  my $t = Math::BigFloat->new($x)->bsin(@r)->as_int();

  $x->bone() if $t->is_one();
  $x->bzero() if $t->is_zero();
  $x->round(@r);
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

  # Y    X
  # != 0 -inf result is +- pi
  if ($x->is_inf() || $y->is_inf())
    {
    # upgrade to BigFloat etc.
    return $upgrade->new($y)->batan2($upgrade->new($x),@r) if defined $upgrade;
    if ($y->is_inf())
      {
      if ($x->{sign} eq '-inf')
        {
        # calculate 3 pi/4 => 2.3.. => 2
        $y->bone( substr($y->{sign},0,1) );
        $y->bmul($self->new(2));
        }
      elsif ($x->{sign} eq '+inf')
        {
        # calculate pi/4 => 0.7 => 0
        $y->bzero();
        }
      else
        {
        # calculate pi/2 => 1.5 => 1
        $y->bone( substr($y->{sign},0,1) );
        }
      }
    else
      {
      if ($x->{sign} eq '+inf')
        {
        # calculate pi/4 => 0.7 => 0
        $y->bzero();
        }
      else
        {
        # PI => 3.1415.. => 3
        $y->bone( substr($y->{sign},0,1) );
        $y->bmul($self->new(3));
        }
      }
    return $y;
    }

  return $upgrade->new($y)->batan2($upgrade->new($x),@r) if defined $upgrade;

  require Math::BigFloat;
  my $r = Math::BigFloat->new($y)->batan2(Math::BigFloat->new($x),@r)->as_int();

  $x->{value} = $r->{value};
  $x->{sign} = $r->{sign};

  $x;
  }

sub batan
  {
  # Calculate arcus tangens of x to N digits. Unless upgrading is in effect, returns the
  # result truncated to an integer.
  my ($self,$x,@r) = ref($_[0]) ? (undef,@_) : objectify(1,@_);

  return $x if $x->modify('batan');

  return $x->bnan() if $x->{sign} !~ /^[+-]\z/;	# -inf +inf or NaN => NaN

  return $upgrade->new($x)->batan(@r) if defined $upgrade;

  # calculate the result and truncate it to integer
  my $t = Math::BigFloat->new($x)->batan(@r);

  $x->{value} = $CALC->_new( $x->as_int()->bstr() );
  $x->round(@r);
  }

###############################################################################
# this method returns 0 if the object can be modified, or 1 if not.
# We use a fast constant sub() here, to avoid costly calls. Subclasses
# may override it with special code (f.i. Math::BigInt::Constant does so)

sub modify () { 0; }

1;
__END__

=pod

=head1 NAME

Math::BigInt - Arbitrary size integer/float math package

=head1 SYNOPSIS

  use Math::BigInt;

  # or make it faster with huge numbers: install (optional)
  # Math::BigInt::GMP and always use (it will fall back to
  # pure Perl if the GMP library is not installed):
  # (See also the L<MATH LIBRARY> section!)

  # will warn if Math::BigInt::GMP cannot be found
  use Math::BigInt lib => 'GMP';

  # to supress the warning use this:
  # use Math::BigInt try => 'GMP';

  # dies if GMP cannot be loaded:
  # use Math::BigInt only => 'GMP';

  my $str = '1234567890';
  my @values = (64,74,18);
  my $n = 1; my $sign = '-';

  # Number creation	
  my $x = Math::BigInt->new($str);	# defaults to 0
  my $y = $x->copy();			# make a true copy
  my $nan  = Math::BigInt->bnan(); 	# create a NotANumber
  my $zero = Math::BigInt->bzero();	# create a +0
  my $inf = Math::BigInt->binf();	# create a +inf
  my $inf = Math::BigInt->binf('-');	# create a -inf
  my $one = Math::BigInt->bone();	# create a +1
  my $mone = Math::BigInt->bone('-');	# create a -1

  my $pi = Math::BigInt->bpi();		# returns '3'
					# see Math::BigFloat::bpi()

  $h = Math::BigInt->new('0x123');	# from hexadecimal
  $b = Math::BigInt->new('0b101');	# from binary
  $o = Math::BigInt->from_oct('0101');	# from octal

  # Testing (don't modify their arguments)
  # (return true if the condition is met, otherwise false)

  $x->is_zero();	# if $x is +0
  $x->is_nan();		# if $x is NaN
  $x->is_one();		# if $x is +1
  $x->is_one('-');	# if $x is -1
  $x->is_odd();		# if $x is odd
  $x->is_even();	# if $x is even
  $x->is_pos();		# if $x >= 0
  $x->is_neg();		# if $x <  0
  $x->is_inf($sign);	# if $x is +inf, or -inf (sign is default '+')
  $x->is_int();		# if $x is an integer (not a float)

  # comparing and digit/sign extraction
  $x->bcmp($y);		# compare numbers (undef,<0,=0,>0)
  $x->bacmp($y);	# compare absolutely (undef,<0,=0,>0)
  $x->sign();		# return the sign, either +,- or NaN
  $x->digit($n);	# return the nth digit, counting from right
  $x->digit(-$n);	# return the nth digit, counting from left

  # The following all modify their first argument. If you want to preserve
  # $x, use $z = $x->copy()->bXXX($y); See under L<CAVEATS> for why this is
  # necessary when mixing $a = $b assignments with non-overloaded math.

  $x->bzero();		# set $x to 0
  $x->bnan();		# set $x to NaN
  $x->bone();		# set $x to +1
  $x->bone('-');	# set $x to -1
  $x->binf();		# set $x to inf
  $x->binf('-');	# set $x to -inf

  $x->bneg();		# negation
  $x->babs();		# absolute value
  $x->bnorm();		# normalize (no-op in BigInt)
  $x->bnot();		# two's complement (bit wise not)
  $x->binc();		# increment $x by 1
  $x->bdec();		# decrement $x by 1
  
  $x->badd($y);		# addition (add $y to $x)
  $x->bsub($y);		# subtraction (subtract $y from $x)
  $x->bmul($y);		# multiplication (multiply $x by $y)
  $x->bdiv($y);		# divide, set $x to quotient
			# return (quo,rem) or quo if scalar

  $x->bmuladd($y,$z);	# $x = $x * $y + $z

  $x->bmod($y);		   # modulus (x % y)
  $x->bmodpow($exp,$mod);  # modular exponentation (($num**$exp) % $mod))
  $x->bmodinv($mod);	   # the inverse of $x in the given modulus $mod

  $x->bpow($y);		   # power of arguments (x ** y)
  $x->blsft($y);	   # left shift in base 2
  $x->brsft($y);	   # right shift in base 2
			   # returns (quo,rem) or quo if in scalar context
  $x->blsft($y,$n);	   # left shift by $y places in base $n
  $x->brsft($y,$n);	   # right shift by $y places in base $n
			   # returns (quo,rem) or quo if in scalar context
  
  $x->band($y);		   # bitwise and
  $x->bior($y);		   # bitwise inclusive or
  $x->bxor($y);		   # bitwise exclusive or
  $x->bnot();		   # bitwise not (two's complement)

  $x->bsqrt();		   # calculate square-root
  $x->broot($y);	   # $y'th root of $x (e.g. $y == 3 => cubic root)
  $x->bfac();		   # factorial of $x (1*2*3*4*..$x)

  $x->bnok($y);		   # x over y (binomial coefficient n over k)

  $x->blog();		   # logarithm of $x to base e (Euler's number)
  $x->blog($base);	   # logarithm of $x to base $base (f.i. 2)
  $x->bexp();		   # calculate e ** $x where e is Euler's number
  
  $x->round($A,$P,$mode);  # round to accuracy or precision using mode $mode
  $x->bround($n);	   # accuracy: preserve $n digits
  $x->bfround($n);	   # $n > 0: round $nth digits,
			   # $n < 0: round to the $nth digit after the
			   # dot, no-op for BigInts

  # The following do not modify their arguments in BigInt (are no-ops),
  # but do so in BigFloat:

  $x->bfloor();		   # return integer less or equal than $x
  $x->bceil();		   # return integer greater or equal than $x
  
  # The following do not modify their arguments:

  # greatest common divisor (no OO style)
  my $gcd = Math::BigInt::bgcd(@values);
  # lowest common multiplicator (no OO style)
  my $lcm = Math::BigInt::blcm(@values);	
 
  $x->length();		   # return number of digits in number
  ($xl,$f) = $x->length(); # length of number and length of fraction part,
			   # latter is always 0 digits long for BigInts

  $x->exponent();	   # return exponent as BigInt
  $x->mantissa();	   # return (signed) mantissa as BigInt
  $x->parts();		   # return (mantissa,exponent) as BigInt
  $x->copy();		   # make a true copy of $x (unlike $y = $x;)
  $x->as_int();		   # return as BigInt (in BigInt: same as copy())
  $x->numify();		   # return as scalar (might overflow!)
  
  # conversation to string (do not modify their argument)
  $x->bstr();		   # normalized string (e.g. '3')
  $x->bsstr();		   # norm. string in scientific notation (e.g. '3E0')
  $x->as_hex();		   # as signed hexadecimal string with prefixed 0x
  $x->as_bin();		   # as signed binary string with prefixed 0b
  $x->as_oct();		   # as signed octal string with prefixed 0


  # precision and accuracy (see section about rounding for more)
  $x->precision();	   # return P of $x (or global, if P of $x undef)
  $x->precision($n);	   # set P of $x to $n
  $x->accuracy();	   # return A of $x (or global, if A of $x undef)
  $x->accuracy($n);	   # set A $x to $n

  # Global methods
  Math::BigInt->precision();	# get/set global P for all BigInt objects
  Math::BigInt->accuracy(); 	# get/set global A for all BigInt objects
  Math::BigInt->round_mode();	# get/set global round mode, one of
				# 'even', 'odd', '+inf', '-inf', 'zero', 'trunc' or 'common'
  Math::BigInt->config();	# return hash containing configuration

=head1 DESCRIPTION

All operators (including basic math operations) are overloaded if you
declare your big integers as

  $i = new Math::BigInt '123_456_789_123_456_789';

Operations with overloaded operators preserve the arguments which is
exactly what you expect.

=over 2

=item Input

Input values to these routines may be any string, that looks like a number
and results in an integer, including hexadecimal and binary numbers.

Scalars holding numbers may also be passed, but note that non-integer numbers
may already have lost precision due to the conversation to float. Quote
your input if you want BigInt to see all the digits:

	$x = Math::BigInt->new(12345678890123456789);	# bad
	$x = Math::BigInt->new('12345678901234567890');	# good

You can include one underscore between any two digits.

This means integer values like 1.01E2 or even 1000E-2 are also accepted.
Non-integer values result in NaN.

Hexadecimal (prefixed with "0x") and binary numbers (prefixed with "0b")
are accepted, too. Please note that octal numbers are not recognized
by new(), so the following will print "123":

	perl -MMath::BigInt -le 'print Math::BigInt->new("0123")'
	
To convert an octal number, use from_oct();

	perl -MMath::BigInt -le 'print Math::BigInt->from_oct("0123")'

Currently, Math::BigInt::new() defaults to 0, while Math::BigInt::new('')
results in 'NaN'. This might change in the future, so use always the following
explicit forms to get a zero or NaN:

	$zero = Math::BigInt->bzero(); 
	$nan = Math::BigInt->bnan(); 

C<bnorm()> on a BigInt object is now effectively a no-op, since the numbers 
are always stored in normalized form. If passed a string, creates a BigInt 
object from the input.

=item Output

Output values are BigInt objects (normalized), except for the methods which
return a string (see L<SYNOPSIS>).

Some routines (C<is_odd()>, C<is_even()>, C<is_zero()>, C<is_one()>,
C<is_nan()>, etc.) return true or false, while others (C<bcmp()>, C<bacmp()>)
return either undef (if NaN is involved), <0, 0 or >0 and are suited for sort.

=back

=head1 METHODS

Each of the methods below (except config(), accuracy() and precision())
accepts three additional parameters. These arguments C<$A>, C<$P> and C<$R>
are C<accuracy>, C<precision> and C<round_mode>. Please see the section about
L<ACCURACY and PRECISION> for more information.

=head2 config()

	use Data::Dumper;

	print Dumper ( Math::BigInt->config() );
	print Math::BigInt->config()->{lib},"\n";

Returns a hash containing the configuration, e.g. the version number, lib
loaded etc. The following hash keys are currently filled in with the
appropriate information.

	key		Description
			Example
	============================================================
	lib		Name of the low-level math library
			Math::BigInt::Calc
	lib_version 	Version of low-level math library (see 'lib')
			0.30
	class		The class name of config() you just called
			Math::BigInt
	upgrade		To which class math operations might be upgraded
			Math::BigFloat
	downgrade	To which class math operations might be downgraded
			undef
	precision	Global precision
			undef
	accuracy	Global accuracy
			undef
	round_mode	Global round mode
			even
	version		version number of the class you used
			1.61
	div_scale	Fallback accuracy for div
			40
	trap_nan	If true, traps creation of NaN via croak()
			1
	trap_inf	If true, traps creation of +inf/-inf via croak()
			1

The following values can be set by passing C<config()> a reference to a hash:

	trap_inf trap_nan
        upgrade downgrade precision accuracy round_mode div_scale

Example:
	
	$new_cfg = Math::BigInt->config( { trap_inf => 1, precision => 5 } );

=head2 accuracy()

	$x->accuracy(5);		# local for $x
	CLASS->accuracy(5);		# global for all members of CLASS
					# Note: This also applies to new()!

	$A = $x->accuracy();		# read out accuracy that affects $x
	$A = CLASS->accuracy();		# read out global accuracy

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
        print scalar $x->copy()->bdiv($y, 2);		# print 4300
        print scalar $x->copy()->bdiv($y)->bround(2);	# print 4300

Please see the section about L<ACCURACY AND PRECISION> for further details.

Value must be greater than zero. Pass an undef value to disable it:

	$x->accuracy(undef);
	Math::BigInt->accuracy(undef);

Returns the current accuracy. For C<$x->accuracy()> it will return either the
local accuracy, or if not defined, the global. This means the return value
represents the accuracy that will be in effect for $x:

	$y = Math::BigInt->new(1234567);	# unrounded
	print Math::BigInt->accuracy(4),"\n";	# set 4, print 4
	$x = Math::BigInt->new(123456);		# $x will be automatically rounded!
	print "$x $y\n";			# '123500 1234567'
	print $x->accuracy(),"\n";		# will be 4
	print $y->accuracy(),"\n";		# also 4, since global is 4
	print Math::BigInt->accuracy(5),"\n";	# set to 5, print 5
	print $x->accuracy(),"\n";		# still 4
	print $y->accuracy(),"\n";		# 5, since global is 5

Note: Works also for subclasses like Math::BigFloat. Each class has it's own
globals separated from Math::BigInt, but it is possible to subclass
Math::BigInt and make the globals of the subclass aliases to the ones from
Math::BigInt.

=head2 precision()

	$x->precision(-2);	# local for $x, round at the second digit right of the dot
	$x->precision(2);	# ditto, round at the second digit left of the dot

	CLASS->precision(5);	# Global for all members of CLASS
				# This also applies to new()!
	CLASS->precision(-5);	# ditto

	$P = CLASS->precision();	# read out global precision 
	$P = $x->precision();		# read out precision that affects $x

Note: You probably want to use L<accuracy()> instead. With L<accuracy> you
set the number of digits each result should have, with L<precision> you
set the place where to round!

C<precision()> sets or gets the global or local precision, aka at which digit
before or after the dot to round all results. A set global precision also
applies to all newly created numbers!

In Math::BigInt, passing a negative number precision has no effect since no
numbers have digits after the dot. In L<Math::BigFloat>, it will round all
results to P digits after the dot.

Please see the section about L<ACCURACY AND PRECISION> for further details.

Pass an undef value to disable it:

	$x->precision(undef);
	Math::BigInt->precision(undef);

Returns the current precision. For C<$x->precision()> it will return either the
local precision of $x, or if not defined, the global. This means the return
value represents the prevision that will be in effect for $x:

	$y = Math::BigInt->new(1234567);	# unrounded
	print Math::BigInt->precision(4),"\n";	# set 4, print 4
	$x = Math::BigInt->new(123456);		# will be automatically rounded
	print $x;				# print "120000"!

Note: Works also for subclasses like L<Math::BigFloat>. Each class has its
own globals separated from Math::BigInt, but it is possible to subclass
Math::BigInt and make the globals of the subclass aliases to the ones from
Math::BigInt.

=head2 brsft()

	$x->brsft($y,$n);		

Shifts $x right by $y in base $n. Default is base 2, used are usually 10 and
2, but others work, too.

Right shifting usually amounts to dividing $x by $n ** $y and truncating the
result:


	$x = Math::BigInt->new(10);
	$x->brsft(1);			# same as $x >> 1: 5
	$x = Math::BigInt->new(1234);
	$x->brsft(2,10);		# result 12

There is one exception, and that is base 2 with negative $x:


	$x = Math::BigInt->new(-5);
	print $x->brsft(1);

This will print -3, not -2 (as it would if you divide -5 by 2 and truncate the
result).

=head2 new()

  	$x = Math::BigInt->new($str,$A,$P,$R);

Creates a new BigInt object from a scalar or another BigInt object. The
input is accepted as decimal, hex (with leading '0x') or binary (with leading
'0b').

See L<Input> for more info on accepted input formats.

=head2 from_oct()

	$x = Math::BigInt->from_oct("0775");	# input is octal

=head2 from_hex()

	$x = Math::BigInt->from_hex("0xcafe");	# input is hexadecimal

=head2 from_bin()

	$x = Math::BigInt->from_oct("0x10011");	# input is binary

=head2 bnan()

  	$x = Math::BigInt->bnan();

Creates a new BigInt object representing NaN (Not A Number).
If used on an object, it will set it to NaN:

	$x->bnan();

=head2 bzero()

  	$x = Math::BigInt->bzero();

Creates a new BigInt object representing zero.
If used on an object, it will set it to zero:

	$x->bzero();

=head2 binf()

  	$x = Math::BigInt->binf($sign);

Creates a new BigInt object representing infinity. The optional argument is
either '-' or '+', indicating whether you want infinity or minus infinity.
If used on an object, it will set it to infinity:

	$x->binf();
	$x->binf('-');

=head2 bone()

  	$x = Math::BigInt->binf($sign);

Creates a new BigInt object representing one. The optional argument is
either '-' or '+', indicating whether you want one or minus one.
If used on an object, it will set it to one:

	$x->bone();		# +1
	$x->bone('-');		# -1

=head2 is_one()/is_zero()/is_nan()/is_inf()

  
	$x->is_zero();			# true if arg is +0
	$x->is_nan();			# true if arg is NaN
	$x->is_one();			# true if arg is +1
	$x->is_one('-');		# true if arg is -1
	$x->is_inf();			# true if +inf
	$x->is_inf('-');		# true if -inf (sign is default '+')

These methods all test the BigInt for being one specific value and return
true or false depending on the input. These are faster than doing something
like:

	if ($x == 0)

=head2 is_pos()/is_neg()/is_positive()/is_negative()
	
	$x->is_pos();			# true if > 0
	$x->is_neg();			# true if < 0

The methods return true if the argument is positive or negative, respectively.
C<NaN> is neither positive nor negative, while C<+inf> counts as positive, and
C<-inf> is negative. A C<zero> is neither positive nor negative.

These methods are only testing the sign, and not the value.

C<is_positive()> and C<is_negative()> are aliases to C<is_pos()> and
C<is_neg()>, respectively. C<is_positive()> and C<is_negative()> were
introduced in v1.36, while C<is_pos()> and C<is_neg()> were only introduced
in v1.68.

=head2 is_odd()/is_even()/is_int()

	$x->is_odd();			# true if odd, false for even
	$x->is_even();			# true if even, false for odd
	$x->is_int();			# true if $x is an integer

The return true when the argument satisfies the condition. C<NaN>, C<+inf>,
C<-inf> are not integers and are neither odd nor even.

In BigInt, all numbers except C<NaN>, C<+inf> and C<-inf> are integers.

=head2 bcmp()

	$x->bcmp($y);

Compares $x with $y and takes the sign into account.
Returns -1, 0, 1 or undef.

=head2 bacmp()

	$x->bacmp($y);

Compares $x with $y while ignoring their. Returns -1, 0, 1 or undef.

=head2 sign()

	$x->sign();

Return the sign, of $x, meaning either C<+>, C<->, C<-inf>, C<+inf> or NaN.

If you want $x to have a certain sign, use one of the following methods:

	$x->babs();		# '+'
	$x->babs()->bneg();	# '-'
	$x->bnan();		# 'NaN'
	$x->binf();		# '+inf'
	$x->binf('-');		# '-inf'

=head2 digit()

	$x->digit($n);		# return the nth digit, counting from right

If C<$n> is negative, returns the digit counting from left.

=head2 bneg()

	$x->bneg();

Negate the number, e.g. change the sign between '+' and '-', or between '+inf'
and '-inf', respectively. Does nothing for NaN or zero.

=head2 babs()

	$x->babs();

Set the number to its absolute value, e.g. change the sign from '-' to '+'
and from '-inf' to '+inf', respectively. Does nothing for NaN or positive
numbers.

=head2 bnorm()

	$x->bnorm();			# normalize (no-op)

=head2 bnot()

	$x->bnot();			

Two's complement (bitwise not). This is equivalent to

	$x->binc()->bneg();

but faster.

=head2 binc()

	$x->binc();			# increment x by 1

=head2 bdec()

	$x->bdec();			# decrement x by 1

=head2 badd()

	$x->badd($y);			# addition (add $y to $x)

=head2 bsub()

	$x->bsub($y);			# subtraction (subtract $y from $x)

=head2 bmul()

	$x->bmul($y);			# multiplication (multiply $x by $y)

=head2 bmuladd()

	$x->bmuladd($y,$z);

Multiply $x by $y, and then add $z to the result,

This method was added in v1.87 of Math::BigInt (June 2007).

=head2 bdiv()

	$x->bdiv($y);			# divide, set $x to quotient
					# return (quo,rem) or quo if scalar

=head2 bmod()

	$x->bmod($y);			# modulus (x % y)

=head2 bmodinv()

	num->bmodinv($mod);		# modular inverse

Returns the inverse of C<$num> in the given modulus C<$mod>.  'C<NaN>' is
returned unless C<$num> is relatively prime to C<$mod>, i.e. unless
C<bgcd($num, $mod)==1>.

=head2 bmodpow()

	$num->bmodpow($exp,$mod);	# modular exponentation
					# ($num**$exp % $mod)

Returns the value of C<$num> taken to the power C<$exp> in the modulus
C<$mod> using binary exponentation.  C<bmodpow> is far superior to
writing

	$num ** $exp % $mod

because it is much faster - it reduces internal variables into
the modulus whenever possible, so it operates on smaller numbers.

C<bmodpow> also supports negative exponents.

	bmodpow($num, -1, $mod)

is exactly equivalent to

	bmodinv($num, $mod)

=head2 bpow()

	$x->bpow($y);			# power of arguments (x ** y)

=head2 blog()

	$x->blog($base, $accuracy);	# logarithm of x to the base $base

If C<$base> is not defined, Euler's number (e) is used:

	print $x->blog(undef, 100);	# log(x) to 100 digits

=head2 bexp()

	$x->bexp($accuracy);		# calculate e ** X

Calculates the expression C<e ** $x> where C<e> is Euler's number.

This method was added in v1.82 of Math::BigInt (April 2007).

See also L<blog()>.

=head2 bnok()

	$x->bnok($y);		   # x over y (binomial coefficient n over k)

Calculates the binomial coefficient n over k, also called the "choose"
function. The result is equivalent to:

	( n )      n!
	| - |  = -------
	( k )    k!(n-k)!

This method was added in v1.84 of Math::BigInt (April 2007).

=head2 bpi()

	print Math::BigInt->bpi(100), "\n";		# 3

Returns PI truncated to an integer, with the argument being ignored. This means
under BigInt this always returns C<3>.

If upgrading is in effect, returns PI, rounded to N digits with the
current rounding mode:

	use Math::BigFloat;
	use Math::BigInt upgrade => Math::BigFloat;
	print Math::BigInt->bpi(3), "\n";		# 3.14
	print Math::BigInt->bpi(100), "\n";		# 3.1415....

This method was added in v1.87 of Math::BigInt (June 2007).

=head2 bcos()

	my $x = Math::BigInt->new(1);
	print $x->bcos(100), "\n";

Calculate the cosinus of $x, modifying $x in place.

In BigInt, unless upgrading is in effect, the result is truncated to an
integer.

This method was added in v1.87 of Math::BigInt (June 2007).

=head2 bsin()

	my $x = Math::BigInt->new(1);
	print $x->bsin(100), "\n";

Calculate the sinus of $x, modifying $x in place.

In BigInt, unless upgrading is in effect, the result is truncated to an
integer.

This method was added in v1.87 of Math::BigInt (June 2007).

=head2 batan2()

	my $x = Math::BigInt->new(1);
	my $y = Math::BigInt->new(1);
	print $y->batan2($x), "\n";

Calculate the arcus tangens of C<$y> divided by C<$x>, modifying $y in place.

In BigInt, unless upgrading is in effect, the result is truncated to an
integer.

This method was added in v1.87 of Math::BigInt (June 2007).

=head2 batan()

	my $x = Math::BigFloat->new(0.5);
	print $x->batan(100), "\n";

Calculate the arcus tangens of $x, modifying $x in place.

In BigInt, unless upgrading is in effect, the result is truncated to an
integer.

This method was added in v1.87 of Math::BigInt (June 2007).

=head2 blsft()

	$x->blsft($y);		# left shift in base 2
	$x->blsft($y,$n);	# left shift, in base $n (like 10)

=head2 brsft()

	$x->brsft($y);		# right shift in base 2
	$x->brsft($y,$n);	# right shift, in base $n (like 10)

=head2 band()

	$x->band($y);			# bitwise and

=head2 bior()

	$x->bior($y);			# bitwise inclusive or

=head2 bxor()

	$x->bxor($y);			# bitwise exclusive or

=head2 bnot()

	$x->bnot();			# bitwise not (two's complement)

=head2 bsqrt()

	$x->bsqrt();			# calculate square-root

=head2 broot()

	$x->broot($N);

Calculates the N'th root of C<$x>.

=head2 bfac()

	$x->bfac();			# factorial of $x (1*2*3*4*..$x)

=head2 round()

	$x->round($A,$P,$round_mode);
	
Round $x to accuracy C<$A> or precision C<$P> using the round mode
C<$round_mode>.

=head2 bround()

	$x->bround($N);               # accuracy: preserve $N digits

=head2 bfround()

	$x->bfround($N);

If N is > 0, rounds to the Nth digit from the left. If N < 0, rounds to
the Nth digit after the dot. Since BigInts are integers, the case N < 0
is a no-op for them.

Examples:

	Input		N		Result
	===================================================
	123456.123456	3		123500
	123456.123456	2		123450
	123456.123456	-2		123456.12
	123456.123456	-3		123456.123

=head2 bfloor()

	$x->bfloor();			

Set $x to the integer less or equal than $x. This is a no-op in BigInt, but
does change $x in BigFloat.

=head2 bceil()

	$x->bceil();

Set $x to the integer greater or equal than $x. This is a no-op in BigInt, but
does change $x in BigFloat.

=head2 bgcd()

	bgcd(@values);		# greatest common divisor (no OO style)

=head2 blcm()

	blcm(@values);		# lowest common multiplicator (no OO style)
 
head2 length()

	$x->length();
        ($xl,$fl) = $x->length();

Returns the number of digits in the decimal representation of the number.
In list context, returns the length of the integer and fraction part. For
BigInt's, the length of the fraction part will always be 0.

=head2 exponent()

	$x->exponent();

Return the exponent of $x as BigInt.

=head2 mantissa()

	$x->mantissa();

Return the signed mantissa of $x as BigInt.

=head2 parts()

	$x->parts();		# return (mantissa,exponent) as BigInt

=head2 copy()

	$x->copy();		# make a true copy of $x (unlike $y = $x;)

=head2 as_int()/as_number()

	$x->as_int();	

Returns $x as a BigInt (truncated towards zero). In BigInt this is the same as
C<copy()>. 

C<as_number()> is an alias to this method. C<as_number> was introduced in
v1.22, while C<as_int()> was only introduced in v1.68.
  
=head2 bstr()

	$x->bstr();

Returns a normalized string representation of C<$x>.

=head2 bsstr()

	$x->bsstr();		# normalized string in scientific notation

=head2 as_hex()

	$x->as_hex();		# as signed hexadecimal string with prefixed 0x

=head2 as_bin()

	$x->as_bin();		# as signed binary string with prefixed 0b

=head2 as_oct()

	$x->as_oct();		# as signed octal string with prefixed 0

=head2 numify()

	print $x->numify();

This returns a normal Perl scalar from $x. It is used automatically
whenever a scalar is needed, for instance in array index operations.

This loses precision, to avoid this use L<as_int()> instead.

=head2 modify()

	$x->modify('bpowd');

This method returns 0 if the object can be modified with the given
peration, or 1 if not.

This is used for instance by L<Math::BigInt::Constant>.

=head2 upgrade()/downgrade()

Set/get the class for downgrade/upgrade operations. Thuis is used
for instance by L<bignum>. The defaults are '', thus the following
operation will create a BigInt, not a BigFloat:

	my $i = Math::BigInt->new(123);
	my $f = Math::BigFloat->new('123.1');

	print $i + $f,"\n";			# print 246

=head2 div_scale()

Set/get the number of digits for the default precision in divide
operations.

=head2 round_mode()

Set/get the current round mode.

=head1 ACCURACY and PRECISION

Since version v1.33, Math::BigInt and Math::BigFloat have full support for
accuracy and precision based rounding, both automatically after every
operation, as well as manually.

This section describes the accuracy/precision handling in Math::Big* as it
used to be and as it is now, complete with an explanation of all terms and
abbreviations.

Not yet implemented things (but with correct description) are marked with '!',
things that need to be answered are marked with '?'.

In the next paragraph follows a short description of terms used here (because
these may differ from terms used by others people or documentation).

During the rest of this document, the shortcuts A (for accuracy), P (for
precision), F (fallback) and R (rounding mode) will be used.

=head2 Precision P

A fixed number of digits before (positive) or after (negative)
the decimal point. For example, 123.45 has a precision of -2. 0 means an
integer like 123 (or 120). A precision of 2 means two digits to the left
of the decimal point are zero, so 123 with P = 1 becomes 120. Note that
numbers with zeros before the decimal point may have different precisions,
because 1200 can have p = 0, 1 or 2 (depending on what the inital value
was). It could also have p < 0, when the digits after the decimal point
are zero.

The string output (of floating point numbers) will be padded with zeros:
 
	Initial value   P       A	Result          String
	------------------------------------------------------------
	1234.01         -3      	1000            1000
	1234            -2      	1200            1200
	1234.5          -1      	1230            1230
	1234.001        1       	1234            1234.0
	1234.01         0       	1234            1234
	1234.01         2       	1234.01		1234.01
	1234.01         5       	1234.01		1234.01000

For BigInts, no padding occurs.

=head2 Accuracy A

Number of significant digits. Leading zeros are not counted. A
number may have an accuracy greater than the non-zero digits
when there are zeros in it or trailing zeros. For example, 123.456 has
A of 6, 10203 has 5, 123.0506 has 7, 123.450000 has 8 and 0.000123 has 3.

The string output (of floating point numbers) will be padded with zeros:

	Initial value   P       A	Result          String
	------------------------------------------------------------
	1234.01			3	1230		1230
	1234.01			6	1234.01		1234.01
	1234.1			8	1234.1		1234.1000

For BigInts, no padding occurs.

=head2 Fallback F

When both A and P are undefined, this is used as a fallback accuracy when
dividing numbers.

=head2 Rounding mode R

When rounding a number, different 'styles' or 'kinds'
of rounding are possible. (Note that random rounding, as in
Math::Round, is not implemented.)

=over 2

=item 'trunc'

truncation invariably removes all digits following the
rounding place, replacing them with zeros. Thus, 987.65 rounded
to tens (P=1) becomes 980, and rounded to the fourth sigdig
becomes 987.6 (A=4). 123.456 rounded to the second place after the
decimal point (P=-2) becomes 123.46.

All other implemented styles of rounding attempt to round to the
"nearest digit." If the digit D immediately to the right of the
rounding place (skipping the decimal point) is greater than 5, the
number is incremented at the rounding place (possibly causing a
cascade of incrementation): e.g. when rounding to units, 0.9 rounds
to 1, and -19.9 rounds to -20. If D < 5, the number is similarly
truncated at the rounding place: e.g. when rounding to units, 0.4
rounds to 0, and -19.4 rounds to -19.

However the results of other styles of rounding differ if the
digit immediately to the right of the rounding place (skipping the
decimal point) is 5 and if there are no digits, or no digits other
than 0, after that 5. In such cases:

=item 'even'

rounds the digit at the rounding place to 0, 2, 4, 6, or 8
if it is not already. E.g., when rounding to the first sigdig, 0.45
becomes 0.4, -0.55 becomes -0.6, but 0.4501 becomes 0.5.

=item 'odd'

rounds the digit at the rounding place to 1, 3, 5, 7, or 9 if
it is not already. E.g., when rounding to the first sigdig, 0.45
becomes 0.5, -0.55 becomes -0.5, but 0.5501 becomes 0.6.

=item '+inf'

round to plus infinity, i.e. always round up. E.g., when
rounding to the first sigdig, 0.45 becomes 0.5, -0.55 becomes -0.5,
and 0.4501 also becomes 0.5.

=item '-inf'

round to minus infinity, i.e. always round down. E.g., when
rounding to the first sigdig, 0.45 becomes 0.4, -0.55 becomes -0.6,
but 0.4501 becomes 0.5.

=item 'zero'

round to zero, i.e. positive numbers down, negative ones up.
E.g., when rounding to the first sigdig, 0.45 becomes 0.4, -0.55
becomes -0.5, but 0.4501 becomes 0.5.

=item 'common'

round up if the digit immediately to the right of the rounding place
is 5 or greater, otherwise round down. E.g., 0.15 becomes 0.2 and
0.149 becomes 0.1.

=back

The handling of A & P in MBI/MBF (the old core code shipped with Perl
versions <= 5.7.2) is like this:

=over 2

=item Precision

  * ffround($p) is able to round to $p number of digits after the decimal
    point
  * otherwise P is unused

=item Accuracy (significant digits)

  * fround($a) rounds to $a significant digits
  * only fdiv() and fsqrt() take A as (optional) paramater
    + other operations simply create the same number (fneg etc), or more (fmul)
      of digits
    + rounding/truncating is only done when explicitly calling one of fround
      or ffround, and never for BigInt (not implemented)
  * fsqrt() simply hands its accuracy argument over to fdiv.
  * the documentation and the comment in the code indicate two different ways
    on how fdiv() determines the maximum number of digits it should calculate,
    and the actual code does yet another thing
    POD:
      max($Math::BigFloat::div_scale,length(dividend)+length(divisor))
    Comment:
      result has at most max(scale, length(dividend), length(divisor)) digits
    Actual code:
      scale = max(scale, length(dividend)-1,length(divisor)-1);
      scale += length(divisor) - length(dividend);
    So for lx = 3, ly = 9, scale = 10, scale will actually be 16 (10+9-3).
    Actually, the 'difference' added to the scale is calculated from the
    number of "significant digits" in dividend and divisor, which is derived
    by looking at the length of the mantissa. Which is wrong, since it includes
    the + sign (oops) and actually gets 2 for '+100' and 4 for '+101'. Oops
    again. Thus 124/3 with div_scale=1 will get you '41.3' based on the strange
    assumption that 124 has 3 significant digits, while 120/7 will get you
    '17', not '17.1' since 120 is thought to have 2 significant digits.
    The rounding after the division then uses the remainder and $y to determine
    wether it must round up or down.
 ?  I have no idea which is the right way. That's why I used a slightly more
 ?  simple scheme and tweaked the few failing testcases to match it.

=back

This is how it works now:

=over 2

=item Setting/Accessing

  * You can set the A global via C<< Math::BigInt->accuracy() >> or
    C<< Math::BigFloat->accuracy() >> or whatever class you are using.
  * You can also set P globally by using C<< Math::SomeClass->precision() >>
    likewise.
  * Globals are classwide, and not inherited by subclasses.
  * to undefine A, use C<< Math::SomeCLass->accuracy(undef); >>
  * to undefine P, use C<< Math::SomeClass->precision(undef); >>
  * Setting C<< Math::SomeClass->accuracy() >> clears automatically
    C<< Math::SomeClass->precision() >>, and vice versa.
  * To be valid, A must be > 0, P can have any value.
  * If P is negative, this means round to the P'th place to the right of the
    decimal point; positive values mean to the left of the decimal point.
    P of 0 means round to integer.
  * to find out the current global A, use C<< Math::SomeClass->accuracy() >>
  * to find out the current global P, use C<< Math::SomeClass->precision() >>
  * use C<< $x->accuracy() >> respective C<< $x->precision() >> for the local
    setting of C<< $x >>.
  * Please note that C<< $x->accuracy() >> respective C<< $x->precision() >>
    return eventually defined global A or P, when C<< $x >>'s A or P is not
    set.

=item Creating numbers

  * When you create a number, you can give the desired A or P via:
    $x = Math::BigInt->new($number,$A,$P);
  * Only one of A or P can be defined, otherwise the result is NaN
  * If no A or P is give ($x = Math::BigInt->new($number) form), then the
    globals (if set) will be used. Thus changing the global defaults later on
    will not change the A or P of previously created numbers (i.e., A and P of
    $x will be what was in effect when $x was created)
  * If given undef for A and P, B<no> rounding will occur, and the globals will
    B<not> be used. This is used by subclasses to create numbers without
    suffering rounding in the parent. Thus a subclass is able to have its own
    globals enforced upon creation of a number by using
    C<< $x = Math::BigInt->new($number,undef,undef) >>:

	use Math::BigInt::SomeSubclass;
	use Math::BigInt;

	Math::BigInt->accuracy(2);
	Math::BigInt::SomeSubClass->accuracy(3);
	$x = Math::BigInt::SomeSubClass->new(1234);	

    $x is now 1230, and not 1200. A subclass might choose to implement
    this otherwise, e.g. falling back to the parent's A and P.

=item Usage

  * If A or P are enabled/defined, they are used to round the result of each
    operation according to the rules below
  * Negative P is ignored in Math::BigInt, since BigInts never have digits
    after the decimal point
  * Math::BigFloat uses Math::BigInt internally, but setting A or P inside
    Math::BigInt as globals does not tamper with the parts of a BigFloat.
    A flag is used to mark all Math::BigFloat numbers as 'never round'.

=item Precedence

  * It only makes sense that a number has only one of A or P at a time.
    If you set either A or P on one object, or globally, the other one will
    be automatically cleared.
  * If two objects are involved in an operation, and one of them has A in
    effect, and the other P, this results in an error (NaN).
  * A takes precedence over P (Hint: A comes before P).
    If neither of them is defined, nothing is used, i.e. the result will have
    as many digits as it can (with an exception for fdiv/fsqrt) and will not
    be rounded.
  * There is another setting for fdiv() (and thus for fsqrt()). If neither of
    A or P is defined, fdiv() will use a fallback (F) of $div_scale digits.
    If either the dividend's or the divisor's mantissa has more digits than
    the value of F, the higher value will be used instead of F.
    This is to limit the digits (A) of the result (just consider what would
    happen with unlimited A and P in the case of 1/3 :-)
  * fdiv will calculate (at least) 4 more digits than required (determined by
    A, P or F), and, if F is not used, round the result
    (this will still fail in the case of a result like 0.12345000000001 with A
    or P of 5, but this can not be helped - or can it?)
  * Thus you can have the math done by on Math::Big* class in two modi:
    + never round (this is the default):
      This is done by setting A and P to undef. No math operation
      will round the result, with fdiv() and fsqrt() as exceptions to guard
      against overflows. You must explicitly call bround(), bfround() or
      round() (the latter with parameters).
      Note: Once you have rounded a number, the settings will 'stick' on it
      and 'infect' all other numbers engaged in math operations with it, since
      local settings have the highest precedence. So, to get SaferRound[tm],
      use a copy() before rounding like this:

        $x = Math::BigFloat->new(12.34);
        $y = Math::BigFloat->new(98.76);
        $z = $x * $y;                           # 1218.6984
        print $x->copy()->fround(3);            # 12.3 (but A is now 3!)
        $z = $x * $y;                           # still 1218.6984, without
                                                # copy would have been 1210!

    + round after each op:
      After each single operation (except for testing like is_zero()), the
      method round() is called and the result is rounded appropriately. By
      setting proper values for A and P, you can have all-the-same-A or
      all-the-same-P modes. For example, Math::Currency might set A to undef,
      and P to -2, globally.

 ?Maybe an extra option that forbids local A & P settings would be in order,
 ?so that intermediate rounding does not 'poison' further math? 

=item Overriding globals

  * you will be able to give A, P and R as an argument to all the calculation
    routines; the second parameter is A, the third one is P, and the fourth is
    R (shift right by one for binary operations like badd). P is used only if
    the first parameter (A) is undefined. These three parameters override the
    globals in the order detailed as follows, i.e. the first defined value
    wins:
    (local: per object, global: global default, parameter: argument to sub)
      + parameter A
      + parameter P
      + local A (if defined on both of the operands: smaller one is taken)
      + local P (if defined on both of the operands: bigger one is taken)
      + global A
      + global P
      + global F
  * fsqrt() will hand its arguments to fdiv(), as it used to, only now for two
    arguments (A and P) instead of one

=item Local settings

  * You can set A or P locally by using C<< $x->accuracy() >> or
    C<< $x->precision() >>
    and thus force different A and P for different objects/numbers.
  * Setting A or P this way immediately rounds $x to the new value.
  * C<< $x->accuracy() >> clears C<< $x->precision() >>, and vice versa.

=item Rounding

  * the rounding routines will use the respective global or local settings.
    fround()/bround() is for accuracy rounding, while ffround()/bfround()
    is for precision
  * the two rounding functions take as the second parameter one of the
    following rounding modes (R):
    'even', 'odd', '+inf', '-inf', 'zero', 'trunc', 'common'
  * you can set/get the global R by using C<< Math::SomeClass->round_mode() >>
    or by setting C<< $Math::SomeClass::round_mode >>
  * after each operation, C<< $result->round() >> is called, and the result may
    eventually be rounded (that is, if A or P were set either locally,
    globally or as parameter to the operation)
  * to manually round a number, call C<< $x->round($A,$P,$round_mode); >>
    this will round the number by using the appropriate rounding function
    and then normalize it.
  * rounding modifies the local settings of the number:

        $x = Math::BigFloat->new(123.456);
        $x->accuracy(5);
        $x->bround(4);

    Here 4 takes precedence over 5, so 123.5 is the result and $x->accuracy()
    will be 4 from now on.

=item Default values

  * R: 'even'
  * F: 40
  * A: undef
  * P: undef

=item Remarks

  * The defaults are set up so that the new code gives the same results as
    the old code (except in a few cases on fdiv):
    + Both A and P are undefined and thus will not be used for rounding
      after each operation.
    + round() is thus a no-op, unless given extra parameters A and P

=back

=head1 Infinity and Not a Number

While BigInt has extensive handling of inf and NaN, certain quirks remain.

=over 2

=item oct()/hex()

These perl routines currently (as of Perl v.5.8.6) cannot handle passed
inf.

	te@linux:~> perl -wle 'print 2 ** 3333'
	inf
	te@linux:~> perl -wle 'print 2 ** 3333 == 2 ** 3333'
	1
	te@linux:~> perl -wle 'print oct(2 ** 3333)'
	0
	te@linux:~> perl -wle 'print hex(2 ** 3333)'
	Illegal hexadecimal digit 'i' ignored at -e line 1.
	0

The same problems occur if you pass them Math::BigInt->binf() objects. Since
overloading these routines is not possible, this cannot be fixed from BigInt.

=item ==, !=, <, >, <=, >= with NaNs

BigInt's bcmp() routine currently returns undef to signal that a NaN was
involved in a comparison. However, the overload code turns that into
either 1 or '' and thus operations like C<< NaN != NaN >> might return
wrong values.

=item log(-inf)

C<< log(-inf) >> is highly weird. Since log(-x)=pi*i+log(x), then
log(-inf)=pi*i+inf. However, since the imaginary part is finite, the real
infinity "overshadows" it, so the number might as well just be infinity.
However, the result is a complex number, and since BigInt/BigFloat can only
have real numbers as results, the result is NaN.

=item exp(), cos(), sin(), atan2()

These all might have problems handling infinity right.
 
=back

=head1 INTERNALS

The actual numbers are stored as unsigned big integers (with seperate sign).

You should neither care about nor depend on the internal representation; it
might change without notice. Use B<ONLY> method calls like C<< $x->sign(); >>
instead relying on the internal representation.

=head2 MATH LIBRARY

Math with the numbers is done (by default) by a module called
C<Math::BigInt::Calc>. This is equivalent to saying:

	use Math::BigInt try => 'Calc';

You can change this backend library by using:

	use Math::BigInt try => 'GMP';

B<Note>: General purpose packages should not be explicit about the library
to use; let the script author decide which is best.

If your script works with huge numbers and Calc is too slow for them,
you can also for the loading of one of these libraries and if none
of them can be used, the code will die:

	use Math::BigInt only => 'GMP,Pari';

The following would first try to find Math::BigInt::Foo, then
Math::BigInt::Bar, and when this also fails, revert to Math::BigInt::Calc:

	use Math::BigInt try => 'Foo,Math::BigInt::Bar';

The library that is loaded last will be used. Note that this can be
overwritten at any time by loading a different library, and numbers
constructed with different libraries cannot be used in math operations
together.

=head3 What library to use?

B<Note>: General purpose packages should not be explicit about the library
to use; let the script author decide which is best.

L<Math::BigInt::GMP> and L<Math::BigInt::Pari> are in cases involving big
numbers much faster than Calc, however it is slower when dealing with very
small numbers (less than about 20 digits) and when converting very large
numbers to decimal (for instance for printing, rounding, calculating their
length in decimal etc).

So please select carefully what libary you want to use.

Different low-level libraries use different formats to store the numbers.
However, you should B<NOT> depend on the number having a specific format
internally.

See the respective math library module documentation for further details.

=head2 SIGN

The sign is either '+', '-', 'NaN', '+inf' or '-inf'.

A sign of 'NaN' is used to represent the result when input arguments are not
numbers or as a result of 0/0. '+inf' and '-inf' represent plus respectively
minus infinity. You will get '+inf' when dividing a positive number by 0, and
'-inf' when dividing any negative number by 0.

=head2 mantissa(), exponent() and parts()

C<mantissa()> and C<exponent()> return the said parts of the BigInt such
that:

        $m = $x->mantissa();
        $e = $x->exponent();
        $y = $m * ( 10 ** $e );
        print "ok\n" if $x == $y;

C<< ($m,$e) = $x->parts() >> is just a shortcut that gives you both of them
in one go. Both the returned mantissa and exponent have a sign.

Currently, for BigInts C<$e> is always 0, except +inf and -inf, where it is
C<+inf>; and for NaN, where it is C<NaN>; and for C<$x == 0>, where it is C<1>
(to be compatible with Math::BigFloat's internal representation of a zero as
C<0E1>).

C<$m> is currently just a copy of the original number. The relation between
C<$e> and C<$m> will stay always the same, though their real values might
change.

=head1 EXAMPLES
 
  use Math::BigInt;

  sub bint { Math::BigInt->new(shift); }

  $x = Math::BigInt->bstr("1234")      	# string "1234"
  $x = "$x";                         	# same as bstr()
  $x = Math::BigInt->bneg("1234");   	# BigInt "-1234"
  $x = Math::BigInt->babs("-12345"); 	# BigInt "12345"
  $x = Math::BigInt->bnorm("-0.00"); 	# BigInt "0"
  $x = bint(1) + bint(2);            	# BigInt "3"
  $x = bint(1) + "2";                	# ditto (auto-BigIntify of "2")
  $x = bint(1);                      	# BigInt "1"
  $x = $x + 5 / 2;                   	# BigInt "3"
  $x = $x ** 3;                      	# BigInt "27"
  $x *= 2;                           	# BigInt "54"
  $x = Math::BigInt->new(0);       	# BigInt "0"
  $x--;                              	# BigInt "-1"
  $x = Math::BigInt->badd(4,5)		# BigInt "9"
  print $x->bsstr();			# 9e+0

Examples for rounding:

  use Math::BigFloat;
  use Test;

  $x = Math::BigFloat->new(123.4567);
  $y = Math::BigFloat->new(123.456789);
  Math::BigFloat->accuracy(4);		# no more A than 4

  ok ($x->copy()->fround(),123.4);	# even rounding
  print $x->copy()->fround(),"\n";	# 123.4
  Math::BigFloat->round_mode('odd');	# round to odd
  print $x->copy()->fround(),"\n";	# 123.5
  Math::BigFloat->accuracy(5);		# no more A than 5
  Math::BigFloat->round_mode('odd');	# round to odd
  print $x->copy()->fround(),"\n";	# 123.46
  $y = $x->copy()->fround(4),"\n";	# A = 4: 123.4
  print "$y, ",$y->accuracy(),"\n";	# 123.4, 4

  Math::BigFloat->accuracy(undef);	# A not important now
  Math::BigFloat->precision(2); 	# P important
  print $x->copy()->bnorm(),"\n";	# 123.46
  print $x->copy()->fround(),"\n";	# 123.46

Examples for converting:

  my $x = Math::BigInt->new('0b1'.'01' x 123);
  print "bin: ",$x->as_bin()," hex:",$x->as_hex()," dec: ",$x,"\n";

=head1 Autocreating constants

After C<use Math::BigInt ':constant'> all the B<integer> decimal, hexadecimal
and binary constants in the given scope are converted to C<Math::BigInt>.
This conversion happens at compile time. 

In particular,

  perl -MMath::BigInt=:constant -e 'print 2**100,"\n"'

prints the integer value of C<2**100>. Note that without conversion of 
constants the expression 2**100 will be calculated as perl scalar.

Please note that strings and floating point constants are not affected,
so that

  	use Math::BigInt qw/:constant/;

	$x = 1234567890123456789012345678901234567890
		+ 123456789123456789;
	$y = '1234567890123456789012345678901234567890'
		+ '123456789123456789';

do not work. You need an explicit Math::BigInt->new() around one of the
operands. You should also quote large constants to protect loss of precision:

	use Math::BigInt;

	$x = Math::BigInt->new('1234567889123456789123456789123456789');

Without the quotes Perl would convert the large number to a floating point
constant at compile time and then hand the result to BigInt, which results in
an truncated result or a NaN.

This also applies to integers that look like floating point constants:

	use Math::BigInt ':constant';

	print ref(123e2),"\n";
	print ref(123.2e2),"\n";

will print nothing but newlines. Use either L<bignum> or L<Math::BigFloat>
to get this to work.

=head1 PERFORMANCE

Using the form $x += $y; etc over $x = $x + $y is faster, since a copy of $x
must be made in the second case. For long numbers, the copy can eat up to 20%
of the work (in the case of addition/subtraction, less for
multiplication/division). If $y is very small compared to $x, the form
$x += $y is MUCH faster than $x = $x + $y since making the copy of $x takes
more time then the actual addition.

With a technique called copy-on-write, the cost of copying with overload could
be minimized or even completely avoided. A test implementation of COW did show
performance gains for overloaded math, but introduced a performance loss due
to a constant overhead for all other operations. So Math::BigInt does currently
not COW.

The rewritten version of this module (vs. v0.01) is slower on certain
operations, like C<new()>, C<bstr()> and C<numify()>. The reason are that it
does now more work and handles much more cases. The time spent in these
operations is usually gained in the other math operations so that code on
the average should get (much) faster. If they don't, please contact the author.

Some operations may be slower for small numbers, but are significantly faster
for big numbers. Other operations are now constant (O(1), like C<bneg()>,
C<babs()> etc), instead of O(N) and thus nearly always take much less time.
These optimizations were done on purpose.

If you find the Calc module to slow, try to install any of the replacement
modules and see if they help you. 

=head2 Alternative math libraries

You can use an alternative library to drive Math::BigInt. See the section
L<MATH LIBRARY> for more information.

For more benchmark results see L<http://bloodgate.com/perl/benchmarks.html>.

=head2 SUBCLASSING

=head1 Subclassing Math::BigInt

The basic design of Math::BigInt allows simple subclasses with very little
work, as long as a few simple rules are followed:

=over 2

=item *

The public API must remain consistent, i.e. if a sub-class is overloading
addition, the sub-class must use the same name, in this case badd(). The
reason for this is that Math::BigInt is optimized to call the object methods
directly.

=item *

The private object hash keys like C<$x->{sign}> may not be changed, but
additional keys can be added, like C<$x->{_custom}>.

=item *

Accessor functions are available for all existing object hash keys and should
be used instead of directly accessing the internal hash keys. The reason for
this is that Math::BigInt itself has a pluggable interface which permits it
to support different storage methods.

=back

More complex sub-classes may have to replicate more of the logic internal of
Math::BigInt if they need to change more basic behaviors. A subclass that
needs to merely change the output only needs to overload C<bstr()>. 

All other object methods and overloaded functions can be directly inherited
from the parent class.

At the very minimum, any subclass will need to provide its own C<new()> and can
store additional hash keys in the object. There are also some package globals
that must be defined, e.g.:

  # Globals
  $accuracy = undef;
  $precision = -2;       # round to 2 decimal places
  $round_mode = 'even';
  $div_scale = 40;

Additionally, you might want to provide the following two globals to allow
auto-upgrading and auto-downgrading to work correctly:

  $upgrade = undef;
  $downgrade = undef;

This allows Math::BigInt to correctly retrieve package globals from the 
subclass, like C<$SubClass::precision>.  See t/Math/BigInt/Subclass.pm or
t/Math/BigFloat/SubClass.pm completely functional subclass examples.

Don't forget to 

	use overload;

in your subclass to automatically inherit the overloading from the parent. If
you like, you can change part of the overloading, look at Math::String for an
example.

=head1 UPGRADING

When used like this:

	use Math::BigInt upgrade => 'Foo::Bar';

certain operations will 'upgrade' their calculation and thus the result to
the class Foo::Bar. Usually this is used in conjunction with Math::BigFloat:

	use Math::BigInt upgrade => 'Math::BigFloat';

As a shortcut, you can use the module C<bignum>:

	use bignum;

Also good for oneliners:

	perl -Mbignum -le 'print 2 ** 255'

This makes it possible to mix arguments of different classes (as in 2.5 + 2)
as well es preserve accuracy (as in sqrt(3)).

Beware: This feature is not fully implemented yet.

=head2 Auto-upgrade

The following methods upgrade themselves unconditionally; that is if upgrade
is in effect, they will always hand up their work:

=over 2

=item bsqrt()

=item div()

=item blog()

=item bexp()

=back

Beware: This list is not complete.

All other methods upgrade themselves only when one (or all) of their
arguments are of the class mentioned in $upgrade (This might change in later
versions to a more sophisticated scheme):

=head1 EXPORTS

C<Math::BigInt> exports nothing by default, but can export the following methods:

	bgcd
	blcm

=head1 CAVEATS

Some things might not work as you expect them. Below is documented what is
known to be troublesome:

=over 1

=item bstr(), bsstr() and 'cmp'

Both C<bstr()> and C<bsstr()> as well as automated stringify via overload now
drop the leading '+'. The old code would return '+3', the new returns '3'.
This is to be consistent with Perl and to make C<cmp> (especially with
overloading) to work as you expect. It also solves problems with C<Test.pm>,
because its C<ok()> uses 'eq' internally. 

Mark Biggar said, when asked about to drop the '+' altogether, or make only
C<cmp> work:

	I agree (with the first alternative), don't add the '+' on positive
	numbers.  It's not as important anymore with the new internal 
	form for numbers.  It made doing things like abs and neg easier,
	but those have to be done differently now anyway.

So, the following examples will now work all as expected:

	use Test;
        BEGIN { plan tests => 1 }
	use Math::BigInt;

	my $x = new Math::BigInt 3*3;
	my $y = new Math::BigInt 3*3;

	ok ($x,3*3);
	print "$x eq 9" if $x eq $y;
	print "$x eq 9" if $x eq '9';
	print "$x eq 9" if $x eq 3*3;

Additionally, the following still works:
	
	print "$x == 9" if $x == $y;
	print "$x == 9" if $x == 9;
	print "$x == 9" if $x == 3*3;

There is now a C<bsstr()> method to get the string in scientific notation aka
C<1e+2> instead of C<100>. Be advised that overloaded 'eq' always uses bstr()
for comparison, but Perl will represent some numbers as 100 and others
as 1e+308. If in doubt, convert both arguments to Math::BigInt before 
comparing them as strings:

	use Test;
        BEGIN { plan tests => 3 }
	use Math::BigInt;

	$x = Math::BigInt->new('1e56'); $y = 1e56;
	ok ($x,$y);			# will fail
	ok ($x->bsstr(),$y);		# okay
	$y = Math::BigInt->new($y);
	ok ($x,$y);			# okay

Alternatively, simple use C<< <=> >> for comparisons, this will get it
always right. There is not yet a way to get a number automatically represented
as a string that matches exactly the way Perl represents it.

See also the section about L<Infinity and Not a Number> for problems in
comparing NaNs.

=item int()

C<int()> will return (at least for Perl v5.7.1 and up) another BigInt, not a 
Perl scalar:

	$x = Math::BigInt->new(123);
	$y = int($x);				# BigInt 123
	$x = Math::BigFloat->new(123.45);
	$y = int($x);				# BigInt 123

In all Perl versions you can use C<as_number()> or C<as_int> for the same
effect:

	$x = Math::BigFloat->new(123.45);
	$y = $x->as_number();			# BigInt 123
	$y = $x->as_int();			# ditto

This also works for other subclasses, like Math::String.

If you want a real Perl scalar, use C<numify()>:

	$y = $x->numify();			# 123 as scalar

This is seldom necessary, though, because this is done automatically, like
when you access an array:

	$z = $array[$x];			# does work automatically

=item length

The following will probably not do what you expect:

	$c = Math::BigInt->new(123);
	print $c->length(),"\n";		# prints 30

It prints both the number of digits in the number and in the fraction part
since print calls C<length()> in list context. Use something like: 
	
	print scalar $c->length(),"\n";		# prints 3 

=item bdiv

The following will probably not do what you expect:

	print $c->bdiv(10000),"\n";

It prints both quotient and remainder since print calls C<bdiv()> in list
context. Also, C<bdiv()> will modify $c, so be careful. You probably want
to use
	
	print $c / 10000,"\n";
	print scalar $c->bdiv(10000),"\n";  # or if you want to modify $c

instead.

The quotient is always the greatest integer less than or equal to the
real-valued quotient of the two operands, and the remainder (when it is
nonzero) always has the same sign as the second operand; so, for
example,

	  1 / 4  => ( 0, 1)
	  1 / -4 => (-1,-3)
	 -3 / 4  => (-1, 1)
	 -3 / -4 => ( 0,-3)
	-11 / 2  => (-5,1)
	 11 /-2  => (-5,-1)

As a consequence, the behavior of the operator % agrees with the
behavior of Perl's built-in % operator (as documented in the perlop
manpage), and the equation

	$x == ($x / $y) * $y + ($x % $y)

holds true for any $x and $y, which justifies calling the two return
values of bdiv() the quotient and remainder. The only exception to this rule
are when $y == 0 and $x is negative, then the remainder will also be
negative. See below under "infinity handling" for the reasoning behind this.

Perl's 'use integer;' changes the behaviour of % and / for scalars, but will
not change BigInt's way to do things. This is because under 'use integer' Perl
will do what the underlying C thinks is right and this is different for each
system. If you need BigInt's behaving exactly like Perl's 'use integer', bug
the author to implement it ;)

=item infinity handling

Here are some examples that explain the reasons why certain results occur while
handling infinity:

The following table shows the result of the division and the remainder, so that
the equation above holds true. Some "ordinary" cases are strewn in to show more
clearly the reasoning:

	A /  B  =   C,     R so that C *    B +    R =    A
     =========================================================
	5 /   8 =   0,     5 	     0 *    8 +    5 =    5
	0 /   8 =   0,     0	     0 *    8 +    0 =    0
	0 / inf =   0,     0	     0 *  inf +    0 =    0
	0 /-inf =   0,     0	     0 * -inf +    0 =    0
	5 / inf =   0,     5	     0 *  inf +    5 =    5
	5 /-inf =   0,     5	     0 * -inf +    5 =    5
	-5/ inf =   0,    -5	     0 *  inf +   -5 =   -5
	-5/-inf =   0,    -5	     0 * -inf +   -5 =   -5
       inf/   5 =  inf,    0	   inf *    5 +    0 =  inf
      -inf/   5 = -inf,    0      -inf *    5 +    0 = -inf
       inf/  -5 = -inf,    0	  -inf *   -5 +    0 =  inf
      -inf/  -5 =  inf,    0       inf *   -5 +    0 = -inf
	 5/   5 =    1,    0         1 *    5 +    0 =    5
	-5/  -5 =    1,    0         1 *   -5 +    0 =   -5
       inf/ inf =    1,    0         1 *  inf +    0 =  inf
      -inf/-inf =    1,    0         1 * -inf +    0 = -inf
       inf/-inf =   -1,    0        -1 * -inf +    0 =  inf
      -inf/ inf =   -1,    0         1 * -inf +    0 = -inf
	 8/   0 =  inf,    8       inf *    0 +    8 =    8 
       inf/   0 =  inf,  inf       inf *    0 +  inf =  inf 
         0/   0 =  NaN

These cases below violate the "remainder has the sign of the second of the two
arguments", since they wouldn't match up otherwise.

	A /  B  =   C,     R so that C *    B +    R =    A
     ========================================================
      -inf/   0 = -inf, -inf      -inf *    0 +  inf = -inf 
	-8/   0 = -inf,   -8      -inf *    0 +    8 = -8 

=item Modifying and =

Beware of:

        $x = Math::BigFloat->new(5);
        $y = $x;

It will not do what you think, e.g. making a copy of $x. Instead it just makes
a second reference to the B<same> object and stores it in $y. Thus anything
that modifies $x (except overloaded operators) will modify $y, and vice versa.
Or in other words, C<=> is only safe if you modify your BigInts only via
overloaded math. As soon as you use a method call it breaks:

        $x->bmul(2);
        print "$x, $y\n";       # prints '10, 10'

If you want a true copy of $x, use:

        $y = $x->copy();

You can also chain the calls like this, this will make first a copy and then
multiply it by 2:

        $y = $x->copy()->bmul(2);

See also the documentation for overload.pm regarding C<=>.

=item bpow

C<bpow()> (and the rounding functions) now modifies the first argument and
returns it, unlike the old code which left it alone and only returned the
result. This is to be consistent with C<badd()> etc. The first three will
modify $x, the last one won't:

	print bpow($x,$i),"\n"; 	# modify $x
	print $x->bpow($i),"\n"; 	# ditto
	print $x **= $i,"\n";		# the same
	print $x ** $i,"\n";		# leave $x alone 

The form C<$x **= $y> is faster than C<$x = $x ** $y;>, though.

=item Overloading -$x

The following:

	$x = -$x;

is slower than

	$x->bneg();

since overload calls C<sub($x,0,1);> instead of C<neg($x)>. The first variant
needs to preserve $x since it does not know that it later will get overwritten.
This makes a copy of $x and takes O(N), but $x->bneg() is O(1).

=item Mixing different object types

In Perl you will get a floating point value if you do one of the following:

	$float = 5.0 + 2;
	$float = 2 + 5.0;
	$float = 5 / 2;

With overloaded math, only the first two variants will result in a BigFloat:

	use Math::BigInt;
	use Math::BigFloat;
	
	$mbf = Math::BigFloat->new(5);
	$mbi2 = Math::BigInteger->new(5);
	$mbi = Math::BigInteger->new(2);

					# what actually gets called:
	$float = $mbf + $mbi;		# $mbf->badd()
	$float = $mbf / $mbi;		# $mbf->bdiv()
	$integer = $mbi + $mbf;		# $mbi->badd()
	$integer = $mbi2 / $mbi;	# $mbi2->bdiv()
	$integer = $mbi2 / $mbf;	# $mbi2->bdiv()

This is because math with overloaded operators follows the first (dominating)
operand, and the operation of that is called and returns thus the result. So,
Math::BigInt::bdiv() will always return a Math::BigInt, regardless whether
the result should be a Math::BigFloat or the second operant is one.

To get a Math::BigFloat you either need to call the operation manually,
make sure the operands are already of the proper type or casted to that type
via Math::BigFloat->new():
	
	$float = Math::BigFloat->new($mbi2) / $mbi;	# = 2.5

Beware of simple "casting" the entire expression, this would only convert
the already computed result:

	$float = Math::BigFloat->new($mbi2 / $mbi);	# = 2.0 thus wrong!

Beware also of the order of more complicated expressions like:

	$integer = ($mbi2 + $mbi) / $mbf;		# int / float => int
	$integer = $mbi2 / Math::BigFloat->new($mbi);	# ditto

If in doubt, break the expression into simpler terms, or cast all operands
to the desired resulting type.

Scalar values are a bit different, since:
	
	$float = 2 + $mbf;
	$float = $mbf + 2;

will both result in the proper type due to the way the overloaded math works.

This section also applies to other overloaded math packages, like Math::String.

One solution to you problem might be autoupgrading|upgrading. See the
pragmas L<bignum>, L<bigint> and L<bigrat> for an easy way to do this.

=item bsqrt()

C<bsqrt()> works only good if the result is a big integer, e.g. the square
root of 144 is 12, but from 12 the square root is 3, regardless of rounding
mode. The reason is that the result is always truncated to an integer.

If you want a better approximation of the square root, then use:

	$x = Math::BigFloat->new(12);
	Math::BigFloat->precision(0);
	Math::BigFloat->round_mode('even');
	print $x->copy->bsqrt(),"\n";		# 4

	Math::BigFloat->precision(2);
	print $x->bsqrt(),"\n";			# 3.46
	print $x->bsqrt(3),"\n";		# 3.464

=item brsft()

For negative numbers in base see also L<brsft|brsft>.

=back

=head1 LICENSE

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=head1 SEE ALSO

L<Math::BigFloat>, L<Math::BigRat> and L<Math::Big> as well as
L<Math::BigInt::BitVect>, L<Math::BigInt::Pari> and  L<Math::BigInt::GMP>.

The pragmas L<bignum>, L<bigint> and L<bigrat> also might be of interest
because they solve the autoupgrading/downgrading issue, at least partly.

The package at
L<http://search.cpan.org/search?mode=module&query=Math%3A%3ABigInt> contains
more documentation including a full version history, testcases, empty
subclass files and benchmarks.

=head1 AUTHORS

Original code by Mark Biggar, overloaded interface by Ilya Zakharevich.
Completely rewritten by Tels http://bloodgate.com in late 2000, 2001 - 2006
and still at it in 2007.

Many people contributed in one or more ways to the final beast, see the file
CREDITS for an (incomplete) list. If you miss your name, please drop me a
mail. Thank you!

=cut
