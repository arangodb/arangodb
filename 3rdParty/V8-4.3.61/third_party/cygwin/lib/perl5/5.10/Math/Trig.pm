#
# Trigonometric functions, mostly inherited from Math::Complex.
# -- Jarkko Hietaniemi, since April 1997
# -- Raphael Manfredi, September 1996 (indirectly: because of Math::Complex)
#

require Exporter;
package Math::Trig;

use 5.005;
use strict;

use Math::Complex 1.54;
use Math::Complex qw(:trig :pi);

use vars qw($VERSION $PACKAGE @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);

@ISA = qw(Exporter);

$VERSION = 1.18;

my @angcnv = qw(rad2deg rad2grad
		deg2rad deg2grad
		grad2rad grad2deg);

my @areal = qw(asin_real acos_real);

@EXPORT = (@{$Math::Complex::EXPORT_TAGS{'trig'}},
	   @angcnv, @areal);

my @rdlcnv = qw(cartesian_to_cylindrical
		cartesian_to_spherical
		cylindrical_to_cartesian
		cylindrical_to_spherical
		spherical_to_cartesian
		spherical_to_cylindrical);

my @greatcircle = qw(
		     great_circle_distance
		     great_circle_direction
		     great_circle_bearing
		     great_circle_waypoint
		     great_circle_midpoint
		     great_circle_destination
		    );

my @pi = qw(pi pi2 pi4 pip2 pip4);

@EXPORT_OK = (@rdlcnv, @greatcircle, @pi, 'Inf');

# See e.g. the following pages:
# http://www.movable-type.co.uk/scripts/LatLong.html
# http://williams.best.vwh.net/avform.htm

%EXPORT_TAGS = ('radial' => [ @rdlcnv ],
	        'great_circle' => [ @greatcircle ],
	        'pi'     => [ @pi ]);

sub _DR  () { pi2/360 }
sub _RD  () { 360/pi2 }
sub _DG  () { 400/360 }
sub _GD  () { 360/400 }
sub _RG  () { 400/pi2 }
sub _GR  () { pi2/400 }

#
# Truncating remainder.
#

sub _remt ($$) {
    # Oh yes, POSIX::fmod() would be faster. Possibly. If it is available.
    $_[0] - $_[1] * int($_[0] / $_[1]);
}

#
# Angle conversions.
#

sub rad2rad($)     { _remt($_[0], pi2) }

sub deg2deg($)     { _remt($_[0], 360) }

sub grad2grad($)   { _remt($_[0], 400) }

sub rad2deg ($;$)  { my $d = _RD * $_[0]; $_[1] ? $d : deg2deg($d) }

sub deg2rad ($;$)  { my $d = _DR * $_[0]; $_[1] ? $d : rad2rad($d) }

sub grad2deg ($;$) { my $d = _GD * $_[0]; $_[1] ? $d : deg2deg($d) }

sub deg2grad ($;$) { my $d = _DG * $_[0]; $_[1] ? $d : grad2grad($d) }

sub rad2grad ($;$) { my $d = _RG * $_[0]; $_[1] ? $d : grad2grad($d) }

sub grad2rad ($;$) { my $d = _GR * $_[0]; $_[1] ? $d : rad2rad($d) }

#
# acos and asin functions which always return a real number
#

sub acos_real {
    return 0  if $_[0] >=  1;
    return pi if $_[0] <= -1;
    return acos($_[0]);
}

sub asin_real {
    return  &pip2 if $_[0] >=  1;
    return -&pip2 if $_[0] <= -1;
    return asin($_[0]);
}

sub cartesian_to_spherical {
    my ( $x, $y, $z ) = @_;

    my $rho = sqrt( $x * $x + $y * $y + $z * $z );

    return ( $rho,
             atan2( $y, $x ),
             $rho ? acos_real( $z / $rho ) : 0 );
}

sub spherical_to_cartesian {
    my ( $rho, $theta, $phi ) = @_;

    return ( $rho * cos( $theta ) * sin( $phi ),
             $rho * sin( $theta ) * sin( $phi ),
             $rho * cos( $phi   ) );
}

