package Log::Message::Item;

use strict;
use Params::Check qw[check];
use Log::Message::Handlers;

### for the messages to store ###
use Carp ();

BEGIN {
    use vars qw[$AUTOLOAD $VERSION];

    $VERSION    =   $Log::Message::VERSION;
}

### create a new item.
### note that only an id (position on the stack), message and a reference
### to its parent are required. all the other things it can fill in itself
sub new {
    my $class   = shift;
    my %hash    = @_;

    my $tmpl = {
        when        => { no_override    => 1,   default    => scalar localtime },
        id          => { required       => 1    },
        message     => { required       => 1    },
        parent      => { required        => 1    },
        level       => { default        => ''   },      # default may be conf dependant
        tag         => { default        => ''   },      # default may be conf dependant
        longmess    => { default        => _clean(Carp::longmess()) },
        shortmess   => { default        => _clean(Carp::shortmess())},
    };

    my $args = check($tmpl, \%hash) or return undef;

    return bless $args, $class;
}

sub _clean { map { s/\s*//; chomp; $_ } shift; }

sub remove {
    my $item = shift;
    my $self = $item->parent;

    return splice( @{$self->{STACK}}, $item->id, 1, undef );
}

sub AUTOLOAD {
    my $self = $_[0];

    $AUTOLOAD =~ s/.+:://;

    return $self->{$AUTOLOAD} if exists $self->{$AUTOLOAD};

    local $Carp::CarpLevel = $Carp::CarpLevel + 3;

    {   no strict 'refs';
        return *{"Log::Message::Handlers::${AUTOLOAD}"}->(@_);
    }
}

sub DESTROY { 1 }

1;

__END__

=pod

=head1 NAME

Log::Message::Item  - Message objects for Log::Message

=head1 SYNOPSIS

    # Implicitly used by Log::Message to create Log::Message::Item objects

    print "this is the message's id: ",     $item->id;

    print "this is the message stored: ",   $item->message;

    print "this is when it happened: ",     $item->when;

    print "the message was tagged: ",       $item->tag;

    print "this was the severity level: ",  $item->level;

    $item->remove;  # delete the item from the stack it was on

    # Besides these methods, you can also call the handlers on
    # the object specificallly.
    # See the Log::Message::Handlers manpage for documentation on what
    # handlers are available by default and how to add your own


=head1 DESCRIPTION

Log::Message::Item is a class that generates generic Log items.
These items are stored on a Log::Message stack, so see the Log::Message
manpage about details how to retrieve them.

You should probably not create new items by yourself, but use the
storing mechanism provided by Log::Message.

However, the accessors and handlers are of interest if you want to do
fine tuning of how your messages are handled.

The accessors and methods are described below, the handlers are
documented in the Log::Message::Handlers manpage.

=head1 Methods and Accessors

=head2 remove

Calling remove will remove the object from the stack it was on, so it
will not show up any more in subsequent fetches of messages.

You can still call accessors and handlers on it however, to handle it
as you will.

=head2 id

Returns the internal ID of the item. This may be useful for comparing
since the ID is incremented each time a new item is created.
Therefore, an item with ID 4 must have been logged before an item with
ID 9.

=head2 when

Returns the timestamp of when the message was logged

=head2 message

The actual message that was stored

=head2 level

The severity type of this message, as well as the name of the handler
that was called upon storing it.

=head2 tag

Returns the identification tag that was put on the message.

=head2 shortmess

Returns the equivalent of a C<Carp::shortmess> for this item.
See the C<Carp> manpage for details.

=head2 longmess

Returns the equivalent of a C<Carp::longmess> for this item, which
is essentially a stack trace.
See the C<Carp> manpage for details.

=head2 parent

Returns a reference to the Log::Message object that stored this item.
This is useful if you want to have access to the full stack in a
handler.

=head1 SEE ALSO

L<Log::Message>, L<Log::Message::Handlers>, L<Log::Message::Config>

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

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
