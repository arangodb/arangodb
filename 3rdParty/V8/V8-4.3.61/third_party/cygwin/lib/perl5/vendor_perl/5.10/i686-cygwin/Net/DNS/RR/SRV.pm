package Net::DNS::RR::SRV;
#
# $Id: SRV.pm 583 2006-05-03 12:24:18Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 583 $)[1];



__PACKAGE__->set_rrsort_func("priority",
			       sub {
				   my ($a,$b)=($Net::DNS::a,$Net::DNS::b);
				   $a->{'priority'} <=> $b->{'priority'}
				   ||
				       $b->{'weight'} <=> $a->{'weight'}
}
);


__PACKAGE__->set_rrsort_func("default_sort",
			       __PACKAGE__->get_rrsort_func("priority")

    );

__PACKAGE__->set_rrsort_func("weight",
			       sub {
				   my ($a,$b)=($Net::DNS::a,$Net::DNS::b);
				   $b->{"weight"} <=> $a->{"weight"}
				   ||
				       $a->{"priority"} <=> $b->{"priority"}
}
);



sub new {
	my ($class, $self, $data, $offset) = @_;

	if ($self->{'rdlength'} > 0) {
		@{$self}{qw(priority weight port)} = unpack("\@$offset n3", $$data);
		$offset += 3 * Net::DNS::INT16SZ();
		
		($self->{'target'}) = Net::DNS::Packet::dn_expand($data, $offset);
	}

	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;

	if ($string && ($string =~ /^(\d+)\s+(\d+)\s+(\d+)\s+(\S+)$/)) {
		@{$self}{qw(priority weight port target)} = ($1, $2, $3, $4);

		$self->{'target'} =~ s/\.+$//;
	}

	return bless $self, $class;
}

sub rdatastr {
	my $self = shift;
	my $rdatastr;

	if (exists $self->{'priority'}) {
		$rdatastr = join(' ', @{$self}{qw(priority weight port target)});
		$rdatastr =~ s/(.*[^\.])$/$1./;
	} else {
		$rdatastr = '';
	}

	return $rdatastr;
}

sub rr_rdata {
	my ($self, $packet, $offset) = @_;
	my $rdata = '';

	if (exists $self->{'priority'}) {
		$rdata .= pack('n3', @{$self}{qw(priority weight port)});
		$rdata .=  $self->_name2wire ($self->{"target"});

	      }
	
	return $rdata;
}


1;
__END__

=head1 NAME

Net::DNS::RR::SRV - DNS SRV resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS Service (SRV) resource records.

=head1 METHODS

=head2 priority

    print "priority = ", $rr->priority, "\n";

Returns the priority for this target host.

=head2 weight

    print "weight = ", $rr->weight, "\n";

Returns the weight for this target host.

=head2 port

    print "port = ", $rr->port, "\n";

Returns the port on this target host for the service.

=head2 target

    print "target = ", $rr->target, "\n";

Returns the target host.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 2782

=cut
