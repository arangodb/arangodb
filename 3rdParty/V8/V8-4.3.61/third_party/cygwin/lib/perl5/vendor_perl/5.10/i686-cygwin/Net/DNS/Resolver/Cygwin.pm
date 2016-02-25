package Net::DNS::Resolver::Cygwin;
#
# $Id: Cygwin.pm 696 2007-12-28 13:46:20Z olaf $
#

use strict;
use vars qw(@ISA $VERSION);

use Net::DNS::Resolver::Base ();

@ISA	 = qw(Net::DNS::Resolver::Base);
$VERSION = (qw$LastChangedRevision: 696 $)[1];

sub getregkey {
	my $key	  = $_[0] . $_[1];
	my $value = '';

	local *LM;	  

	if (open(LM, "<$key")) {
		$value = <LM>;
		$value =~ s/\0+$//;
		close(LM);
	}
	
	return $value;
}

sub init {
	my ($class) = @_;
	my $defaults = $class->defaults;
	
	local *LM;
	
	my $root = '/proc/registry/HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet/Services/Tcpip/Parameters/';
   
	unless (-d $root) {
		# Doesn't exist, maybe we are on 95/98/Me?
		$root = '/proc/registry/HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet/Services/VxD/MSTCP/';
		-d $root || Carp::croak "can't read registry: $!";
	}

	# Best effort to find a useful domain name for the current host
	# if domain ends up blank, we're probably (?) not connected anywhere
	# a DNS server is interesting either...
	my $domain = getregkey($root, 'Domain') || getregkey($root, 'DhcpDomain') || '';
	
	# If nothing else, the searchlist should probably contain our own domain
	# also see below for domain name devolution if so configured
	# (also remove any duplicates later)
	my $searchlist = "$domain ";
	$searchlist	 .= getregkey($root, 'SearchList');
	
	# This is (probably) adequate on NT4
	my $nt4nameservers = getregkey($root, 'NameServer') || getregkey($root, 'DhcpNameServer');
	my $nameservers = "";
	#
	# but on W2K/XP the registry layout is more advanced due to dynamically
	# appearing connections. So we attempt to handle them, too...
	# opt to silently fail if something isn't ok (maybe we're on NT4)
    # If this doesn't fail override any NT4 style result we found, as it
    # may be there but is not valid.
	# drop any duplicates later
	my $dnsadapters = $root . "DNSRegisteredAdapters/";
	if (opendir(LM, $dnsadapters)) {
		my @adapters = grep($_ ne "." && $_ ne "..", readdir(LM));
		closedir(LM);
		foreach my $adapter (@adapters) {
			my $regadapter = $dnsadapters . $adapter . '/';
			if (-e $regadapter) {
				my $ns = getregkey($regadapter, 'DNSServerAddresses') || '';
				while (length($ns) >= 4) {
					my $addr = join('.', unpack("C4", substr($ns,0,4,"")));
					$nameservers .= " $addr";
				}
			}
		}
	}

	my $interfaces = $root . "Interfaces/";
	if (opendir(LM, $interfaces)) {
		my @ifacelist = grep($_ ne "." && $_ ne "..", readdir(LM));
		closedir(LM);
		foreach my $iface (@ifacelist) {
			my $regiface = $interfaces . $iface . '/';
			if (opendir(LM, $regiface)) {
				closedir(LM);

				my $ns;
				my $ip;
				$ip = getregkey($regiface, "DhcpIPAddress") || getregkey($regiface, "IPAddress");
				$ns = getregkey($regiface, "NameServer") ||
				    getregkey($regiface, "DhcpNameServer") || ''				    unless !$ip || ($ip =~ /0\.0\.0\.0/);
				
				$nameservers .= " $ns" if $ns;
			    }
		    }
	    }
	
	if (!$nameservers) {
		$nameservers = $nt4nameservers;
	}

	if ($domain) {
		$defaults->{'domain'} = $domain;
	}

	my $usedevolution = getregkey($root, 'UseDomainNameDevolution');
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
		# just in case dups were introduced...
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

Net::DNS::Resolver::Cygwin - Cygwin Resolver Class

=head1 SYNOPSIS

 use Net::DNS::Resolver;

=head1 DESCRIPTION

This class implements the cygwin specific portions of C<Net::DNS::Resolver>.

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
