#!/usr/bin/perl -w

package Math::BigInt::Trace;

require 5.005_02;
use strict;

use Exporter;
use Math::BigInt;
use vars qw($VERSION @ISA $PACKAGE @EXPORT_OK
            $accuracy $precision $round_mode $div_scale);

@ISA = qw(Exporter Math::BigInt);

$VERSION = 0.01;

use overload;	# inherit overload from BigInt

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
        my $self = Math::BigInt->new($value,$a,$p,$round_mode);
	bless $self,$class;
	print "MBI new '$value' => '$self' (",ref($self),")";
        return $self;
}

sub import
  {
  print "MBI import ",join(' ',@_);
  my $self = shift;
  Math::BigInt::import($self,@_);		# need it for subclasses
#  $self->export_to_level(1,$self,@_);		# need this ?
  @_ = ();
  }

1;
