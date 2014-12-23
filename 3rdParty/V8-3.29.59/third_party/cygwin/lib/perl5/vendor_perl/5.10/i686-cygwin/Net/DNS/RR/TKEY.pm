package Net::DNS::RR::TKEY;
#
# $Id: TKEY.pm 388 2005-06-22 10:06:05Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);

use Digest::HMAC_MD5;
use MIME::Base64;

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 388 $)[1];

sub new {
	my ($class, $self, $data, $offset) = @_;

	# if we have some data then we are parsing an incoming TKEY packet
	# see RFC2930 for the packet format
	if ($self->{"rdlength"} > 0) {
		($self->{"algorithm"}, $offset) = Net::DNS::Packet::dn_expand($data, $offset);

		@{$self}{qw(inception expiration)} = unpack("\@$offset NN", $$data);
		$offset += Net::DNS::INT32SZ() + Net::DNS::INT32SZ();

		@{$self}{qw(inception expiration)} = unpack("\@$offset nn", $$data);
		$offset += Net::DNS::INT16SZ() + Net::DNS::INT16SZ();

		my ($key_len) = unpack("\@$offset n", $$data);
		$offset += Net::DNS::INT16SZ();
		$self->{"key"} = substr($$data, $offset, $key_len);
		$offset += $key_len;

		my ($other_len) = unpack("\@$offset n", $$data);
		$offset += Net::DNS::INT16SZ();
		$self->{"other_data"} = substr($$data, $offset, $other_len);
		$offset += $other_len;
	}

	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;

	if ($string && ($string =~ /^(.*)$/)) {
		$self->{"key"}     = $1;
	}

	$self->{"algorithm"}   = "gss.microsoft.com";
	$self->{"inception"}   = time;
	$self->{"expiration"}  = time + 24*60*60;
	$self->{"mode"}        = 3; # GSSAPI
	$self->{"error"}       = 0;
	$self->{"other_len"}   = 0;
	$self->{"other_data"}  = "";

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
		$rdatastr = '';
	}

	return $rdatastr;
}

sub rr_rdata {
	my ($self, $packet, $offset) = @_;
	my $rdata = "";

	$packet->{"compnames"} = {};
	$rdata .= $packet->dn_comp($self->{"algorithm"}, 0);
	$rdata .= pack("N", $self->{"inception"});
	$rdata .= pack("N", $self->{"expiration"});
	$rdata .= pack("n", $self->{"mode"});
	$rdata .= pack("n", 0); # error
	$rdata .= pack("n", length($self->{"key"}));
	$rdata .= $self->{"key"};
	$rdata .= pack("n", length($self->{"other_data"}));
	$rdata .= $self->{"other_data"};

	return $rdata;
}

1;
__END__

=head1 NAME

Net::DNS::RR::TKEY - DNS TKEY resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS TKEY resource records.

=head1 METHODS

=head2 algorithm

    $rr->algorithm($algorithm_name);
    print "algorithm = ", $rr->algorithm, "\n";

Gets or sets the domain name that specifies the name of the algorithm.
The default algorithm is gss.microsoft.com

=head2 inception

    $rr->inception(time);
    print "inception = ", $rr->inception, "\n";

Gets or sets the inception time as the number of seconds since 1 Jan 1970
00:00:00 UTC.

The default inception time is the current time.

=head2 expiration

    $rr->expiration(time);
    print "expiration = ", $rr->expiration, "\n";

Gets or sets the expiration time as the number of seconds since 1 Jan 1970
00:00:00 UTC.

The default expiration time is the current time plus 1 day.

=head2 mode

    $rr->mode(3);
    print "mode = ", $rr->mode, "\n";

Sets the key mode (see rfc2930). The default is 3 which corresponds to GSSAPI

=head2 error

    print "error = ", $rr->error, "\n";

Returns the RCODE covering TKEY processing.  See RFC 2930 for details.

=head2 other_len

    print "other len = ", $rr->other_len, "\n";

Returns the length of the Other Data.  Should be zero.

=head2 other_data

    print "other data = ", $rr->other_data, "\n";

Returns the Other Data.  This field should be empty.

=head1 BUGS

This code has not been extensively tested.  Use with caution on
production systems. See http://samba.org/ftp/samba/tsig-gss/ for an
example usage.

=head1 COPYRIGHT

Copyright (c) 2000 Andrew Tridgell.  All rights reserved.  This program
is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=head1 ACKNOWLEDGMENT

The Net::DNS::RR::TKEY module is based on the TSIG module by Michael
Fuhr and Chris Turbeville.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 2845

=cut

