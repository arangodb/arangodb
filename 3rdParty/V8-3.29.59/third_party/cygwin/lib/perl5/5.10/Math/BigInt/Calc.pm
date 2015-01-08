package Math::BigInt::Calc;

use 5.006;
use strict;
# use warnings;	# dont use warnings for older Perls

our $VERSION = '0.52';

# Package to store unsigned big integers in decimal and do math with them

# Internally the numbers are stored in an array with at least 1 element, no
# leading zero parts (except the first) and in base 1eX where X is determined
# automatically at loading time to be the maximum possible value

# todo:
# - fully remove funky $# stuff in div() (maybe - that code scares me...)

# USE_MUL: due to problems on certain os (os390, posix-bc) "* 1e-5" is used
# instead of "/ 1e5" at some places, (marked with USE_MUL). Other platforms
# BS2000, some Crays need USE_DIV instead.
# The BEGIN block is used to determine which of the two variants gives the
# correct result.

# Beware of things like:
# $i = $i * $y + $car; $car = int($i / $BASE); $i = $i % $BASE;
# This works on x86, but fails on ARM (SA1100, iPAQ) due to whoknows what
# reasons. So, use this instead (slower, but correct):
# $i = $i * $y + $car; $car = int($i / $BASE); $i -= $BASE * $car;

##############################################################################
# global constants, flags and accessory

# announce that we are compatible with MBI v1.83 and up
sub api_version () { 2; }
 
# constants for easier life
my ($BASE,$BASE_LEN,$RBASE,$MAX_VAL);
my ($AND_BITS,$XOR_BITS,$OR_BITS);
my ($AND_MASK,$XOR_MASK,$OR_MASK);

sub _base_len 
  {
  # Set/get the BASE_LEN and assorted other, connected values.
  # Used only by the testsuite, the set variant is used only by the BEGIN
  # block below:
  shift;

  my ($b, $int) = @_;
  if (defined $b)
    {
    # avoid redefinitions
    undef &_mul;
    undef &_div;

    if ($] >= 5.008 && $int && $b > 7)
      {
      $BASE_LEN = $b;
      *_mul = \&_mul_use_div_64;
      *_div = \&_div_use_div_64;
      $BASE = int("1e".$BASE_LEN);
      $MAX_VAL = $BASE-1;
      return $BASE_LEN unless wantarray;
      return ($BASE_LEN, $AND_BITS, $XOR_BITS, $OR_BITS, $BASE_LEN, $MAX_VAL, $BASE);
      }

    # find whether we can use mul or div in mul()/div()
    $BASE_LEN = $b+1;
    my $caught = 0;
    while (--$BASE_LEN > 5)
      {
      $BASE = int("1e".$BASE_LEN);
      $RBASE = abs('1e-'.$BASE_LEN);			# see USE_MUL
      $caught = 0;
      $caught += 1 if (int($BASE * $RBASE) != 1);	# should be 1
      $caught += 2 if (int($BASE / $BASE) != 1);	# should be 1
      last if $caught != 3;
      }
    $BASE = int("1e".$BASE_LEN);
    $RBASE = abs('1e-'.$BASE_LEN);			# see USE_MUL
    $MAX_VAL = $BASE-1;
   
    # ($caught & 1) != 0 => cannot use MUL
    # ($caught & 2) != 0 => cannot use DIV
    if ($caught == 2)				# 2
      {
      # must USE_MUL since we cannot use DIV
      *_mul = \&_mul_use_mul;
      *_div = \&_div_use_mul;
      }
    else					# 0 or 1
      {
      # can USE_DIV instead
      *_mul = \&_mul_use_div;
      *_div = \&_div_use_div;
      }
    }
  return $BASE_LEN unless wantarray;
  return ($BASE_LEN, $AND_BITS, $XOR_BITS, $OR_BITS, $BASE_LEN, $MAX_VAL, $BASE);
  }

sub _new
  {
  # (ref to string) return ref to num_array
  # Convert a number from string format (without sign) to internal base
  # 1ex format. Assumes normalized value as input.
  my $il = length($_[1])-1;

  # < BASE_LEN due len-1 above
  return [ int($_[1]) ] if $il < $BASE_LEN;	# shortcut for short numbers

  # this leaves '00000' instead of int 0 and will be corrected after any op
  [ reverse(unpack("a" . ($il % $BASE_LEN+1) 
    . ("a$BASE_LEN" x ($il / $BASE_LEN)), $_[1])) ];
  }                                                                             

BEGIN
  {
  # from Daniel Pfeiffer: determine largest group of digits that is precisely
  # multipliable with itself plus carry
  # Test now changed to expect the proper pattern, not a result off by 1 or 2
  my ($e, $num) = 3;	# lowest value we will use is 3+1-1 = 3
  do 
    {
    $num = ('9' x ++$e) + 0;
    $num *= $num + 1.0;
    } while ("$num" =~ /9{$e}0{$e}/);	# must be a certain pattern
  $e--; 				# last test failed, so retract one step
  # the limits below brush the problems with the test above under the rug:
  # the test should be able to find the proper $e automatically
  $e = 5 if $^O =~ /^uts/;	# UTS get's some special treatment
  $e = 5 if $^O =~ /^unicos/;	# unicos is also problematic (6 seems to work
				# there, but we play safe)

  my $int = 0;
  if ($e > 7)
    {
    use integer;
    my $e1 = 7;
    $num = 7;
    do 
      {
      $num = ('9' x ++$e1) + 0;
      $num *= $num + 1;
      } while ("$num" =~ /9{$e1}0{$e1}/);	# must be a certain pattern
    $e1--; 					# last test failed, so retract one step
    if ($e1 > 7)
      { 
      $int = 1; $e = $e1; 
      }
    }
 
  __PACKAGE__->_base_len($e,$int);	# set and store

  use integer;
  # find out how many bits _and, _or and _xor can take (old default = 16)
  # I don't think anybody has yet 128 bit scalars, so let's play safe.
  local $^W = 0;	# don't warn about 'nonportable number'
  $AND_BITS = 15; $XOR_BITS = 15; $OR_BITS = 15;

  # find max bits, we will not go higher than numberofbits that fit into $BASE
  # to make _and etc simpler (and faster for smaller, slower for large numbers)
  my $max = 16;
  while (2 ** $max < $BASE) { $max++; }
  {
    no integer;
    $max = 16 if $] < 5.006;	# older Perls might not take >16 too well
  }
  my ($x,$y,$z);
  do {
    $AND_BITS++;
    $x = CORE::oct('0b' . '1' x $AND_BITS); $y = $x & $x;
    $z = (2 ** $AND_BITS) - 1;
    } while ($AND_BITS < $max && $x == $z && $y == $x);
  $AND_BITS --;						# retreat one step
  do {
    $XOR_BITS++;
    $x = CORE::oct('0b' . '1' x $XOR_BITS); $y = $x ^ 0;
    $z = (2 ** $XOR_BITS) - 1;
    } while ($XOR_BITS < $max && $x == $z && $y == $x);
  $XOR_BITS --;						# retreat one step
  do {
    $OR_BITS++;
    $x = CORE::oct('0b' . '1' x $OR_BITS); $y = $x | $x;
    $z = (2 ** $OR_BITS) - 1;
    } while ($OR_BITS < $max && $x == $z && $y == $x);
  $OR_BITS --;						# retreat one step
  
  $AND_MASK = __PACKAGE__->_new( ( 2 ** $AND_BITS ));
  $XOR_MASK = __PACKAGE__->_new( ( 2 ** $XOR_BITS ));
  $OR_MASK = __PACKAGE__->_new( ( 2 ** $OR_BITS ));

  # We can compute the approximate lenght no faster than the real length:
  *_alen = \&_len;
  }

###############################################################################

sub _zero
  {
  # create a zero
  [ 0 ];
  }

sub _one
  {
  # create a one
  [ 1 ];
  }

sub _two
  {
  # create a two (used internally for shifting)
  [ 2 ];
  }

sub _ten
  {
  # create a 10 (used internally for shifting)
  [ 10 ];
  }

sub _1ex
  {
  # create a 1Ex
  my $rem = $_[1] % $BASE_LEN;		# remainder
  my $parts = $_[1] / $BASE_LEN;	# parts

  # 000000, 000000, 100 
  [ (0) x $parts, '1' . ('0' x $rem) ];
  }

sub _copy
  {
  # make a true copy
  [ @{$_[1]} ];
  }

# catch and throw away
sub import { }

##############################################################################
# convert back to string and number

sub _str
  {
  # (ref to BINT) return num_str
  # Convert number from internal base 100000 format to string format.
  # internal format is always normalized (no leading zeros, "-0" => "+0")
  my $ar = $_[1];

  my $l = scalar @$ar;				# number of parts
  if ($l < 1)					# should not happen
    {
    require Carp;
    Carp::croak("$_[1] has no elements");
    }

  my $ret = "";
  # handle first one different to strip leading zeros from it (there are no
  # leading zero parts in internal representation)
  $l --; $ret .= int($ar->[$l]); $l--;
  # Interestingly, the pre-padd method uses more time
  # the old grep variant takes longer (14 vs. 10 sec)
  my $z = '0' x ($BASE_LEN-1);                            
  while ($l >= 0)
    {
    $ret .= substr($z.$ar->[$l],-$BASE_LEN); # fastest way I could think of
    $l--;
    }
  $ret;
  }                                                                             

sub _num
  {
  # Make a number (scalar int/float) from a BigInt object 
  my $x = $_[1];

  return 0+$x->[0] if scalar @$x == 1;  # below $BASE
  my $fac = 1;
  my $num = 0;
  foreach (@$x)
    {
    $num += $fac*$_; $fac *= $BASE;
    }
  $num; 
  }

##############################################################################
# actual math code

sub _add
  {
  # (ref to int_num_array, ref to int_num_array)
  # routine to add two base 1eX numbers
  # stolen from Knuth Vol 2 Algorithm A pg 231
  # there are separate routines to add and sub as per Knuth pg 233
  # This routine clobbers up array x, but not y.
 
  my ($c,$x,$y) = @_;

  return $x if (@$y == 1) && $y->[0] == 0;		# $x + 0 => $x
  if ((@$x == 1) && $x->[0] == 0)			# 0 + $y => $y->copy
    {
    # twice as slow as $x = [ @$y ], but nec. to retain $x as ref :(
    @$x = @$y; return $x;		
    }
 
  # for each in Y, add Y to X and carry. If after that, something is left in
  # X, foreach in X add carry to X and then return X, carry
  # Trades one "$j++" for having to shift arrays
  my $i; my $car = 0; my $j = 0;
  for $i (@$y)
    {
    $x->[$j] -= $BASE if $car = (($x->[$j] += $i + $car) >= $BASE) ? 1 : 0;
    $j++;
    }
  while ($car != 0)
    {
    $x->[$j] -= $BASE if $car = (($x->[$j] += $car) >= $BASE) ? 1 : 0; $j++;
    }
  $x;
  }                                                                             

