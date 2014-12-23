package Net::DNS::RR::LOC;
#
# $Id: LOC.pm 388 2005-06-22 10:06:05Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(
        @ISA $VERSION @poweroften $reference_alt
        $reference_latlon $conv_sec $conv_min $conv_deg
        $default_min $default_sec $default_size
        $default_horiz_pre $default_vert_pre
);

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 388 $)[1];

# Powers of 10 from 0 to 9 (used to speed up calculations).
@poweroften = (1, 10, 100, 1_000, 10_000, 100_000, 1_000_000,
               10_000_000, 100_000_000, 1_000_000_000);

# Reference altitude in centimeters (see RFC 1876).
$reference_alt = 100_000 * 100;

# Reference lat/lon (see RFC 1876).
$reference_latlon = 2**31;

# Conversions to/from thousandths of a degree.
$conv_sec = 1000;
$conv_min = 60 * $conv_sec;
$conv_deg = 60 * $conv_min;

# Defaults (from RFC 1876, Section 3).
$default_min       = 0;
$default_sec       = 0;
$default_size      = 1;
$default_horiz_pre = 10_000;
$default_vert_pre  = 10;

sub new {
	my ($class, $self, $data, $offset) = @_;

	if ($self->{"rdlength"} > 0) {
		my ($version) = unpack("\@$offset C", $$data);
		++$offset;
	
		$self->{"version"} = $version;
	
		if ($version == 0) {
			my ($size) = unpack("\@$offset C", $$data);
			$size = precsize_ntoval($size);
			++$offset;
	
			my ($horiz_pre) = unpack("\@$offset C", $$data);
			$horiz_pre = precsize_ntoval($horiz_pre);
			++$offset;
	
			my ($vert_pre) = unpack("\@$offset C", $$data);
			$vert_pre = precsize_ntoval($vert_pre);
			++$offset;
	
			my ($latitude) = unpack("\@$offset N", $$data);
			$offset += Net::DNS::INT32SZ();
	
			my ($longitude) = unpack("\@$offset N", $$data);
			$offset += Net::DNS::INT32SZ();
	
			my ($altitude) = unpack("\@$offset N", $$data);
			$offset += Net::DNS::INT32SZ();
	
			$self->{"size"}      = $size;
			$self->{"horiz_pre"} = $horiz_pre;
			$self->{"vert_pre"}  = $vert_pre;
			$self->{"latitude"}  = $latitude;
			$self->{"longitude"} = $longitude;
			$self->{"altitude"}  = $altitude;
		}
		else {
			# What to do for unsupported versions?
		}
	}

	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;

	if ($string && 
	    $string =~ /^ (\d+) \s+		# deg lat
			  ((\d+) \s+)?		# min lat
			  (([\d.]+) \s+)?	# sec lat
			  (N|S) \s+		# hem lat
			  (\d+) \s+		# deg lon
			  ((\d+) \s+)?		# min lon
			  (([\d.]+) \s+)?	# sec lon
			  (E|W) \s+		# hem lon
			  (-?[\d.]+) m? 	# altitude
			  (\s+ ([\d.]+) m?)?	# size
			  (\s+ ([\d.]+) m?)?	# horiz precision
			  (\s+ ([\d.]+) m?)? 	# vert precision
		       /ix) {

		# What to do for other versions?
		my $version = 0;

		my ($latdeg, $latmin, $latsec, $lathem) = ($1, $3, $5, $6);
		my ($londeg, $lonmin, $lonsec, $lonhem) = ($7, $9, $11, $12);
		my ($alt, $size, $horiz_pre, $vert_pre) = ($13, $15, $17, $19);

		$latmin    = $default_min       unless $latmin;
		$latsec    = $default_sec       unless $latsec;
		$lathem    = uc($lathem);

		$lonmin    = $default_min       unless $lonmin;
		$lonsec    = $default_sec       unless $lonsec;
		$lonhem    = uc($lonhem);

		$size      = $default_size      unless $size;
		$horiz_pre = $default_horiz_pre unless $horiz_pre;
		$vert_pre  = $default_vert_pre  unless $vert_pre;

		$self->{"version"}   = $version;
		$self->{"size"}      = $size * 100;
		$self->{"horiz_pre"} = $horiz_pre * 100;
		$self->{"vert_pre"}  = $vert_pre * 100;
		$self->{"latitude"}  = dms2latlon($latdeg, $latmin, $latsec,
						  $lathem);
		$self->{"longitude"} = dms2latlon($londeg, $lonmin, $lonsec,
						  $lonhem);
		$self->{"altitude"}  = $alt * 100 + $reference_alt;
	}

	return bless $self, $class;
}

