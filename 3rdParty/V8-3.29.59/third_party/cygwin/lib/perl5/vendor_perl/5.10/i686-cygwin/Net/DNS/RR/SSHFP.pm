package Net::DNS::RR::SSHFP;
#
# $Id: SSHFP.pm 626 2007-02-02 07:31:32Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION $HasBabble);

BEGIN {
	eval {
		require Digest::BubbleBabble; 
		Digest::BubbleBabble->import(qw(bubblebabble))
	};
		
	$HasBabble = $@ ? 0 : 1;
	
}

$VERSION = (qw$LastChangedRevision: 626 $)[1];

@ISA = qw(Net::DNS::RR);

my %algtype = (
	RSA => 1,
	DSA => 2,
);

my %fingerprinttype = (
	'SHA-1' => 1,
);

my %fingerprinttypebyval = reverse %fingerprinttype;
my %algtypebyval	     = reverse %algtype;


sub new {
    my ($class, $self, $data, $offset) = @_;
    
	if ($self->{'rdlength'} > 0) {
		my $offsettoalg    = $offset;
		my $offsettofptype = $offset+1;
		my $offsettofp     = $offset+2;
		my $fplength       = 20;   # This will need to change if other fingerprint types
								   # are being deployed.
	
	
		$self->{'algorithm'} = unpack('C', substr($$data, $offsettoalg, 1));
		$self->{'fptype'}    = unpack('C', substr($$data, $offsettofptype, 1));
	
		unless (defined $fingerprinttypebyval{$self->{'fptype'}}){
		  warn "This fingerprint type $self->{'fptype'} has not yet been implemented, creation of SSHFP failed\n." ;
		  return undef;
		}

							   
		# All this is SHA-1 dependend
		$self->{'fpbin'} = substr($$data,$offsettofp, $fplength); # SHA1 digest 20 bytes long
	
		$self->{'fingerprint'} = uc unpack('H*', $self->{'fpbin'});
    }
    
    
    return bless $self, $class;
}


sub new_from_string {
	my ($class, $self, $string) = @_;
	
	if ($string) {		
		$string =~ tr/()//d;
		$string =~ s/;.*$//mg;
		$string =~ s/\n//g;
		
		@{$self}{qw(algorithm fptype fingerprint)} = split(m/\s+/, $string, 3);
					
		# We allow spaces in the fingerprint.
		$self->{'fingerprint'} =~ s/\s//g;		
    }
	    
	return bless $self, $class;
}



sub rdatastr {
	my $self     = shift;
	my $rdatastr = '';

	if (exists $self->{"algorithm"}) {
		$rdatastr = join('  ', @{$self}{qw(algorithm fptype fingerprint)}) 
					.' ; ' . $self->babble;
	} 

	return $rdatastr;
}

sub rr_rdata {
    my $self = shift;

    if (exists $self->{"algorithm"}) {   	    
    	return pack('C2',  @{$self}{qw(algorithm fptype)}) . $self->fpbin;
    }
    
    return '';

}



sub babble {
    my $self = shift;
    
    if ($HasBabble) {
		return bubblebabble(Digest => $self->fpbin);	
    } else {
		return "";
    }
}


sub fpbin {
	my ($self) = @_;
			
	return $self->{'fpbin'} ||= pack('H*', $self->{'fingerprint'});
}
	

1;


=head1 NAME

Net::DNS::RR::SSHFP - DNS SSHFP resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for Delegation signer (SSHFP) resource records.

=head1 METHODS

In addition to the regular methods 


=head2 algorithm

    print "algoritm" = ", $rr->algorithm, "\n";

Returns the RR's algorithm field in decimal representation

    1 = RSA
    2 = DSS


=head2 fingerprint

    print "fingerprint" = ", $rr->fingerprint, "\n";

Returns the SHA1 fingerprint over the label and key in hexadecimal
representation.


=head2 fpbin

    $fpbin = $rr->fpbin;

Returns the fingerprint as binary material.


=head2 fptype

   print "fingerprint type" . " = " . $rr->fptype ."\n";

Returns the fingerprint type of the SSHFP RR.

=head2 babble

   print $rr->babble;

If Digest::BubbleBabble is available on the sytem this method returns the
'BabbleBubble' representation of the fingerprint. The 'BabbleBubble'
string may be handy for telephone confirmation.

The 'BabbleBubble' string returned as a comment behind the RDATA when
the string method is called.

The method returns an empty string if Digest::BubbleBable is not installed.

=head1 TODO 

=head1 ACKNOWLEDGEMENT

Jakob Schlyter for code review and supplying patches.

=head1 COPYRIGHT

Copyright (c) 2004 RIPE NCC, Olaf Kolkman.

"All rights reserved, This program is free software; you may redistribute it
and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
draft-ietf-dnssext-delegation-signer

=cut