sub spherical_to_cylindrical {
    my ( $x, $y, $z ) = spherical_to_cartesian( @_ );

    return ( sqrt( $x * $x + $y * $y ), $_[1], $z );
}

sub cartesian_to_cylindrical {
    my ( $x, $y, $z ) = @_;

    return ( sqrt( $x * $x + $y * $y ), atan2( $y, $x ), $z );
}

sub cylindrical_to_cartesian {
    my ( $rho, $theta, $z ) = @_;

    return ( $rho * cos( $theta ), $rho * sin( $theta ), $z );
}

sub cylindrical_to_spherical {
    return ( cartesian_to_spherical( cylindrical_to_cartesian( @_ ) ) );
}

sub great_circle_distance {
    my ( $theta0, $phi0, $theta1, $phi1, $rho ) = @_;

    $rho = 1 unless defined $rho; # Default to the unit sphere.

    my $lat0 = pip2 - $phi0;
    my $lat1 = pip2 - $phi1;

    return $rho *
	acos_real( cos( $lat0 ) * cos( $lat1 ) * cos( $theta0 - $theta1 ) +
		   sin( $lat0 ) * sin( $lat1 ) );
}

sub great_circle_direction {
    my ( $theta0, $phi0, $theta1, $phi1 ) = @_;

    my $distance = &great_circle_distance;

    my $lat0 = pip2 - $phi0;
    my $lat1 = pip2 - $phi1;

    my $direction =
 	acos_real((sin($lat1) - sin($lat0) * cos($distance)) /
		  (cos($lat0) * sin($distance)));
  
    $direction = pi2 - $direction
	if sin($theta1 - $theta0) < 0;

    return rad2rad($direction);
}

*great_circle_bearing         = \&great_circle_direction;

sub great_circle_waypoint {
    my ( $theta0, $phi0, $theta1, $phi1, $point ) = @_;

    $point = 0.5 unless defined $point;

    my $d = great_circle_distance( $theta0, $phi0, $theta1, $phi1 );

    return undef if $d == pi;

    my $sd = sin($d);

    return ($theta0, $phi0) if $sd == 0;

    my $A = sin((1 - $point) * $d) / $sd;
    my $B = sin(     $point  * $d) / $sd;

    my $lat0 = pip2 - $phi0;
    my $lat1 = pip2 - $phi1;

    my $x = $A * cos($lat0) * cos($theta0) + $B * cos($lat1) * cos($theta1);
    my $y = $A * cos($lat0) * sin($theta0) + $B * cos($lat1) * sin($theta1);
    my $z = $A * sin($lat0)                + $B * sin($lat1);

    my $theta = atan2($y, $x);
    my $phi   = acos_real($z);

    return ($theta, $phi);
}

sub great_circle_midpoint {
    great_circle_waypoint(@_[0..3], 0.5);
}

sub great_circle_destination {
    my ( $theta0, $phi0, $dir0, $dst ) = @_;

    my $lat0 = pip2 - $phi0;

    my $phi1   = asin_real(sin($lat0)*cos($dst) +
			   cos($lat0)*sin($dst)*cos($dir0));

    my $theta1 = $theta0 + atan2(sin($dir0)*sin($dst)*cos($lat0),
				 cos($dst)-sin($lat0)*sin($phi1));

    my $dir1 = great_circle_bearing($theta1, $phi1, $theta0, $phi0) + pi;

    $dir1 -= pi2 if $dir1 > pi2;

    return ($theta1, $phi1, $dir1);
}

1;

__END__
=pod

=head1 NAME

Math::Trig - trigonometric functions

=head1 SYNOPSIS

    use Math::Trig;

    $x = tan(0.9);
    $y = acos(3.7);
    $z = asin(2.4);

    $halfpi = pi/2;

    $rad = deg2rad(120);

    # Import constants pi2, pip2, pip4 (2*pi, pi/2, pi/4).
    use Math::Trig ':pi';

    # Import the conversions between cartesian/spherical/cylindrical.
    use Math::Trig ':radial';

        # Import the great circle formulas.
    use Math::Trig ':great_circle';

