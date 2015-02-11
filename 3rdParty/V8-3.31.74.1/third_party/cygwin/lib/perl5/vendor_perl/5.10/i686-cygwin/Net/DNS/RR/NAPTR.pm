package Net::DNS::RR::NAPTR;
#
# $Id: NAPTR.pm 583 2006-05-03 12:24:18Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 583 $)[1];




__PACKAGE__->set_rrsort_func("order",
			       sub {
				   my ($a,$b)=($Net::DNS::a,$Net::DNS::b);
				   $a->{'order'} <=> $b->{'order'}
				   ||
				       $a->{'preference'} <=> $b->{'preference'}
}
);


__PACKAGE__->set_rrsort_func("default_sort",
			       __PACKAGE__->get_rrsort_func("order")

    );

__PACKAGE__->set_rrsort_func("preference",
			       sub {
				   my ($a,$b)=($Net::DNS::a,$Net::DNS::b);
				   $a->{"preference"} <=> $b->{"preference"}
				   ||
				       $a->{"order"} <=> $b->{"order"}
}
);



sub new {
	my ($class, $self, $data, $offset) = @_;

	if ($self->{"rdlength"} > 0) {
		($self->{"order"} ) = unpack("\@$offset n", $$data);
		$offset += Net::DNS::INT16SZ();
		
		($self->{"preference"}) = unpack("\@$offset n", $$data);
		$offset += Net::DNS::INT16SZ();
		
		my ($len) = unpack("\@$offset C", $$data);
		++$offset;
		($self->{"flags"}) = unpack("\@$offset a$len", $$data);
		$offset += $len;
		
		$len = unpack("\@$offset C", $$data);
		++$offset;
		($self->{"service"}) = unpack("\@$offset a$len", $$data);
		$offset += $len;
		
		$len = unpack("\@$offset C", $$data);
		++$offset;
		($self->{"regexp"}) = unpack("\@$offset a$len", $$data);
		$offset += $len;
		
		($self->{"replacement"}) = Net::DNS::Packet::dn_expand($data, $offset);
	}
  
	return bless $self, $class;
}

sub new_from_string {
	my ($class, $self, $string) = @_;

	if ($string && $string =~ /^      (\d+)      \s+
				          (\d+)      \s+
				     ['"] (.*?) ['"] \s+
				     ['"] (.*?) ['"] \s+
				     ['"] (.*?) ['"] \s+
				          (\S+) $/x) {

		$self->{"order"}       = $1;
		$self->{"preference"}  = $2;
		$self->{"flags"}       = $3;
		$self->{"service"}     = $4;
		$self->{"regexp"}      = $5;
		$self->{"replacement"} = $6;
		$self->{"replacement"} =~ s/\.+$//;
	}

	return bless $self, $class;
}

sub rdatastr {
	my $self = shift;
	my $rdatastr;

	if (exists $self->{"order"}) {
		$rdatastr = $self->{"order"}       . ' '   .
		            $self->{"preference"}  . ' "'  .
		            $self->{"flags"}       . '" "' .
		            $self->{"service"}     . '" "' .
		            $self->{"regexp"}      . '" '  .
		            $self->{"replacement"} . '.';
	}
	else {
		$rdatastr = '';
	}

	return $rdatastr;
}

sub rr_rdata {
	my ($self, $packet, $offset) = @_;
	my $rdata = "";

	if (exists $self->{"order"}) {

		$rdata .= pack("n2", $self->{"order"}, $self->{"preference"});

		$rdata .= pack("C", length $self->{"flags"});
		$rdata .= $self->{"flags"};

		$rdata .= pack("C", length $self->{"service"});
		$rdata .= $self->{"service"};

		$rdata .= pack("C", length $self->{"regexp"});
		$rdata .= $self->{"regexp"};

		$rdata .= $self->_name2wire ($self->{"replacement"});

	      }

	return $rdata;
}

1;
__END__

=head1 NAME

Net::DNS::RR::NAPTR - DNS NAPTR resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS Naming Authority Pointer (NAPTR) resource records.

=head1 METHODS

=head2 order

    print "order = ", $rr->order, "\n";

Returns the order field.

=head2 preference

    print "preference = ", $rr->preference, "\n";

Returns the preference field.

=head2 flags

    print "flags = ", $rr->flags, "\n";

Returns the flags field.

=head2 service

    print "service = ", $rr->service, "\n";

Returns the service field.

=head2 regexp

    print "regexp = ", $rr->regexp, "\n";

Returns the regexp field.

=head2 replacement

    print "replacement = ", $rr->replacement, "\n";

Returns the replacement field.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.
Portions Copyright (c) 2005 Olaf Kolkman NLnet Labs.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

B<Net::DNS::RR::NAPTR> is based on code contributed by Ryan Moats.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 2168

=cut
