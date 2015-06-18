package TAP::Parser::Iterator;

use strict;
use vars qw($VERSION);

use TAP::Parser::Iterator::Array   ();
use TAP::Parser::Iterator::Stream  ();
use TAP::Parser::Iterator::Process ();

=head1 NAME

TAP::Parser::Iterator - Internal TAP::Parser Iterator

=head1 VERSION

Version 3.10

=cut

$VERSION = '3.10';

=head1 SYNOPSIS

  use TAP::Parser::Iterator;
  my $it = TAP::Parser::Iterator->new(\*TEST);
  my $it = TAP::Parser::Iterator->new(\@array);

  my $line = $it->next;

Originally ripped off from L<Test::Harness>.

=head1 DESCRIPTION

B<FOR INTERNAL USE ONLY!>

This is a simple iterator wrapper for arrays and filehandles.

=head2 Class Methods

=head3 C<new>

 my $iter = TAP::Parser::Iterator->new( $array_reference );
 my $iter = TAP::Parser::Iterator->new( $filehandle );

Create an iterator.

=head2 Instance Methods

=head3 C<next>

 while ( my $item = $iter->next ) { ... }

Iterate through it, of course.

=head3 C<next_raw>

 while ( my $item = $iter->next_raw ) { ... }

Iterate raw input without applying any fixes for quirky input syntax.

=cut

sub new {
    my ( $proto, $thing ) = @_;

    my $ref = ref $thing;
    if ( $ref eq 'GLOB' || $ref eq 'IO::Handle' ) {
        return TAP::Parser::Iterator::Stream->new($thing);
    }
    elsif ( $ref eq 'ARRAY' ) {
        return TAP::Parser::Iterator::Array->new($thing);
    }
    elsif ( $ref eq 'HASH' ) {
        return TAP::Parser::Iterator::Process->new($thing);
    }
    else {
        die "Can't iterate with a $ref";
    }
}

sub next {
    my $self = shift;
    my $line = $self->next_raw;

    # vms nit:  When encountering 'not ok', vms often has the 'not' on a line
    # by itself:
    #   not
    #   ok 1 - 'I hate VMS'
    if ( defined($line) and $line =~ /^\s*not\s*$/ ) {
        $line .= ( $self->next_raw || '' );
    }

    return $line;
}

=head3 C<handle_unicode>

If necessary switch the input stream to handle unicode. This only has
any effect for I/O handle based streams.

=cut

sub handle_unicode { }

=head3 C<get_select_handles>

Return a list of filehandles that may be used upstream in a select()
call to signal that this Iterator is ready. Iterators that are not
handle based should return an empty list.

=cut

sub get_select_handles {return}

1;
