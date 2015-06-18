package Math::BigInt::CalcEmu;

use 5.006002;
use strict;
# use warnings;	# dont use warnings for older Perls
use vars qw/$VERSION/;

$VERSION = '0.05';

package Math::BigInt;

# See SYNOPSIS below.

my $CALC_EMU;

BEGIN
  {
  $CALC_EMU = Math::BigInt->config()->{'lib'};
  # register us with MBI to get notified of future lib changes
  Math::BigInt::_register_callback( __PACKAGE__, sub { $CALC_EMU = $_[0]; } );
  }

sub __emu_band
  {
  my ($self,$x,$y,$sx,$sy,@r) = @_;

  return $x->bzero(@r) if $y->is_zero() || $x->is_zero();
  
  my $sign = 0;					# sign of result
  $sign = 1 if $sx == -1 && $sy == -1;

  my ($bx,$by);

  if ($sx == -1)				# if x is negative
    {
    # two's complement: inc and flip all "bits" in $bx
    $bx = $x->binc()->as_hex();			# -1 => 0, -2 => 1, -3 => 2 etc
    $bx =~ s/-?0x//;
    $bx =~ tr/0123456789abcdef/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  else
    {
    $bx = $x->as_hex();				# get binary representation
    $bx =~ s/-?0x//;
    $bx =~ tr/fedcba9876543210/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  if ($sy == -1)				# if y is negative
    {
    # two's complement: inc and flip all "bits" in $by
    $by = $y->copy()->binc()->as_hex();		# -1 => 0, -2 => 1, -3 => 2 etc
    $by =~ s/-?0x//;
    $by =~ tr/0123456789abcdef/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  else
    {
    $by = $y->as_hex();				# get binary representation
    $by =~ s/-?0x//;
    $by =~ tr/fedcba9876543210/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  # now we have bit-strings from X and Y, reverse them for padding
  $bx = reverse $bx;
  $by = reverse $by;

  # padd the shorter string
  my $xx = "\x00"; $xx = "\x0f" if $sx == -1;
  my $yy = "\x00"; $yy = "\x0f" if $sy == -1;
  my $diff = CORE::length($bx) - CORE::length($by);
  if ($diff > 0)
    {
    # if $yy eq "\x00", we can cut $bx, otherwise we need to padd $by
    $by .= $yy x $diff;
    }
  elsif ($diff < 0)
    {
    # if $xx eq "\x00", we can cut $by, otherwise we need to padd $bx
    $bx .= $xx x abs($diff);
    }
  
  # and the strings together
  my $r = $bx & $by;

  # and reverse the result again
  $bx = reverse $r;

  # One of $x or $y was negative, so need to flip bits in the result.
  # In both cases (one or two of them negative, or both positive) we need
  # to get the characters back.
  if ($sign == 1)
    {
    $bx =~ tr/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/0123456789abcdef/;
    }
  else
    {
    $bx =~ tr/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/fedcba9876543210/;
    }

  # leading zeros will be stripped by _from_hex()
  $bx = '0x' . $bx;
  $x->{value} = $CALC_EMU->_from_hex( $bx );

  # calculate sign of result
  $x->{sign} = '+';
  $x->{sign} = '-' if $sign == 1 && !$x->is_zero();

  $x->bdec() if $sign == 1;

  $x->round(@r);
  }

sub __emu_bior
  {
  my ($self,$x,$y,$sx,$sy,@r) = @_;

  return $x->round(@r) if $y->is_zero();

  my $sign = 0;					# sign of result
  $sign = 1 if ($sx == -1) || ($sy == -1);

  my ($bx,$by);

  if ($sx == -1)				# if x is negative
    {
    # two's complement: inc and flip all "bits" in $bx
    $bx = $x->binc()->as_hex();			# -1 => 0, -2 => 1, -3 => 2 etc
    $bx =~ s/-?0x//;
    $bx =~ tr/0123456789abcdef/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  else
    {
    $bx = $x->as_hex();				# get binary representation
    $bx =~ s/-?0x//;
    $bx =~ tr/fedcba9876543210/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  if ($sy == -1)				# if y is negative
    {
    # two's complement: inc and flip all "bits" in $by
    $by = $y->copy()->binc()->as_hex();		# -1 => 0, -2 => 1, -3 => 2 etc
    $by =~ s/-?0x//;
    $by =~ tr/0123456789abcdef/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  else
    {
    $by = $y->as_hex();				# get binary representation
    $by =~ s/-?0x//;
    $by =~ tr/fedcba9876543210/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  # now we have bit-strings from X and Y, reverse them for padding
  $bx = reverse $bx;
  $by = reverse $by;

  # padd the shorter string
  my $xx = "\x00"; $xx = "\x0f" if $sx == -1;
  my $yy = "\x00"; $yy = "\x0f" if $sy == -1;
  my $diff = CORE::length($bx) - CORE::length($by);
  if ($diff > 0)
    {
    $by .= $yy x $diff;
    }
  elsif ($diff < 0)
    {
    $bx .= $xx x abs($diff);
    }

  # or the strings together
  my $r = $bx | $by;

  # and reverse the result again
  $bx = reverse $r;

  # one of $x or $y was negative, so need to flip bits in the result
  # in both cases (one or two of them negative, or both positive) we need
  # to get the characters back.
  if ($sign == 1)
    {
    $bx =~ tr/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/0123456789abcdef/;
    }
  else
    {
    $bx =~ tr/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/fedcba9876543210/;
    }

  # leading zeros will be stripped by _from_hex()
  $bx = '0x' . $bx;
  $x->{value} = $CALC_EMU->_from_hex( $bx );

  # calculate sign of result
  $x->{sign} = '+';
  $x->{sign} = '-' if $sign == 1 && !$x->is_zero();

  # if one of X or Y was negative, we need to decrement result
  $x->bdec() if $sign == 1;

  $x->round(@r);
  }

sub __emu_bxor
  {
  my ($self,$x,$y,$sx,$sy,@r) = @_;

  return $x->round(@r) if $y->is_zero();

  my $sign = 0;					# sign of result
  $sign = 1 if $x->{sign} ne $y->{sign};

  my ($bx,$by);

  if ($sx == -1)				# if x is negative
    {
    # two's complement: inc and flip all "bits" in $bx
    $bx = $x->binc()->as_hex();			# -1 => 0, -2 => 1, -3 => 2 etc
    $bx =~ s/-?0x//;
    $bx =~ tr/0123456789abcdef/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  else
    {
    $bx = $x->as_hex();				# get binary representation
    $bx =~ s/-?0x//;
    $bx =~ tr/fedcba9876543210/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  if ($sy == -1)				# if y is negative
    {
    # two's complement: inc and flip all "bits" in $by
    $by = $y->copy()->binc()->as_hex();		# -1 => 0, -2 => 1, -3 => 2 etc
    $by =~ s/-?0x//;
    $by =~ tr/0123456789abcdef/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  else
    {
    $by = $y->as_hex();				# get binary representation
    $by =~ s/-?0x//;
    $by =~ tr/fedcba9876543210/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  # now we have bit-strings from X and Y, reverse them for padding
  $bx = reverse $bx;
  $by = reverse $by;

  # padd the shorter string
  my $xx = "\x00"; $xx = "\x0f" if $sx == -1;
  my $yy = "\x00"; $yy = "\x0f" if $sy == -1;
  my $diff = CORE::length($bx) - CORE::length($by);
  if ($diff > 0)
    {
    $by .= $yy x $diff;
    }
  elsif ($diff < 0)
    {
    $bx .= $xx x abs($diff);
    }

  # xor the strings together
  my $r = $bx ^ $by;

  # and reverse the result again
  $bx = reverse $r;

  # one of $x or $y was negative, so need to flip bits in the result
  # in both cases (one or two of them negative, or both positive) we need
  # to get the characters back.
  if ($sign == 1)
    {
    $bx =~ tr/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/0123456789abcdef/;
    }
  else
    {
    $bx =~ tr/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/fedcba9876543210/;
    }

  # leading zeros will be stripped by _from_hex()
  $bx = '0x' . $bx;
  $x->{value} = $CALC_EMU->_from_hex( $bx );

  # calculate sign of result
  $x->{sign} = '+';
  $x->{sign} = '-' if $sx != $sy && !$x->is_zero();

  $x->bdec() if $sign == 1;

  $x->round(@r);
  }

##############################################################################
##############################################################################

1;
__END__

=head1 NAME

Math::BigInt::CalcEmu - Emulate low-level math with BigInt code

=head1 SYNOPSIS

	use Math::BigInt::CalcEmu;

=head1 DESCRIPTION

Contains routines that emulate low-level math functions in BigInt, e.g.
optional routines the low-level math package does not provide on it's own.

Will be loaded on demand and called automatically by BigInt.

Stuff here is really low-priority to optimize, since it is far better to
implement the operation in the low-level math libary directly, possible even
using a call to the native lib.

=head1 METHODS

=head2 __emu_bxor

=head2 __emu_band

=head2 __emu_bior

=head1 LICENSE
 
This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself. 

=head1 AUTHORS

(c) Tels http://bloodgate.com 2003, 2004 - based on BigInt code by
Tels from 2001-2003.

=head1 SEE ALSO

L<Math::BigInt>, L<Math::BigFloat>, L<Math::BigInt::BitVect>,
L<Math::BigInt::GMP> and L<Math::BigInt::Pari>.

=cut
