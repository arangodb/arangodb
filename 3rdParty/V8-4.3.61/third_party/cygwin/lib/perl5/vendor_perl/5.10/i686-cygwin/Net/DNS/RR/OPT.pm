package Net::DNS::RR::OPT;
#
# $Id: OPT.pm 393 2005-07-01 19:21:52Z olaf $
#

use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION %extendedrcodesbyname %extendedrcodesbyval $EDNSVERSION);

use Carp;

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 393 $)[1];

$EDNSVERSION = 0;

%extendedrcodesbyname = (
	"ONLY_RDATA" => 0,		# No name specified see 4.6 of 2671 
	"UNDEF1"     => 1,
	"UNDEF2"     => 2,
	"UNDEF3"     => 3,
	"UNDEF4"     => 4,
	"UNDEF5"     => 5,
	"UNDEF6"     => 6,
	"UNDEF7"     => 7,
	"UNDEF8"     => 8,
	"UNDEF9"     => 9,
	"UNDEF10"    => 10,
	"UNDEF11"    => 11,
	"UNDEF12"    => 12,
	"UNDEF13"    => 13,
	"UNDEF14"    => 14,
	"UNDEF15"    => 15,
	"BADVERS"    => 16,		# RFC 2671
);
%extendedrcodesbyval = reverse %extendedrcodesbyname;



sub new {
	my ($class, $self, $data, $offset) = @_;

	$self->{"name"} = "" ;   # should allway be "root"

	if ($self->{"rdlength"} > 0) {
		$self->{"optioncode"}   = unpack("n", substr($$data, $offset, 2));
		$self->{"optionlength"} = unpack("n", substr($$data, $offset+2, 2));
		$self->{"optiondata"}   = unpack("n", substr($$data, $offset+4, $self->{"optionlength"}));
	}

	$self->{"_rcode_flags"}  = pack("N",$self->{"ttl"});
	
	$self->{"extendedrcode"} = unpack("C", substr($self->{"_rcode_flags"}, 0, 1));
	$self->{"ednsversion"}   = unpack("C", substr($self->{"_rcode_flags"}, 1, 1));
	$self->{"ednsflags"}     = unpack("n", substr($self->{"_rcode_flags"}, 2, 2));
	
	
	return bless $self, $class;
}






sub new_from_string {
    my ($class, $self ) = @_;
    
    # There is no such thing as an OPT RR in a ZONE file. 
    # Not implemented!
    croak "You should not try to create a OPT RR from a string\nNot implemented";
    return bless $self, $class;
}



sub new_from_hash {
	my ($class, $self ) = @_;
	
	$self->{"name"} = "" ;   # should allway be "root"
	# Setting the MTU smaller then 512 does not make sense 
	# should we test for a maximum here?
	if ($self->{"class"} eq "IN" || $self->{"class"} < 512) {
		$self->{"class"} = 512;    # Default value...
	}
	
	$self->{"extendedrcode"} = 0 unless exists $self->{"extendedrcode"};
	
	$self->{"ednsflags"}   = 0 unless exists $self->{"ednsflags"};
	$self->{"ednsversion"} = $EDNSVERSION unless exists $self->{"ednsversion"};
	$self->{"ttl"}= unpack ("N", 
		pack("C", $self->{"extendedrcode"}) .
		pack("C", $self->{"ednsversion"})  .
		pack("n", $self->{"ednsflags"})
	);
	
	if (exists  $self->{"optioncode"}) {
		$self->{"optiondata"}   = "" if ! exists  $self->{"optiondata"};
		$self->{"optionlength"} = length $self->{"optiondata"}
	}
	
	return bless $self, $class;

}





sub string {
   my $self = shift;
   return
	"; EDNS Version "     . $self->{"ednsversion"} . 
	"\t UDP Packetsize: " .  $self->{"class"} . 
	"\n; EDNS-RCODE:\t"   . $self->{"extendedrcode"} .
	" (" . $extendedrcodesbyval{ $self->{"extendedrcode"} }. ")" .
	"\n; EDNS-FLAGS:\t"   . sprintf("0x%04x", $self->{"ednsflags"}) .
	"\n";
}


sub rdatastr {
	return '; Parsing of OPT rdata is not yet implemented';
}


sub rr_rdata {
	my $self = shift;
	my $rdata;
	
	if (exists $self->{"optioncode"}) {
		$rdata  = pack("n", $self->{"optioncode"});
		$rdata .= pack("n", $self->{"optionlength"}); 
		$rdata .= $self->{"optiondata"}
	} else {
		$rdata = "";
	}
	
	return $rdata;
}







sub do{
    my $self=shift;
    return ( 0x8000 & $self->{"ednsflags"} );
}



sub set_do{
    my $self=shift;
    return $self->{"ednsflags"} = ( 0x8000 | $self->{"ednsflags"} );

}



sub clear_do{
    my $self=shift;
    return $self->{"ednsflags"} = ( ~0x8000 & $self->{"ednsflags"} );

}



sub size {
    my $self=shift;
    my $size=shift;
    $self->{"class"}=$size if defined($size);
    return $self->{"class"};
}




1;
__END__

=head1 NAME

Net::DNS::RR::OPT - DNS OPT

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for EDNS pseudo resource record OPT.

=head1 METHODS

This object should only be used inside the Net::DNS classes itself.

=head2 new 

Since "OPT" is a pseudo record and should not be stored in
masterfiles; Therefore we have not implemented a method to create this
RR from string.

One may create the object from a hash. See RFC 2671 for details for
the meaning of the hash keys.

 $rr= new Net::DNS::RR {
    name => "",     # Ignored and set to ""
    type => "OPT",  
    class => 1024,    # sets UDP payload size
    extendedrcode =>  0x00,    # sets the extended RCODE 1 octets
    ednsflags     =>  0x0000,  # sets the ednsflags (2octets)  
    optioncode   =>   0x0      # 2 octets
    optiondata   =>   0x0      # optionlength octets
 }    

The ednsversion is set to 0 for now. The ttl value is determined from 
the extendedrcode, the ednsversion and the ednsflag.
The rdata is constructed from the optioncode and optiondata 
see section 4.4 of RFC 2671

If optioncode is left undefined then we do not expect any RDATA.

The defaults are no rdata.   


=head2 do, set_do, clear_do

    $opt->set_do;

Reads, sets and clears the do flag. (first bit in the ednssflags);


=head2 size

    $opt->size(1498);
    print "Packet size:". $opt->size() ;
 
Sets or gets the packet size.


=head1 TODO

- This class is tailored to use with dnssec. 

- Do some range checking on the input.

- This class probably needs subclasses once OPTION codes start to be defined.

- look at use of extended labels

=head1 COPYRIGHT

Copyright (c) 2001, 2002  RIPE NCC.  Author Olaf M. Kolkman

All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the author not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.


THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS; IN NO EVENT SHALL
AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

Based on, and contains, code by Copyright (c) 1997-2002 Michael Fuhr.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 2435 Section 3

=cut
