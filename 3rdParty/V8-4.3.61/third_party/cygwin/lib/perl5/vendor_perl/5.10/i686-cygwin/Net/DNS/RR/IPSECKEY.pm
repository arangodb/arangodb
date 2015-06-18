package Net::DNS::RR::IPSECKEY;


#
# $Id: IPSECKEY.pm 654 2007-06-20 15:02:50Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION );
use Socket;

use MIME::Base64;

$VERSION = (qw$LastChangedRevision: 654 $)[1];

@ISA = qw(Net::DNS::RR);

    
#my %gatetype = (
#    0 => "No gateway is present.",
#    1 => "A 4-byte IPv4 address is present.",
#    2 => "A 16-byte IPv6 address is present.",
#    3 => "A wire-encoded domain name is present.",
#    );

#my %algtype = (
#	RSA => 1,
#	DSA => 2,
#);

#my %fingerprinttype = (
#	'SHA-1' => 1,
#);

#my %fingerprinttypebyval = reverse %fingerprinttype;
#my %gatetypebyval= reverse %gatetype;
#my %algtypebyval	     = reverse %algtype;


sub new {
    my ($class, $self, $data, $offset) = @_;
    
    my $offsettoprec    = $offset;
    my $offsettogatetype = $offset+1;
    my $offsettoalgor    = $offset+2;
    my $offsettogateway  = $offset+3;
    my $offsettopubkey;
		
    $self->{'precedence'} = unpack('C', substr($$data, $offsettoprec, 1));
    $self->{'gatetype'}    = unpack('C', substr($$data, $offsettogatetype, 1));
    $self->{'algorithm'}    = unpack('C', substr($$data, $offsettoalgor, 1));

    if ($self->{'gatetype'}==0){
	$self->{'gateway'}='.';
	$offsettopubkey= $offsettogateway;
    }elsif($self->{'gatetype'}==1){
	$self->{'gateway'} = inet_ntoa(substr($$data, $offsettogateway, 4));
	$offsettopubkey= $offsettogateway+4;
    }elsif($self->{'gatetype'}==2){
	$offsettopubkey= $offsettogateway+16;
	my @addr = unpack("\@$offsettogateway n8", $$data);
	$self->{'gateway'} = sprintf("%x:%x:%x:%x:%x:%x:%x:%x", @addr);
    }elsif($self->{'gatetype'}==3){
	($self->{'gateway'}, $offsettopubkey) = Net::DNS::Packet::dn_expand($data, $offsettogateway);

    }else{
	die "Could not parse packet, no known gateway type (".$self->{'gatetype'}.")";
    }	
    my($pubmaterial)=substr($$data, $offsettopubkey,
			    ($self->{"rdlength"}-$offsettopubkey+$offset));

    $self->{"pubbin"}=$pubmaterial;

    return bless $self, $class;
}



sub new_from_string {
	my ($class, $self, $string) = @_;
	if ($string && ($string =~ /^(\d+)\s+(\d)\s+(\d)\s+(\S+)\s+(\S+)$/)) {
		$self->{"precedence"} = $1;
		$self->{"gatetype"} = $2;
		$self->{"algorithm"} = $3;
		if ($self->{"gatetype"}==2){
			# Using the AAAA.pm parsing functionality.
			my $AAAA=Net::DNS::RR->new("FOO AAAA ".$4);
			$self->{"gateway"}=$AAAA->rdatastr;
		}else	
		{
			$self->{"gateway"}= $4;
		}
		$self->{"pubkey"}= $5;
	}


	return bless $self, $class;
}


sub pubkey {
  my $self=shift;

  $self->{"pubkey"}= encode_base64($self->{"pubbin"},"") unless defined $self->{"pubkey"};
 
  return $self->{"pubkey"};
}


sub pubbin {
  my $self=shift;
  $self->{"pubbin"}= decode_base64($self->{"pubkey"}) unless defined $self->{"pubbin"};

  return $self->{"pubbin"};
}


sub rdatastr {
	my $self     = shift;
	my $rdatastr = '';
	return "" unless defined $self->{precedence};
	$rdatastr .= $self->{"precedence"} . " ". $self->{"gatetype"} . " " .
	  $self->{"algorithm"}. " ";
	if ($self->{"gatetype"}==0){
	  $rdatastr .= ". ";
	}else{
	  $rdatastr .= $self->{"gateway"}. " ";
	}
	$rdatastr .= $self->pubkey();

	return $rdatastr;
}

sub rr_rdata {
	my $self = shift;
	my $rdata = "";
	if (exists $self->{"precedence"}) {
		$rdata .= pack("C", $self->{"precedence"});
		$rdata .= pack("C", $self->{"gatetype"});
		$rdata .= pack("C", $self->{"algorithm"});
		if ($self->{"gatetype"}==1 ){
			$rdata .= inet_aton($self->{"gateway"});
		}elsif($self->{"gatetype"}==2){	
			my @addr = split(/:/, $self->{"gateway"});
			$rdata .= pack("n8", map { hex $_ } @addr);
		}elsif($self->{"gatetype"}==3){	
			# No Compression _name2wire will do.
			$rdata .= $self->_name2wire($self->{"gateway"});
		}
		$rdata .= $self->pubbin();
	}

	return $rdata;

}



	

1;


=head1 NAME

Net::DNS::RR::IPSECKEY - DNS IPSECKEY resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

CLASS for the IPSECKEY RR.

=head1 METHODS

In addition to the regular methods 


=head2 algorithm

Returns the RR's algorithm field in decimal representation

    1 = RSA
    2 = DSA

=head2 precedence  

Returns the presedence

=head2 	gatetype  

Returns the gatetype.

   0  "No gateway is present.",
   1  "A 4-byte IPv4 address is present.",
   2  "A 16-byte IPv6 address is present.",
   3  "A wire-encoded domain name is present.",

=head2 gateway

Returns the gateway in the relevant string notation.

=head2 pubkey

Returns the public key in base64 notation

=head2 pubbin 

Returns the binary public key material in a string.

=head1 TODO

Check on validity of algorithm and gatetype.

=head1 COPYRIGHT

Copyright (c) 2007 NLnet LAbs, Olaf Kolkman.

"All rights reserved, This program is free software; you may redistribute it
and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
draft-ietf-dnssext-delegation-signer

=cut





