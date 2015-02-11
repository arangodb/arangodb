package Net::DNS::RR::HINFO;
#
# $Id: HINFO.pm 639 2007-05-25 12:00:15Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);
use Net::DNS::RR::TXT;

@ISA     = qw(Net::DNS::RR Net::DNS::RR::TXT);
$VERSION = (qw$LastChangedRevision: 639 $)[1];

sub new {
	my ($class, $self, $data, $offset) = @_;

	if ($self->{"rdlength"} > 0) {
		my ($cpu, $os, $len);

		($len) = unpack("\@$offset C", $$data);
		++$offset;
		$cpu = substr($$data, $offset, $len);
		$offset += $len;

		($len) = unpack("\@$offset C", $$data);
		++$offset;
		$os = substr($$data, $offset, $len);
		$offset += $len;

		$self->{"cpu"} = $cpu;
		$self->{"os"}  = $os;
	}

	return bless $self, $class;
}

sub new_from_string {
	my ( $class, $self, $rdata_string ) = @_ ;
	
	bless $self, $class;
        
	$self->_build_char_str_list($rdata_string);
	my @elements= $self->char_str_list();
	if (@elements==2){
		
		
		$self->{"cpu"} = $elements[0];
		$self->{"os"}  = $elements[1];
	}else{
		return;
	}
	
	
	return bless $self, $class;
}

sub rdatastr {
	my $self = shift;
	
	return $self->{"cpu"}
	  ? qq("$self->{cpu}" "$self->{os}")
	    : '';
}

sub rr_rdata {
	my $self = shift;
	my $rdata = "";

	if (exists $self->{"cpu"}) {
		$rdata .= pack("C", length $self->{"cpu"});
		$rdata .= $self->{"cpu"};

		$rdata .= pack("C", length $self->{"os"});
		$rdata .= $self->{"os"};
	}

	return $rdata;
}

1;
__END__

=head1 NAME

Net::DNS::RR::HINFO - DNS HINFO resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS Host Information (HINFO) resource records.

=head1 METHODS

=head2 cpu

    print "cpu = ", $rr->cpu, "\n";

Returns the CPU type for this RR.

=head2 os

    print "os = ", $rr->os, "\n";

Returns the operating system type for this RR.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 
Portions Copyright (c) 2002-2004 Chris Reinhardt
Portions Copyright (c) 2007 NLnet Labs

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 1035 Section 3.3.2

=cut
