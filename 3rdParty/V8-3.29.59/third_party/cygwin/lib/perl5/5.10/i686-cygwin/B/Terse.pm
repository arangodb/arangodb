package B::Terse;

our $VERSION = '1.05';

use strict;
use B qw(class @specialsv_name);
use B::Concise qw(concise_subref set_style_standard);
use Carp;

sub terse {
    my ($order, $subref) = @_;
    set_style_standard("terse");
    if ($order eq "exec") {
	concise_subref('exec', $subref);
    } else {
	concise_subref('basic', $subref);
    }
}

sub compile {
    my @args = @_;
    my $order = @args ? shift(@args) : "";
    $order = "-exec" if $order eq "exec";
    unshift @args, $order if $order ne "";
    B::Concise::compile("-terse", @args);
}

sub indent {
    my ($level) = @_ ? shift : 0;
    return "    " x $level;
}

# Don't use this, at least on OPs in subroutines: it has no way of
# getting to the pad, and will give wrong answers or crash.
sub B::OP::terse {
    carp "B::OP::terse is deprecated; use B::Concise instead";
    B::Concise::b_terse(@_);
}

sub B::SV::terse {
    my($sv, $level) = (@_, 0);
    my %info;
    B::Concise::concise_sv($sv, \%info);
    my $s = indent($level)
	. B::Concise::fmt_line(\%info, $sv,
				 "#svclass~(?((#svaddr))?)~#svval", 0);
    chomp $s;
    print "$s\n" unless defined wantarray;
    $s;
}

sub B::NULL::terse {
    my ($sv, $level) = (@_, 0);
    my $s = indent($level) . sprintf "%s (0x%lx)", class($sv), $$sv;
    print "$s\n" unless defined wantarray;
    $s;
}

sub B::SPECIAL::terse {
    my ($sv, $level) = (@_, 0);
    my $s = indent($level)
	. sprintf( "%s #%d %s", class($sv), $$sv, $specialsv_name[$$sv]);
    print "$s\n" unless defined wantarray;
    $s;
}

1;

__END__

=head1 NAME

B::Terse - Walk Perl syntax tree, printing terse info about ops

=head1 SYNOPSIS

	perl -MO=Terse[,OPTIONS] foo.pl

=head1 DESCRIPTION

This version of B::Terse is really just a wrapper that calls B::Concise
with the B<-terse> option. It is provided for compatibility with old scripts
(and habits) but using B::Concise directly is now recommended instead.

For compatibility with the old B::Terse, this module also adds a
method named C<terse> to B::OP and B::SV objects. The B::SV method is
largely compatible with the old one, though authors of new software
might be advised to choose a more user-friendly output format. The
B::OP C<terse> method, however, doesn't work well. Since B::Terse was
first written, much more information in OPs has migrated to the
scratchpad datastructure, but the C<terse> interface doesn't have any
way of getting to the correct pad. As a kludge, the new version will
always use the pad for the main program, but for OPs in subroutines
this will give the wrong answer or crash.

=head1 AUTHOR

The original version of B::Terse was written by Malcolm Beattie,
E<lt>mbeattie@sable.ox.ac.ukE<gt>. This wrapper was written by Stephen
McCamant, E<lt>smcc@MIT.EDUE<gt>.

=cut
