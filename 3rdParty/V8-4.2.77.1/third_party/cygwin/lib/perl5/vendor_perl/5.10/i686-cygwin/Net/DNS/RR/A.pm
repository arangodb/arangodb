package Net::DNS::RR::A;
#
# $Id: A.pm 546 2005-12-16 15:23:03Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 


use vars qw(@ISA $VERSION);

use Socket;

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 546 $)[1];

sub new {
	my ($class, $self, $data, $offset) = @_;

	if ($self->{"rdlength"} > 0) {
		$self->{"address"} = inet_ntoa(substr($$data, $offset, 4));
	}

	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;

	if ($string && ($string =~ /^(\d+)\.(\d+)\.(\d+)\.(\d+)\s*$/o)
	    && ($1 >= 0) && ($1 <= 255)
	    && ($2 >= 0) && ($2 <= 255)
	    && ($3 >= 0) && ($3 <= 255)
	    && ($4 >= 0) && ($4 <= 255) ) {

		$self->{"address"} = "$1.$2.$3.$4";
	}

	return bless $self, $class;
}

sub rdatastr {
	my $self = shift;

	return $self->{"address"} || '';
}

sub rr_rdata {
	my $self = shift;

	return exists $self->{"address"}
			  ? inet_aton($self->{"address"})
			  : "";
}

1;
__END__

=head1 NAME

Net::DNS::RR::A - DNS A resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS Address (A) resource records.

=head1 METHODS

=head2 address

    print "address = ", $rr->address, "\n";

Returns the RR's address field.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 1035 Section 3.4.1

=cut
