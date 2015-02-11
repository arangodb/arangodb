package Net::DNS::RR::TSIG;
#
# $Id: TSIG.pm 388 2005-06-22 10:06:05Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);

use Digest::HMAC_MD5;
use MIME::Base64;

use constant DEFAULT_ALGORITHM => "HMAC-MD5.SIG-ALG.REG.INT";
use constant DEFAULT_FUDGE     => 300;

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 388 $)[1];

# a signing function for the HMAC-MD5 algorithm. This can be overridden using
# the sign_func element
sub sign_hmac {
	my ($key, $data) = @_;

	$key =~ s/ //g;
	$key = decode_base64($key);

	my $hmac = Digest::HMAC_MD5->new($key);
	$hmac->add($data);

	return $hmac->digest;
}

sub new {
	my ($class, $self, $data, $offset) = @_;

	if ($self->{"rdlength"} > 0) {
		($self->{"algorithm"}, $offset) = Net::DNS::Packet::dn_expand($data, $offset);

		my ($time_high, $time_low) = unpack("\@$offset nN", $$data);
		$self->{"time_signed"} = $time_low;	# bug
		$offset += Net::DNS::INT16SZ() + Net::DNS::INT32SZ();

		@{$self}{qw(fudge mac_size)} = unpack("\@$offset nn", $$data);
		$offset += Net::DNS::INT16SZ() + Net::DNS::INT16SZ();

		$self->{"mac"} = substr($$data, $offset, $self->{'mac_size'});
		$offset += $self->{'mac_size'};

		@{$self}{qw(original_id error other_len)} = unpack("\@$offset nnn", $$data);
		$offset += Net::DNS::INT16SZ() * 3;

		my $odata = substr($$data, $offset, $self->{'other_len'});
		my ($odata_high, $odata_low) = unpack("nN", $odata);
		$self->{"other_data"} = $odata_low;
	}

	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;

	if ($string && ($string =~ /^(.*)$/)) {
		$self->{"key"}     = $1;
	}

	$self->{"algorithm"}   = DEFAULT_ALGORITHM;
	$self->{"time_signed"} = time;
	$self->{"fudge"}       = DEFAULT_FUDGE;
	$self->{"mac_size"}    = 0;
	$self->{"mac"}         = "";
	$self->{"original_id"} = 0;
	$self->{"error"}       = 0;
	$self->{"other_len"}   = 0;
	$self->{"other_data"}  = "";
	$self->{"sign_func"}   = \&sign_hmac;

	# RFC 2845 Section 2.3
	$self->{"class"} = "ANY";

	return bless $self, $class;
}

sub error {
	my $self = shift;

	my $rcode;
	my $error = $self->{"error"};

	if (defined($error)) {
		$rcode = $Net::DNS::rcodesbyval{$error} || $error;
	}

	return $rcode;
}

sub mac_size {
	my $self = shift;
	return length(defined($self->{"mac"}) ? $self->{"mac"} : "");
}

sub mac {
	my $self = shift;
	my $mac = unpack("H*", $self->{"mac"}) if defined($self->{"mac"});
	return $mac;
}

sub rdatastr {
	my $self = shift;

	my $error = $self->error;
	$error = "UNDEFINED" unless defined $error;

	my $rdatastr;

	if (exists $self->{"algorithm"}) {
		$rdatastr = "$self->{algorithm}. $error";
		if ($self->{"other_len"} && defined($self->{"other_data"})) {
			$rdatastr .= " $self->{other_data}";
		}
	} else {
		$rdatastr = "";
	}

	return $rdatastr;
}

# return the data that needs to be signed/verified. This is useful for
# external TSIG verification routines
sub sig_data {
	my ($self, $packet) = @_;
	my ($newpacket, $sigdata);

	# XXX this is horrible.  $pkt = Net::DNS::Packet->clone($packet); maybe?
	bless($newpacket = {},"Net::DNS::Packet");
	%{$newpacket} = %{$packet};
	bless($newpacket->{"header"} = {},"Net::DNS::Header");
	$newpacket->{"additional"} = [];
	%{$newpacket->{"header"}} = %{$packet->{"header"}};
	@{$newpacket->{"additional"}} = @{$packet->{"additional"}};
	shift(@{$newpacket->{"additional"}});
	$newpacket->{"header"}{"arcount"}--;
	$newpacket->{"compnames"} = {};

	# Add the request MAC if present (used to validate responses).
	$sigdata .= pack("H*", $self->{"request_mac"})
	    if $self->{"request_mac"};

	$sigdata .= $newpacket->data;

	# Don't compress the record (key) name.
	my $tmppacket = Net::DNS::Packet->new("");
	$sigdata .= $tmppacket->dn_comp(lc($self->{"name"}), 0);
	
	$sigdata .= pack("n", $Net::DNS::classesbyname{uc($self->{"class"})});
	$sigdata .= pack("N", $self->{"ttl"});
	
	# Don't compress the algorithm name.
	$tmppacket->{"compnames"} = {};
	$sigdata .= $tmppacket->dn_comp(lc($self->{"algorithm"}), 0);
	
	$sigdata .= pack("nN", 0, $self->{"time_signed"});	# bug
	$sigdata .= pack("n", $self->{"fudge"});
	$sigdata .= pack("nn", $self->{"error"}, $self->{"other_len"});
	
	$sigdata .= pack("nN", 0, $self->{"other_data"})
	    if $self->{"other_data"};
	
	return $sigdata;
}

