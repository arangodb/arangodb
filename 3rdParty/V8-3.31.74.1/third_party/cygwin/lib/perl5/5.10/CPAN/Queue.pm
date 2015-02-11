# -*- Mode: cperl; coding: utf-8; cperl-indent-level: 4 -*-
use strict;
package CPAN::Queue::Item;

# CPAN::Queue::Item::new ;
sub new {
    my($class,@attr) = @_;
    my $self = bless { @attr }, $class;
    return $self;
}

sub as_string {
    my($self) = @_;
    $self->{qmod};
}

# r => requires, b => build_requires, c => commandline
sub reqtype {
    my($self) = @_;
    $self->{reqtype};
}

package CPAN::Queue;

# One use of the queue is to determine if we should or shouldn't
# announce the availability of a new CPAN module

# Now we try to use it for dependency tracking. For that to happen
# we need to draw a dependency tree and do the leaves first. This can
# easily be reached by running CPAN.pm recursively, but we don't want
# to waste memory and run into deep recursion. So what we can do is
# this:

# CPAN::Queue is the package where the queue is maintained. Dependencies
# often have high priority and must be brought to the head of the queue,
# possibly by jumping the queue if they are already there. My first code
# attempt tried to be extremely correct. Whenever a module needed
# immediate treatment, I either unshifted it to the front of the queue,
# or, if it was already in the queue, I spliced and let it bypass the
# others. This became a too correct model that made it impossible to put
# an item more than once into the queue. Why would you need that? Well,
# you need temporary duplicates as the manager of the queue is a loop
# that
#
#  (1) looks at the first item in the queue without shifting it off
#
#  (2) cares for the item
#
#  (3) removes the item from the queue, *even if its agenda failed and
#      even if the item isn't the first in the queue anymore* (that way
#      protecting against never ending queues)
#
# So if an item has prerequisites, the installation fails now, but we
# want to retry later. That's easy if we have it twice in the queue.
#
# I also expect insane dependency situations where an item gets more
# than two lives in the queue. Simplest example is triggered by 'install
# Foo Foo Foo'. People make this kind of mistakes and I don't want to
# get in the way. I wanted the queue manager to be a dumb servant, not
# one that knows everything.
#
# Who would I tell in this model that the user wants to be asked before
# processing? I can't attach that information to the module object,
# because not modules are installed but distributions. So I'd have to
# tell the distribution object that it should ask the user before
# processing. Where would the question be triggered then? Most probably
# in CPAN::Distribution::rematein.

use vars qw{ @All $VERSION };
$VERSION = sprintf "%.6f", substr(q$Rev: 2212 $,4)/1000000 + 5.4;

# CPAN::Queue::queue_item ;
sub queue_item {
    my($class,@attr) = @_;
    my $item = "$class\::Item"->new(@attr);
    $class->qpush($item);
    return 1;
}

# CPAN::Queue::qpush ;
sub qpush {
    my($class,$obj) = @_;
    push @All, $obj;
    CPAN->debug(sprintf("in new All[%s]",
                        join("",map {sprintf " %s\[%s]\n",$_->{qmod},$_->{reqtype}} @All),
                       )) if $CPAN::DEBUG;
}

# CPAN::Queue::first ;
sub first {
    my $obj = $All[0];
    $obj;
}

# CPAN::Queue::delete_first ;
sub delete_first {
    my($class,$what) = @_;
    my $i;
    for my $i (0..$#All) {
        if (  $All[$i]->{qmod} eq $what ) {
            splice @All, $i, 1;
            return;
        }
    }
}

# CPAN::Queue::jumpqueue ;
sub jumpqueue {
    my $class = shift;
    my @what = @_;
    CPAN->debug(sprintf("before jumpqueue All[%s] what[%s]",
                        join("",
                             map {sprintf " %s\[%s]\n",$_->{qmod},$_->{reqtype}} @All, @what
                            ))) if $CPAN::DEBUG;
    unless (defined $what[0]{reqtype}) {
        # apparently it was not the Shell that sent us this enquiry,
        # treat it as commandline
        $what[0]{reqtype} = "c";
    }
    my $inherit_reqtype = $what[0]{reqtype} =~ /^(c|r)$/ ? "r" : "b";
  WHAT: for my $what_tuple (@what) {
        my($what,$reqtype) = @$what_tuple{qw(qmod reqtype)};
        if ($reqtype eq "r"
            &&
            $inherit_reqtype eq "b"
           ) {
            $reqtype = "b";
        }
        my $jumped = 0;
        for (my $i=0; $i<$#All;$i++) { #prevent deep recursion
            # CPAN->debug("i[$i]this[$All[$i]{qmod}]what[$what]") if $CPAN::DEBUG;
            if ($All[$i]{qmod} eq $what) {
                $jumped++;
                if ($jumped >= 50) {
                    die "PANIC: object[$what] 50 instances on the queue, looks like ".
                        "some recursiveness has hit";
                } elsif ($jumped > 25) { # one's OK if e.g. just processing
                                    # now; more are OK if user typed
                                    # it several times
                    my $sleep = sprintf "%.1f", $jumped/10;
                    $CPAN::Frontend->mywarn(
qq{Warning: Object [$what] queued $jumped times, sleeping $sleep secs!\n}
                    );
                    $CPAN::Frontend->mysleep($sleep);
                    # next WHAT;
                }
            }
        }
        my $obj = "$class\::Item"->new(
                                       qmod => $what,
                                       reqtype => $reqtype
                                      );
        unshift @All, $obj;
    }
    CPAN->debug(sprintf("after jumpqueue All[%s]",
                        join("",map {sprintf " %s\[%s]\n",$_->{qmod},$_->{reqtype}} @All)
                       )) if $CPAN::DEBUG;
}

# CPAN::Queue::exists ;
sub exists {
    my($self,$what) = @_;
    my @all = map { $_->{qmod} } @All;
    my $exists = grep { $_->{qmod} eq $what } @All;
    # warn "in exists what[$what] all[@all] exists[$exists]";
    $exists;
}

# CPAN::Queue::delete ;
sub delete {
    my($self,$mod) = @_;
    @All = grep { $_->{qmod} ne $mod } @All;
    CPAN->debug(sprintf("after delete mod[%s] All[%s]",
                        $mod,
                        join("",map {sprintf " %s\[%s]\n",$_->{qmod},$_->{reqtype}} @All)
                       )) if $CPAN::DEBUG;
}

# CPAN::Queue::nullify_queue ;
sub nullify_queue {
    @All = ();
}

1;

__END__

=head1 LICENSE

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
