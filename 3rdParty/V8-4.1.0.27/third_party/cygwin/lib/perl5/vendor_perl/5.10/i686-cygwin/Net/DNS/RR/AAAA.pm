package Net::DNS::RR::AAAA;
#
# $Id: AAAA.pm 388 2005-06-22 10:06:05Z olaf $
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
		my @addr = unpack("\@$offset n8", $$data);
		$self->{"address"} = sprintf("%x:%x:%x:%x:%x:%x:%x:%x", @addr);
	}
	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;

	if ($string) {
		my @addr;

		# I think this is correct, per RFC 1884 Sections 2.2 & 2.4.4.
		if ($string =~ /^(.*):(\d+)\.(\d+)\.(\d+)\.(\d+)$/) {
			my ($front, $a, $b, $c, $d) = ($1, $2, $3, $4, $5);
			$string = $front . sprintf(":%x:%x",
						   ($a << 8 | $b),
						   ($c << 8 | $d));
		}
			
		if ($string =~ /^(.*)::(.*)$/) {
			my ($front, $back) = ($1, $2);
			my @front = split(/:/, $front);
			my @back  = split(/:/, $back);
			my $fill = 8 - (@front ? $#front + 1 : 0)
				     - (@back  ? $#back  + 1 : 0);
			my @middle = (0) x $fill;
			@addr = (@front, @middle, @back);
		}
		else {
			@addr = split(/:/, $string);
			if (@addr < 8) {
				@addr = ((0) x (8 - @addr), @addr);
			}
		}

		$self->{"address"} = sprintf("%x:%x:%x:%x:%x:%x:%x:%x",
					     map { hex $_ } @addr);
	}

	return bless $self, $class;
}

sub rdatastr {
	my $self = shift;

	return $self->{"address"} || '';
}

sub rr_rdata {
	my $self = shift;
	my $rdata = "";

	if (exists $self->{"address"}) {
		my @addr = split(/:/, $self->{"address"});
		$rdata .= pack("n8", map { hex $_ } @addr);
	}

	return $rdata;
}

1;
__END__

=head1 NAME

Net::DNS::RR::AAAA - DNS AAAA resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS IPv6 Address (AAAA) resource records.

=head1 METHODS

=head2 address

    print "address = ", $rr->address, "\n";

Returns the RR's address field.

=head1 BUGS

The C<string> method returns only the preferred method of address
representation ("x:x:x:x:x:x:x:x", as documented in RFC 1884,
Section 2.2, Para 1).

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 1886 Section 2, RFC 1884 Sections 2.2 & 2.4.4

=cut
