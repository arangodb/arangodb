package Net::DNS::RR::RP;
#
# $Id: RP.pm 702 2008-01-21 10:01:21Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 702 $)[1];

sub new {
	my ($class, $self, $data, $offset) = @_;

	if ($self->{"rdlength"} > 0) {
		($self->{"mbox"},     $offset) = Net::DNS::Packet::dn_expand($data, $offset);
		($self->{"txtdname"}, $offset) = Net::DNS::Packet::dn_expand($data, $offset);
	}

	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;

	if ($string && ($string =~ /^(\S+)\s+(\S+)$/)) {
		$self->{"mbox"}     = $1;
		$self->{"txtdname"} = $2;
		$self->{"mbox"}     =~ s/\.+$//;
		$self->{"txtdname"} =~ s/\.+$//;
	}

	return bless $self, $class;
}

sub rdatastr {
	my $self = shift;
	if (exists $self->{"mbox"}) {
		return "$self->{mbox}. $self->{txtdname}.";
	}
	return '';
	
}

sub rr_rdata {
	my ($self, $packet, $offset) = @_;
	my $rdata = "";

	if (exists $self->{"mbox"}) {
		$rdata .= $packet->dn_comp($self->{"mbox"}, $offset);
		$rdata .= $packet->dn_comp($self->{"txtdname"},
					   $offset + length $rdata);
	}

	return $rdata;
}


sub _canonicalRdata {
    my $self  = shift;
    my $rdata = "";
	if (exists $self->{"mbox"}) {
		$rdata .= $self->_name2wire(lc($self->{"mbox"}));
		$rdata .= $self->_name2wire(lc($self->{"txtdname"}));


	}

    return $rdata;
}



1;
__END__

=head1 NAME

Net::DNS::RR::RP - DNS RP resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS Responsible Person (RP) resource records.

=head1 METHODS

=head2 mbox

    print "mbox = ", $rr->mbox, "\n";

Returns a domain name that specifies the mailbox for the responsible person.

=head2 txtdname

    print "txtdname = ", $rr->txtdname, "\n";

Returns a domain name that specifies a TXT record containing further
information about the responsible person.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 1183 Section 2.2

=cut
