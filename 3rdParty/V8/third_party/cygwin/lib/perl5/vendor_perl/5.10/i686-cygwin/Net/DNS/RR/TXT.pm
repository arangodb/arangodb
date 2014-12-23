package Net::DNS::RR::TXT;
#
# $Id: TXT.pm 582 2006-04-25 07:12:19Z olaf $
#
use strict;
BEGIN { 
    eval { require bytes; }
} 
use vars qw(@ISA $VERSION);

use Text::ParseWords;

@ISA     = qw(Net::DNS::RR);
$VERSION = (qw$LastChangedRevision: 582 $)[1];

sub new {
	my ($class, $self, $data, $offset) = @_;
	
	my $rdlength = $self->{'rdlength'} or return bless $self, $class;
	my $end      = $offset + $rdlength;
	
	while ($offset < $end) {
		my $strlen = unpack("\@$offset C", $$data);
		++$offset;
		
		my $char_str = substr($$data, $offset, $strlen);
		$offset += $strlen;
		
		push(@{$self->{'char_str_list'}}, $char_str);
	}

	return bless $self, $class;
}

sub new_from_string {
    my ( $class, $self, $rdata_string ) = @_ ;
    
    bless $self, $class;
        
    $self->_build_char_str_list($rdata_string);

    return $self;
}

sub txtdata {
	my $self = shift;
	return join(' ',  $self->char_str_list());
}

sub rdatastr {
	my $self = shift;
		
	if ($self->char_str_list) {
		return join(' ', map { 
			my $str = $_;  
			$str =~ s/"/\\"/g;  
			qq("$str");
		} @{$self->{'char_str_list'}});
	} 
	
	return '';
}

sub _build_char_str_list {
	my ($self, $rdata_string) = @_;
	
	my @words;

	@words= shellwords($rdata_string) if $rdata_string;

	$self->{'char_str_list'} = [];

	if (@words) {
		foreach my $string (@words) {
		    $string =~ s/\\"/"/g;
		    push(@{$self->{'char_str_list'}}, $string);
		}
	}
}

sub char_str_list {
	my $self = shift;
	
	if (not $self->{'char_str_list'}) {
		$self->_build_char_str_list( $self->{'txtdata'} );
	}

	return @{$self->{'char_str_list'}}; # unquoted strings
}

sub rr_rdata {
	my $self  = shift;
	my $rdata = '';

	foreach my $string ($self->char_str_list) {
	    $rdata .= pack("C", length $string );
	    $rdata .= $string;
	}

	return $rdata;
}

1;
__END__

=head1 NAME

Net::DNS::RR::TXT - DNS TXT resource record

=head1 SYNOPSIS

C<use Net::DNS::RR>;

=head1 DESCRIPTION

Class for DNS Text (TXT) resource records.

=head1 METHODS

=head2 txtdata

    print "txtdata = ", $rr->txtdata, "\n";

Returns the descriptive text as a single string, regardless of actual 
number of <character-string> elements.  Of questionable value.  Should 
be deprecated.  

Use C<< $txt->rdatastr() >> or C<< $txt->char_str_list() >> instead.


=head2 char_str_list

 print "Individual <character-string> list: \n\t", 
       join("\n\t", $rr->char_str_list());

Returns a list of the individual <character-string> elements, 
as unquoted strings.  Used by TXT->rdatastr and TXT->rr_rdata.


=head1 FEATURES

The RR.pm module accepts semi-colons as a start of a comment. This is
to allow the RR.pm to deal with RFC1035 specified zonefile format.

For some applications of the TXT RR the semicolon is relevant, you
will need to escape it on input.

Also note that you should specify the several character strings
separately. The easiest way to do so is to include the whole argument
in single quotes and the several character strings in double
quotes. Double quotes inside the character strings will need to be
escaped.

my $TXTrr=Net::DNS::RR->new('txt2.t.net-dns.org.	60	IN
	TXT  "Test1 \" \; more stuff"  "Test2"');

would result in 
$TXTrr->char_str_list())[0] containing 'Test1 " ; more stuff'
and
$TXTrr->char_str_list())[1] containing 'Test2'


=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.
Portions Copyright (c) 2005 Olaf Kolkman (NLnet Labs)

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Header>, L<Net::DNS::Question>, L<Net::DNS::RR>,
RFC 1035 Section 3.3.14

=cut
