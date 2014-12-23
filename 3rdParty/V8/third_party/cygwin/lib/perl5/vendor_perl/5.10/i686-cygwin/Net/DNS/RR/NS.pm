#
# $Id: NS.pm 707 2008-02-06 22:27:28Z olaf $
#
package Net::DNS::RR::NS;

use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 707 $)[1];

sub new {
	my ($class, $self, $data, $offset) = @_;

	if ($self->{"rdlength"} > 0) {
		($self->{"nsdname"}) = Net::DNS::Packet::dn_expand($data, $offset);
	}

	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;

	if ($string) {
		$string =~ s/\.+$//;
		$self->{"nsdname"} = $string;
	}

	return bless $self, $class;
}

sub rdatastr {
	my $self = shift;

	return $self->{"nsdname"} ? "$self->{nsdname}." : '';
}

sub rr_rdata {
	my ($self, $packet, $offset) = @_;
	my $rdata = "";

	if (exists $self->{"nsdname"}) {
		$rdata .= $packet->dn_comp($self->{"nsdname"}, $offset);
	}

	return $rdata;
}



sub _canonicalRdata {
    # rdata contains a compressed domainname... we should not have that.
	my ($self) = @_;
	my $rdata;
	$rdata= $self->_name2wire(lc($self->{"nsdname"}));
	return $rdata;
}


1;
__END__

=head1 NAME

Net::DNS::RR::NS - DNS NS resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS Name Server (NS) resource records.

=head1 METHODS

=head2 nsdname

    print "nsdname = ", $rr->nsdname, "\n";

Returns the name of the nameserver.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

Portions Copyright (c) 2005 O.M, Kolkman, RIPE NCC.

Portions Copyright (c) 2005-2006 O.M, Kolkman, NLnet Labs.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 1035 Section 3.3.11

=cut
