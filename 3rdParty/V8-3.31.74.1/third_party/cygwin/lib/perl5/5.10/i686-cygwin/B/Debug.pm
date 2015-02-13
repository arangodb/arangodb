package B::Debug;

our $VERSION = '1.05_02';

use strict;
use B qw(peekop class walkoptree walkoptree_exec
         main_start main_root cstring sv_undef @specialsv_name);
# <=5.008 had @specialsv_name exported from B::Asmdata
BEGIN {
    use Config;
    my $ithreads = $Config{'useithreads'} eq 'define';
    eval qq{
	sub ITHREADS() { $ithreads }
	sub VERSION() { $] }
    }; die $@ if $@;
}

my %done_gv;

sub _printop {
  my $op = shift;
  my $addr = ${$op} ? $op->ppaddr : '';
  $addr =~ s/^PL_ppaddr// if $addr;
  return sprintf "0x%x %s %s", ${$op}, ${$op} ? class($op) : '', $addr;
}

sub B::OP::debug {
    my ($op) = @_;
    printf <<'EOT', class($op), $$op, $op->ppaddr, _printop($op->next), _printop($op->sibling), $op->targ, $op->type;
%s (0x%lx)
	op_ppaddr	%s
	op_next		%s
	op_sibling	%s
	op_targ		%d
	op_type		%d
EOT
    if ($] > 5.009) {
	printf <<'EOT', $op->opt;
	op_opt		%d
EOT
    } else {
	printf <<'EOT', $op->seq;
	op_seq		%d
EOT
    }
    printf <<'EOT', $op->flags, $op->private;
	op_flags	%d
	op_private	%d
EOT
}

sub B::UNOP::debug {
    my ($op) = @_;
    $op->B::OP::debug();
    printf "\top_first\t%s\n", _printop($op->first);
}

sub B::BINOP::debug {
    my ($op) = @_;
    $op->B::UNOP::debug();
    printf "\top_last \t%s\n", _printop($op->last);
}

sub B::LOOP::debug {
    my ($op) = @_;
    $op->B::BINOP::debug();
    printf <<'EOT', _printop($op->redoop), _printop($op->nextop), _printop($op->lastop);
	op_redoop	%s
	op_nextop	%s
	op_lastop	%s
EOT
}

sub B::LOGOP::debug {
    my ($op) = @_;
    $op->B::UNOP::debug();
    printf "\top_other\t%s\n", _printop($op->other);
}

sub B::LISTOP::debug {
    my ($op) = @_;
    $op->B::BINOP::debug();
    printf "\top_children\t%d\n", $op->children;
}

sub B::PMOP::debug {
    my ($op) = @_;
    $op->B::LISTOP::debug();
    printf "\top_pmreplroot\t0x%x\n", ${$op->pmreplroot};
    printf "\top_pmreplstart\t0x%x\n", ${$op->pmreplstart};
    printf "\top_pmnext\t0x%x\n", ${$op->pmnext} if $] < 5.009005;
    if (ITHREADS) {
      printf "\top_pmstashpv\t%s\n", cstring($op->pmstashpv);
      printf "\top_pmoffset\t%d\n", $op->pmoffset;
    } else {
      printf "\top_pmstash\t%s\n", cstring($op->pmstash);
    }
    printf "\top_precomp->precomp\t%s\n", cstring($op->precomp);
    printf "\top_pmflags\t0x%x\n", $op->pmflags;
    printf "\top_reflags\t0x%x\n", $op->reflags if $] >= 5.009;
    printf "\top_pmpermflags\t0x%x\n", $op->pmpermflags if $] < 5.009;
    printf "\top_pmdynflags\t0x%x\n", $op->pmdynflags if $] < 5.009;
    $op->pmreplroot->debug;
}

sub B::COP::debug {
    my ($op) = @_;
    $op->B::OP::debug();
    my $cop_io = class($op->io) eq 'SPECIAL' ? '' : $op->io->as_string;
    printf <<'EOT', $op->label, $op->stashpv, $op->file, $op->cop_seq, $op->arybase, $op->line, ${$op->warnings}, cstring($cop_io);
	cop_label	"%s"
	cop_stashpv	"%s"
	cop_file	"%s"
	cop_seq		%d
	cop_arybase	%d
	cop_line	%d
	cop_warnings	0x%x
	cop_io		%s
EOT
}

sub B::SVOP::debug {
    my ($op) = @_;
    $op->B::OP::debug();
    printf "\top_sv\t\t0x%x\n", ${$op->sv};
    $op->sv->debug;
}

sub B::PVOP::debug {
    my ($op) = @_;
    $op->B::OP::debug();
    printf "\top_pv\t\t%s\n", cstring($op->pv);
}

sub B::PADOP::debug {
    my ($op) = @_;
    $op->B::OP::debug();
    printf "\top_padix\t%ld\n", $op->padix;
}

sub B::NULL::debug {
    my ($sv) = @_;
    if ($$sv == ${sv_undef()}) {
	print "&sv_undef\n";
    } else {
	printf "NULL (0x%x)\n", $$sv;
    }
}

sub B::SV::debug {
    my ($sv) = @_;
    if (!$$sv) {
	print class($sv), " = NULL\n";
	return;
    }
    printf <<'EOT', class($sv), $$sv, $sv->REFCNT, $sv->FLAGS;
%s (0x%x)
	REFCNT		%d
	FLAGS		0x%x
EOT
}

