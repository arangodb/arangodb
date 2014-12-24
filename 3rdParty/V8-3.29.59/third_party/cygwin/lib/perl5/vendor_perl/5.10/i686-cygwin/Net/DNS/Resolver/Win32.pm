package Net::DNS::Resolver::Win32;
#
# $Id: Win32.pm 588 2006-05-22 20:28:00Z olaf $
#

use strict;
use vars qw(@ISA $VERSION);

use Net::DNS::Resolver::Base ();

@ISA     = qw(Net::DNS::Resolver::Base);
$VERSION = (qw$LastChangedRevision: 588 $)[1];

use Win32::Registry;

sub init {
  
        my $debug=0;
	my ($class) = @_;
	
	my $defaults = $class->defaults;
	
	my ($resobj, %keys);

	my $root = 'SYSTEM\CurrentControlSet\Services\Tcpip\Parameters';

	unless ($main::HKEY_LOCAL_MACHINE->Open($root, $resobj)) {
		# Didn't work, maybe we are on 95/98/Me?
		$root = 'SYSTEM\CurrentControlSet\Services\VxD\MSTCP';
		$main::HKEY_LOCAL_MACHINE->Open($root, $resobj)
			or Carp::croak "can't read registry: $!";
	}

	$resobj->GetValues(\%keys)
		or Carp::croak "can't read registry values: $!";

	# Best effort to find a useful domain name for the current host
	# if domain ends up blank, we're probably (?) not connected anywhere
	# a DNS server is interesting either...
	my $domain = $keys{'Domain'}->[2] || $keys{'DhcpDomain'}->[2] || '';
	
	# If nothing else, the searchlist should probably contain our own domain
	# also see below for domain name devolution if so configured
	# (also remove any duplicates later)
	my $searchlist = "$domain ";
	$searchlist  .= $keys{'SearchList'}->[2];
	
	# This is (probably) adequate on NT4
	# NameServer overrides DhcpNameServer if both exist
	my $nt4nameservers = $keys{'NameServer'}->[2] || $keys{'DhcpNameServer'}->[2];
	my $nameservers = "";



#
#
#  This code is agued to be broken see ticket rt.cpan.org ticket 11931
#  There seems to be sufficient reason to remove this code
#
#  For details see https://rt.cpan.org/Ticket/Display.html?id=11931
#
#
#	#
#	# but on W2K/XP the registry layout is more advanced due to dynamically
#	# appearing connections. So we attempt to handle them, too...
#	# opt to silently fail if something isn't ok (maybe we're on NT4)
#	# drop any duplicates later, but must ignore NT4 style entries if in 2K/XP
#	my $dnsadapters;
#	$resobj->Open("DNSRegisteredAdapters", $dnsadapters);
#	if ($dnsadapters) {
#		my @adapters;
#		$dnsadapters->GetKeys(\@adapters);
#		foreach my $adapter (@adapters) {
#			my $regadapter;
#			$dnsadapters->Open($adapter, $regadapter);
#			if ($regadapter) {
#				my($type,$ns);
#				$regadapter->QueryValueEx("DNSServerAddresses", $type, $ns);
#				while (length($ns) >= 4) {
#					my $addr = join('.', unpack("C4", substr($ns,0,4,"")));
#					$nameservers .= " $addr";
#				}
#			}
#		}
#	}




  # This code was introduced by Hanno Stock, see ticket 1193 dd May 19 2006
  # 
  # it should work on Win2K and XP and looks for the DNS services
  # using the BIND key
  # 

  my $bind_linkage;
  my @sorted_interfaces;
	print ";; DNS: Getting sorted interface list\n" if $debug;
  $main::HKEY_LOCAL_MACHINE->Open('SYSTEM\CurrentControlSet\Services\Tcpip\Linkage',
   $bind_linkage);
	if($bind_linkage){
	  my $bind_linkage_list;
	  my $type;
	  $bind_linkage->QueryValueEx('Bind', $type, $bind_linkage_list);
	  if($bind_linkage_list){
	    @sorted_interfaces = split(m/[^\w{}\\-]+/s, $bind_linkage_list);
	  }
	  foreach my $interface (@sorted_interfaces){
	    $interface =~ s/^\\device\\//i;
	    print ";; DNS:Interface: $interface\n" if $debug;
	  }
	}


	my $interfaces;
	$resobj->Open("Interfaces", $interfaces);
	if ($interfaces) {
	  my @ifacelist;
	  if(@sorted_interfaces){
	    @ifacelist = @sorted_interfaces;
	  }else{
	    $interfaces->GetKeys(\@ifacelist);
	  }
	  foreach my $iface (@ifacelist) {
		my $regiface;
		$interfaces->Open($iface, $regiface);
		
		if ($regiface) {
		    my $ns;
		    my $type;
		    my $ip;
		    my $ipdhcp;
		    $regiface->QueryValueEx("IPAddress", $type, $ip);
		    $regiface->QueryValueEx("DhcpIPAddress", $type, $ipdhcp);
		    if (($ip && !($ip =~ /0\.0\.0\.0/)) || ($ipdhcp && !($ipdhcp =~ /0\.0
\.0\.0/))) {
			# NameServer overrides DhcpNameServer if both exist
			$regiface->QueryValueEx("NameServer", $type, $ns);
			$regiface->QueryValueEx("DhcpNameServer", $type, $ns) unless $ns;
			$nameservers .= " $ns" if $ns;
		    }
		}
	    }
	}
	if (!$nameservers) {
	    $nameservers = $nt4nameservers;
	}

	if ($domain) {
		$defaults->{'domain'} = $domain;
	}

	my $usedevolution = $keys{'UseDomainNameDevolution'}->[2];
	if ($searchlist) {
		# fix devolution if configured, and simultaneously make sure no dups (but keep the order)
		my @a;
		my %h;
		foreach my $entry (split(m/[\s,]+/, $searchlist)) {
			push(@a, $entry) unless $h{$entry};
			$h{$entry} = 1;
			if ($usedevolution) {
				# as long there's more than two pieces, cut
				while ($entry =~ m#\..+\.#) {
					$entry =~ s#^[^\.]+\.(.+)$#$1#;
					push(@a, $entry) unless $h{$entry};
					$h{$entry} = 1;
					}
				}
			}
		$defaults->{'searchlist'} = \@a;
	}

	if ($nameservers) {
		# remove blanks and dupes
		my @a;
		my %h;
		foreach my $ns (split(m/[\s,]+/, $nameservers)) {
			push @a, $ns unless (!$ns || $h{$ns});
			$h{$ns} = 1;
		}
		$defaults->{'nameservers'} = [map { m/(.*)/ } @a];
	}

	$class->read_env;

	if (!$defaults->{'domain'} && @{$defaults->{'searchlist'}}) {
		$defaults->{'domain'} = $defaults->{'searchlist'}[0];
	} elsif (!@{$defaults->{'searchlist'}} && $defaults->{'domain'}) {
		$defaults->{'searchlist'} = [ $defaults->{'domain'} ];
	}
}

1;
__END__


=head1 NAME

Net::DNS::Resolver::Win32 - Windows Resolver Class

=head1 SYNOPSIS

 use Net::DNS::Resolver;

=head1 DESCRIPTION

This class implements the windows specific portions of C<Net::DNS::Resolver>.

No user serviceable parts inside, see L<Net::DNS::Resolver|Net::DNS::Resolver>
for all your resolving needs.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>

=cut
