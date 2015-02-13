package Net::DNS::RR::X25;
#
# $Id: X25.pm 388 2005-06-22 10:06:05Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 388 $)[1];

sub new {
	my ($class, $self, $data, $offset) = @_;

	if ($self->{"rdlength"} > 0) {
		my ($len) = unpack("\@$offset C", $$data);
		++$offset;
		$self->{"psdn"} = substr($$data, $offset, $len);
		$offset += $len;
	}

	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;

	if ($string && $string =~ /^\s*["']?(.*?)["']?\s*$/) {
		$self->{"psdn"} = $1;
	}

	return bless $self, $class;
}

sub rdatastr {
	my $self = shift;

	return exists $self->{"psdn"}
	       ? qq("$self->{psdn}")
	       : '';
}

sub rr_rdata {
	my $self = shift;
	my $rdata = "";

	if (exists $self->{"psdn"}) {
		$rdata .= pack("C", length $self->{"psdn"});
		$rdata .= $self->{"psdn"};
	}

	return $rdata;
}

1;
__END__

=head1 NAME

Net::DNS::RR::X25 - DNS X25 resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS X25 resource records.

=head1 METHODS

=head2 psdn

    print "psdn = ", $rr->psdn, "\n";

Returns the PSDN address.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 1183 Section 3.1

=cut