sub rdatastr {
	my $self = shift;
	my $rdatastr;

	if (exists $self->{"version"}) {
		if ($self->{"version"} == 0) {
			my $lat       = $self->{"latitude"};
			my $lon       = $self->{"longitude"};
			my $altitude  = $self->{"altitude"};
			my $size      = $self->{"size"};
			my $horiz_pre = $self->{"horiz_pre"};
			my $vert_pre  = $self->{"vert_pre"};
	
			$altitude   = ($altitude - $reference_alt) / 100;
			$size      /= 100;
			$horiz_pre /= 100;
			$vert_pre  /= 100;
	
			$rdatastr = latlon2dms($lat, "NS")       . " " .
			            latlon2dms($lon, "EW")       . " " .
			            sprintf("%.2fm", $altitude)  . " " .
			            sprintf("%.2fm", $size)      . " " .
			            sprintf("%.2fm", $horiz_pre) . " " .
			            sprintf("%.2fm", $vert_pre);
		} else {
			$rdatastr = "; version " . $self->{"version"} . " not supported";
		}
	} else {
		$rdatastr = '';
	}

	return $rdatastr;
}

sub rr_rdata {
	my $self = shift;
	my $rdata = "";

	if (exists $self->{"version"}) {
		$rdata .= pack("C", $self->{"version"});
		if ($self->{"version"} == 0) {
			$rdata .= pack("C3", precsize_valton($self->{"size"}),
					     precsize_valton($self->{"horiz_pre"}),
					     precsize_valton($self->{"vert_pre"}));
			$rdata .= pack("N3", $self->{"latitude"},
					     $self->{"longitude"},
					     $self->{"altitude"});
		}
		else {
			# What to do for other versions?
		}
	}

	return $rdata;
}

sub precsize_ntoval {
	my $prec = shift;

	my $mantissa = (($prec >> 4) & 0x0f) % 10;
	my $exponent = ($prec & 0x0f) % 10;
	return $mantissa * $poweroften[$exponent];
}

sub precsize_valton {
	my $val = shift;

	my $exponent = 0;
	while ($val >= 10) {
		$val /= 10;
		++$exponent;
	}
	return (int($val) << 4) | ($exponent & 0x0f);
}

sub latlon2dms {
	my ($rawmsec, $hems) = @_;

	# Tried to use modulus here, but Perl dumped core if
	# the value was >= 2**31.

	my ($abs, $deg, $min, $sec, $msec, $hem);

	$abs  = abs($rawmsec - $reference_latlon);
	$deg  = int($abs / $conv_deg);
	$abs  -= $deg * $conv_deg;
	$min  = int($abs / $conv_min); 
	$abs -= $min * $conv_min;
	$sec  = int($abs / $conv_sec);
	$abs -= $sec * $conv_sec;
	$msec = $abs;

	$hem = substr($hems, ($rawmsec >= $reference_latlon ? 0 : 1), 1);

	return sprintf("%d %02d %02d.%03d %s", $deg, $min, $sec, $msec, $hem);
}

sub dms2latlon {
	my ($deg, $min, $sec, $hem) = @_;
	my ($retval);

	$retval = ($deg * $conv_deg) + ($min * $conv_min) + ($sec * $conv_sec);
	$retval = -$retval if ($hem eq "S") || ($hem eq "W");
	$retval += $reference_latlon;
	return $retval;
}

sub latlon {
	my $self = shift;
	my ($retlat, $retlon);

	if ($self->{"version"} == 0) {
		$retlat = latlon2deg($self->{"latitude"});
		$retlon = latlon2deg($self->{"longitude"});
	}
	else {
		$retlat = $retlon = undef;
	}

	return ($retlat, $retlon);
}

sub latlon2deg {
	my $rawmsec = shift;
	my $deg;

	$deg = ($rawmsec - $reference_latlon) / $conv_deg;
	return $deg;
}

1;
__END__

=head1 NAME

Net::DNS::RR::LOC - DNS LOC resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS Location (LOC) resource records.  See RFC 1876 for
details.

=head1 METHODS

=head2 version

    print "version = ", $rr->version, "\n";

Returns the version number of the representation; programs should
always check this.  C<Net::DNS> currently supports only version 0.

=head2 size 

    print "size = ", $rr->size, "\n";

Returns the diameter of a sphere enclosing the described entity,
in centimeters.

=head2 horiz_pre

    print "horiz_pre = ", $rr->horiz_pre, "\n";

Returns the horizontal precision of the data, in centimeters.

=head2 vert_pre

    print "vert_pre = ", $rr->vert_pre, "\n";

Returns the vertical precision of the data, in centimeters.

=head2 latitude

    print "latitude = ", $rr->latitude, "\n";

Returns the latitude of the center of the sphere described by
the C<size> method, in thousandths of a second of arc.  2**31
represents the equator; numbers above that are north latitude.

=head2 longitude

    print "longitude = ", $rr->longitude, "\n";

Returns the longitude of the center of the sphere described by
the C<size> method, in thousandths of a second of arc.  2**31
represents the prime meridian; numbers above that are east
longitude.

=head2 latlon

    ($lat, $lon) = $rr->latlon;
    system("xearth", "-pos", "fixed $lat $lon");

Returns the latitude and longitude as floating-point degrees.
Positive numbers represent north latitude or east longitude;
negative numbers represent south latitude or west longitude.

=head2 altitude

    print "altitude = ", $rr->altitude, "\n";

Returns the altitude of the center of the sphere described by
the C<size> method, in centimeters, from a base of 100,000m
below the WGS 84 reference spheroid used by GPS.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.
Some of the code and documentation is based on RFC 1876 and on code
contributed by Christopher Davis.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 1876

=cut
