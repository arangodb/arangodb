package Log::Message::Handlers;
use strict;

=pod

=head1 NAME

Log::Message::Handlers - Message handlers for Log::Message

=head1 SYNOPSIS

    # Implicitly used by Log::Message to serve as handlers for
    # Log::Message::Item objects

    # Create your own file with a package called
    # Log::Message::Handlers to add to the existing ones, or to even
    # overwrite them

    $item->carp;

    $item->trace;


=head1 DESCRIPTION

Log::Message::Handlers provides handlers for Log::Message::Item objects.
The handler corresponding to the level (see Log::Message::Item manpage
for an explanation about levels) will be called automatically upon
storing the error.

Handlers may also explicitly be called on an Log::Message::Item object
if one so desires (see the Log::Message manpage on how to retrieve the
Item objects).

=head1 Default Handlers

=head2 log

Will simply log the error on the stack, and do nothing special

=cut

sub log { 1 }

=head2 carp

Will carp (see the Carp manpage) with the error, and add the timestamp
of when it occurred.

=cut

sub carp {
    my $self = shift;
    warn join " ", $self->message, $self->shortmess, 'at', $self->when, "\n";
}

=head2 croak

Will croak (see the Carp manpage) with the error, and add the
timestamp of when it occurred.

=cut

sub croak {
    my $self = shift;
    die join " ", $self->message, $self->shortmess, 'at', $self->when, "\n";
}

=head2 cluck

Will cluck (see the Carp manpage) with the error, and add the
timestamp of when it occurred.

=cut

sub cluck {
    my $self = shift;
    warn join " ", $self->message, $self->longmess, 'at', $self->when, "\n";
}

=head2 confess

Will confess (see the Carp manpage) with the error, and add the
timestamp of when it occurred

=cut

sub confess {
    my $self = shift;
    die join " ", $self->message, $self->longmess, 'at', $self->when, "\n";
}

=head2 die

Will simply die with the error message of the item

=cut

sub die  { die  shift->message; }


=head2 warn

Will simply warn with the error message of the item

=cut

sub warn { warn shift->message; }


=head2 trace

Will provide a traceback of this error item back to the first one that
occurrent, clucking with every item as it comes across it.

=cut

sub trace {
    my $self = shift;

    for my $item( $self->parent->retrieve( chrono => 0 ) ) {
        $item->cluck;
    }
}

=head1 Custom Handlers

If you wish to provide your own handlers, you can simply do the
following:

=over 4

=item *

Create a file that holds a package by the name of
C<Log::Message::Handlers>

=item *

Create subroutines with the same name as the levels you wish to
handle in the Log::Message module (see the Log::Message manpage for
explanation on levels)

=item *

Require that file in your program, or add it in your configuration
(see the Log::Message::Config manpage for explanation on how to use a
config file)

=back

And that is it, the handler will now be available to handle messages
for you.

The arguments a handler may receive are those specified by the
C<extra> key, when storing the message.
See the Log::Message manpage for details on the arguments.

=head1 SEE ALSO

L<Log::Message>, L<Log::Message::Item>, L<Log::Message::Config>

=head1 AUTHOR

This module by
Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 Acknowledgements

Thanks to Ann Barcomb for her suggestions.

=head1 COPYRIGHT

This module is
copyright (c) 2002 Jos Boumans E<lt>kane@cpan.orgE<gt>.
All rights reserved.

This library is free software;
you may redistribute and/or modify it under the same
terms as Perl itself.

=cut

1;

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
