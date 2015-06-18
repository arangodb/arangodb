package IPC::Run3::ProfArrayBuffer;

$VERSION = 0.038;

=head1 NAME

IPC::Run3::ProfArrayBuffer - Store profile events in RAM in an array

=head1 SYNOPSIS

=head1 DESCRIPTION

=cut

use strict;

=head1 METHODS

=over

=item C<< IPC::Run3::ProfArrayBuffer->new() >>

=cut

sub new {
    my $class = ref $_[0] ? ref shift : shift;

    my $self = bless { @_ }, $class;

    $self->{Events} = [];

    return $self;
}

=item C<< $buffer->app_call(@events) >>

=item C<< $buffer->app_exit(@events) >>

=item C<< $buffer->run_exit(@events) >>

The three above methods push the given events onto the stack of recorded
events.

=cut

for my $subname ( qw(app_call app_exit run_exit) ) {
  no strict 'refs';
  *{$subname} = sub {
      push @{shift->{Events}}, [ $subname => @_ ];
  };
}

=item get_events

Returns a list of all the events.  Each event is an ARRAY reference
like:

   [ "app_call", 1.1, ... ];

=cut

sub get_events {
    my $self = shift;
    @{$self->{Events}};
}

=back

=head1 LIMITATIONS

=head1 COPYRIGHT

Copyright 2003, R. Barrie Slaymaker, Jr., All Rights Reserved

=head1 LICENSE

You may use this module under the terms of the BSD, Artistic, or GPL licenses,
any version.

=head1 AUTHOR

Barrie Slaymaker E<lt>barries@slaysys.comE<gt>

=cut

1;
