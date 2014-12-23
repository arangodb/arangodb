package CPANPLUS::Error;

use strict;

use Log::Message private => 0;;

=pod

=head1 NAME

CPANPLUS::Error

=head1 SYNOPSIS

    use CPANPLUS::Error qw[cp_msg cp_error];

=head1 DESCRIPTION

This module provides the error handling code for the CPANPLUS
libraries, and is mainly intended for internal use.

=head1 FUNCTIONS

=head2 cp_msg("message string" [,VERBOSE])

Records a message on the stack, and prints it to C<STDOUT> (or actually
C<$MSG_FH>, see the C<GLOBAL VARIABLES> section below), if the
C<VERBOSE> option is true.
The C<VERBOSE> option defaults to false.

=head2 msg()

An alias for C<cp_msg>.

=head2 cp_error("error string" [,VERBOSE])

Records an error on the stack, and prints it to C<STDERR> (or actually
C<$ERROR_FH>, see the C<GLOBAL VARIABLES> sections below), if the
C<VERBOSE> option is true.
The C<VERBOSE> options defaults to true.

=head2 error()

An alias for C<cp_error>.

=head1 CLASS METHODS

=head2 CPANPLUS::Error->stack()

Retrieves all the items on the stack. Since C<CPANPLUS::Error> is
implemented using C<Log::Message>, consult its manpage for the
function C<retrieve> to see what is returned and how to use the items.

=head2 CPANPLUS::Error->stack_as_string([TRACE])

Returns the whole stack as a printable string. If the C<TRACE> option is
true all items are returned with C<Carp::longmess> output, rather than
just the message.
C<TRACE> defaults to false.

=head2 CPANPLUS::Error->flush()

Removes all the items from the stack and returns them. Since
C<CPANPLUS::Error> is  implemented using C<Log::Message>, consult its
manpage for the function C<retrieve> to see what is returned and how
to use the items.

=cut

BEGIN {
    use Exporter;
    use Params::Check   qw[check];
    use vars            qw[@EXPORT @ISA $ERROR_FH $MSG_FH];

    @ISA        = 'Exporter';
    @EXPORT     = qw[cp_error cp_msg error msg];

    my $log     = new Log::Message;

    for my $func ( @EXPORT ) {
        no strict 'refs';
        
        my $prefix  = 'cp_';
        my $name    = $func;
        $name       =~ s/^$prefix//g;
        
        *$func = sub {
                        my $msg     = shift;
                        
                        ### no point storing non-messages
                        return unless defined $msg;
                        
                        $log->store(
                                message => $msg,
                                tag     => uc $name,
                                level   => $prefix . $name,
                                extra   => [@_]
                        );
                };
    }

    sub flush {
        return reverse $log->flush;
    }

    sub stack {
        return $log->retrieve( chrono => 1 );
    }

    sub stack_as_string {
        my $class = shift;
        my $trace = shift() ? 1 : 0;

        return join $/, map {
                        '[' . $_->tag . '] [' . $_->when . '] ' .
                        ($trace ? $_->message . ' ' . $_->longmess
                                : $_->message);
                    } __PACKAGE__->stack;
    }
}

=head1 GLOBAL VARIABLES

=over 4

=item $ERROR_FH

This is the filehandle all the messages sent to C<error()> are being
printed. This defaults to C<*STDERR>.

=item $MSG_FH

This is the filehandle all the messages sent to C<msg()> are being
printed. This default to C<*STDOUT>.

=cut
local $| = 1;
$ERROR_FH   = \*STDERR;
$MSG_FH     = \*STDOUT;

package Log::Message::Handlers;
use Carp ();

{

    sub cp_msg {
        my $self    = shift;
        my $verbose = shift;

        ### so you don't want us to print the msg? ###
        return if defined $verbose && $verbose == 0;

        my $old_fh = select $CPANPLUS::Error::MSG_FH;

        print '['. $self->tag . '] ' . $self->message . "\n";
        select $old_fh;

        return;
    }

    sub cp_error {
        my $self    = shift;
        my $verbose = shift;

        ### so you don't want us to print the error? ###
        return if defined $verbose && $verbose == 0;

        my $old_fh = select $CPANPLUS::Error::ERROR_FH;

        ### is only going to be 1 for now anyway ###
        ### C::I may not be loaded, so do a can() check first
        my $cb      = CPANPLUS::Internals->can('_return_all_objects')
                        ? (CPANPLUS::Internals->_return_all_objects)[0]
                        : undef;

        ### maybe we didn't initialize an internals object (yet) ###
        my $debug   = $cb ? $cb->configure_object->get_conf('debug') : 0;
        my $msg     =  '['. $self->tag . '] ' . $self->message . "\n";

        ### i'm getting this warning in the test suite:
        ### Ambiguous call resolved as CORE::warn(), qualify as such or
        ### use & at CPANPLUS/Error.pm line 57.
        ### no idea where it's coming from, since there's no 'sub warn'
        ### anywhere to be found, but i'll mark it explicitly nonetheless
        ### --kane
        print $debug ? Carp::shortmess($msg) : $msg . "\n";

        select $old_fh;

        return;
    }
}

1;

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
