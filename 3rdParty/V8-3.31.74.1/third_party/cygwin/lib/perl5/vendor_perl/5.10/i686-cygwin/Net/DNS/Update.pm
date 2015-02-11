package Net::DNS::Update;
#
# $Id: Update.pm 517 2005-11-21 08:38:47Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw($VERSION @ISA);

use Net::DNS;

@ISA     = qw(Net::DNS::Packet);
$VERSION = (qw$LastChangedRevision: 517 $)[1];

=head1 NAME

Net::DNS::Update - Create a DNS update packet

=head1 SYNOPSIS

C<use Net::DNS::Update;>

=head1 DESCRIPTION

C<Net::DNS::Update> is a subclass of C<Net::DNS::Packet>,
to be used for making DNS dynamic updates.  Programmers
should refer to RFC 2136 for the semantics of dynamic updates.

WARNING:  This code is still under development.  Please use with
caution on production nameservers.

=head1 METHODS

=head2 new

    $packet = Net::DNS::Update->new;
    $packet = Net::DNS::Update->new('example.com');
    $packet = Net::DNS::Update->new('example.com', 'HS');

Returns a C<Net::DNS::Update> object suitable for performing a DNS
dynamic update.  Specifically, it creates a packet with the header
opcode set to UPDATE and the zone record type to SOA (per RFC 2136,
Section 2.3).

Programs must use the C<push> method to add RRs to the prerequisite,
update, and additional sections before performing the update.

Arguments are the zone name and the class.  If the zone is omitted,
the default domain will be taken from the resolver configuration.
If the class is omitted, it defaults to IN.

Future versions of C<Net::DNS> may provide a simpler interface
for making dynamic updates.

=cut

sub new {
	my ($package, $zone, $class) = @_;

	unless ($zone) {
		my $res = Net::DNS::Resolver->new;
		$zone = ($res->searchlist)[0];
		return unless $zone;
	}

	my $type  = 'SOA';
	$class  ||= 'IN';

	my $self = $package->SUPER::new($zone, $type, $class) || return;

	$self->header->opcode('UPDATE');
	$self->header->rd(0);

	$self->{'seen'} = {};


	return $self;
}


=head1 EXAMPLES

The first example below shows a complete program; subsequent examples
show only the creation of the update packet.

=head2 Add a new host

 #!/usr/bin/perl -w
 
 use Net::DNS;
 use strict;
 
 # Create the update packet.
 my $update = Net::DNS::Update->new('example.com');
 
 # Prerequisite is that no A records exist for the name.
 $update->push(pre => nxrrset('foo.example.com. A'));
 
 # Add two A records for the name.
 $update->push(update => rr_add('foo.example.com. 86400 A 192.168.1.2'));
 $update->push(update => rr_add('foo.example.com. 86400 A 172.16.3.4'));
 
 # Send the update to the zone's primary master.
 my $res = Net::DNS::Resolver->new;
 $res->nameservers('primary-master.example.com');
 
 my $reply = $res->send($update);
 
 # Did it work?
 if ($reply) {
     if ($reply->header->rcode eq 'NOERROR') {
         print "Update succeeded\n";
     } else {
         print 'Update failed: ', $reply->header->rcode, "\n";
     }
 } else {
     print 'Update failed: ', $res->errorstring, "\n";
 }

=head2 Add an MX record for a name that already exists

    my $update = Net::DNS::Update->new('example.com');
    $update->push(pre    => yxdomain('example.com'));
    $update->push(update => rr_add('example.com MX 10 mailhost.example.com'));

=head2 Add a TXT record for a name that doesn't exist

    my $update = Net::DNS::Update->new('example.com');
    $update->push(pre    => nxdomain('info.example.com'));
    $update->push(update => rr_add('info.example.com TXT "yabba dabba doo"'));

=head2 Delete all A records for a name

    my $update = Net::DNS::Update->new('example.com');
    $update->push(pre    => yxrrset('foo.example.com A'));
    $update->push(update => rr_del('foo.example.com A'));

=head2 Delete all RRs for a name

    my $update = Net::DNS::Update->new('example.com');
    $update->push(pre    => yxdomain('byebye.example.com'));
    $update->push(update => rr_del('byebye.example.com'));

=head2 Perform a signed update

    my $key_name = 'tsig-key';
    my $key      = 'awwLOtRfpGE+rRKF2+DEiw==';

    my $update = Net::DNS::Update->new('example.com');
    $update->push(update => rr_add('foo.example.com A 10.1.2.3'));
    $update->push(update => rr_add('bar.example.com A 10.4.5.6'));
    $update->sign_tsig($key_name, $key);

=head2 Another way to perform a signed update

    my $key_name = 'tsig-key';
    my $key      = 'awwLOtRfpGE+rRKF2+DEiw==';

    my $update = Net::DNS::Update->new('example.com');
    $update->push(update     => rr_add('foo.example.com A 10.1.2.3'));
    $update->push(update     => rr_add('bar.example.com A 10.4.5.6'));
    $update->push(additional => Net::DNS::RR->new("$key_name TSIG $key"));

=head2 Perform a signed update with a customized TSIG record

    my $key_name = 'tsig-key';
    my $key      = 'awwLOtRfpGE+rRKF2+DEiw==';

    my $tsig = Net::DNS::RR->new("$key_name TSIG $key");
    $tsig->fudge(60);

    my $update = Net::DNS::Update->new('example.com');
    $update->push(update     => rr_add('foo.example.com A 10.1.2.3'));
    $update->push(update     => rr_add('bar.example.com A 10.4.5.6'));
    $update->push(additional => $tsig);

=head1 BUGS

This code is still under development.  Please use with caution on
production nameservers.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Header>,
L<Net::DNS::Packet>, L<Net::DNS::Question>, L<Net::DNS::RR>, RFC 2136,
RFC 2845

=cut

1;
