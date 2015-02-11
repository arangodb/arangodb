package Log::Message::Simple;

use strict;
use Log::Message private => 0;;

BEGIN { 
    use vars qw[$VERSION]; 
    $VERSION = 0.04; 
}
        

=pod

=head1 NAME

Log::Message::Simple

=head1 SYNOPSIS

    use Log::Message::Simple qw[msg error debug
                                carp croak cluck confess];

    use Log::Message::Simple qw[:STD :CARP];

    ### standard reporting functionality
    msg(    "Connecting to database",           $verbose );
    error(  "Database connection failed: $@",   $verbose );
    debug(  "Connection arguments were: $args", $debug );

    ### standard carp functionality
    carp(   "Wrong arguments passed: @_" );
    croak(  "Fatal: wrong arguments passed: @_" );
    cluck(  "Wrong arguments passed -- including stacktrace: @_" );
    confess("Fatal: wrong arguments passed -- including stacktrace: @_" );

    ### retrieve individual message
    my @stack = Log::Message::Simple->stack;
    my @stack = Log::Message::Simple->flush;

    ### retrieve the entire stack in printable form
    my $msgs  = Log::Message::Simple->stack_as_string;
    my $trace = Log::Message::Simple->stack_as_string(1);

    ### redirect output
    local $Log::Message::Simple::MSG_FH     = \*STDERR;
    local $Log::Message::Simple::ERROR_FH   = \*STDERR;
    local $Log::Message::Simple::DEBUG_FH   = \*STDERR;
    
    ### force a stacktrace on error
    local $Log::Message::Simple::STACKTRACE_ON_ERROR = 1

=head1 DESCRIPTION

This module provides standardized logging facilities using the
C<Log::Message> module.

=head1 FUNCTIONS

=head2 msg("message string" [,VERBOSE])

Records a message on the stack, and prints it to C<STDOUT> (or actually
C<$MSG_FH>, see the C<GLOBAL VARIABLES> section below), if the
C<VERBOSE> option is true.
The C<VERBOSE> option defaults to false.

Exported by default, or using the C<:STD> tag.

=head2 debug("message string" [,VERBOSE])

Records a debug message on the stack, and prints it to C<STDOUT> (or
actually C<$DEBUG_FH>, see the C<GLOBAL VARIABLES> section below), 
if the C<VERBOSE> option is true.
The C<VERBOSE> option defaults to false.

Exported by default, or using the C<:STD> tag.

=head2 error("error string" [,VERBOSE])

Records an error on the stack, and prints it to C<STDERR> (or actually
C<$ERROR_FH>, see the C<GLOBAL VARIABLES> sections below), if the
C<VERBOSE> option is true.
The C<VERBOSE> options defaults to true.

Exported by default, or using the C<:STD> tag.

=cut 

{   package Log::Message::Handlers;
    
    sub msg {
        my $self    = shift;
        my $verbose = shift || 0;

        ### so you don't want us to print the msg? ###
        return if defined $verbose && $verbose == 0;

        my $old_fh = select $Log::Message::Simple::MSG_FH;
        print '['. $self->tag (). '] ' . $self->message . "\n";
        select $old_fh;

        return;
    }

    sub debug {
        my $self    = shift;
        my $verbose = shift || 0;

        ### so you don't want us to print the msg? ###
        return if defined $verbose && $verbose == 0;

        my $old_fh = select $Log::Message::Simple::DEBUG_FH;
        print '['. $self->tag (). '] ' . $self->message . "\n";
        select $old_fh;

        return;
    }

    sub error {
        my $self    = shift;
        my $verbose = shift;
           $verbose = 1 unless defined $verbose;    # default to true

        ### so you don't want us to print the error? ###
        return if defined $verbose && $verbose == 0;

        my $old_fh = select $Log::Message::Simple::ERROR_FH;

        my $msg     = '['. $self->tag . '] ' . $self->message;

        print $Log::Message::Simple::STACKTRACE_ON_ERROR 
                    ? Carp::shortmess($msg) 
                    : $msg . "\n";

        select $old_fh;

        return;
    }
}

=head2 carp();

Provides functionality equal to C<Carp::carp()> while still logging
to the stack.

Exported by using the C<:CARP> tag.

=head2 croak();

Provides functionality equal to C<Carp::croak()> while still logging
to the stack.

Exported by using the C<:CARP> tag.

=head2 confess();

Provides functionality equal to C<Carp::confess()> while still logging
to the stack.

Exported by using the C<:CARP> tag.

=head2 cluck();

Provides functionality equal to C<Carp::cluck()> while still logging
to the stack.

Exported by using the C<:CARP> tag.

=head1 CLASS METHODS

=head2 Log::Message::Simple->stack()

Retrieves all the items on the stack. Since C<Log::Message::Simple> is
implemented using C<Log::Message>, consult its manpage for the
function C<retrieve> to see what is returned and how to use the items.

=head2 Log::Message::Simple->stack_as_string([TRACE])

Returns the whole stack as a printable string. If the C<TRACE> option is
true all items are returned with C<Carp::longmess> output, rather than
just the message.
C<TRACE> defaults to false.

=head2 Log::Message::Simple->flush()

Removes all the items from the stack and returns them. Since
C<Log::Message::Simple> is  implemented using C<Log::Message>, consult its
manpage for the function C<retrieve> to see what is returned and how
to use the items.

=cut

BEGIN {
    use Exporter;
    use Params::Check   qw[ check ];
    use vars            qw[ @EXPORT @EXPORT_OK %EXPORT_TAGS @ISA ];;

    @ISA            = 'Exporter';
    @EXPORT         = qw[error msg debug];
    @EXPORT_OK      = qw[carp cluck croak confess];
    
    %EXPORT_TAGS    = (
        STD     => \@EXPORT,
        CARP    => \@EXPORT_OK,
        ALL     => [ @EXPORT, @EXPORT_OK ],
    );        

    my $log         = new Log::Message;

    for my $func ( @EXPORT, @EXPORT_OK ) {
        no strict 'refs';
        
                        ### up the carplevel for the carp emulation
                        ### functions
        *$func = sub {  local $Carp::CarpLevel += 2
                            if grep { $_ eq $func } @EXPORT_OK;
                            
                        my $msg     = shift;
                        $log->store(
                                message => $msg,
                                tag     => uc $func,
                                level   => $func,
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

=item $DEBUG_FH

This is the filehandle all the messages sent to C<debug()> are being
printed. This default to C<*STDOUT>.

=item $STACKTRACE_ON_ERROR

If this option is set to C<true>, every call to C<error()> will 
generate a stacktrace using C<Carp::shortmess()>.
Defaults to C<false>

=cut

BEGIN {
    use vars qw[ $ERROR_FH $MSG_FH $DEBUG_FH $STACKTRACE_ON_ERROR ];

    local $| = 1;
    $ERROR_FH               = \*STDERR;
    $MSG_FH                 = \*STDOUT;
    $DEBUG_FH               = \*STDOUT;
    
    $STACKTRACE_ON_ERROR    = 0;
}


1;

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