sub _inc
  {
  # (ref to int_num_array, ref to int_num_array)
  # Add 1 to $x, modify $x in place
  my ($c,$x) = @_;

  for my $i (@$x)
    {
    return $x if (($i += 1) < $BASE);		# early out
    $i = 0;					# overflow, next
    }
  push @$x,1 if (($x->[-1] || 0) == 0);		# last overflowed, so extend
  $x;
  }                                                                             

sub _dec
  {
  # (ref to int_num_array, ref to int_num_array)
  # Sub 1 from $x, modify $x in place
  my ($c,$x) = @_;

  my $MAX = $BASE-1;				# since MAX_VAL based on BASE
  for my $i (@$x)
    {
    last if (($i -= 1) >= 0);			# early out
    $i = $MAX;					# underflow, next
    }
  pop @$x if $x->[-1] == 0 && @$x > 1;		# last underflowed (but leave 0)
  $x;
  }                                                                             

sub _sub
  {
  # (ref to int_num_array, ref to int_num_array, swap)
  # subtract base 1eX numbers -- stolen from Knuth Vol 2 pg 232, $x > $y
  # subtract Y from X by modifying x in place
  my ($c,$sx,$sy,$s) = @_;
 
  my $car = 0; my $i; my $j = 0;
  if (!$s)
    {
    for $i (@$sx)
      {
      last unless defined $sy->[$j] || $car;
      $i += $BASE if $car = (($i -= ($sy->[$j] || 0) + $car) < 0); $j++;
      }
    # might leave leading zeros, so fix that
    return __strip_zeros($sx);
    }
  for $i (@$sx)
    {
    # we can't do an early out if $x is < than $y, since we
    # need to copy the high chunks from $y. Found by Bob Mathews.
    #last unless defined $sy->[$j] || $car;
    $sy->[$j] += $BASE
     if $car = (($sy->[$j] = $i-($sy->[$j]||0) - $car) < 0);
    $j++;
    }
  # might leave leading zeros, so fix that
  __strip_zeros($sy);
  }                                                                             

sub _mul_use_mul
  {
  # (ref to int_num_array, ref to int_num_array)
  # multiply two numbers in internal representation
  # modifies first arg, second need not be different from first
  my ($c,$xv,$yv) = @_;

  if (@$yv == 1)
    {
    # shortcut for two very short numbers (improved by Nathan Zook)
    # works also if xv and yv are the same reference, and handles also $x == 0
    if (@$xv == 1)
      {
      if (($xv->[0] *= $yv->[0]) >= $BASE)
         {
         $xv->[0] = $xv->[0] - ($xv->[1] = int($xv->[0] * $RBASE)) * $BASE;
         };
      return $xv;
      }
    # $x * 0 => 0
    if ($yv->[0] == 0)
      {
      @$xv = (0);
      return $xv;
      }
    # multiply a large number a by a single element one, so speed up
    my $y = $yv->[0]; my $car = 0;
    foreach my $i (@$xv)
      {
      $i = $i * $y + $car; $car = int($i * $RBASE); $i -= $car * $BASE;
      }
    push @$xv, $car if $car != 0;
    return $xv;
    }
  # shortcut for result $x == 0 => result = 0
  return $xv if ( ((@$xv == 1) && ($xv->[0] == 0)) ); 

  # since multiplying $x with $x fails, make copy in this case
  $yv = [@$xv] if $xv == $yv;	# same references?

  my @prod = (); my ($prod,$car,$cty,$xi,$yi);

  for $xi (@$xv)
    {
    $car = 0; $cty = 0;

    # slow variant
#    for $yi (@$yv)
#      {
#      $prod = $xi * $yi + ($prod[$cty] || 0) + $car;
#      $prod[$cty++] =
#       $prod - ($car = int($prod * RBASE)) * $BASE;  # see USE_MUL
#      }
#    $prod[$cty] += $car if $car; # need really to check for 0?
#    $xi = shift @prod;

    # faster variant
    # looping through this if $xi == 0 is silly - so optimize it away!
    $xi = (shift @prod || 0), next if $xi == 0;
    for $yi (@$yv)
      {
      $prod = $xi * $yi + ($prod[$cty] || 0) + $car;
##     this is actually a tad slower
##        $prod = $prod[$cty]; $prod += ($car + $xi * $yi);	# no ||0 here
      $prod[$cty++] =
       $prod - ($car = int($prod * $RBASE)) * $BASE;  # see USE_MUL
      }
    $prod[$cty] += $car if $car; # need really to check for 0?
    $xi = shift @prod || 0;	# || 0 makes v5.005_3 happy
    }
  push @$xv, @prod;
  # can't have leading zeros
#  __strip_zeros($xv);
  $xv;
  }                                                                             

sub _mul_use_div_64
  {
  # (ref to int_num_array, ref to int_num_array)
  # multiply two numbers in internal representation
  # modifies first arg, second need not be different from first
  # works for 64 bit integer with "use integer"
  my ($c,$xv,$yv) = @_;

  use integer;
  if (@$yv == 1)
    {
    # shortcut for two small numbers, also handles $x == 0
    if (@$xv == 1)
      {
      # shortcut for two very short numbers (improved by Nathan Zook)
      # works also if xv and yv are the same reference, and handles also $x == 0
      if (($xv->[0] *= $yv->[0]) >= $BASE)
          {
          $xv->[0] =
              $xv->[0] - ($xv->[1] = $xv->[0] / $BASE) * $BASE;
          };
      return $xv;
      }
    # $x * 0 => 0
    if ($yv->[0] == 0)
      {
      @$xv = (0);
      return $xv;
      }
    # multiply a large number a by a single element one, so speed up
    my $y = $yv->[0]; my $car = 0;
    foreach my $i (@$xv)
      {
      #$i = $i * $y + $car; $car = $i / $BASE; $i -= $car * $BASE;
      $i = $i * $y + $car; $i -= ($car = $i / $BASE) * $BASE;
      }
    push @$xv, $car if $car != 0;
    return $xv;
    }
  # shortcut for result $x == 0 => result = 0
  return $xv if ( ((@$xv == 1) && ($xv->[0] == 0)) ); 

  # since multiplying $x with $x fails, make copy in this case
  $yv = [@$xv] if $xv == $yv;	# same references?

  my @prod = (); my ($prod,$car,$cty,$xi,$yi);
  for $xi (@$xv)
    {
    $car = 0; $cty = 0;
    # looping through this if $xi == 0 is silly - so optimize it away!
    $xi = (shift @prod || 0), next if $xi == 0;
    for $yi (@$yv)
      {
      $prod = $xi * $yi + ($prod[$cty] || 0) + $car;
      $prod[$cty++] = $prod - ($car = $prod / $BASE) * $BASE;
      }
    $prod[$cty] += $car if $car; # need really to check for 0?
    $xi = shift @prod || 0;	# || 0 makes v5.005_3 happy
    }
  push @$xv, @prod;
  $xv;
  }                                                                             

sub _mul_use_div
  {
  # (ref to int_num_array, ref to int_num_array)
  # multiply two numbers in internal representation
  # modifies first arg, second need not be different from first
  my ($c,$xv,$yv) = @_;

  if (@$yv == 1)
    {
    # shortcut for two small numbers, also handles $x == 0
    if (@$xv == 1)
      {
      # shortcut for two very short numbers (improved by Nathan Zook)
      # works also if xv and yv are the same reference, and handles also $x == 0
      if (($xv->[0] *= $yv->[0]) >= $BASE)
          {
          $xv->[0] =
              $xv->[0] - ($xv->[1] = int($xv->[0] / $BASE)) * $BASE;
          };
      return $xv;
      }
    # $x * 0 => 0
    if ($yv->[0] == 0)
      {
      @$xv = (0);
      return $xv;
      }
    # multiply a large number a by a single element one, so speed up
    my $y = $yv->[0]; my $car = 0;
    foreach my $i (@$xv)
      {
      $i = $i * $y + $car; $car = int($i / $BASE); $i -= $car * $BASE;
      # This (together with use integer;) does not work on 32-bit Perls
      #$i = $i * $y + $car; $i -= ($car = $i / $BASE) * $BASE;
      }
    push @$xv, $car if $car != 0;
    return $xv;
    }
  # shortcut for result $x == 0 => result = 0
  return $xv if ( ((@$xv == 1) && ($xv->[0] == 0)) ); 

  # since multiplying $x with $x fails, make copy in this case
  $yv = [@$xv] if $xv == $yv;	# same references?

  my @prod = (); my ($prod,$car,$cty,$xi,$yi);
  for $xi (@$xv)
    {
    $car = 0; $cty = 0;
    # looping through this if $xi == 0 is silly - so optimize it away!
    $xi = (shift @prod || 0), next if $xi == 0;
    for $yi (@$yv)
      {
      $prod = $xi * $yi + ($prod[$cty] || 0) + $car;
      $prod[$cty++] = $prod - ($car = int($prod / $BASE)) * $BASE;
      }
    $prod[$cty] += $car if $car; # need really to check for 0?
    $xi = shift @prod || 0;	# || 0 makes v5.005_3 happy
    }
  push @$xv, @prod;
  # can't have leading zeros
#  __strip_zeros($xv);
  $xv;
  }                                                                             