sub B::RV::debug {
    my ($rv) = @_;
    B::SV::debug($rv);
    printf <<'EOT', ${$rv->RV};
	RV		0x%x
EOT
    $rv->RV->debug;
}

sub B::PV::debug {
    my ($sv) = @_;
    $sv->B::SV::debug();
    my $pv = $sv->PV();
    printf <<'EOT', cstring($pv), length($pv);
	xpv_pv		%s
	xpv_cur		%d
EOT
}

sub B::IV::debug {
    my ($sv) = @_;
    $sv->B::SV::debug();
    printf "\txiv_iv\t\t%d\n", $sv->IV;
}

sub B::NV::debug {
    my ($sv) = @_;
    $sv->B::IV::debug();
    printf "\txnv_nv\t\t%s\n", $sv->NV;
}

sub B::PVIV::debug {
    my ($sv) = @_;
    $sv->B::PV::debug();
    printf "\txiv_iv\t\t%d\n", $sv->IV;
}

sub B::PVNV::debug {
    my ($sv) = @_;
    $sv->B::PVIV::debug();
    printf "\txnv_nv\t\t%s\n", $sv->NV;
}

sub B::PVLV::debug {
    my ($sv) = @_;
    $sv->B::PVNV::debug();
    printf "\txlv_targoff\t%d\n", $sv->TARGOFF;
    printf "\txlv_targlen\t%u\n", $sv->TARGLEN;
    printf "\txlv_type\t%s\n", cstring(chr($sv->TYPE));
}

sub B::BM::debug {
    my ($sv) = @_;
    $sv->B::PVNV::debug();
    printf "\txbm_useful\t%d\n", $sv->USEFUL;
    printf "\txbm_previous\t%u\n", $sv->PREVIOUS;
    printf "\txbm_rare\t%s\n", cstring(chr($sv->RARE));
}

sub B::CV::debug {
    my ($sv) = @_;
    $sv->B::PVNV::debug();
    my ($stash) = $sv->STASH;
    my ($start) = $sv->START;
    my ($root) = $sv->ROOT;
    my ($padlist) = $sv->PADLIST;
    my ($file) = $sv->FILE;
    my ($gv) = $sv->GV;
    printf <<'EOT', $$stash, $$start, $$root, $$gv, $file, $sv->DEPTH, $padlist, ${$sv->OUTSIDE}, $sv->OUTSIDE_SEQ;
	STASH		0x%x
	START		0x%x
	ROOT		0x%x
	GV		0x%x
	FILE		%s
	DEPTH		%d
	PADLIST		0x%x
	OUTSIDE		0x%x
	OUTSIDE_SEQ	%d
EOT
    $start->debug if $start;
    $root->debug if $root;
    $gv->debug if $gv;
    $padlist->debug if $padlist;
}

sub B::AV::debug {
    my ($av) = @_;
    $av->B::SV::debug;
    my(@array) = $av->ARRAY;
    print "\tARRAY\t\t(", join(", ", map("0x" . $$_, @array)), ")\n";
    printf <<'EOT', scalar(@array), $av->MAX, $av->OFF;
	FILL		%d
	MAX		%d
	OFF		%d
EOT
    printf <<'EOT', $av->AvFLAGS if $] < 5.009;
	AvFLAGS		%d
EOT
}

sub B::GV::debug {
    my ($gv) = @_;
    if ($done_gv{$$gv}++) {
	printf "GV %s::%s\n", $gv->STASH->NAME, $gv->SAFENAME;
	return;
    }
    my ($sv) = $gv->SV;
    my ($av) = $gv->AV;
    my ($cv) = $gv->CV;
    $gv->B::SV::debug;
    printf <<'EOT', $gv->SAFENAME, $gv->STASH->NAME, $gv->STASH, $$sv, $gv->GvREFCNT, $gv->FORM, $$av, ${$gv->HV}, ${$gv->EGV}, $$cv, $gv->CVGEN, $gv->LINE, $gv->FILE, $gv->GvFLAGS;
	NAME		%s
	STASH		%s (0x%x)
	SV		0x%x
	GvREFCNT	%d
	FORM		0x%x
	AV		0x%x
	HV		0x%x
	EGV		0x%x
	CV		0x%x
	CVGEN		%d
	LINE		%d
	FILE		%s
	GvFLAGS		0x%x
EOT
    $sv->debug if $sv;
    $av->debug if $av;
    $cv->debug if $cv;
}

sub B::SPECIAL::debug {
    my $sv = shift;
    print $specialsv_name[$$sv], "\n";
}

sub compile {
    my $order = shift;
    B::clearsym();
    if ($order && $order eq "exec") {
        return sub { walkoptree_exec(main_start, "debug") }
    } else {
        return sub { walkoptree(main_root, "debug") }
    }
}

1;

__END__

=head1 NAME

B::Debug - Walk Perl syntax tree, printing debug info about ops

=head1 SYNOPSIS

	perl -MO=Debug[,OPTIONS] foo.pl

=head1 DESCRIPTION

See F<ext/B/README> and the newer L<B::Concise>, L<B::Terse>.

=head1 OPTIONS

With option -exec, walks tree in execute order,
otherwise in basic order.

=head1 AUTHOR

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>

=cut
