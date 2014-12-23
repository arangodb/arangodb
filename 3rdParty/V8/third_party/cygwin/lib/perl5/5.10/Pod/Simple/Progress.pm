
require 5;
package Pod::Simple::Progress;
$VERSION = "1.01";
use strict;

# Objects of this class are used for noting progress of an
#  operation every so often.  Messages delivered more often than that
#  are suppressed.
#
# There's actually nothing in here that's specific to Pod processing;
#  but it's ad-hoc enough that I'm not willing to give it a name that
#  implies that it's generally useful, like "IO::Progress" or something.
#
# -- sburke
#
#--------------------------------------------------------------------------

sub new {
  my($class,$delay) = @_;
  my $self = bless {'quiet_until' => 1},  ref($class) || $class;
  $self->to(*STDOUT{IO});
  $self->delay(defined($delay) ? $delay : 5);
  return $self;
}

sub copy { 
  my $orig = shift;
  bless {%$orig, 'quiet_until' => 1}, ref($orig);
}
#--------------------------------------------------------------------------

sub reach {
  my($self, $point, $note) = @_;
  if( (my $now = time) >= $self->{'quiet_until'}) {
    my $goal;
    my    $to = $self->{'to'};
    print $to join('',
      ($self->{'quiet_until'} == 1) ? () : '... ',
      (defined $point) ? (
        '#',
        ($goal = $self->{'goal'}) ? (
          ' ' x (length($goal) - length($point)),
          $point, '/', $goal,
        ) : $point,
        $note ? ': ' : (),
      ) : (),
      $note || '',
      "\n"
    );
    $self->{'quiet_until'} = $now + $self->{'delay'};
  }
  return $self;
}

#--------------------------------------------------------------------------

sub done {
  my($self, $note) = @_;
  $self->{'quiet_until'} = 1;
  return $self->reach( undef, $note );
}

#--------------------------------------------------------------------------
# Simple accessors:

sub delay {
  return $_[0]{'delay'} if @_ == 1; $_[0]{'delay'} = $_[1]; return $_[0] }
sub goal {
  return $_[0]{'goal' } if @_ == 1; $_[0]{'goal' } = $_[1]; return $_[0] }
sub to   {
  return $_[0]{'to'   } if @_ == 1; $_[0]{'to'   } = $_[1]; return $_[0] }

#--------------------------------------------------------------------------

unless(caller) { # Simple self-test:
  my $p = __PACKAGE__->new->goal(5);
  $p->reach(1, "Primus!");
  sleep 1;
  $p->reach(2, "Secundus!");
  sleep 3;
  $p->reach(3, "Tertius!");
  sleep 5;
  $p->reach(4);
  $p->reach(5, "Quintus!");
  sleep 1;
  $p->done("All done");
}

#--------------------------------------------------------------------------
1;
__END__

