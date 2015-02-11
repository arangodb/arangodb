use strict;

package Pod::Coverage;
use Devel::Symdump;
use B;
use Pod::Find qw(pod_where);

BEGIN { defined &TRACE_ALL or eval 'sub TRACE_ALL () { 0 }' }

use vars qw/ $VERSION /;
$VERSION = '0.19';

=head1 NAME

Pod::Coverage - Checks if the documentation of a module is comprehensive

=head1 SYNOPSIS

  # in the beginnning...
  perl -MPod::Coverage=Pod::Coverage -e666

  # all in one invocation
  use Pod::Coverage package => 'Fishy';

  # straight OO
  use Pod::Coverage;
  my $pc = Pod::Coverage->new(package => 'Pod::Coverage');
  print "We rock!" if $pc->coverage == 1;


=head1 DESCRIPTION

Developers hate writing documentation.  They'd hate it even more if
their computer tattled on them, but maybe they'll be even more
thankful in the long run.  Even if not, F<perlmodstyle> tells you to, so
you must obey.

This module provides a mechanism for determining if the pod for a
given module is comprehensive.

It expects to find either a C<< =head(n>1) >> or an C<=item> block documenting a
subroutine.

Consider:
 # an imaginary Foo.pm
 package Foo;

 =item foo

 The foo sub

 = cut

 sub foo {}
 sub bar {}

 1;
 __END__

In this example C<Foo::foo> is covered, but C<Foo::bar> is not, so the C<Foo>
package is only 50% (0.5) covered

=head2 Methods

=over

=item Pod::Coverage->new(package => $package)

Creates a new Pod::Coverage object.

C<package> the name of the package to analyse

C<private> an array of regexen which define what symbols are regarded
as private (and so need not be documented) defaults to [ qr/^_/,
qr/^import$/, qr/^DESTROY$/, qr/^AUTOLOAD$/, qr/^bootstrap$/,
        qr/^(TIE( SCALAR | ARRAY | HASH | HANDLE ) |
             FETCH | STORE | UNTIE | FETCHSIZE | STORESIZE |
             POP | PUSH | SHIFT | UNSHIFT | SPLICE | DELETE |
             EXISTS | EXTEND | CLEAR | FIRSTKEY | NEXTKEY | PRINT | PRINTF |
             WRITE | READLINE | GETC | READ | CLOSE | BINMODE | OPEN |
             EOF | FILENO | SEEK | TELL)$/x,
        qr/^( MODIFY | FETCH )_( REF | SCALAR | ARRAY | HASH | CODE |
                                 GLOB | FORMAT | IO)_ATTRIBUTES$/x,
        qr/^CLONE(_SKIP)?$/,
]

This should cover all the usual magical methods for tie()d objects,
attributes, generally all the methods that are typically not called by
a user, but instead being used internally by perl.

C<also_private> items are appended to the private list

C<trustme> an array of regexen which define what symbols you just want
us to assume are properly documented even if we can't find any docs
for them

If C<pod_from> is supplied, that file is parsed for the documentation,
rather than using Pod::Find

If C<nonwhitespace> is supplied, then only POD sections which have
non-whitespace characters will count towards being documented.

=cut

