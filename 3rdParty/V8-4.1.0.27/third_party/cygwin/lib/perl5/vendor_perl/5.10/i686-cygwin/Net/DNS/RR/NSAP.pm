package Net::DNS::RR::NSAP;
#
# $Id: NSAP.pm 388 2005-06-22 10:06:05Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 388 $)[1];

sub new {
	my ($class, $self, $data, $offset) = @_;

	if ($self->{"rdlength"} > 0) {
		my $afi = unpack("\@$offset C", $$data);
		$self->{"afi"} = sprintf("%02x", $afi);
		++$offset;

		if ($self->{"afi"} eq "47") {
			my @idi = unpack("\@$offset C2", $$data);
			$offset += 2;

			my $dfi = unpack("\@$offset C", $$data);
			$offset += 1;

			my @aa = unpack("\@$offset C3", $$data);
			$offset += 3;

			my @rsvd = unpack("\@$offset C2", $$data);
			$offset += 2;

			my @rd = unpack("\@$offset C2", $$data);
			$offset += 2;

			my @area = unpack("\@$offset C2", $$data);
			$offset += 2;

			my @id = unpack("\@$offset C6", $$data);
			$offset += 6;

			my $sel = unpack("\@$offset C", $$data);
			$offset += 1;

			$self->{"idi"}  = sprintf("%02x" x 2, @idi);
			$self->{"dfi"}  = sprintf("%02x" x 1, $dfi);
			$self->{"aa"}   = sprintf("%02x" x 3, @aa);
			$self->{"rsvd"} = sprintf("%02x" x 2, @rsvd);
			$self->{"rd"}   = sprintf("%02x" x 2, @rd);
			$self->{"area"} = sprintf("%02x" x 2, @area);
			$self->{"id"}   = sprintf("%02x" x 6, @id);
			$self->{"sel"}  = sprintf("%02x" x 1, $sel);

		} else {
			# What to do for unsupported versions?
		}
	}

	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;
	
	if ($string) {
		$string =~ s/\.//g;  # remove all dots.
		$string =~ s/^0x//;  # remove leading 0x
		
		if ($string =~ /^[a-zA-Z0-9]{40}$/) {
			@{ $self }{ qw(afi idi dfi aa rsvd rd area id sel) } = 
				unpack("A2A4A2A6A4A4A4A12A2", $string);
		} 
	}
	
	return bless $self, $class;
}
	

sub idp {
	my $self = shift;

	return join('', $self->{"afi"},
		            $self->{"idi"});
}

sub dsp {
	my $self = shift;

	return join('', 
			 $self->{"dfi"},
			 $self->{"aa"},
			 $self->rsvd,
			 $self->{"rd"},
			 $self->{"area"},
			 $self->{"id"},
			 $self->{"sel"});
}

sub rsvd {
	my $self = shift;

	return exists $self->{"rsvd"} ? $self->{"rsvd"} : "0000";
}

sub rdatastr {
	my $self = shift;
	my $rdatastr;

	if (exists $self->{"afi"}) {
		if ($self->{"afi"} eq "47") {
			$rdatastr = join('', $self->idp, $self->dsp);
		} else {
			$rdatastr = "; AFI $self->{'afi'} not supported";
		}
	} else {
		$rdatastr = '';
	}

	return $rdatastr;
}

sub rr_rdata {
	my $self = shift;
	my $rdata = "";

	if (exists $self->{"afi"}) {
		$rdata .= pack("C", hex($self->{"afi"}));

		if ($self->{"afi"} eq "47") {
			$rdata .= str2bcd($self->{"idi"},  2);
			$rdata .= str2bcd($self->{"dfi"},  1);
			$rdata .= str2bcd($self->{"aa"},   3);
			$rdata .= str2bcd(0,               2);	# rsvd
			$rdata .= str2bcd($self->{"rd"},   2);
			$rdata .= str2bcd($self->{"area"}, 2);
			$rdata .= str2bcd($self->{"id"},   6);
			$rdata .= str2bcd($self->{"sel"},  1);
		}

		# Checks for other versions would go here.
	}

	return $rdata;
}

#------------------------------------------------------------------------------
# Usage:  str2bcd(STRING, NUM_BYTES)
#
# Takes a string representing a hex number of arbitrary length and
# returns an equivalent BCD string of NUM_BYTES length (with
# NUM_BYTES * 2 digits), adding leading zeros if necessary.
#------------------------------------------------------------------------------

# This can't be the best way....
sub str2bcd {
	my ($string, $bytes) = @_;
	my $retval = "";

	my $digits = $bytes * 2;
	$string = sprintf("%${digits}s", $string);
	$string =~ tr/ /0/;

	my $i;
	for ($i = 0; $i < $bytes; ++$i) {
		my $bcd = substr($string, $i*2, 2);
		$retval .= pack("C", hex $bcd);
	}

	return $retval;
}

1;
__END__

=head1 NAME

Net::DNS::RR::NSAP - DNS NSAP resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS Network Service Access Point (NSAP) resource records.

=head1 METHODS

=head2 idp

    print "idp = ", $rr->idp, "\n";

Returns the RR's initial domain part (the AFI and IDI fields).

=head2 dsp

    print "dsp = ", $rr->dsp, "\n";

Returns the RR's domain specific part (the DFI, AA, Rsvd, RD, Area,
ID, and SEL fields).

=head2 afi

    print "afi = ", $rr->afi, "\n";

Returns the RR's authority and format identifier.  C<Net::DNS>
currently supports only AFI 47 (GOSIP Version 2).

=head2 idi

    print "idi = ", $rr->idi, "\n";

Returns the RR's initial domain identifier.

=head2 dfi

    print "dfi = ", $rr->dfi, "\n";

Returns the RR's DSP format identifier.

=head2 aa 

    print "aa = ", $rr->aa, "\n";

Returns the RR's administrative authority.

=head2 rsvd 

    print "rsvd = ", $rr->rsvd, "\n";

Returns the RR's reserved field.

=head2 rd

    print "rd = ", $rr->rd, "\n";

Returns the RR's routing domain identifier.

=head2 area

    print "area = ", $rr->area, "\n";

Returns the RR's area identifier.

=head2 id

    print "id = ", $rr->id, "\n";

Returns the RR's system identifier.

=head2 sel

    print "sel = ", $rr->sel, "\n";

Returns the RR's NSAP selector.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.. 

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 1706.

=cut