=head1 DESCRIPTION

C<Math::Trig> defines many trigonometric functions not defined by the
core Perl which defines only the C<sin()> and C<cos()>.  The constant
B<pi> is also defined as are a few convenience functions for angle
conversions, and I<great circle formulas> for spherical movement.

=head1 TRIGONOMETRIC FUNCTIONS

The tangent

=over 4

=item B<tan>

=back

The cofunctions of the sine, cosine, and tangent (cosec/csc and cotan/cot
are aliases)

B<csc>, B<cosec>, B<sec>, B<sec>, B<cot>, B<cotan>

The arcus (also known as the inverse) functions of the sine, cosine,
and tangent

B<asin>, B<acos>, B<atan>

The principal value of the arc tangent of y/x

B<atan2>(y, x)

The arcus cofunctions of the sine, cosine, and tangent (acosec/acsc
and acotan/acot are aliases).  Note that atan2(0, 0) is not well-defined.

B<acsc>, B<acosec>, B<asec>, B<acot>, B<acotan>

The hyperbolic sine, cosine, and tangent

B<sinh>, B<cosh>, B<tanh>

The cofunctions of the hyperbolic sine, cosine, and tangent (cosech/csch
and cotanh/coth are aliases)

B<csch>, B<cosech>, B<sech>, B<coth>, B<cotanh>

The area (also known as the inverse) functions of the hyperbolic
sine, cosine, and tangent

B<asinh>, B<acosh>, B<atanh>

The area cofunctions of the hyperbolic sine, cosine, and tangent
(acsch/acosech and acoth/acotanh are aliases)

B<acsch>, B<acosech>, B<asech>, B<acoth>, B<acotanh>

The trigonometric constant B<pi> and some of handy multiples
of it are also defined.

B<pi, pi2, pi4, pip2, pip4>

=head2 ERRORS DUE TO DIVISION BY ZERO

The following functions

    acoth
    acsc
    acsch
    asec
    asech
    atanh
    cot
    coth
    csc
    csch
    sec
    sech
    tan
    tanh

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
C<atanh>, C<acoth>, the argument cannot be C<1> (one).  For the
C<atanh>, C<acoth>, the argument cannot be C<-1> (minus one).  For the
C<tan>, C<sec>, C<tanh>, C<sech>, the argument cannot be I<pi/2 + k *
pi>, where I<k> is any integer.

Note that atan2(0, 0) is not well-defined.

=head2 SIMPLE (REAL) ARGUMENTS, COMPLEX RESULTS

Please note that some of the trigonometric functions can break out
from the B<real axis> into the B<complex plane>. For example
C<asin(2)> has no definition for plain real numbers but it has
definition for complex numbers.

In Perl terms this means that supplying the usual Perl numbers (also
known as scalars, please see L<perldata>) as input for the
trigonometric functions might produce as output results that no more
are simple real numbers: instead they are complex numbers.

The C<Math::Trig> handles this by using the C<Math::Complex> package
which knows how to handle complex numbers, please see L<Math::Complex>
for more information. In practice you need not to worry about getting
complex numbers as results because the C<Math::Complex> takes care of
details like for example how to display complex numbers. For example:

    print asin(2), "\n";

should produce something like this (take or leave few last decimals):

    1.5707963267949-1.31695789692482i

That is, a complex number with the real part of approximately C<1.571>
and the imaginary part of approximately C<-1.317>.

=head1 PLANE ANGLE CONVERSIONS

(Plane, 2-dimensional) angles may be converted with the following functions.

=over

=item deg2rad

    $radians  = deg2rad($degrees);

=item grad2rad

    $radians  = grad2rad($gradians);

=item rad2deg

    $degrees  = rad2deg($radians);

=item grad2deg

    $degrees  = grad2deg($gradians);