sub new {
    my $referent = shift;
    my %args     = @_;
    my $class    = ref $referent || $referent;

    my $private = $args{private} || [
        qr/^_/,
        qr/^import$/,
        qr/^DESTROY$/,
        qr/^AUTOLOAD$/,
        qr/^bootstrap$/,
        qr/^\(/,
        qr/^(TIE( SCALAR | ARRAY | HASH | HANDLE ) |
             FETCH | STORE | UNTIE | FETCHSIZE | STORESIZE |
             POP | PUSH | SHIFT | UNSHIFT | SPLICE | DELETE |
             EXISTS | EXTEND | CLEAR | FIRSTKEY | NEXTKEY | PRINT | PRINTF |
             WRITE | READLINE | GETC | READ | CLOSE | BINMODE | OPEN |
             EOF | FILENO | SEEK | TELL)$/x,
        qr/^( MODIFY | FETCH )_( REF | SCALAR | ARRAY | HASH | CODE |
                                 GLOB | FORMAT | IO)_ATTRIBUTES $/x,
        qr/^CLONE(_SKIP)?$/,
    ];
    push @$private, @{ $args{also_private} || [] };
    my $trustme       = $args{trustme}       || [];
    my $nonwhitespace = $args{nonwhitespace} || undef;

    my $self = bless {
        @_,
        private       => $private,
        trustme       => $trustme,
        nonwhitespace => $nonwhitespace
    }, $class;
}

=item $object->coverage

Gives the coverage as a value in the range 0 to 1

=cut

sub coverage {
    my $self = shift;

    my $package = $self->{package};
    my $pods    = $self->_get_pods;
    return unless $pods;

    my %symbols = map { $_ => 0 } $self->_get_syms($package);

    print "tying shoelaces\n" if TRACE_ALL;
    for my $pod (@$pods) {
        $symbols{$pod} = 1 if exists $symbols{$pod};
    }

    foreach my $sym ( keys %symbols ) {
        $symbols{$sym} = 1 if $self->_trustme_check($sym);
    }

    # stash the results for later
    $self->{symbols} = \%symbols;

    if (TRACE_ALL) {
        require Data::Dumper;
        print Data::Dumper::Dumper($self);
    }

    my $symbols = scalar keys %symbols;
    my $documented = scalar grep {$_} values %symbols;
    unless ($symbols) {
        $self->{why_unrated} = "no public symbols defined";
        return;
    }
    return $documented / $symbols;
}

=item $object->why_unrated

C<< $object->coverage >> may return C<undef>, to indicate that it was
unable to deduce coverage for a package.  If this happens you should
be able to check C<why_unrated> to get a useful excuse.

=cut

sub why_unrated {
    my $self = shift;
    $self->{why_unrated};
}

=item $object->naked/$object->uncovered

Returns a list of uncovered routines, will implicitly call coverage if
it's not already been called.

Note, private and 'trustme' identifiers will be skipped.

=cut

sub naked {
    my $self = shift;
    $self->{symbols} or $self->coverage;
    return unless $self->{symbols};
    return grep { !$self->{symbols}{$_} } keys %{ $self->{symbols} };
}

*uncovered = \&naked;

=item $object->covered

Returns a list of covered routines, will implicitly call coverage if
it's not previously been called.

As with C<naked>, private and 'trustme' identifiers will be skipped.

=cut

sub covered {
    my $self = shift;
    $self->{symbols} or $self->coverage;
    return unless $self->{symbols};
    return grep { $self->{symbols}{$_} } keys %{ $self->{symbols} };
}

sub import {
    my $self = shift;
    return unless @_;

    # one argument - just a package
    scalar @_ == 1 and unshift @_, 'package';

    # we were called with arguments
    my $pc     = $self->new(@_);
    my $rating = $pc->coverage;
    $rating = 'unrated (' . $pc->why_unrated . ')'
        unless defined $rating;
    print $pc->{package}, " has a $self rating of $rating\n";
    my @looky_here = $pc->naked;
    if ( @looky_here > 1 ) {
        print "The following are uncovered: ", join( ", ", sort @looky_here ),
            "\n";
    } elsif (@looky_here) {
        print "'$looky_here[0]' is uncovered\n";
    }
}

=back

=head2 Debugging support

In order to allow internals debugging, while allowing the optimiser to
do its thang, C<Pod::Coverage> uses constant subs to define how it traces.

Use them like so

 sub Pod::Coverage::TRACE_ALL () { 1 }
 use Pod::Coverage;

Supported constants are:

=over

=item TRACE_ALL

Trace everything.

Well that's all there is so far, are you glad you came?

=back

=head2 Inheritance interface

These abstract methods while functional in C<Pod::Coverage> may make
your life easier if you want to extend C<Pod::Coverage> to fit your
house style more closely.

B<NOTE> Please consider this interface as in a state of flux until
this comment goes away.

=over

=item $object->_CvGV($symbol)

Return the GV for the coderef supplied.  Used by C<_get_syms> to identify
locally defined code.

You probably won't need to override this one.

=item $object->_get_syms($package)

return a list of symbols to check for from the specified packahe

=cut

# this one walks the symbol tree
sub _get_syms {
    my $self    = shift;
    my $package = shift;

    print "requiring '$package'\n" if TRACE_ALL;
    eval qq{ require $package };
    print "require failed with $@\n" if TRACE_ALL and $@;
    return if $@;

    print "walking symbols\n" if TRACE_ALL;
    my $syms = Devel::Symdump->new($package);

    my @symbols;
    for my $sym ( $syms->functions ) {

        # see if said method wasn't just imported from elsewhere
        my $glob = do { no strict 'refs'; \*{$sym} };
        my $o = B::svref_2object($glob);

        # in 5.005 this flag is not exposed via B, though it exists
        my $imported_cv = eval { B::GVf_IMPORTED_CV() } || 0x80;
        next if $o->GvFLAGS & $imported_cv;

        # check if it's on the whitelist
        $sym =~ s/$self->{package}:://;
        next if $self->_private_check($sym);

        push @symbols, $sym;
    }
    return @symbols;
}

=item _get_pods

Extract pod markers from the currently active package.

Return an arrayref or undef on fail.

=cut

sub _get_pods {
    my $self = shift;

    my $package = $self->{package};

    print "getting pod location for '$package'\n" if TRACE_ALL;
    $self->{pod_from} ||= pod_where( { -inc => 1 }, $package );

    my $pod_from = $self->{pod_from};
    unless ($pod_from) {
        $self->{why_unrated} = "couldn't find pod";
        return;
    }

    print "parsing '$pod_from'\n" if TRACE_ALL;
    my $pod = Pod::Coverage::Extractor->new;
    $pod->{nonwhitespace} = $self->{nonwhitespace};
    $pod->parse_from_file( $pod_from, '/dev/null' );

    return $pod->{identifiers} || [];
}

=item _private_check($symbol)

return true if the symbol should be considered private

=cut

sub _private_check {
    my $self = shift;
    my $sym  = shift;
    return grep { $sym =~ /$_/ } @{ $self->{private} };
}

=item _trustme_check($symbol)

return true if the symbol is a 'trustme' symbol

=cut

sub _trustme_check {
    my ( $self, $sym ) = @_;
    return grep { $sym =~ /$_/ } @{ $self->{trustme} };
}

sub _CvGV {
    my $self = shift;
    my $cv   = shift;
    my $b_cv = B::svref_2object($cv);

    # perl 5.6.2's B doesn't have an object_2svref.  in 5.8 you can
    # just do this:
    # return *{ $b_cv->GV->object_2svref };
    # but for backcompat we're forced into this uglyness:
    no strict 'refs';
    return *{ $b_cv->GV->STASH->NAME . "::" . $b_cv->GV->NAME };
}

package Pod::Coverage::Extractor;
use Pod::Parser;
use base 'Pod::Parser';

use constant debug => 0;

# extract subnames from a pod stream
sub command {
    my $self = shift;
    my ( $command, $text, $line_num ) = @_;
    if ( $command eq 'item' || $command =~ /^head(?:2|3|4)/ ) {

        # take a closer look
        my @pods = ( $text =~ /\s*([^\s\|,\/]+)/g );
        $self->{recent} = [];

        foreach my $pod (@pods) {
            print "Considering: '$pod'\n" if debug;

            # it's dressed up like a method cal
            $pod =~ /-E<\s*gt\s*>(.*)/ and $pod = $1;
            $pod =~ /->(.*)/           and $pod = $1;

            # it's used as a (bare) fully qualified name
            $pod =~ /\w+(?:::\w+)*::(\w+)/ and $pod = $1;

            # it's wrapped in a pod style B<>
            $pod =~ s/[A-Z]<//g;
            $pod =~ s/>//g;

            # has arguments, or a semicolon
            $pod =~ /(\w+)\s*[;\(]/ and $pod = $1;

            print "Adding: '$pod'\n" if debug;
            push @{ $self->{ $self->{nonwhitespace}
                    ? "recent"
                    : "identifiers" } }, $pod;
        }
    }
}

sub textblock {
    my $self = shift;
    my ( $text, $line_num ) = shift;
    if ( $self->{nonwhitespace} and $text =~ /\S/ and $self->{recent} ) {
        push @{ $self->{identifiers} }, @{ $self->{recent} };
        $self->{recent} = [];
    }
}

1;

__END__

=back

=head1 BUGS

Due to the method used to identify documented subroutines
C<Pod::Coverage> may completely miss your house style and declare your
code undocumented.  Patches and/or failing tests welcome.

=head1 TODO

=over

=item Widen the rules for identifying documentation

=item Improve the code coverage of the test suite.  C<Devel::Cover> rocks so hard.

=back

=head1 SEE ALSO

L<Test::More>, L<Devel::Cover>

=head1 AUTHORS

Richard Clamp <richardc@unixbeard.net>

Michael Stevens <mstevens@etla.org>

some contributions from David Cantrell <david@cantrell.org.uk>

=head1 COPYRIGHT

Copyright (c) 2001, 2003, 2004, 2006, 2007 Richard Clamp, Michael
Stevens. All rights reserved.  This program is free software; you can
redistribute it and/or modify it under the same terms as Perl itself.

=cut
