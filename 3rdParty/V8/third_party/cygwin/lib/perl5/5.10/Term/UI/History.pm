package Term::UI::History;

use strict;
use base 'Exporter';
use base 'Log::Message::Simple';

=pod

=head1 NAME

Term::UI::History

=head1 SYNOPSIS

    use Term::UI::History qw[history];

    history("Some message");

    ### retrieve the history in printable form
    $hist  = Term::UI::History->history_as_string;

    ### redirect output
    local $Term::UI::History::HISTORY_FH = \*STDERR;

=head1 DESCRIPTION

This module provides the C<history> function for C<Term::UI>,
printing and saving all the C<UI> interaction.

Refer to the C<Term::UI> manpage for details on usage from
C<Term::UI>.

This module subclasses C<Log::Message::Simple>. Refer to its
manpage for additional functionality available via this package.

=head1 FUNCTIONS

=head2 history("message string" [,VERBOSE])

Records a message on the stack, and prints it to C<STDOUT> 
(or actually C<$HISTORY_FH>, see the C<GLOBAL VARIABLES> section 
below), if the C<VERBOSE> option is true.

The C<VERBOSE> option defaults to true.

=cut

BEGIN {
    use Log::Message private => 0;

    use vars      qw[ @EXPORT $HISTORY_FH ];
    @EXPORT     = qw[ history ];
    my $log     = new Log::Message;
    $HISTORY_FH = \*STDOUT;

    for my $func ( @EXPORT ) {
        no strict 'refs';
        
        *$func = sub {  my $msg     = shift;
                        $log->store(
                                message => $msg,
                                tag     => uc $func,
                                level   => $func,
                                extra   => [@_]
                        );
                };
    }

    sub history_as_string {
        my $class = shift;

        return join $/, map { $_->message } __PACKAGE__->stack;
    }
}


{   package Log::Message::Handlers;
    
    sub history {
        my $self    = shift;
        my $verbose = shift;
           $verbose = 1 unless defined $verbose;    # default to true

        ### so you don't want us to print the msg? ###
        return if defined $verbose && $verbose == 0;

        local $| = 1;
        my $old_fh = select $Term::UI::History::HISTORY_FH;

        print $self->message . "\n";
        select $old_fh;

        return;
    }
}


=head1 GLOBAL VARIABLES

=over 4

=item $HISTORY_FH

This is the filehandle all the messages sent to C<history()> are being
printed. This defaults to C<*STDOUT>.

=back

=head1 See Also

C<Log::Message::Simple>, C<Term::UI>

=head1 AUTHOR

This module by
Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 COPYRIGHT

This module is
copyright (c) 2005 Jos Boumans E<lt>kane@cpan.orgE<gt>.
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
