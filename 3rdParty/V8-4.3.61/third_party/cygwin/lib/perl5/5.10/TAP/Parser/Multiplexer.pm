package TAP::Parser::Multiplexer;

use strict;
use IO::Select;
use vars qw($VERSION);

use constant IS_WIN32 => $^O =~ /^(MS)?Win32$/;
use constant IS_VMS => $^O eq 'VMS';
use constant SELECT_OK => !( IS_VMS || IS_WIN32 );

=head1 NAME

TAP::Parser::Multiplexer - Multiplex multiple TAP::Parsers

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

=head1 SYNOPSIS

    use TAP::Parser::Multiplexer;

    my $mux = TAP::Parser::Multiplexer->new;
    $mux->add( $parser1, $stash1 );
    $mux->add( $parser2, $stash2 );
    while ( my ( $parser, $stash, $result ) = $mux->next ) {
        # do stuff
    }

=head1 DESCRIPTION

C<TAP::Parser::Multiplexer> gathers input from multiple TAP::Parsers.
Internally it calls select on the input file handles for those parsers
to wait for one or more of them to have input available.

See L<TAP::Harness> for an example of its use.

=head1 METHODS

=head2 Class Methods

=head3 C<new>

    my $mux = TAP::Parser::Multiplexer->new;

Returns a new C<TAP::Parser::Multiplexer> object.

=cut

sub new {
    my ($class) = @_;
    return bless {
        select => IO::Select->new,
        avid   => [],                # Parsers that can't select
        count  => 0,
    }, $class;
}

##############################################################################

=head2 Instance Methods

=head3 C<add>

  $mux->add( $parser, $stash );

Add a TAP::Parser to the multiplexer. C<$stash> is an optional opaque
reference that will be returned from C<next> along with the parser and
the next result.

=cut

sub add {
    my ( $self, $parser, $stash ) = @_;

    if ( SELECT_OK && ( my @handles = $parser->get_select_handles ) ) {
        my $sel = $self->{select};

        # We have to turn handles into file numbers here because by
        # the time we want to remove them from our IO::Select they
        # will already have been closed by the iterator.
        my @filenos = map { fileno $_ } @handles;
        for my $h (@handles) {
            $sel->add( [ $h, $parser, $stash, @filenos ] );
        }

        $self->{count}++;
    }
    else {
        push @{ $self->{avid} }, [ $parser, $stash ];
    }
}

=head3 C<parsers>

  my $count   = $mux->parsers;

Returns the number of parsers. Parsers are removed from the multiplexer
when their input is exhausted.

=cut

sub parsers {
    my $self = shift;
    return $self->{count} + scalar @{ $self->{avid} };
}

sub _iter {
    my $self = shift;

    my $sel   = $self->{select};
    my $avid  = $self->{avid};
    my @ready = ();

    return sub {

        # Drain all the non-selectable parsers first
        if (@$avid) {
            my ( $parser, $stash ) = @{ $avid->[0] };
            my $result = $parser->next;
            shift @$avid unless defined $result;
            return ( $parser, $stash, $result );
        }

        unless (@ready) {
            return unless $sel->count;

            # TODO: Win32 doesn't do select properly on handles...
            @ready = $sel->can_read;
        }

        my ( $h, $parser, $stash, @handles ) = @{ shift @ready };
        my $result = $parser->next;

        unless ( defined $result ) {
            $sel->remove(@handles);
            $self->{count}--;

            # Force another can_read - we may now have removed a handle
            # thought to have been ready.
            @ready = ();
        }

        return ( $parser, $stash, $result );
    };
}

=head3 C<next>

Return a result from the next available parser. Returns a list
containing the parser from which the result came, the stash that
corresponds with that parser and the result.

    my ( $parser, $stash, $result ) = $mux->next;

If C<$result> is undefined the corresponding parser has reached the end
of its input (and will automatically be removed from the multiplexer).

When all parsers are exhausted an empty list will be returned.

    if ( my ( $parser, $stash, $result ) = $mux->next ) {
        if ( ! defined $result ) {
            # End of this parser
        }
        else {
            # Process result
        }
    }
    else {
        # All parsers finished
    }

=cut

sub next {
    my $self = shift;
    return ( $self->{_iter} ||= $self->_iter )->();
}

=head1 See Also

L<TAP::Parser>

L<TAP::Harness>

=cut

1;
