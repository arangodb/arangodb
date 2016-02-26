package Math::BigInt::FastCalc;

use 5.006;
use strict;
# use warnings;	# dont use warnings for older Perls

use DynaLoader;
use Math::BigInt::Calc;

use vars qw/@ISA $VERSION $BASE $BASE_LEN/;

@ISA = qw(DynaLoader);

$VERSION = '0.19';

bootstrap Math::BigInt::FastCalc $VERSION;

##############################################################################
# global constants, flags and accessory

# announce that we are compatible with MBI v1.70 and up
sub api_version () { 1; }
 
BEGIN
  {
  # use Calc to override the methods that we do not provide in XS

  for my $method (qw/
    str
    add sub mul div
    rsft lsft
    mod modpow modinv
    gcd
    pow root sqrt log_int fac nok
    digit check
    from_hex from_bin from_oct as_hex as_bin as_oct
    zeros base_len
    xor or and
    alen 1ex
    /)
    {
    no strict 'refs';
    *{'Math::BigInt::FastCalc::_' . $method} = \&{'Math::BigInt::Calc::_' . $method};
    }
  my ($AND_BITS, $XOR_BITS, $OR_BITS, $BASE_LEN_SMALL, $MAX_VAL);
 
  # store BASE_LEN and BASE to later pass it to XS code 
  ($BASE_LEN, $AND_BITS, $XOR_BITS, $OR_BITS, $BASE_LEN_SMALL, $MAX_VAL, $BASE) =
    Math::BigInt::Calc::_base_len();

  }

sub import
  {
  _set_XS_BASE($BASE, $BASE_LEN);
  }

##############################################################################
##############################################################################

1;
__END__
=pod

=head1 NAME

Math::BigInt::FastCalc - Math::BigInt::Calc with some XS for more speed

=head1 SYNOPSIS

Provides support for big integer calculations. Not intended to be used by
other modules. Other modules which sport the same functions can also be used
to support Math::BigInt, like L<Math::BigInt::GMP> or L<Math::BigInt::Pari>.

=head1 DESCRIPTION

In order to allow for multiple big integer libraries, Math::BigInt was
rewritten to use library modules for core math routines. Any module which
follows the same API as this can be used instead by using the following:

	use Math::BigInt lib => 'libname';

'libname' is either the long name ('Math::BigInt::Pari'), or only the short
version like 'Pari'. To use this library:

	use Math::BigInt lib => 'FastCalc';

Note that from L<Math::BigInt> v1.76 onwards, FastCalc will be loaded
automatically, if possible.

=head1 STORAGE

FastCalc works exactly like Calc, in stores the numbers in decimal form,
chopped into parts.

=head1 METHODS

The following functions are now implemented in FastCalc.xs:

	_is_odd		_is_even	_is_one		_is_zero
	_is_two		_is_ten
	_zero		_one		_two		_ten
	_acmp		_len		_num
	_inc		_dec
	__strip_zeros	_copy

=head1 LICENSE
 
This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself. 

=head1 AUTHORS

Original math code by Mark Biggar, rewritten by Tels L<http://bloodgate.com/>
in late 2000.
Seperated from BigInt and shaped API with the help of John Peacock.
Fixed, sped-up and enhanced by Tels http://bloodgate.com 2001-2003.
Further streamlining (api_version 1 etc.) by Tels 2004-2007.

=head1 SEE ALSO

L<Math::BigInt>, L<Math::BigFloat>,
L<Math::BigInt::GMP>, L<Math::BigInt::FastCalc> and L<Math::BigInt::Pari>.

=cut
