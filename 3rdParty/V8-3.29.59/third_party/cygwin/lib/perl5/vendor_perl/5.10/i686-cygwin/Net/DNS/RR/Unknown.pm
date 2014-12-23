package Net::DNS::RR::Unknown;
#
# $Id: Unknown.pm 388 2005-06-22 10:06:05Z olaf $
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
	
	my $length = $self->{'rdlength'};
	
	if ($length > 0) {
		$self->{'rdata'}    = substr($$data, $offset,$length);
		$self->{'rdatastr'} = "\\# $length " . unpack('H*',  $self->{'rdata'});
	}
	
	return bless $self, $class;
}


sub rdatastr {
	my $self = shift;
	
	if (exists $self->{'rdatastr'}) {
		return $self->{'rdatastr'};
	} else {
		if (exists $self->{"rdata"}){
			my $data= $self->{'rdata'};
			
			return  "\\# ". length($data) . "  " . unpack('H*',  $data);
		}
	}
	
	return "#NO DATA";
}


# sub rr_rdata is inherited from RR.pm. Note that $self->{'rdata'}
# should always be defined



1;
__END__

=head1 NAME

Net::DNS::RR::Unknown - Unknown RR record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for dealing with unknown RR types (RFC3597)

=head1 METHODS

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

Portions Copyright (c) 2003  Olaf M. Kolkman, RIPE NCC.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<Net::DNS>, L<Net::DNS::RR>, RFC 3597

=cut
