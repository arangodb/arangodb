package Net::DNS::RR::SPF;
#
# $Id: SPF.pm 684 2007-10-10 12:32:22Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);
use Net::DNS::RR::TXT;


@ISA     = qw(Net::DNS::RR::TXT);
$VERSION = (qw$LastChangedRevision: 684 $)[1];

1;

=head1 NAME

Net::DNS::RR::SPF - DNS SPF resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

This is a clone of the TXT record. This class therfore completely inherits
all properties of the Net::DNS::RR::TXT class.

Please see the L<Net::DNS::RR::TXT> perldocumentation for details

=head1 COPYRIGHT

Copyright (c) 2005 Olaf Kolkman (NLnet Labs)

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 1035 Section 3.3.14, RFC 4408


=cut

