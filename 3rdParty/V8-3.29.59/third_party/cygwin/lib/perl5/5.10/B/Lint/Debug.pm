package B::Lint::Debug;

=head1 NAME

B::Lint::Debug - Adds debugging stringification to B::

=head1 DESCRIPTION

This module injects stringification to a B::OP*/B::SPECIAL. This
should not be loaded unless you're debugging.

=cut

package B::SPECIAL;
use overload '""' => sub {
    my $self = shift @_;
    "SPECIAL($$self)";
};

package B::OP;
use overload '""' => sub {
    my $self  = shift @_;
    my $class = ref $self;
    $class =~ s/\AB:://xms;
    my $name = $self->name;
    "$class($name)";
};

package B::SVOP;
use overload '""' => sub {
    my $self  = shift @_;
    my $class = ref $self;
    $class =~ s/\AB:://xms;
    my $name = $self->name;
    "$class($name," . $self->sv . "," . $self->gv . ")";
};

package B::SPECIAL;
sub DESTROY { }
our $AUTOLOAD;

sub AUTOLOAD {
    my $cx = 0;
    print "AUTOLOAD $AUTOLOAD\n";

    package DB;
    while ( my @stuff = caller $cx ) {

        print "$cx: [@DB::args] [@stuff]\n";
        if ( ref $DB::args[0] ) {
            if ( $DB::args[0]->can('padix') ) {
                print "    PADIX: " . $DB::args[0]->padix . "\n";
            }
            if ( $DB::args[0]->can('targ') ) {
                print "    TARG: " . $DB::args[0]->targ . "\n";
                for ( B::Lint::cv()->PADLIST->ARRAY ) {
                    print +( $_->ARRAY )[ $DB::args[0]->targ ] . "\n";
                }
            }
        }
        ++$cx;
    }
}

1;