sub _div_use_mul
  {
  # ref to array, ref to array, modify first array and return remainder if 
  # in list context

  # see comments in _div_use_div() for more explanations

  my ($c,$x,$yorg) = @_;
  
  # the general div algorithmn here is about O(N*N) and thus quite slow, so
  # we first check for some special cases and use shortcuts to handle them.

  # This works, because we store the numbers in a chunked format where each
  # element contains 5..7 digits (depending on system).

  # if both numbers have only one element:
  if (@$x == 1 && @$yorg == 1)
    {
    # shortcut, $yorg and $x are two small numbers
    if (wantarray)
      {
      my $r = [ $x->[0] % $yorg->[0] ];
      $x->[0] = int($x->[0] / $yorg->[0]);
      return ($x,$r); 
      }
    else
      {
      $x->[0] = int($x->[0] / $yorg->[0]);
      return $x; 
      }
    }

  # if x has more than one, but y has only one element:
  if (@$yorg == 1)
    {
    my $rem;
    $rem = _mod($c,[ @$x ],$yorg) if wantarray;

    # shortcut, $y is < $BASE
    my $j = scalar @$x; my $r = 0; 
    my $y = $yorg->[0]; my $b;
    while ($j-- > 0)
      {
      $b = $r * $BASE + $x->[$j];
      $x->[$j] = int($b/$y);
      $r = $b % $y;
      }
    pop @$x if @$x > 1 && $x->[-1] == 0;	# splice up a leading zero 
    return ($x,$rem) if wantarray;
    return $x;
    }

  # now x and y have more than one element

  # check whether y has more elements than x, if yet, the result will be 0
  if (@$yorg > @$x)
    {
    my $rem;
    $rem = [@$x] if wantarray;                  # make copy
    splice (@$x,1);                             # keep ref to original array
    $x->[0] = 0;                                # set to 0
    return ($x,$rem) if wantarray;              # including remainder?
    return $x;					# only x, which is [0] now
    }
  # check whether the numbers have the same number of elements, in that case
  # the result will fit into one element and can be computed efficiently
  if (@$yorg == @$x)
    {
    my $rem;
    # if $yorg has more digits than $x (it's leading element is longer than
    # the one from $x), the result will also be 0:
    if (length(int($yorg->[-1])) > length(int($x->[-1])))
      {
      $rem = [@$x] if wantarray;		# make copy
      splice (@$x,1);				# keep ref to org array
      $x->[0] = 0;				# set to 0
      return ($x,$rem) if wantarray;		# including remainder?
      return $x;
      }
    # now calculate $x / $yorg
    if (length(int($yorg->[-1])) == length(int($x->[-1])))
      {
      # same length, so make full compare

      my $a = 0; my $j = scalar @$x - 1;
      # manual way (abort if unequal, good for early ne)
      while ($j >= 0)
        {
        last if ($a = $x->[$j] - $yorg->[$j]); $j--;
        }
      # $a contains the result of the compare between X and Y
      # a < 0: x < y, a == 0: x == y, a > 0: x > y
      if ($a <= 0)
        {
        $rem = [ 0 ];                   # a = 0 => x == y => rem 0
        $rem = [@$x] if $a != 0;        # a < 0 => x < y => rem = x
        splice(@$x,1);                  # keep single element
        $x->[0] = 0;                    # if $a < 0
        $x->[0] = 1 if $a == 0;         # $x == $y
        return ($x,$rem) if wantarray;
        return $x;
        }
      # $x >= $y, so proceed normally
      }
    }

  # all other cases:

  my $y = [ @$yorg ];				# always make copy to preserve

  my ($car,$bar,$prd,$dd,$xi,$yi,@q,$v2,$v1,@d,$tmp,$q,$u2,$u1,$u0);

  $car = $bar = $prd = 0;
  if (($dd = int($BASE/($y->[-1]+1))) != 1) 
    {
    for $xi (@$x) 
      {
      $xi = $xi * $dd + $car;
      $xi -= ($car = int($xi * $RBASE)) * $BASE;	# see USE_MUL
      }
    push(@$x, $car); $car = 0;
    for $yi (@$y) 
      {
      $yi = $yi * $dd + $car;
      $yi -= ($car = int($yi * $RBASE)) * $BASE;	# see USE_MUL
      }
    }
  else 
    {
    push(@$x, 0);
    }
  @q = (); ($v2,$v1) = @$y[-2,-1];
  $v2 = 0 unless $v2;
  while ($#$x > $#$y) 
    {
    ($u2,$u1,$u0) = @$x[-3..-1];
    $u2 = 0 unless $u2;
    #warn "oups v1 is 0, u0: $u0 $y->[-2] $y->[-1] l ",scalar @$y,"\n"
    # if $v1 == 0;
    $q = (($u0 == $v1) ? $MAX_VAL : int(($u0*$BASE+$u1)/$v1));
    --$q while ($v2*$q > ($u0*$BASE+$u1-$q*$v1)*$BASE+$u2);
    if ($q)
      {
      ($car, $bar) = (0,0);
      for ($yi = 0, $xi = $#$x-$#$y-1; $yi <= $#$y; ++$yi,++$xi) 
        {
        $prd = $q * $y->[$yi] + $car;
        $prd -= ($car = int($prd * $RBASE)) * $BASE;	# see USE_MUL
	$x->[$xi] += $BASE if ($bar = (($x->[$xi] -= $prd + $bar) < 0));
	}
      if ($x->[-1] < $car + $bar) 
        {
        $car = 0; --$q;
	for ($yi = 0, $xi = $#$x-$#$y-1; $yi <= $#$y; ++$yi,++$xi) 
          {
	  $x->[$xi] -= $BASE
	   if ($car = (($x->[$xi] += $y->[$yi] + $car) >= $BASE));
	  }
	}   
      }
    pop(@$x);
    unshift(@q, $q);
    }
  if (wantarray) 
    {
    @d = ();
    if ($dd != 1)  
      {
      $car = 0; 
      for $xi (reverse @$x) 
        {
        $prd = $car * $BASE + $xi;
        $car = $prd - ($tmp = int($prd / $dd)) * $dd; # see USE_MUL
        unshift(@d, $tmp);
        }
      }
    else 
      {
      @d = @$x;
      }
    @$x = @q;
    my $d = \@d; 
    __strip_zeros($x);
    __strip_zeros($d);
    return ($x,$d);
    }
  @$x = @q;
  __strip_zeros($x);
  $x;
  }

sub _div_use_div_64
  {
  # ref to array, ref to array, modify first array and return remainder if 
  # in list context
  # This version works on 64 bit integers
  my ($c,$x,$yorg) = @_;

  use integer;
  # the general div algorithmn here is about O(N*N) and thus quite slow, so
  # we first check for some special cases and use shortcuts to handle them.

  # This works, because we store the numbers in a chunked format where each
  # element contains 5..7 digits (depending on system).

  # if both numbers have only one element:
  if (@$x == 1 && @$yorg == 1)
    {
    # shortcut, $yorg and $x are two small numbers
    if (wantarray)
      {
      my $r = [ $x->[0] % $yorg->[0] ];
      $x->[0] = int($x->[0] / $yorg->[0]);
      return ($x,$r); 
      }
    else
      {
      $x->[0] = int($x->[0] / $yorg->[0]);
      return $x; 
      }
    }
  # if x has more than one, but y has only one element:
  if (@$yorg == 1)
    {
    my $rem;
    $rem = _mod($c,[ @$x ],$yorg) if wantarray;

    # shortcut, $y is < $BASE
    my $j = scalar @$x; my $r = 0; 
    my $y = $yorg->[0]; my $b;
    while ($j-- > 0)
      {
      $b = $r * $BASE + $x->[$j];
      $x->[$j] = int($b/$y);
      $r = $b % $y;
      }
    pop @$x if @$x > 1 && $x->[-1] == 0;	# splice up a leading zero 
    return ($x,$rem) if wantarray;
    return $x;
    }
  # now x and y have more than one element

  # check whether y has more elements than x, if yet, the result will be 0
  if (@$yorg > @$x)
    {
    my $rem;
    $rem = [@$x] if wantarray;			# make copy
    splice (@$x,1);				# keep ref to original array
    $x->[0] = 0;				# set to 0
    return ($x,$rem) if wantarray;		# including remainder?
    return $x;					# only x, which is [0] now
    }
  # check whether the numbers have the same number of elements, in that case
  # the result will fit into one element and can be computed efficiently
  if (@$yorg == @$x)
    {
    my $rem;
    # if $yorg has more digits than $x (it's leading element is longer than
    # the one from $x), the result will also be 0:
    if (length(int($yorg->[-1])) > length(int($x->[-1])))
      {
      $rem = [@$x] if wantarray;		# make copy
      splice (@$x,1);				# keep ref to org array
      $x->[0] = 0;				# set to 0
      return ($x,$rem) if wantarray;		# including remainder?
      return $x;
      }
    # now calculate $x / $yorg

    if (length(int($yorg->[-1])) == length(int($x->[-1])))
      {
      # same length, so make full compare

      my $a = 0; my $j = scalar @$x - 1;
      # manual way (abort if unequal, good for early ne)
      while ($j >= 0)
        {
        last if ($a = $x->[$j] - $yorg->[$j]); $j--;
        }
      # $a contains the result of the compare between X and Y
      # a < 0: x < y, a == 0: x == y, a > 0: x > y
      if ($a <= 0)
        {
        $rem = [ 0 ];			# a = 0 => x == y => rem 0
        $rem = [@$x] if $a != 0;	# a < 0 => x < y => rem = x
        splice(@$x,1);			# keep single element
        $x->[0] = 0;			# if $a < 0
        $x->[0] = 1 if $a == 0; 	# $x == $y
        return ($x,$rem) if wantarray;	# including remainder?
        return $x;
        }
      # $x >= $y, so proceed normally

      }
    }

  # all other cases:

  my $y = [ @$yorg ];				# always make copy to preserve
 
  my ($car,$bar,$prd,$dd,$xi,$yi,@q,$v2,$v1,@d,$tmp,$q,$u2,$u1,$u0);

  $car = $bar = $prd = 0;
  if (($dd = int($BASE/($y->[-1]+1))) != 1) 
    {
    for $xi (@$x) 
      {
      $xi = $xi * $dd + $car;
      $xi -= ($car = int($xi / $BASE)) * $BASE;
      }
    push(@$x, $car); $car = 0;
    for $yi (@$y) 
      {
      $yi = $yi * $dd + $car;
      $yi -= ($car = int($yi / $BASE)) * $BASE;
      }
    }
  else 
    {
    push(@$x, 0);
    }

  # @q will accumulate the final result, $q contains the current computed
  # part of the final result

  @q = (); ($v2,$v1) = @$y[-2,-1];
  $v2 = 0 unless $v2;
  while ($#$x > $#$y) 
    {
    ($u2,$u1,$u0) = @$x[-3..-1];
    $u2 = 0 unless $u2;
    #warn "oups v1 is 0, u0: $u0 $y->[-2] $y->[-1] l ",scalar @$y,"\n"
    # if $v1 == 0;
    $q = (($u0 == $v1) ? $MAX_VAL : int(($u0*$BASE+$u1)/$v1));
    --$q while ($v2*$q > ($u0*$BASE+$u1-$q*$v1)*$BASE+$u2);
    if ($q)
      {
      ($car, $bar) = (0,0);
      for ($yi = 0, $xi = $#$x-$#$y-1; $yi <= $#$y; ++$yi,++$xi) 
        {
        $prd = $q * $y->[$yi] + $car;
        $prd -= ($car = int($prd / $BASE)) * $BASE;
	$x->[$xi] += $BASE if ($bar = (($x->[$xi] -= $prd + $bar) < 0));
	}
      if ($x->[-1] < $car + $bar) 
        {
        $car = 0; --$q;
	for ($yi = 0, $xi = $#$x-$#$y-1; $yi <= $#$y; ++$yi,++$xi) 
          {
	  $x->[$xi] -= $BASE
	   if ($car = (($x->[$xi] += $y->[$yi] + $car) >= $BASE));
	  }
	}   
      }
    pop(@$x); unshift(@q, $q);
    }
  if (wantarray) 
    {
    @d = ();
    if ($dd != 1)  
      {
      $car = 0; 
      for $xi (reverse @$x) 
        {
        $prd = $car * $BASE + $xi;
        $car = $prd - ($tmp = int($prd / $dd)) * $dd;
        unshift(@d, $tmp);
        }
      }
    else 
      {
      @d = @$x;
      }
    @$x = @q;
    my $d = \@d; 
    __strip_zeros($x);
    __strip_zeros($d);
    return ($x,$d);
    }
  @$x = @q;
  __strip_zeros($x);
  $x;
  }

sub _div_use_div
  {
  # ref to array, ref to array, modify first array and return remainder if 
  # in list context
  my ($c,$x,$yorg) = @_;

  # the general div algorithmn here is about O(N*N) and thus quite slow, so
  # we first check for some special cases and use shortcuts to handle them.

  # This works, because we store the numbers in a chunked format where each
  # element contains 5..7 digits (depending on system).

  # if both numbers have only one element:
  if (@$x == 1 && @$yorg == 1)
    {
    # shortcut, $yorg and $x are two small numbers
    if (wantarray)
      {
      my $r = [ $x->[0] % $yorg->[0] ];
      $x->[0] = int($x->[0] / $yorg->[0]);
      return ($x,$r); 
      }
    else
      {
      $x->[0] = int($x->[0] / $yorg->[0]);
      return $x; 
      }
    }
  # if x has more than one, but y has only one element:
  if (@$yorg == 1)
    {
    my $rem;
    $rem = _mod($c,[ @$x ],$yorg) if wantarray;

    # shortcut, $y is < $BASE
    my $j = scalar @$x; my $r = 0; 
    my $y = $yorg->[0]; my $b;
    while ($j-- > 0)
      {
      $b = $r * $BASE + $x->[$j];
      $x->[$j] = int($b/$y);
      $r = $b % $y;
      }
    pop @$x if @$x > 1 && $x->[-1] == 0;	# splice up a leading zero 
    return ($x,$rem) if wantarray;
    return $x;
    }
  # now x and y have more than one element

  # check whether y has more elements than x, if yet, the result will be 0
  if (@$yorg > @$x)
    {
    my $rem;
    $rem = [@$x] if wantarray;			# make copy
    splice (@$x,1);				# keep ref to original array
    $x->[0] = 0;				# set to 0
    return ($x,$rem) if wantarray;		# including remainder?
    return $x;					# only x, which is [0] now
    }
  # check whether the numbers have the same number of elements, in that case
  # the result will fit into one element and can be computed efficiently
  if (@$yorg == @$x)
    {
    my $rem;
    # if $yorg has more digits than $x (it's leading element is longer than
    # the one from $x), the result will also be 0:
    if (length(int($yorg->[-1])) > length(int($x->[-1])))
      {
      $rem = [@$x] if wantarray;		# make copy
      splice (@$x,1);				# keep ref to org array
      $x->[0] = 0;				# set to 0
      return ($x,$rem) if wantarray;		# including remainder?
      return $x;
      }
    # now calculate $x / $yorg

    if (length(int($yorg->[-1])) == length(int($x->[-1])))
      {
      # same length, so make full compare

      my $a = 0; my $j = scalar @$x - 1;
      # manual way (abort if unequal, good for early ne)
      while ($j >= 0)
        {
        last if ($a = $x->[$j] - $yorg->[$j]); $j--;
        }
      # $a contains the result of the compare between X and Y
      # a < 0: x < y, a == 0: x == y, a > 0: x > y
      if ($a <= 0)
        {
        $rem = [ 0 ];			# a = 0 => x == y => rem 0
        $rem = [@$x] if $a != 0;	# a < 0 => x < y => rem = x
        splice(@$x,1);			# keep single element
        $x->[0] = 0;			# if $a < 0
        $x->[0] = 1 if $a == 0; 	# $x == $y
        return ($x,$rem) if wantarray;	# including remainder?
        return $x;
        }
      # $x >= $y, so proceed normally

      }
    }

  # all other cases:

  my $y = [ @$yorg ];				# always make copy to preserve
 
  my ($car,$bar,$prd,$dd,$xi,$yi,@q,$v2,$v1,@d,$tmp,$q,$u2,$u1,$u0);

  $car = $bar = $prd = 0;
  if (($dd = int($BASE/($y->[-1]+1))) != 1) 
    {
    for $xi (@$x) 
      {
      $xi = $xi * $dd + $car;
      $xi -= ($car = int($xi / $BASE)) * $BASE;
      }
    push(@$x, $car); $car = 0;
    for $yi (@$y) 
      {
      $yi = $yi * $dd + $car;
      $yi -= ($car = int($yi / $BASE)) * $BASE;
      }
    }
  else 
    {
    push(@$x, 0);
    }

  # @q will accumulate the final result, $q contains the current computed
  # part of the final result

  @q = (); ($v2,$v1) = @$y[-2,-1];
  $v2 = 0 unless $v2;
  while ($#$x > $#$y) 
    {
    ($u2,$u1,$u0) = @$x[-3..-1];
    $u2 = 0 unless $u2;
    #warn "oups v1 is 0, u0: $u0 $y->[-2] $y->[-1] l ",scalar @$y,"\n"
    # if $v1 == 0;
    $q = (($u0 == $v1) ? $MAX_VAL : int(($u0*$BASE+$u1)/$v1));
    --$q while ($v2*$q > ($u0*$BASE+$u1-$q*$v1)*$BASE+$u2);
    if ($q)
      {
      ($car, $bar) = (0,0);
      for ($yi = 0, $xi = $#$x-$#$y-1; $yi <= $#$y; ++$yi,++$xi) 
        {
        $prd = $q * $y->[$yi] + $car;
        $prd -= ($car = int($prd / $BASE)) * $BASE;
	$x->[$xi] += $BASE if ($bar = (($x->[$xi] -= $prd + $bar) < 0));
	}
      if ($x->[-1] < $car + $bar) 
        {
        $car = 0; --$q;
	for ($yi = 0, $xi = $#$x-$#$y-1; $yi <= $#$y; ++$yi,++$xi) 
          {
	  $x->[$xi] -= $BASE
	   if ($car = (($x->[$xi] += $y->[$yi] + $car) >= $BASE));
	  }
	}   
      }
    pop(@$x); unshift(@q, $q);
    }
  if (wantarray) 
    {
    @d = ();
    if ($dd != 1)  
      {
      $car = 0; 
      for $xi (reverse @$x) 
        {
        $prd = $car * $BASE + $xi;
        $car = $prd - ($tmp = int($prd / $dd)) * $dd;
        unshift(@d, $tmp);
        }
      }
    else 
      {
      @d = @$x;
      }
    @$x = @q;
    my $d = \@d; 
    __strip_zeros($x);
    __strip_zeros($d);
    return ($x,$d);
    }
  @$x = @q;
  __strip_zeros($x);
  $x;
  }

##############################################################################
# testing

sub _acmp
  {
  # internal absolute post-normalized compare (ignore signs)
  # ref to array, ref to array, return <0, 0, >0
  # arrays must have at least one entry; this is not checked for
  my ($c,$cx,$cy) = @_;
 
  # shortcut for short numbers 
  return (($cx->[0] <=> $cy->[0]) <=> 0) 
   if scalar @$cx == scalar @$cy && scalar @$cx == 1;

  # fast comp based on number of array elements (aka pseudo-length)
  my $lxy = (scalar @$cx - scalar @$cy)
  # or length of first element if same number of elements (aka difference 0)
    ||
  # need int() here because sometimes the last element is '00018' vs '18'
   (length(int($cx->[-1])) - length(int($cy->[-1])));
  return -1 if $lxy < 0;				# already differs, ret
  return 1 if $lxy > 0;					# ditto

  # manual way (abort if unequal, good for early ne)
  my $a; my $j = scalar @$cx;
  while (--$j >= 0)
    {
    last if ($a = $cx->[$j] - $cy->[$j]);
    }
  $a <=> 0;
  }

sub _len
  {
  # compute number of digits in base 10

  # int() because add/sub sometimes leaves strings (like '00005') instead of
  # '5' in this place, thus causing length() to report wrong length
  my $cx = $_[1];

  (@$cx-1)*$BASE_LEN+length(int($cx->[-1]));
  }

sub _digit
  {
  # return the nth digit, negative values count backward
  # zero is rightmost, so _digit(123,0) will give 3
  my ($c,$x,$n) = @_;

  my $len = _len('',$x);

  $n = $len+$n if $n < 0;		# -1 last, -2 second-to-last
  $n = abs($n);				# if negative was too big
  $len--; $n = $len if $n > $len;	# n to big?
  
  my $elem = int($n / $BASE_LEN);	# which array element
  my $digit = $n % $BASE_LEN;		# which digit in this element
  $elem = '0' x $BASE_LEN . @$x[$elem];	# get element padded with 0's
  substr($elem,-$digit-1,1);
  }

sub _zeros
  {
  # return amount of trailing zeros in decimal
  # check each array elem in _m for having 0 at end as long as elem == 0
  # Upon finding a elem != 0, stop
  my $x = $_[1];

  return 0 if scalar @$x == 1 && $x->[0] == 0;

  my $zeros = 0; my $elem;
  foreach my $e (@$x)
    {
    if ($e != 0)
      {
      $elem = "$e";				# preserve x
      $elem =~ s/.*?(0*$)/$1/;			# strip anything not zero
      $zeros *= $BASE_LEN;			# elems * 5
      $zeros += length($elem);			# count trailing zeros
      last;					# early out
      }
    $zeros ++;					# real else branch: 50% slower!
    }
  $zeros;
  }

##############################################################################
# _is_* routines

sub _is_zero
  {
  # return true if arg is zero 
  (((scalar @{$_[1]} == 1) && ($_[1]->[0] == 0))) <=> 0;
  }

sub _is_even
  {
  # return true if arg is even
  (!($_[1]->[0] & 1)) <=> 0; 
  }

sub _is_odd
  {
  # return true if arg is even
  (($_[1]->[0] & 1)) <=> 0; 
  }

sub _is_one
  {
  # return true if arg is one
  (scalar @{$_[1]} == 1) && ($_[1]->[0] == 1) <=> 0; 
  }

sub _is_two
  {
  # return true if arg is two 
  (scalar @{$_[1]} == 1) && ($_[1]->[0] == 2) <=> 0; 
  }

sub _is_ten
  {
  # return true if arg is ten 
  (scalar @{$_[1]} == 1) && ($_[1]->[0] == 10) <=> 0; 
  }

sub __strip_zeros
  {
  # internal normalization function that strips leading zeros from the array
  # args: ref to array
  my $s = shift;
 
  my $cnt = scalar @$s; # get count of parts
  my $i = $cnt-1;
  push @$s,0 if $i < 0;		# div might return empty results, so fix it

  return $s if @$s == 1;		# early out

  #print "strip: cnt $cnt i $i\n";
  # '0', '3', '4', '0', '0',
  #  0    1    2    3    4
  # cnt = 5, i = 4
  # i = 4
  # i = 3
  # => fcnt = cnt - i (5-2 => 3, cnt => 5-1 = 4, throw away from 4th pos)
  # >= 1: skip first part (this can be zero)
  while ($i > 0) { last if $s->[$i] != 0; $i--; }
  $i++; splice @$s,$i if ($i < $cnt); # $i cant be 0
  $s;                                                                    
  }                                                                             

###############################################################################
# check routine to test internal state for corruptions

sub _check
  {
  # used by the test suite
  my $x = $_[1];

  return "$x is not a reference" if !ref($x);

  # are all parts are valid?
  my $i = 0; my $j = scalar @$x; my ($e,$try);
  while ($i < $j)
    {
    $e = $x->[$i]; $e = 'undef' unless defined $e;
    $try = '=~ /^[\+]?[0-9]+\$/; '."($x, $e)";
    last if $e !~ /^[+]?[0-9]+$/;
    $try = '=~ /^[\+]?[0-9]+\$/; '."($x, $e) (stringify)";
    last if "$e" !~ /^[+]?[0-9]+$/;
    $try = '=~ /^[\+]?[0-9]+\$/; '."($x, $e) (cat-stringify)";
    last if '' . "$e" !~ /^[+]?[0-9]+$/;
    $try = ' < 0 || >= $BASE; '."($x, $e)";
    last if $e <0 || $e >= $BASE;
    # this test is disabled, since new/bnorm and certain ops (like early out
    # in add/sub) are allowed/expected to leave '00000' in some elements
    #$try = '=~ /^00+/; '."($x, $e)";
    #last if $e =~ /^00+/;
    $i++;
    }
  return "Illegal part '$e' at pos $i (tested: $try)" if $i < $j;
  0;
  }


###############################################################################

sub _mod
  {
  # if possible, use mod shortcut
  my ($c,$x,$yo) = @_;

  # slow way since $y to big
  if (scalar @$yo > 1)
    {
    my ($xo,$rem) = _div($c,$x,$yo);
    return $rem;
    }

  my $y = $yo->[0];
  # both are single element arrays
  if (scalar @$x == 1)
    {
    $x->[0] %= $y;
    return $x;
    }

  # @y is a single element, but @x has more than one element
  my $b = $BASE % $y;
  if ($b == 0)
    {
    # when BASE % Y == 0 then (B * BASE) % Y == 0
    # (B * BASE) % $y + A % Y => A % Y
    # so need to consider only last element: O(1)
    $x->[0] %= $y;
    }
  elsif ($b == 1)
    {
    # else need to go through all elements: O(N), but loop is a bit simplified
    my $r = 0;
    foreach (@$x)
      {
      $r = ($r + $_) % $y;		# not much faster, but heh...
      #$r += $_ % $y; $r %= $y;
      }
    $r = 0 if $r == $y;
    $x->[0] = $r;
    }
  else
    {
    # else need to go through all elements: O(N)
    my $r = 0; my $bm = 1;
    foreach (@$x)
      {
      $r = ($_ * $bm + $r) % $y;
      $bm = ($bm * $b) % $y;

      #$r += ($_ % $y) * $bm;
      #$bm *= $b;
      #$bm %= $y;
      #$r %= $y;
      }
    $r = 0 if $r == $y;
    $x->[0] = $r;
    }
  splice (@$x,1);		# keep one element of $x
  $x;
  }

##############################################################################
# shifts

sub _rsft
  {
  my ($c,$x,$y,$n) = @_;

  if ($n != 10)
    {
    $n = _new($c,$n); return _div($c,$x, _pow($c,$n,$y));
    }

  # shortcut (faster) for shifting by 10)
  # multiples of $BASE_LEN
  my $dst = 0;				# destination
  my $src = _num($c,$y);		# as normal int
  my $xlen = (@$x-1)*$BASE_LEN+length(int($x->[-1]));  # len of x in digits
  if ($src >= $xlen or ($src == $xlen and ! defined $x->[1]))
    {
    # 12345 67890 shifted right by more than 10 digits => 0
    splice (@$x,1);                    # leave only one element
    $x->[0] = 0;                       # set to zero
    return $x;
    }
  my $rem = $src % $BASE_LEN;		# remainder to shift
  $src = int($src / $BASE_LEN);		# source
  if ($rem == 0)
    {
    splice (@$x,0,$src);		# even faster, 38.4 => 39.3
    }
  else
    {
    my $len = scalar @$x - $src;	# elems to go
    my $vd; my $z = '0'x $BASE_LEN;
    $x->[scalar @$x] = 0;		# avoid || 0 test inside loop
    while ($dst < $len)
      {
      $vd = $z.$x->[$src];
      $vd = substr($vd,-$BASE_LEN,$BASE_LEN-$rem);
      $src++;
      $vd = substr($z.$x->[$src],-$rem,$rem) . $vd;
      $vd = substr($vd,-$BASE_LEN,$BASE_LEN) if length($vd) > $BASE_LEN;
      $x->[$dst] = int($vd);
      $dst++;
      }
    splice (@$x,$dst) if $dst > 0;		# kill left-over array elems
    pop @$x if $x->[-1] == 0 && @$x > 1;	# kill last element if 0
    } # else rem == 0
  $x;
  }

sub _lsft
  {
  my ($c,$x,$y,$n) = @_;

  if ($n != 10)
    {
    $n = _new($c,$n); return _mul($c,$x, _pow($c,$n,$y));
    }

  # shortcut (faster) for shifting by 10) since we are in base 10eX
  # multiples of $BASE_LEN:
  my $src = scalar @$x;			# source
  my $len = _num($c,$y);		# shift-len as normal int
  my $rem = $len % $BASE_LEN;		# remainder to shift
  my $dst = $src + int($len/$BASE_LEN);	# destination
  my $vd;				# further speedup
  $x->[$src] = 0;			# avoid first ||0 for speed
  my $z = '0' x $BASE_LEN;
  while ($src >= 0)
    {
    $vd = $x->[$src]; $vd = $z.$vd;
    $vd = substr($vd,-$BASE_LEN+$rem,$BASE_LEN-$rem);
    $vd .= $src > 0 ? substr($z.$x->[$src-1],-$BASE_LEN,$rem) : '0' x $rem;
    $vd = substr($vd,-$BASE_LEN,$BASE_LEN) if length($vd) > $BASE_LEN;
    $x->[$dst] = int($vd);
    $dst--; $src--;
    }
  # set lowest parts to 0
  while ($dst >= 0) { $x->[$dst--] = 0; }
  # fix spurios last zero element
  splice @$x,-1 if $x->[-1] == 0;
  $x;
  }

sub _pow
  {
  # power of $x to $y
  # ref to array, ref to array, return ref to array
  my ($c,$cx,$cy) = @_;

  if (scalar @$cy == 1 && $cy->[0] == 0)
    {
    splice (@$cx,1); $cx->[0] = 1;		# y == 0 => x => 1
    return $cx;
    }
  if ((scalar @$cx == 1 && $cx->[0] == 1) ||	#    x == 1
      (scalar @$cy == 1 && $cy->[0] == 1))	# or y == 1
    {
    return $cx;
    }
  if (scalar @$cx == 1 && $cx->[0] == 0)
    {
    splice (@$cx,1); $cx->[0] = 0;		# 0 ** y => 0 (if not y <= 0)
    return $cx;
    }

  my $pow2 = _one();

  my $y_bin = _as_bin($c,$cy); $y_bin =~ s/^0b//;
  my $len = length($y_bin);
  while (--$len > 0)
    {
    _mul($c,$pow2,$cx) if substr($y_bin,$len,1) eq '1';		# is odd?
    _mul($c,$cx,$cx);
    }

  _mul($c,$cx,$pow2);
  $cx;
  }

sub _nok
  {
  # n over k
  # ref to array, return ref to array
  my ($c,$n,$k) = @_;

  # ( 7 )    7!          7*6*5 * 4*3*2*1   7 * 6 * 5
  # ( - ) = --------- =  --------------- = ---------
  # ( 3 )   3! (7-3)!    3*2*1 * 4*3*2*1   3 * 2 * 1 

  # compute n - k + 2 (so we start with 5 in the example above)
  my $x = _copy($c,$n);

  _sub($c,$n,$k);
  if (!_is_one($c,$n))
    {
    _inc($c,$n);
    my $f = _copy($c,$n); _inc($c,$f);		# n = 5, f = 6, d = 2
    my $d = _two($c);
    while (_acmp($c,$f,$x) <= 0)		# f < n ?
      {
      # n = (n * f / d) == 5 * 6 / 2 => n == 3
      $n = _mul($c,$n,$f); $n = _div($c,$n,$d);
      # f = 7, d = 3
      _inc($c,$f); _inc($c,$d);
      }
    }
  else 
    {
    # keep ref to $n and set it to 1
    splice (@$n,1); $n->[0] = 1;
    }
  $n;
  }

my @factorials = (
  1,
  1,
  2,
  2*3,
  2*3*4,
  2*3*4*5,
  2*3*4*5*6,
  2*3*4*5*6*7,
);

sub _fac
  {
  # factorial of $x
  # ref to array, return ref to array
  my ($c,$cx) = @_;

  if ((@$cx == 1) && ($cx->[0] <= 7))
    {
    $cx->[0] = $factorials[$cx->[0]];		# 0 => 1, 1 => 1, 2 => 2 etc.
    return $cx;
    }

  if ((@$cx == 1) && 		# we do this only if $x >= 12 and $x <= 7000
      ($cx->[0] >= 12 && $cx->[0] < 7000))
    {

  # Calculate (k-j) * (k-j+1) ... k .. (k+j-1) * (k + j)
  # See http://blogten.blogspot.com/2007/01/calculating-n.html
  # The above series can be expressed as factors:
  #   k * k - (j - i) * 2
  # We cache k*k, and calculate (j * j) as the sum of the first j odd integers

  # This will not work when N exceeds the storage of a Perl scalar, however,
  # in this case the algorithm would be way to slow to terminate, anyway.

  # As soon as the last element of $cx is 0, we split it up and remember
  # how many zeors we got so far. The reason is that n! will accumulate
  # zeros at the end rather fast.
  my $zero_elements = 0;

  # If n is even, set n = n -1
  my $k = _num($c,$cx); my $even = 1;
  if (($k & 1) == 0)
    {
    $even = $k; $k --;
    }
  # set k to the center point
  $k = ($k + 1) / 2;
#  print "k $k even: $even\n";
  # now calculate k * k
  my $k2 = $k * $k;
  my $odd = 1; my $sum = 1;
  my $i = $k - 1;
  # keep reference to x
  my $new_x = _new($c, $k * $even);
  @$cx = @$new_x;
  if ($cx->[0] == 0)
    {
    $zero_elements ++; shift @$cx;
    }
#  print STDERR "x = ", _str($c,$cx),"\n";
  my $BASE2 = int(sqrt($BASE))-1;
  my $j = 1; 
  while ($j <= $i)
    {
    my $m = ($k2 - $sum); $odd += 2; $sum += $odd; $j++;
    while ($j <= $i && ($m < $BASE2) && (($k2 - $sum) < $BASE2))
      {
      $m *= ($k2 - $sum);
      $odd += 2; $sum += $odd; $j++;
#      print STDERR "\n k2 $k2 m $m sum $sum odd $odd\n"; sleep(1);
      }
    if ($m < $BASE)
      {
      _mul($c,$cx,[$m]);
      }
    else
      {
      _mul($c,$cx,$c->_new($m));
      }
    if ($cx->[0] == 0)
      {
      $zero_elements ++; shift @$cx;
      }
#    print STDERR "Calculate $k2 - $sum = $m (x = ", _str($c,$cx),")\n";
    }
  # multiply in the zeros again
  unshift @$cx, (0) x $zero_elements; 
  return $cx;
  }

  # go forward until $base is exceeded
  # limit is either $x steps (steps == 100 means a result always too high) or
  # $base.
  my $steps = 100; $steps = $cx->[0] if @$cx == 1;
  my $r = 2; my $cf = 3; my $step = 2; my $last = $r;
  while ($r*$cf < $BASE && $step < $steps)
    {
    $last = $r; $r *= $cf++; $step++;
    }
  if ((@$cx == 1) && $step == $cx->[0])
    {
    # completely done, so keep reference to $x and return
    $cx->[0] = $r;
    return $cx;
    }
  
  # now we must do the left over steps
  my $n;					# steps still to do
  if (scalar @$cx == 1)
    {
    $n = $cx->[0];
    }
  else
    {
    $n = _copy($c,$cx);
    }

  # Set $cx to the last result below $BASE (but keep ref to $x)
  $cx->[0] = $last; splice (@$cx,1);
  # As soon as the last element of $cx is 0, we split it up and remember
  # how many zeors we got so far. The reason is that n! will accumulate
  # zeros at the end rather fast.
  my $zero_elements = 0;

  # do left-over steps fit into a scalar?
  if (ref $n eq 'ARRAY')
    {
    # No, so use slower inc() & cmp()
    # ($n is at least $BASE here)
    my $base_2 = int(sqrt($BASE)) - 1;
    #print STDERR "base_2: $base_2\n"; 
    while ($step < $base_2)
      {
      if ($cx->[0] == 0)
        {
        $zero_elements ++; shift @$cx;
        }
      my $b = $step * ($step + 1); $step += 2;
      _mul($c,$cx,[$b]);
      }
    $step = [$step];
    while (_acmp($c,$step,$n) <= 0)
      {
      if ($cx->[0] == 0)
        {
        $zero_elements ++; shift @$cx;
        }
      _mul($c,$cx,$step); _inc($c,$step);
      }
    }
  else
    {
    # Yes, so we can speed it up slightly
  
#    print "# left over steps $n\n";

    my $base_4 = int(sqrt(sqrt($BASE))) - 2;
    #print STDERR "base_4: $base_4\n";
    my $n4 = $n - 4; 
    while ($step < $n4 && $step < $base_4)
      {
      if ($cx->[0] == 0)
        {
        $zero_elements ++; shift @$cx;
        }
      my $b = $step * ($step + 1); $step += 2; $b *= $step * ($step + 1); $step += 2;
      _mul($c,$cx,[$b]);
      }
    my $base_2 = int(sqrt($BASE)) - 1;
    my $n2 = $n - 2; 
    #print STDERR "base_2: $base_2\n"; 
    while ($step < $n2 && $step < $base_2)
      {
      if ($cx->[0] == 0)
        {
        $zero_elements ++; shift @$cx;
        }
      my $b = $step * ($step + 1); $step += 2;
      _mul($c,$cx,[$b]);
      }
    # do what's left over
    while ($step <= $n)
      {
      _mul($c,$cx,[$step]); $step++;
      if ($cx->[0] == 0)
        {
        $zero_elements ++; shift @$cx;
        }
      }
    }
  # multiply in the zeros again
  unshift @$cx, (0) x $zero_elements;
  $cx;			# return result
  }

#############################################################################

sub _log_int
  {
  # calculate integer log of $x to base $base
  # ref to array, ref to array - return ref to array
  my ($c,$x,$base) = @_;

  # X == 0 => NaN
  return if (scalar @$x == 1 && $x->[0] == 0);
  # BASE 0 or 1 => NaN
  return if (scalar @$base == 1 && $base->[0] < 2);
  my $cmp = _acmp($c,$x,$base); # X == BASE => 1
  if ($cmp == 0)
    {
    splice (@$x,1); $x->[0] = 1;
    return ($x,1)
    }
  # X < BASE
  if ($cmp < 0)
    {
    splice (@$x,1); $x->[0] = 0;
    return ($x,undef);
    }

  my $x_org = _copy($c,$x);		# preserve x
  splice(@$x,1); $x->[0] = 1;		# keep ref to $x

  # Compute a guess for the result based on:
  # $guess = int ( length_in_base_10(X) / ( log(base) / log(10) ) )
  my $len = _len($c,$x_org);
  my $log = log($base->[-1]) / log(10);

  # for each additional element in $base, we add $BASE_LEN to the result,
  # based on the observation that log($BASE,10) is BASE_LEN and
  # log(x*y) == log(x) + log(y):
  $log += ((scalar @$base)-1) * $BASE_LEN;

  # calculate now a guess based on the values obtained above:
  my $res = int($len / $log);

  $x->[0] = $res;
  my $trial = _pow ($c, _copy($c, $base), $x);
  my $a = _acmp($c,$trial,$x_org);

#  print STDERR "# trial ", _str($c,$x)," was: $a (0 = exact, -1 too small, +1 too big)\n";

  # found an exact result?
  return ($x,1) if $a == 0;

  if ($a > 0)
    {
    # or too big
    _div($c,$trial,$base); _dec($c, $x);
    while (($a = _acmp($c,$trial,$x_org)) > 0)
      {
#      print STDERR "# big _log_int at ", _str($c,$x), "\n"; 
      _div($c,$trial,$base); _dec($c, $x);
      }
    # result is now exact (a == 0), or too small (a < 0)
    return ($x, $a == 0 ? 1 : 0);
    }

  # else: result was to small
  _mul($c,$trial,$base);

  # did we now get the right result?
  $a = _acmp($c,$trial,$x_org);

  if ($a == 0)				# yes, exactly
    {
    _inc($c, $x);
    return ($x,1); 
    }
  return ($x,0) if $a > 0;  

  # Result still too small (we should come here only if the estimate above
  # was very off base):
 
  # Now let the normal trial run obtain the real result
  # Simple loop that increments $x by 2 in each step, possible overstepping
  # the real result

  my $base_mul = _mul($c, _copy($c,$base), $base);	# $base * $base

  while (($a = _acmp($c,$trial,$x_org)) < 0)
    {
#    print STDERR "# small _log_int at ", _str($c,$x), "\n"; 
    _mul($c,$trial,$base_mul); _add($c, $x, [2]);
    }

  my $exact = 1;
  if ($a > 0)
    {
    # overstepped the result
    _dec($c, $x);
    _div($c,$trial,$base);
    $a = _acmp($c,$trial,$x_org);
    if ($a > 0)
      {
      _dec($c, $x);
      }
    $exact = 0 if $a != 0;		# a = -1 => not exact result, a = 0 => exact
    }
  
  ($x,$exact);				# return result
  }

# for debugging:
  use constant DEBUG => 0;
  my $steps = 0;
  sub steps { $steps };

sub _sqrt
  {
  # square-root of $x in place
  # Compute a guess of the result (by rule of thumb), then improve it via
  # Newton's method.
  my ($c,$x) = @_;

  if (scalar @$x == 1)
    {
    # fits into one Perl scalar, so result can be computed directly
    $x->[0] = int(sqrt($x->[0]));
    return $x;
    } 
  my $y = _copy($c,$x);
  # hopefully _len/2 is < $BASE, the -1 is to always undershot the guess
  # since our guess will "grow"
  my $l = int((_len($c,$x)-1) / 2);	

  my $lastelem = $x->[-1];					# for guess
  my $elems = scalar @$x - 1;
  # not enough digits, but could have more?
  if ((length($lastelem) <= 3) && ($elems > 1))
    {
    # right-align with zero pad
    my $len = length($lastelem) & 1;
    print "$lastelem => " if DEBUG;
    $lastelem .= substr($x->[-2] . '0' x $BASE_LEN,0,$BASE_LEN);
    # former odd => make odd again, or former even to even again
    $lastelem = $lastelem / 10 if (length($lastelem) & 1) != $len;
    print "$lastelem\n" if DEBUG;
    }

  # construct $x (instead of _lsft($c,$x,$l,10)
  my $r = $l % $BASE_LEN;	# 10000 00000 00000 00000 ($BASE_LEN=5)
  $l = int($l / $BASE_LEN);
  print "l =  $l " if DEBUG;

  splice @$x,$l;		# keep ref($x), but modify it

  # we make the first part of the guess not '1000...0' but int(sqrt($lastelem))
  # that gives us:
  # 14400 00000 => sqrt(14400) => guess first digits to be 120
  # 144000 000000 => sqrt(144000) => guess 379

  print "$lastelem (elems $elems) => " if DEBUG;
  $lastelem = $lastelem / 10 if ($elems & 1 == 1);		# odd or even?
  my $g = sqrt($lastelem); $g =~ s/\.//;			# 2.345 => 2345
  $r -= 1 if $elems & 1 == 0;					# 70 => 7

  # padd with zeros if result is too short
  $x->[$l--] = int(substr($g . '0' x $r,0,$r+1));
  print "now ",$x->[-1] if DEBUG;
  print " would have been ", int('1' . '0' x $r),"\n" if DEBUG;

  # If @$x > 1, we could compute the second elem of the guess, too, to create
  # an even better guess. Not implemented yet. Does it improve performance?
  $x->[$l--] = 0 while ($l >= 0);	# all other digits of guess are zero

  print "start x= ",_str($c,$x),"\n" if DEBUG;
  my $two = _two();
  my $last = _zero();
  my $lastlast = _zero();
  $steps = 0 if DEBUG;
  while (_acmp($c,$last,$x) != 0 && _acmp($c,$lastlast,$x) != 0)
    {
    $steps++ if DEBUG;
    $lastlast = _copy($c,$last);
    $last = _copy($c,$x);
    _add($c,$x, _div($c,_copy($c,$y),$x));
    _div($c,$x, $two );
    print " x= ",_str($c,$x),"\n" if DEBUG;
    }
  print "\nsteps in sqrt: $steps, " if DEBUG;
  _dec($c,$x) if _acmp($c,$y,_mul($c,_copy($c,$x),$x)) < 0;	# overshot? 
  print " final ",$x->[-1],"\n" if DEBUG;
  $x;
  }

sub _root
  {
  # take n'th root of $x in place (n >= 3)
  my ($c,$x,$n) = @_;
 
  if (scalar @$x == 1)
    {
    if (scalar @$n > 1)
      {
      # result will always be smaller than 2 so trunc to 1 at once
      $x->[0] = 1;
      }
    else
      {
      # fits into one Perl scalar, so result can be computed directly
      # cannot use int() here, because it rounds wrongly (try 
      # (81 ** 3) ** (1/3) to see what I mean)
      #$x->[0] = int( $x->[0] ** (1 / $n->[0]) );
      # round to 8 digits, then truncate result to integer
      $x->[0] = int ( sprintf ("%.8f", $x->[0] ** (1 / $n->[0]) ) );
      }
    return $x;
    } 

  # we know now that X is more than one element long

  # if $n is a power of two, we can repeatedly take sqrt($X) and find the
  # proper result, because sqrt(sqrt($x)) == root($x,4)
  my $b = _as_bin($c,$n);
  if ($b =~ /0b1(0+)$/)
    {
    my $count = CORE::length($1);	# 0b100 => len('00') => 2
    my $cnt = $count;			# counter for loop
    unshift (@$x, 0);			# add one element, together with one
					# more below in the loop this makes 2
    while ($cnt-- > 0)
      {
      # 'inflate' $X by adding one element, basically computing
      # $x * $BASE * $BASE. This gives us more $BASE_LEN digits for result
      # since len(sqrt($X)) approx == len($x) / 2.
      unshift (@$x, 0);
      # calculate sqrt($x), $x is now one element to big, again. In the next
      # round we make that two, again.
      _sqrt($c,$x);
      }
    # $x is now one element to big, so truncate result by removing it
    splice (@$x,0,1);
    } 
  else
    {
    # trial computation by starting with 2,4,8,16 etc until we overstep
    my $step;
    my $trial = _two();

    # while still to do more than X steps
    do
      {
      $step = _two();
      while (_acmp($c, _pow($c, _copy($c, $trial), $n), $x) < 0)
        {
        _mul ($c, $step, [2]);
        _add ($c, $trial, $step);
        }

      # hit exactly?
      if (_acmp($c, _pow($c, _copy($c, $trial), $n), $x) == 0)
        {
        @$x = @$trial;			# make copy while preserving ref to $x
        return $x;
        }
      # overstepped, so go back on step
      _sub($c, $trial, $step);
      } while (scalar @$step > 1 || $step->[0] > 128);

    # reset step to 2
    $step = _two();
    # add two, because $trial cannot be exactly the result (otherwise we would
    # alrady have found it)
    _add($c, $trial, $step);
 
    # and now add more and more (2,4,6,8,10 etc)
    while (_acmp($c, _pow($c, _copy($c, $trial), $n), $x) < 0)
      {
      _add ($c, $trial, $step);
      }

    # hit not exactly? (overstepped)
    if (_acmp($c, _pow($c, _copy($c, $trial), $n), $x) > 0)
      {
      _dec($c,$trial);
      }

    # hit not exactly? (overstepped)
    # 80 too small, 81 slightly too big, 82 too big
    if (_acmp($c, _pow($c, _copy($c, $trial), $n), $x) > 0)
      {
      _dec ($c, $trial); 
      }

    @$x = @$trial;			# make copy while preserving ref to $x
    return $x;
    }
  $x; 
  }

##############################################################################
# binary stuff

sub _and
  {
  my ($c,$x,$y) = @_;

  # the shortcut makes equal, large numbers _really_ fast, and makes only a
  # very small performance drop for small numbers (e.g. something with less
  # than 32 bit) Since we optimize for large numbers, this is enabled.
  return $x if _acmp($c,$x,$y) == 0;		# shortcut
  
  my $m = _one(); my ($xr,$yr);
  my $mask = $AND_MASK;

  my $x1 = $x;
  my $y1 = _copy($c,$y);			# make copy
  $x = _zero();
  my ($b,$xrr,$yrr);
  use integer;
  while (!_is_zero($c,$x1) && !_is_zero($c,$y1))
    {
    ($x1, $xr) = _div($c,$x1,$mask);
    ($y1, $yr) = _div($c,$y1,$mask);

    # make ints() from $xr, $yr
    # this is when the AND_BITS are greater than $BASE and is slower for
    # small (<256 bits) numbers, but faster for large numbers. Disabled
    # due to KISS principle

#    $b = 1; $xrr = 0; foreach (@$xr) { $xrr += $_ * $b; $b *= $BASE; }
#    $b = 1; $yrr = 0; foreach (@$yr) { $yrr += $_ * $b; $b *= $BASE; }
#    _add($c,$x, _mul($c, _new( $c, ($xrr & $yrr) ), $m) );
    
    # 0+ due to '&' doesn't work in strings
    _add($c,$x, _mul($c, [ 0+$xr->[0] & 0+$yr->[0] ], $m) );
    _mul($c,$m,$mask);
    }
  $x;
  }

sub _xor
  {
  my ($c,$x,$y) = @_;

  return _zero() if _acmp($c,$x,$y) == 0;	# shortcut (see -and)

  my $m = _one(); my ($xr,$yr);
  my $mask = $XOR_MASK;

  my $x1 = $x;
  my $y1 = _copy($c,$y);			# make copy
  $x = _zero();
  my ($b,$xrr,$yrr);
  use integer;
  while (!_is_zero($c,$x1) && !_is_zero($c,$y1))
    {
    ($x1, $xr) = _div($c,$x1,$mask);
    ($y1, $yr) = _div($c,$y1,$mask);
    # make ints() from $xr, $yr (see _and())
    #$b = 1; $xrr = 0; foreach (@$xr) { $xrr += $_ * $b; $b *= $BASE; }
    #$b = 1; $yrr = 0; foreach (@$yr) { $yrr += $_ * $b; $b *= $BASE; }
    #_add($c,$x, _mul($c, _new( $c, ($xrr ^ $yrr) ), $m) );

    # 0+ due to '^' doesn't work in strings
    _add($c,$x, _mul($c, [ 0+$xr->[0] ^ 0+$yr->[0] ], $m) );
    _mul($c,$m,$mask);
    }
  # the loop stops when the shorter of the two numbers is exhausted
  # the remainder of the longer one will survive bit-by-bit, so we simple
  # multiply-add it in
  _add($c,$x, _mul($c, $x1, $m) ) if !_is_zero($c,$x1);
  _add($c,$x, _mul($c, $y1, $m) ) if !_is_zero($c,$y1);
  
  $x;
  }

sub _or
  {
  my ($c,$x,$y) = @_;

  return $x if _acmp($c,$x,$y) == 0;		# shortcut (see _and)

  my $m = _one(); my ($xr,$yr);
  my $mask = $OR_MASK;

  my $x1 = $x;
  my $y1 = _copy($c,$y);			# make copy
  $x = _zero();
  my ($b,$xrr,$yrr);
  use integer;
  while (!_is_zero($c,$x1) && !_is_zero($c,$y1))
    {
    ($x1, $xr) = _div($c,$x1,$mask);
    ($y1, $yr) = _div($c,$y1,$mask);
    # make ints() from $xr, $yr (see _and())
#    $b = 1; $xrr = 0; foreach (@$xr) { $xrr += $_ * $b; $b *= $BASE; }
#    $b = 1; $yrr = 0; foreach (@$yr) { $yrr += $_ * $b; $b *= $BASE; }
#    _add($c,$x, _mul($c, _new( $c, ($xrr | $yrr) ), $m) );
    
    # 0+ due to '|' doesn't work in strings
    _add($c,$x, _mul($c, [ 0+$xr->[0] | 0+$yr->[0] ], $m) );
    _mul($c,$m,$mask);
    }
  # the loop stops when the shorter of the two numbers is exhausted
  # the remainder of the longer one will survive bit-by-bit, so we simple
  # multiply-add it in
  _add($c,$x, _mul($c, $x1, $m) ) if !_is_zero($c,$x1);
  _add($c,$x, _mul($c, $y1, $m) ) if !_is_zero($c,$y1);
  
  $x;
  }

sub _as_hex
  {
  # convert a decimal number to hex (ref to array, return ref to string)
  my ($c,$x) = @_;

  # fits into one element (handle also 0x0 case)
  return sprintf("0x%x",$x->[0]) if @$x == 1;

  my $x1 = _copy($c,$x);

  my $es = '';
  my ($xr, $h, $x10000);
  if ($] >= 5.006)
    {
    $x10000 = [ 0x10000 ]; $h = 'h4';
    }
  else
    {
    $x10000 = [ 0x1000 ]; $h = 'h3';
    }
  while (@$x1 != 1 || $x1->[0] != 0)		# _is_zero()
    {
    ($x1, $xr) = _div($c,$x1,$x10000);
    $es .= unpack($h,pack('V',$xr->[0]));
    }
  $es = reverse $es;
  $es =~ s/^[0]+//;   # strip leading zeros
  '0x' . $es;					# return result prepended with 0x
  }

sub _as_bin
  {
  # convert a decimal number to bin (ref to array, return ref to string)
  my ($c,$x) = @_;

  # fits into one element (and Perl recent enough), handle also 0b0 case
  # handle zero case for older Perls
  if ($] <= 5.005 && @$x == 1 && $x->[0] == 0)
    {
    my $t = '0b0'; return $t;
    }
  if (@$x == 1 && $] >= 5.006)
    {
    my $t = sprintf("0b%b",$x->[0]);
    return $t;
    }
  my $x1 = _copy($c,$x);

  my $es = '';
  my ($xr, $b, $x10000);
  if ($] >= 5.006)
    {
    $x10000 = [ 0x10000 ]; $b = 'b16';
    }
  else
    {
    $x10000 = [ 0x1000 ]; $b = 'b12';
    }
  while (!(@$x1 == 1 && $x1->[0] == 0))		# _is_zero()
    {
    ($x1, $xr) = _div($c,$x1,$x10000);
    $es .= unpack($b,pack('v',$xr->[0]));
    }
  $es = reverse $es;
  $es =~ s/^[0]+//;   # strip leading zeros
  '0b' . $es;					# return result prepended with 0b
  }

sub _as_oct
  {
  # convert a decimal number to octal (ref to array, return ref to string)
  my ($c,$x) = @_;

  # fits into one element (handle also 0 case)
  return sprintf("0%o",$x->[0]) if @$x == 1;

  my $x1 = _copy($c,$x);

  my $es = '';
  my $xr;
  my $x1000 = [ 0100000 ];
  while (@$x1 != 1 || $x1->[0] != 0)		# _is_zero()
    {
    ($x1, $xr) = _div($c,$x1,$x1000);
    $es .= reverse sprintf("%05o", $xr->[0]);
    }
  $es = reverse $es;
  $es =~ s/^[0]+//;   # strip leading zeros
  '0' . $es;					# return result prepended with 0
  }

sub _from_oct
  {
  # convert a octal number to decimal (string, return ref to array)
  my ($c,$os) = @_;

  # for older Perls, play safe
  my $m = [ 0100000 ];
  my $d = 5;					# 5 digits at a time

  my $mul = _one();
  my $x = _zero();

  my $len = int( (length($os)-1)/$d );		# $d digit parts, w/o the '0'
  my $val; my $i = -$d;
  while ($len >= 0)
    {
    $val = substr($os,$i,$d);			# get oct digits
    $val = CORE::oct($val);
    $i -= $d; $len --;
    my $adder = [ $val ];
    _add ($c, $x, _mul ($c, $adder, $mul ) ) if $val != 0;
    _mul ($c, $mul, $m ) if $len >= 0; 		# skip last mul
    }
  $x;
  }

sub _from_hex
  {
  # convert a hex number to decimal (string, return ref to array)
  my ($c,$hs) = @_;

  my $m = _new($c, 0x10000000);			# 28 bit at a time (<32 bit!)
  my $d = 7;					# 7 digits at a time
  if ($] <= 5.006)
    {
    # for older Perls, play safe
    $m = [ 0x10000 ];				# 16 bit at a time (<32 bit!)
    $d = 4;					# 4 digits at a time
    }

  my $mul = _one();
  my $x = _zero();

  my $len = int( (length($hs)-2)/$d );		# $d digit parts, w/o the '0x'
  my $val; my $i = -$d;
  while ($len >= 0)
    {
    $val = substr($hs,$i,$d);			# get hex digits
    $val =~ s/^0x// if $len == 0;		# for last part only because
    $val = CORE::hex($val);			# hex does not like wrong chars
    $i -= $d; $len --;
    my $adder = [ $val ];
    # if the resulting number was to big to fit into one element, create a
    # two-element version (bug found by Mark Lakata - Thanx!)
    if (CORE::length($val) > $BASE_LEN)
      {
      $adder = _new($c,$val);
      }
    _add ($c, $x, _mul ($c, $adder, $mul ) ) if $val != 0;
    _mul ($c, $mul, $m ) if $len >= 0; 		# skip last mul
    }
  $x;
  }

sub _from_bin
  {
  # convert a hex number to decimal (string, return ref to array)
  my ($c,$bs) = @_;

  # instead of converting X (8) bit at a time, it is faster to "convert" the
  # number to hex, and then call _from_hex.

  my $hs = $bs;
  $hs =~ s/^[+-]?0b//;					# remove sign and 0b
  my $l = length($hs);					# bits
  $hs = '0' x (8-($l % 8)) . $hs if ($l % 8) != 0;	# padd left side w/ 0
  my $h = '0x' . unpack('H*', pack ('B*', $hs));	# repack as hex
  
  $c->_from_hex($h);
  }

##############################################################################
# special modulus functions

sub _modinv
  {
  # modular inverse
  my ($c,$x,$y) = @_;

  my $u = _zero($c); my $u1 = _one($c);
  my $a = _copy($c,$y); my $b = _copy($c,$x);

  # Euclid's Algorithm for bgcd(), only that we calc bgcd() ($a) and the
  # result ($u) at the same time. See comments in BigInt for why this works.
  my $q;
  ($a, $q, $b) = ($b, _div($c,$a,$b));		# step 1
  my $sign = 1;
  while (!_is_zero($c,$b))
    {
    my $t = _add($c, 				# step 2:
       _mul($c,_copy($c,$u1), $q) ,		#  t =  u1 * q
       $u );					#     + u
    $u = $u1;					#  u = u1, u1 = t
    $u1 = $t;
    $sign = -$sign;
    ($a, $q, $b) = ($b, _div($c,$a,$b));	# step 1
    }

  # if the gcd is not 1, then return NaN
  return (undef,undef) unless _is_one($c,$a);
 
  ($u1, $sign == 1 ? '+' : '-');
  }

sub _modpow
  {
  # modulus of power ($x ** $y) % $z
  my ($c,$num,$exp,$mod) = @_;

  # in the trivial case,
  if (_is_one($c,$mod))
    {
    splice @$num,0,1; $num->[0] = 0;
    return $num;
    }
  if ((scalar @$num == 1) && (($num->[0] == 0) || ($num->[0] == 1)))
    {
    $num->[0] = 1;
    return $num;
    }

#  $num = _mod($c,$num,$mod);	# this does not make it faster

  my $acc = _copy($c,$num); my $t = _one();

  my $expbin = _as_bin($c,$exp); $expbin =~ s/^0b//;
  my $len = length($expbin);
  while (--$len >= 0)
    {
    if ( substr($expbin,$len,1) eq '1')			# is_odd
      {
      _mul($c,$t,$acc);
      $t = _mod($c,$t,$mod);
      }
    _mul($c,$acc,$acc);
    $acc = _mod($c,$acc,$mod);
    }
  @$num = @$t;
  $num;
  }

sub _gcd
  {
  # greatest common divisor
  my ($c,$x,$y) = @_;

  while ( (scalar @$y != 1) || ($y->[0] != 0) )		# while ($y != 0)
    {
    my $t = _copy($c,$y);
    $y = _mod($c, $x, $y);
    $x = $t;
    }
  $x;
  }

##############################################################################
##############################################################################

1;
__END__

=head1 NAME

Math::BigInt::Calc - Pure Perl module to support Math::BigInt

=head1 SYNOPSIS

Provides support for big integer calculations. Not intended to be used by other
modules. Other modules which sport the same functions can also be used to support
Math::BigInt, like Math::BigInt::GMP or Math::BigInt::Pari.

=head1 DESCRIPTION

In order to allow for multiple big integer libraries, Math::BigInt was
rewritten to use library modules for core math routines. Any module which
follows the same API as this can be used instead by using the following:

	use Math::BigInt lib => 'libname';

'libname' is either the long name ('Math::BigInt::Pari'), or only the short
version like 'Pari'.

=head1 STORAGE

=head1 METHODS

The following functions MUST be defined in order to support the use by
Math::BigInt v1.70 or later:

	api_version()	return API version, 1 for v1.70, 2 for v1.83
	_new(string)	return ref to new object from ref to decimal string
	_zero()		return a new object with value 0
	_one()		return a new object with value 1
	_two()		return a new object with value 2
	_ten()		return a new object with value 10

	_str(obj)	return ref to a string representing the object
	_num(obj)	returns a Perl integer/floating point number
			NOTE: because of Perl numeric notation defaults,
			the _num'ified obj may lose accuracy due to 
			machine-dependent floating point size limitations
                    
	_add(obj,obj)	Simple addition of two objects
	_mul(obj,obj)	Multiplication of two objects
	_div(obj,obj)	Division of the 1st object by the 2nd
			In list context, returns (result,remainder).
			NOTE: this is integer math, so no
			fractional part will be returned.
			The second operand will be not be 0, so no need to
			check for that.
	_sub(obj,obj)	Simple subtraction of 1 object from another
			a third, optional parameter indicates that the params
			are swapped. In this case, the first param needs to
			be preserved, while you can destroy the second.
			sub (x,y,1) => return x - y and keep x intact!
	_dec(obj)	decrement object by one (input is guaranteed to be > 0)
	_inc(obj)	increment object by one


	_acmp(obj,obj)	<=> operator for objects (return -1, 0 or 1)

	_len(obj)	returns count of the decimal digits of the object
	_digit(obj,n)	returns the n'th decimal digit of object

	_is_one(obj)	return true if argument is 1
	_is_two(obj)	return true if argument is 2
	_is_ten(obj)	return true if argument is 10
	_is_zero(obj)	return true if argument is 0
	_is_even(obj)	return true if argument is even (0,2,4,6..)
	_is_odd(obj)	return true if argument is odd (1,3,5,7..)

	_copy		return a ref to a true copy of the object

	_check(obj)	check whether internal representation is still intact
			return 0 for ok, otherwise error message as string

	_from_hex(str)	return new object from a hexadecimal string
	_from_bin(str)	return new object from a binary string
	_from_oct(str)	return new object from an octal string
	
	_as_hex(str)	return string containing the value as
			unsigned hex string, with the '0x' prepended.
			Leading zeros must be stripped.
	_as_bin(str)	Like as_hex, only as binary string containing only
			zeros and ones. Leading zeros must be stripped and a
			'0b' must be prepended.
	
	_rsft(obj,N,B)	shift object in base B by N 'digits' right
	_lsft(obj,N,B)	shift object in base B by N 'digits' left
	
	_xor(obj1,obj2)	XOR (bit-wise) object 1 with object 2
			Note: XOR, AND and OR pad with zeros if size mismatches
	_and(obj1,obj2)	AND (bit-wise) object 1 with object 2
	_or(obj1,obj2)	OR (bit-wise) object 1 with object 2

	_mod(obj1,obj2)	Return remainder of div of the 1st by the 2nd object
	_sqrt(obj)	return the square root of object (truncated to int)
	_root(obj)	return the n'th (n >= 3) root of obj (truncated to int)
	_fac(obj)	return factorial of object 1 (1*2*3*4..)
	_pow(obj1,obj2)	return object 1 to the power of object 2
			return undef for NaN
	_zeros(obj)	return number of trailing decimal zeros
	_modinv		return inverse modulus
	_modpow		return modulus of power ($x ** $y) % $z
	_log_int(X,N)	calculate integer log() of X in base N
			X >= 0, N >= 0 (return undef for NaN)
			returns (RESULT, EXACT) where EXACT is:
			 1     : result is exactly RESULT
			 0     : result was truncated to RESULT
			 undef : unknown whether result is exactly RESULT
        _gcd(obj,obj)	return Greatest Common Divisor of two objects

The following functions are REQUIRED for an api_version of 2 or greater:

	_1ex($x)	create the number 1Ex where x >= 0
	_alen(obj)	returns approximate count of the decimal digits of the
			object. This estimate MUST always be greater or equal
			to what _len() returns.
        _nok(n,k)	calculate n over k (binomial coefficient)

The following functions are optional, and can be defined if the underlying lib
has a fast way to do them. If undefined, Math::BigInt will use pure Perl (hence
slow) fallback routines to emulate these:
	
	_signed_or
	_signed_and
	_signed_xor

Input strings come in as unsigned but with prefix (i.e. as '123', '0xabc'
or '0b1101').

So the library needs only to deal with unsigned big integers. Testing of input
parameter validity is done by the caller, so you need not worry about
underflow (f.i. in C<_sub()>, C<_dec()>) nor about division by zero or similar
cases.

The first parameter can be modified, that includes the possibility that you
return a reference to a completely different object instead. Although keeping
the reference and just changing its contents is preferred over creating and
returning a different reference.

Return values are always references to objects, strings, or true/false for
comparison routines.

=head1 WRAP YOUR OWN

If you want to port your own favourite c-lib for big numbers to the
Math::BigInt interface, you can take any of the already existing modules as
a rough guideline. You should really wrap up the latest BigInt and BigFloat
testsuites with your module, and replace in them any of the following:

	use Math::BigInt;

by this:

	use Math::BigInt lib => 'yourlib';

This way you ensure that your library really works 100% within Math::BigInt.

=head1 LICENSE
 
This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself. 

=head1 AUTHORS

Original math code by Mark Biggar, rewritten by Tels L<http://bloodgate.com/>
in late 2000.
Seperated from BigInt and shaped API with the help of John Peacock.

Fixed, speed-up, streamlined and enhanced by Tels 2001 - 2007.

=head1 SEE ALSO

L<Math::BigInt>, L<Math::BigFloat>,
L<Math::BigInt::GMP>, L<Math::BigInt::FastCalc> and L<Math::BigInt::Pari>.

=cut
