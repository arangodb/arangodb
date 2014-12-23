#!/usr/bin/perl -w

package Math::BigFloat::Trace;

require 5.005_02;
use strict;

use Exporter;
use Math::BigFloat;
use vars qw($VERSION @ISA $PACKAGE @EXPORT_OK
            $accuracy $precision $round_mode $div_scale);

@ISA = qw(Exporter Math::BigFloat);

$VERSION = 0.01;

use overload;	# inherit overload from BigFloat

# Globals
$accuracy = $precision = undef;
$round_mode = 'even';
$div_scale = 40;

sub new
{
        my $proto  = shift;
        my $class  = ref($proto) || $proto;

        my $value       = shift;
	my $a = $accuracy; $a = $_[0] if defined $_[0];
	my $p = $precision; $p = $_[1] if defined $_[1];
        my $self = Math::BigFloat->new($value,$a,$p,$round_mode);

#	remember, downgrading may return a BigInt, so don't meddle with class	
#	bless $self,$class;

	print "MBF new '$value' => '$self' (",ref($self),")";
        return $self;
}

sub import
  {
  print "MBF import ",join(' ',@_);
  my $self = shift;

  # we catch the constants, the rest goes go BigFloat
  my @a = ();
  foreach (@_)
    {
    push @a, $_ if $_ ne ':constant';
    }
  overload::constant float => sub { $self->new(shift); }; 

  Math::BigFloat->import(@a);		# need it for subclasses
#  $self->export_to_level(1,$self,@_);		# need this ?
  }

1;