=item deg2grad

    $gradians = deg2grad($degrees);

=item rad2grad

    $gradians = rad2grad($radians);

=back

The full circle is 2 I<pi> radians or I<360> degrees or I<400> gradians.
The result is by default wrapped to be inside the [0, {2pi,360,400}[ circle.
If you don't want this, supply a true second argument:

    $zillions_of_radians  = deg2rad($zillions_of_degrees, 1);
    $negative_degrees     = rad2deg($negative_radians, 1);

You can also do the wrapping explicitly by rad2rad(), deg2deg(), and
grad2grad().

=over 4

=item rad2rad

    $radians_wrapped_by_2pi = rad2rad($radians);

=item deg2deg

    $degrees_wrapped_by_360 = deg2deg($degrees);

=item grad2grad

    $gradians_wrapped_by_400 = grad2grad($gradians);

=back

=head1 RADIAL COORDINATE CONVERSIONS

B<Radial coordinate systems> are the B<spherical> and the B<cylindrical>
systems, explained shortly in more detail.

You can import radial coordinate conversion functions by using the
C<:radial> tag:

    use Math::Trig ':radial';

    ($rho, $theta, $z)     = cartesian_to_cylindrical($x, $y, $z);
    ($rho, $theta, $phi)   = cartesian_to_spherical($x, $y, $z);
    ($x, $y, $z)           = cylindrical_to_cartesian($rho, $theta, $z);
    ($rho_s, $theta, $phi) = cylindrical_to_spherical($rho_c, $theta, $z);
    ($x, $y, $z)           = spherical_to_cartesian($rho, $theta, $phi);
    ($rho_c, $theta, $z)   = spherical_to_cylindrical($rho_s, $theta, $phi);

B<All angles are in radians>.

=head2 COORDINATE SYSTEMS

B<Cartesian> coordinates are the usual rectangular I<(x, y, z)>-coordinates.

Spherical coordinates, I<(rho, theta, pi)>, are three-dimensional
coordinates which define a point in three-dimensional space.  They are
based on a sphere surface.  The radius of the sphere is B<rho>, also
known as the I<radial> coordinate.  The angle in the I<xy>-plane
(around the I<z>-axis) is B<theta>, also known as the I<azimuthal>
coordinate.  The angle from the I<z>-axis is B<phi>, also known as the
I<polar> coordinate.  The North Pole is therefore I<0, 0, rho>, and
the Gulf of Guinea (think of the missing big chunk of Africa) I<0,
pi/2, rho>.  In geographical terms I<phi> is latitude (northward
positive, southward negative) and I<theta> is longitude (eastward
positive, westward negative).

B<BEWARE>: some texts define I<theta> and I<phi> the other way round,
some texts define the I<phi> to start from the horizontal plane, some
texts use I<r> in place of I<rho>.

Cylindrical coordinates, I<(rho, theta, z)>, are three-dimensional
coordinates which define a point in three-dimensional space.  They are
based on a cylinder surface.  The radius of the cylinder is B<rho>,
also known as the I<radial> coordinate.  The angle in the I<xy>-plane
(around the I<z>-axis) is B<theta>, also known as the I<azimuthal>
coordinate.  The third coordinate is the I<z>, pointing up from the
B<theta>-plane.

=head2 3-D ANGLE CONVERSIONS

Conversions to and from spherical and cylindrical coordinates are
available.  Please notice that the conversions are not necessarily
reversible because of the equalities like I<pi> angles being equal to
I<-pi> angles.

=over 4

=item cartesian_to_cylindrical

    ($rho, $theta, $z) = cartesian_to_cylindrical($x, $y, $z);

=item cartesian_to_spherical

    ($rho, $theta, $phi) = cartesian_to_spherical($x, $y, $z);

=item cylindrical_to_cartesian

    ($x, $y, $z) = cylindrical_to_cartesian($rho, $theta, $z);

=item cylindrical_to_spherical

    ($rho_s, $theta, $phi) = cylindrical_to_spherical($rho_c, $theta, $z);

Notice that when C<$z> is not 0 C<$rho_s> is not equal to C<$rho_c>.

=item spherical_to_cartesian

    ($x, $y, $z) = spherical_to_cartesian($rho, $theta, $phi);

=item spherical_to_cylindrical

    ($rho_c, $theta, $z) = spherical_to_cylindrical($rho_s, $theta, $phi);

Notice that when C<$z> is not 0 C<$rho_c> is not equal to C<$rho_s>.

=back

=head1 GREAT CIRCLE DISTANCES AND DIRECTIONS

A great circle is section of a circle that contains the circle
diameter: the shortest distance between two (non-antipodal) points on
the spherical surface goes along the great circle connecting those two
points.

=head2 great_circle_distance

You can compute spherical distances, called B<great circle distances>,
by importing the great_circle_distance() function:

  use Math::Trig 'great_circle_distance';

  $distance = great_circle_distance($theta0, $phi0, $theta1, $phi1, [, $rho]);

The I<great circle distance> is the shortest distance between two
points on a sphere.  The distance is in C<$rho> units.  The C<$rho> is
optional, it defaults to 1 (the unit sphere), therefore the distance
defaults to radians.

If you think geographically the I<theta> are longitudes: zero at the
Greenwhich meridian, eastward positive, westward negative -- and the
I<phi> are latitudes: zero at the North Pole, northward positive,
southward negative.  B<NOTE>: this formula thinks in mathematics, not
geographically: the I<phi> zero is at the North Pole, not at the
Equator on the west coast of Africa (Bay of Guinea).  You need to
subtract your geographical coordinates from I<pi/2> (also known as 90
degrees).

  $distance = great_circle_distance($lon0, pi/2 - $lat0,
                                    $lon1, pi/2 - $lat1, $rho);

=head2 great_circle_direction

The direction you must follow the great circle (also known as I<bearing>)
can be computed by the great_circle_direction() function:

  use Math::Trig 'great_circle_direction';

  $direction = great_circle_direction($theta0, $phi0, $theta1, $phi1);

=head2 great_circle_bearing

Alias 'great_circle_bearing' for 'great_circle_direction' is also available.

  use Math::Trig 'great_circle_bearing';

  $direction = great_circle_bearing($theta0, $phi0, $theta1, $phi1);

The result of great_circle_direction is in radians, zero indicating
straight north, pi or -pi straight south, pi/2 straight west, and
-pi/2 straight east.

=head2 great_circle_destination

You can inversely compute the destination if you know the
starting point, direction, and distance:

  use Math::Trig 'great_circle_destination';

  # $diro is the original direction,
  # for example from great_circle_bearing().
  # $distance is the angular distance in radians,
  # for example from great_circle_distance().
  # $thetad and $phid are the destination coordinates,
  # $dird is the final direction at the destination.

  ($thetad, $phid, $dird) =
    great_circle_destination($theta, $phi, $diro, $distance);

or the midpoint if you know the end points:

=head2 great_circle_midpoint

  use Math::Trig 'great_circle_midpoint';

  ($thetam, $phim) =
    great_circle_midpoint($theta0, $phi0, $theta1, $phi1);

The great_circle_midpoint() is just a special case of

=head2 great_circle_waypoint

  use Math::Trig 'great_circle_waypoint';

  ($thetai, $phii) =
    great_circle_waypoint($theta0, $phi0, $theta1, $phi1, $way);

Where the $way is a value from zero ($theta0, $phi0) to one ($theta1,
$phi1).  Note that antipodal points (where their distance is I<pi>
radians) do not have waypoints between them (they would have an an
"equator" between them), and therefore C<undef> is returned for
antipodal points.  If the points are the same and the distance
therefore zero and all waypoints therefore identical, the first point
(either point) is returned.

The thetas, phis, direction, and distance in the above are all in radians.

You can import all the great circle formulas by

  use Math::Trig ':great_circle';

Notice that the resulting directions might be somewhat surprising if
you are looking at a flat worldmap: in such map projections the great
circles quite often do not look like the shortest routes --  but for
example the shortest possible routes from Europe or North America to
Asia do often cross the polar regions.  (The common Mercator projection
does B<not> show great circles as straight lines: straight lines in the
Mercator projection are lines of constant bearing.)

=head1 EXAMPLES

To calculate the distance between London (51.3N 0.5W) and Tokyo
(35.7N 139.8E) in kilometers:

    use Math::Trig qw(great_circle_distance deg2rad);

    # Notice the 90 - latitude: phi zero is at the North Pole.
    sub NESW { deg2rad($_[0]), deg2rad(90 - $_[1]) }
    my @L = NESW( -0.5, 51.3);
    my @T = NESW(139.8, 35.7);
    my $km = great_circle_distance(@L, @T, 6378); # About 9600 km.

The direction you would have to go from London to Tokyo (in radians,
straight north being zero, straight east being pi/2).

    use Math::Trig qw(great_circle_direction);

    my $rad = great_circle_direction(@L, @T); # About 0.547 or 0.174 pi.

The midpoint between London and Tokyo being

    use Math::Trig qw(great_circle_midpoint);

    my @M = great_circle_midpoint(@L, @T);

or about 69 N 89 E, in the frozen wastes of Siberia.

B<NOTE>: you B<cannot> get from A to B like this:

   Dist = great_circle_distance(A, B)
   Dir  = great_circle_direction(A, B)
   C    = great_circle_destination(A, Dist, Dir)

and expect C to be B, because the bearing constantly changes when
going from A to B (except in some special case like the meridians or
the circles of latitudes) and in great_circle_destination() one gives
a constant bearing to follow.

=head2 CAVEAT FOR GREAT CIRCLE FORMULAS

The answers may be off by few percentages because of the irregular
(slightly aspherical) form of the Earth.  The errors are at worst
about 0.55%, but generally below 0.3%.

=head2 Real-valued asin and acos

For small inputs asin() and acos() may return complex numbers even
when real numbers would be enough and correct, this happens because of
floating-point inaccuracies.  You can see these inaccuracies for
example by trying theses:

  print cos(1e-6)**2+sin(1e-6)**2 - 1,"\n";
  printf "%.20f", cos(1e-6)**2+sin(1e-6)**2,"\n";

which will print something like this

  -1.11022302462516e-16
  0.99999999999999988898

even though the expected results are of course exactly zero and one.
The formulas used to compute asin() and acos() are quite sensitive to
this, and therefore they might accidentally slip into the complex
plane even when they should not.  To counter this there are two
interfaces that are guaranteed to return a real-valued output.

=over 4

=item asin_real

    use Math::Trig qw(asin_real);

    $real_angle = asin_real($input_sin);

Return a real-valued arcus sine if the input is between [-1, 1],
B<inclusive> the endpoints.  For inputs greater than one, pi/2
is returned.  For inputs less than minus one, -pi/2 is returned.

=item acos_real

    use Math::Trig qw(acos_real);

    $real_angle = acos_real($input_cos);

Return a real-valued arcus cosine if the input is between [-1, 1],
B<inclusive> the endpoints.  For inputs greater than one, zero
is returned.  For inputs less than minus one, pi is returned.

=back

=head1 BUGS

Saying C<use Math::Trig;> exports many mathematical routines in the
caller environment and even overrides some (C<sin>, C<cos>).  This is
construed as a feature by the Authors, actually... ;-)

The code is not optimized for speed, especially because we use
C<Math::Complex> and thus go quite near complex numbers while doing
the computations even when the arguments are not. This, however,
cannot be completely avoided if we want things like C<asin(2)> to give
an answer instead of giving a fatal runtime error.

Do not attempt navigation using these formulas.

L<Math::Complex>

=head1 AUTHORS

Jarkko Hietaniemi <F<jhi!at!iki.fi>> and 
Raphael Manfredi <F<Raphael_Manfredi!at!pobox.com>>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut

# eof
