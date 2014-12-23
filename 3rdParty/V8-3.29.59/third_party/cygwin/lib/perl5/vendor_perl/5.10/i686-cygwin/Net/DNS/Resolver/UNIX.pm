package Net::DNS::Resolver::UNIX;
#
# $Id: UNIX.pm 482 2005-09-02 13:34:33Z olaf $
#

use strict;
use vars qw(@ISA $VERSION);

use Net::DNS::Resolver::Base ();

@ISA     = qw(Net::DNS::Resolver::Base);
$VERSION = (qw$LastChangedRevision: 482 $)[1];

my $resolv_conf = '/etc/resolv.conf';
my $dotfile     = '.resolv.conf';

my @config_path;
push(@config_path, $ENV{'HOME'}) if exists $ENV{'HOME'};
push(@config_path, '.');

sub init {
	my ($class) = @_;
	
	$class->read_config_file($resolv_conf) if -f $resolv_conf && -r _; 
	
	foreach my $dir (@config_path) {
		my $file = "$dir/$dotfile";
		$class->read_config_file($file) if -f $file && -r _ && -o _;
	}
	
	$class->read_env;
	
	my $defaults = $class->defaults;
	
	if (!$defaults->{'domain'} && @{$defaults->{'searchlist'}}) {
		$defaults->{'domain'} = $defaults->{'searchlist'}[0];
	} elsif (!@{$defaults->{'searchlist'}} && $defaults->{'domain'}) {
		$defaults->{'searchlist'} = [ $defaults->{'domain'} ];
	}
}
	
1;
__END__


=head1 NAME

Net::DNS::Resolver::UNIX - UNIX Resolver Class

=head1 SYNOPSIS

 use Net::DNS::Resolver;

=head1 DESCRIPTION

This class implements the UNIX specific portions of C<Net::DNS::Resolver>.

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
