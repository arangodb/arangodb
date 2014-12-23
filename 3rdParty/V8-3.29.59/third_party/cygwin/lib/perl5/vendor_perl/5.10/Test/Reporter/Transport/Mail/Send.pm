use strict;
use warnings;
package Test::Reporter::Transport::Mail::Send;
use base 'Test::Reporter::Transport';
use vars qw/$VERSION/;
$VERSION = '1.4002';
$VERSION = eval $VERSION;

use Mail::Send;

sub new {
    my ($class, @args) = @_;
    bless { args => \@args } => $class;
}

sub send {
    my ($self, $report, $recipients) = @_;
    $recipients ||= [];

    my $via = $report->via();
    my $msg = Mail::Send->new();

    my $cc_str;
    if (@$recipients) {
        $cc_str = join ', ', @$recipients;
        chomp $recipients;
        chomp $recipients;
    }

    $via = ', via ' . $via if $via;

    $msg->to($report->address());
    $msg->set('From', $report->from());
    $msg->subject($report->subject());
    $msg->add('X-Reported-Via', "Test::Reporter $Test::Reporter::VERSION$via");
    $msg->add('Cc', $recipients) if @_;

    my $fh = $msg->open( @{ $self->{args} } );

    print $fh $self->report();
    
    $fh->close();
}

1;

__END__

=head1 NAME

Test::Reporter::Transport::Mail::Send - Mail::Send transport for Test::Reporter

=head1 SYNOPSIS

    my $report = Test::Reporter->new(
        transport => 'Mail::Send',
        transport_args => [ @mail_send_args ],
    );

=head1 DESCRIPTION

This module transmits a Test::Reporter report using Mail::Send.

=head1 USAGE

See L<Test::Reporter> and L<Test::Reporter::Transport> for general usage
information.

=head2 Transport Arguments

    $report->transport_args( @mail_send_args );

Any arguments supplied are passed to the Mail::Send constructor.

=head1 METHODS

These methods are only for internal use by Test::Reporter.

=head2 new

    my $sender = Test::Reporter::Transport::Mail::Send->new( 
        @args 
    );
    
The C<new> method is the object constructor.   

=head2 send

    $sender->send( $report );

The C<send> method transmits the report.  

=head1 AUTHOR

=over

=item *

David A. Golden (DAGOLDEN)

=item *

Ricardo Signes (RJBS)

=back

=head1 COPYRIGHT

 Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007, 2008 Adam J. Foxson.
 Copyright (C) 2004, 2005 Richard Soderberg.
 Copyright (C) 2008 David A. Golden
 Copyright (C) 2008 Ricardo Signes

 All rights reserved.

=head1 LICENSE

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=cut
