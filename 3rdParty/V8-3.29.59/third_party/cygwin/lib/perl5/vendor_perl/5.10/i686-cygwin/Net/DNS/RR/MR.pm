package Net::DNS::RR::MR;
#
# $Id: MR.pm 632 2007-03-12 13:24:21Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 632 $)[1];

sub new {
	my ($class, $self, $data, $offset) = @_;

	if ($self->{"rdlength"} > 0) {
		($self->{"newname"}) = Net::DNS::Packet::dn_expand($data, $offset);
	}

	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;

	if ($string) {
		$string =~ s/\.+$//;
		$self->{"newname"} = $string;
	}

	return bless $self, $class;
}

sub rdatastr {
	my $self = shift;

	return $self->{"newname"} ? "$self->{newname}." : '';
}

sub rr_rdata {
	my ($self, $packet, $offset) = @_;
	my $rdata = "";

	if (exists $self->{"newname"}) {
		$rdata .= $packet->dn_comp($self->{"newname"}, $offset);
	}

	return $rdata;
}

sub _canonicalRdata {
    my $self=shift;
    my $rdata = "";
    if (exists $self->{"newname"}) {
		$rdata .= $self->_name2wire(lc($self->{"newname"}));
	}
	return $rdata;
}



1;
__END__

=head1 NAME

Net::DNS::RR::MR - DNS MR resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS Mail Rename (MR) resource records.

=head1 METHODS

=head2 newname

    print "newname = ", $rr->newname, "\n";

Returns the RR's new name field.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 1035 Section 3.3.8

=cut
