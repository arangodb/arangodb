package Net::HTTP::NB;

use strict;
use vars qw($VERSION @ISA);

$VERSION = "5.810";

require Net::HTTP;
@ISA=qw(Net::HTTP);

sub sysread {
    my $self = $_[0];
    if (${*$self}{'httpnb_read_count'}++) {
	${*$self}{'http_buf'} = ${*$self}{'httpnb_save'};
	die "Multi-read\n";
    }
    my $buf;
    my $offset = $_[3] || 0;
    my $n = sysread($self, $_[1], $_[2], $offset);
    ${*$self}{'httpnb_save'} .= substr($_[1], $offset);
    return $n;
}

sub read_response_headers {
    my $self = shift;
    ${*$self}{'httpnb_read_count'} = 0;
    ${*$self}{'httpnb_save'} = ${*$self}{'http_buf'};
    my @h = eval { $self->SUPER::read_response_headers(@_) };
    if ($@) {
	return if $@ eq "Multi-read\n";
	die;
    }
    return @h;
}

sub read_entity_body {
    my $self = shift;
    ${*$self}{'httpnb_read_count'} = 0;
    ${*$self}{'httpnb_save'} = ${*$self}{'http_buf'};
    # XXX I'm not so sure this does the correct thing in case of
    # transfer-encoding tranforms
    my $n = eval { $self->SUPER::read_entity_body(@_); };
    if ($@) {
	$_[0] = "";
	return -1;
    }
    return $n;
}

1;

__END__

=head1 NAME

Net::HTTP::NB - Non-blocking HTTP client

=head1 SYNOPSIS

 use Net::HTTP::NB;
 my $s = Net::HTTP::NB->new(Host => "www.perl.com") || die $@;
 $s->write_request(GET => "/");

 use IO::Select;
 my $sel = IO::Select->new($s);

 READ_HEADER: {
    die "Header timeout" unless $sel->can_read(10);
    my($code, $mess, %h) = $s->read_response_headers;
    redo READ_HEADER unless $code;
 }

 while (1) {
    die "Body timeout" unless $sel->can_read(10);
    my $buf;
    my $n = $s->read_entity_body($buf, 1024);
    last unless $n;
    print $buf;
 }

=head1 DESCRIPTION

Same interface as C<Net::HTTP> but it will never try multiple reads
when the read_response_headers() or read_entity_body() methods are
invoked.  This make it possible to multiplex multiple Net::HTTP::NB
using select without risk blocking.

If read_response_headers() did not see enough data to complete the
headers an empty list is returned.

If read_entity_body() did not see new entity data in its read
the value -1 is returned.

=head1 SEE ALSO

L<Net::HTTP>

=head1 COPYRIGHT

Copyright 2001 Gisle Aas.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
