package Net::DNS::RR::PX;
#
# $Id: PX.pm 632 2007-03-12 13:24:21Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 632 $)[1];



# Highest preference sorted first.

__PACKAGE__->set_rrsort_func("preference",
			       sub {
				   my ($a,$b)=($Net::DNS::a,$Net::DNS::b);
				   $a->{'preference'} <=> $b->{'preference'}
}
);


__PACKAGE__->set_rrsort_func("default_sort",
			       __PACKAGE__->get_rrsort_func("preference")

    );




sub new {
	my ($class, $self, $data, $offset) = @_;

	if ($self->{"rdlength"} > 0) {
		($self->{"preference"}) = unpack("\@$offset n", $$data);
		$offset += Net::DNS::INT16SZ();

		($self->{"map822"},  $offset) = Net::DNS::Packet::dn_expand($data, $offset);
		($self->{"mapx400"}, $offset) = Net::DNS::Packet::dn_expand($data, $offset);
	}

	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;

	if ($string && ($string =~ /^(\d+)\s+(\S+)\s+(\S+)$/)) {
		$self->{"preference"} = $1;
		$self->{"map822"}     = $2;
		$self->{"mapx400"}    = $3;
		$self->{"map822"}     =~ s/\.+$//;;
		$self->{"mapx400"}    =~ s/\.+$//;;
	}

	return bless $self, $class;
}

sub rdatastr {
	my $self = shift;

	return $self->{"preference"}
	       ? "$self->{preference} $self->{map822}. $self->{mapx400}."
	       : '';
}

sub rr_rdata {
	my ($self, $packet, $offset) = @_;
	my $rdata = "";

	if (exists $self->{"preference"}) {
		$rdata .= pack("n", $self->{"preference"});

		$rdata .= $packet->dn_comp($self->{"map822"},
					    $offset + length $rdata);

		$rdata .= $packet->dn_comp($self->{"mapx400"},
					    $offset + length $rdata);
	}

	return $rdata;
}


sub _canonicalRdata {
	my ($self) = shift;
	my $rdata = "";

	if (exists $self->{"preference"}) {
		$rdata .= pack("n", $self->{"preference"});
		$rdata .= $self->_name2wire(lc($self->{"map822"}));
		$rdata .= $self->_name2wire(lc($self->{"mapx400"}));
	}

	return $rdata;
}


1;
__END__

=head1 NAME

Net::DNS::RR::PX - DNS PX resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS X.400 Mail Mapping Information (PX) resource records.

=head1 METHODS

=head2 preference

    print "preference = ", $rr->preference, "\n";

Returns the preference given to this RR.

=head2 map822

    print "map822 = ", $rr->map822, "\n";

Returns the RFC822 part of the RFC1327 mapping information.

=head2 mapx400

    print "mapx400 = ", $rr->mapx400, "\n";

Returns the X.400 part of the RFC1327 mapping information.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.
Portions Copyright (c) 2005 Olaf Kolkman NLnet Labs.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC822, RFC 1327, RFC 2163

=cut