sub rr_rdata {
	my ($self, $packet, $offset) = @_;
	my $rdata = "";

	if (exists $self->{"key"}) {
		# form the data to be signed
		my $sigdata = $self->sig_data($packet);

		# and call the signing function
		$self->{"mac"} = &{$self->{"sign_func"}}($self->{"key"}, $sigdata);
		$self->{"mac_size"} = length($self->{"mac"});

		# construct the signed TSIG record
		$packet->{"compnames"} = {};
		$rdata .= $packet->dn_comp($self->{"algorithm"}, 0);

		$rdata .= pack("nN", 0, $self->{"time_signed"});	# bug
		$rdata .= pack("nn", $self->{"fudge"}, $self->{"mac_size"});
		$rdata .= $self->{"mac"};

		$rdata .= pack("nnn",($packet->{"header"}->{"id"},
		                      $self->{"error"},
		                      $self->{"other_len"}));

		$rdata .= pack("nN", 0, $self->{"other_data"})
		    if $self->{"other_data"};
	}

	return $rdata;
}

1;
__END__

=head1 NAME

Net::DNS::RR::TSIG - DNS TSIG resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS Transaction Signature (TSIG) resource records.

=head1 METHODS

=head2 algorithm

    $rr->algorithm($algorithm_name);
    print "algorithm = ", $rr->algorithm, "\n";

Gets or sets the domain name that specifies the name of the algorithm.
The only algorithm currently supported is HMAC-MD5.SIG-ALG.REG.INT.

=head2 time_signed

    $rr->time_signed(time);
    print "time signed = ", $rr->time_signed, "\n";

Gets or sets the signing time as the number of seconds since 1 Jan 1970
00:00:00 UTC.

The default signing time is the current time.

=head2 fudge

    $rr->fudge(60);
    print "fudge = ", $rr->fudge, "\n";

Gets or sets the "fudge", i.e., the seconds of error permitted in the
signing time.

The default fudge is 300 seconds.

=head2 mac_size

    print "MAC size = ", $rr->mac_size, "\n";

Returns the number of octets in the message authentication code (MAC).
The programmer must call a Net::DNS::Packet object's data method
before this will return anything meaningful.

=head2 mac

    print "MAC = ", $rr->mac, "\n";

Returns the message authentication code (MAC) as a string of hex
characters.  The programmer must call a Net::DNS::Packet object's
data method before this will return anything meaningful.

=head2 original_id

    $rr->original_id(12345);
    print "original ID = ", $rr->original_id, "\n";

Gets or sets the original message ID.

=head2 error

    print "error = ", $rr->error, "\n";

Returns the RCODE covering TSIG processing.  Common values are
NOERROR, BADSIG, BADKEY, and BADTIME.  See RFC 2845 for details.

=head2 other_len

    print "other len = ", $rr->other_len, "\n";

Returns the length of the Other Data.  Should be zero unless the
error is BADTIME.

=head2 other_data

    print "other data = ", $rr->other_data, "\n";

Returns the Other Data.  This field should be empty unless the
error is BADTIME, in which case it will contain the server's
time as the number of seconds since 1 Jan 1970 00:00:00 UTC.

=head2 sig_data

     my $sigdata = $tsig->sig_data($packet);

Returns the packet packed according to RFC2845 in a form for signing. This
is only needed if you want to supply an external signing function, such as is 
needed for TSIG-GSS. 

=head2 sign_func

     sub my_sign_fn($$) {
	     my ($key, $data) = @_;
	     
	     return some_digest_algorithm($key, $data);
     }

     $tsig->sign_func(\&my_sign_fn);

This sets the signing function to be used for this TSIG record. 

The default signing function is HMAC-MD5.

=head1 BUGS

This code is still under development.  Use with caution on production
systems.

The time_signed and other_data fields should be 48-bit unsigned
integers (RFC 2845, Sections 2.3 and 4.5.2).  The current implementation
ignores the upper 16 bits; this will cause problems for times later
than 19 Jan 2038 03:14:07 UTC.

The only builtin algorithm currently supported is
HMAC-MD5.SIG-ALG.REG.INT. You can use other algorithms by supplying an
appropriate sign_func.

=head1 COPYRIGHT

Copyright (c) 2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 ACKNOWLEDGMENT

Most of the code in the Net::DNS::RR::TSIG module was contributed
by Chris Turbeville. 

Support for external signing functions was added by Andrew Tridgell.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 2845

=cut

