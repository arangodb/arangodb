package B::Concise;
# Copyright (C) 2000-2003 Stephen McCamant. All rights reserved.
# This program is free software; you can redistribute and/or modify it
# under the same terms as Perl itself.

# Note: we need to keep track of how many use declarations/BEGIN
# blocks this module uses, so we can avoid printing them when user
# asks for the BEGIN blocks in her program. Update the comments and
# the count in concise_specials if you add or delete one. The
# -MO=Concise counts as use #1.

use strict; # use #2
use warnings; # uses #3 and #4, since warnings uses Carp

use Exporter (); # use #5

our $VERSION   = "0.75";
our @ISA       = qw(Exporter);
our @EXPORT_OK = qw( set_style set_style_standard add_callback
		     concise_subref concise_cv concise_main
		     add_style walk_output compile reset_sequence );
our %EXPORT_TAGS =
    ( io	=> [qw( walk_output compile reset_sequence )],
      style	=> [qw( add_style set_style_standard )],
      cb	=> [qw( add_callback )],
      mech	=> [qw( concise_subref concise_cv concise_main )],  );

# use #6
use B qw(class ppname main_start main_root main_cv cstring svref_2object
	 SVf_IOK SVf_NOK SVf_POK SVf_IVisUV SVf_FAKE OPf_KIDS OPf_SPECIAL
	 CVf_ANON PAD_FAKELEX_ANON PAD_FAKELEX_MULTI SVf_ROK);

my %style =
  ("terse" =>
   ["(?(#label =>\n)?)(*(    )*)#class (#addr) #name (?([#targ])?) "
    . "#svclass~(?((#svaddr))?)~#svval~(?(label \"#coplabel\")?)\n",
    "(*(    )*)goto #class (#addr)\n",
    "#class pp_#name"],
   "concise" =>
   ["#hyphseq2 (*(   (x( ;)x))*)<#classsym> #exname#arg(?([#targarglife])?)"
    . "~#flags(?(/#private)?)(?(:#hints)?)(x(;~->#next)x)\n"
    , "  (*(    )*)     goto #seq\n",
    "(?(<#seq>)?)#exname#arg(?([#targarglife])?)"],
   "linenoise" =>
   ["(x(;(*( )*))x)#noise#arg(?([#targarg])?)(x( ;\n)x)",
    "gt_#seq ",
    "(?(#seq)?)#noise#arg(?([#targarg])?)"],
   "debug" =>
   ["#class (#addr)\n\top_next\t\t#nextaddr\n\top_sibling\t#sibaddr\n\t"
    . "op_ppaddr\tPL_ppaddr[OP_#NAME]\n\top_type\t\t#typenum\n" .
    ($] > 5.009 ? '' : "\top_seq\t\t#seqnum\n")
    . "\top_flags\t#flagval\n\top_private\t#privval\t#hintsval\n"
    . "(?(\top_first\t#firstaddr\n)?)(?(\top_last\t\t#lastaddr\n)?)"
    . "(?(\top_sv\t\t#svaddr\n)?)",
    "    GOTO #addr\n",
    "#addr"],
   "env" => [$ENV{B_CONCISE_FORMAT}, $ENV{B_CONCISE_GOTO_FORMAT},
	     $ENV{B_CONCISE_TREE_FORMAT}],
  );

# Renderings, ie how Concise prints, is controlled by these vars
# primary:
our $stylename;		# selects current style from %style
my $order = "basic";	# how optree is walked & printed: basic, exec, tree

# rendering mechanics:
# these 'formats' are the line-rendering templates
# they're updated from %style when $stylename changes
my ($format, $gotofmt, $treefmt);

# lesser players:
my $base = 36;		# how <sequence#> is displayed
my $big_endian = 1;	# more <sequence#> display
my $tree_style = 0;	# tree-order details
my $banner = 1;		# print banner before optree is traversed
my $do_main = 0;	# force printing of main routine
my $show_src;		# show source code

# another factor: can affect all styles!
our @callbacks;		# allow external management

set_style_standard("concise");

my $curcv;
my $cop_seq_base;

sub set_style {
    ($format, $gotofmt, $treefmt) = @_;
    #warn "set_style: deprecated, use set_style_standard instead\n"; # someday
    die "expecting 3 style-format args\n" unless @_ == 3;
}

sub add_style {
    my ($newstyle,@args) = @_;
    die "style '$newstyle' already exists, choose a new name\n"
	if exists $style{$newstyle};
    die "expecting 3 style-format args\n" unless @args == 3;
    $style{$newstyle} = [@args];
    $stylename = $newstyle; # update rendering state
}

sub set_style_standard {
    ($stylename) = @_; # update rendering state
    die "err: style '$stylename' unknown\n" unless exists $style{$stylename};
    set_style(@{$style{$stylename}});
}

sub add_callback {
    push @callbacks, @_;
}

# output handle, used with all Concise-output printing
our $walkHandle;	# public for your convenience
BEGIN { $walkHandle = \*STDOUT }

sub walk_output { # updates $walkHandle
    my $handle = shift;
    return $walkHandle unless $handle; # allow use as accessor

    if (ref $handle eq 'SCALAR') {
	require Config;
	die "no perlio in this build, can't call walk_output (\\\$scalar)\n"
	    unless $Config::Config{useperlio};
	# in 5.8+, open(FILEHANDLE,MODE,REFERENCE) writes to string
	open my $tmp, '>', $handle;	# but cant re-set existing STDOUT
	$walkHandle = $tmp;		# so use my $tmp as intermediate var
	return $walkHandle;
    }
    my $iotype = ref $handle;
    die "expecting argument/object that can print\n"
	unless $iotype eq 'GLOB' or $iotype and $handle->can('print');
    $walkHandle = $handle;
}

sub concise_subref {
    my($order, $coderef, $name) = @_;
    my $codeobj = svref_2object($coderef);

    return concise_stashref(@_)
	unless ref $codeobj eq 'B::CV';
    concise_cv_obj($order, $codeobj, $name);
}

sub concise_stashref {
    my($order, $h) = @_;
    local *s;
    foreach my $k (sort keys %$h) {
	next unless defined $h->{$k};
	*s = $h->{$k};
	my $coderef = *s{CODE} or next;
	reset_sequence();
	print "FUNC: ", *s, "\n";
	my $codeobj = svref_2object($coderef);
	next unless ref $codeobj eq 'B::CV';
	eval { concise_cv_obj($order, $codeobj, $k) };
	warn "err $@ on $codeobj" if $@;
    }
}

# This should have been called concise_subref, but it was exported
# under this name in versions before 0.56
*concise_cv = \&concise_subref;

sub concise_cv_obj {
    my ($order, $cv, $name) = @_;
    # name is either a string, or a CODE ref (copy of $cv arg??)

    $curcv = $cv;

    if (ref($cv->XSUBANY) =~ /B::(\w+)/) {
	print $walkHandle "$name is a constant sub, optimized to a $1\n";
	return;
    }
    if ($cv->XSUB) {
	print $walkHandle "$name is XS code\n";
	return;
    }
    if (class($cv->START) eq "NULL") {
	no strict 'refs';
	if (ref $name eq 'CODE') {
	    print $walkHandle "coderef $name has no START\n";
	}
	elsif (exists &$name) {
	    print $walkHandle "$name exists in stash, but has no START\n";
	}
	else {
	    print $walkHandle "$name not in symbol table\n";
	}
	return;
    }
    sequence($cv->START);
    if ($order eq "exec") {
	walk_exec($cv->START);
    }
    elsif ($order eq "basic") {
	# walk_topdown($cv->ROOT, sub { $_[0]->concise($_[1]) }, 0);
	my $root = $cv->ROOT;
	unless (ref $root eq 'B::NULL') {
	    walk_topdown($root, sub { $_[0]->concise($_[1]) }, 0);
	} else {
	    print $walkHandle "B::NULL encountered doing ROOT on $cv. avoiding disaster\n";
	}
    } else {
	print $walkHandle tree($cv->ROOT, 0);
    }
}

sub concise_main {
    my($order) = @_;
    sequence(main_start);
    $curcv = main_cv;
    if ($order eq "exec") {
	return if class(main_start) eq "NULL";
	walk_exec(main_start);
    } elsif ($order eq "tree") {
	return if class(main_root) eq "NULL";
	print $walkHandle tree(main_root, 0);
    } elsif ($order eq "basic") {
	return if class(main_root) eq "NULL";
	walk_topdown(main_root,
		     sub { $_[0]->concise($_[1]) }, 0);
    }
}

sub concise_specials {
    my($name, $order, @cv_s) = @_;
    my $i = 1;
    if ($name eq "BEGIN") {
	splice(@cv_s, 0, 8); # skip 7 BEGIN blocks in this file. NOW 8 ??
    } elsif ($name eq "CHECK") {
	pop @cv_s; # skip the CHECK block that calls us
    }
    for my $cv (@cv_s) {
	print $walkHandle "$name $i:\n";
	$i++;
	concise_cv_obj($order, $cv, $name);
    }
}

my $start_sym = "\e(0"; # "\cN" sometimes also works
my $end_sym   = "\e(B"; # "\cO" respectively

my @tree_decorations =
  (["  ", "--", "+-", "|-", "| ", "`-", "-", 1],
   [" ", "-", "+", "+", "|", "`", "", 0],
   ["  ", map("$start_sym$_$end_sym", "qq", "wq", "tq", "x ", "mq", "q"), 1],
   [" ", map("$start_sym$_$end_sym", "q", "w", "t", "x", "m"), "", 0],
  );

my @render_packs; # collect -stash=<packages>

sub compileOpts {
    # set rendering state from options and args
    my (@options,@args);
    if (@_) {
	@options = grep(/^-/, @_);
	@args = grep(!/^-/, @_);
    }
    for my $o (@options) {
	# mode/order
	if ($o eq "-basic") {
	    $order = "basic";
	} elsif ($o eq "-exec") {
	    $order = "exec";
	} elsif ($o eq "-tree") {
	    $order = "tree";
	}
	# tree-specific
	elsif ($o eq "-compact") {
	    $tree_style |= 1;
	} elsif ($o eq "-loose") {
	    $tree_style &= ~1;
	} elsif ($o eq "-vt") {
	    $tree_style |= 2;
	} elsif ($o eq "-ascii") {
	    $tree_style &= ~2;
	}
	# sequence numbering
	elsif ($o =~ /^-base(\d+)$/) {
	    $base = $1;
	} elsif ($o eq "-bigendian") {
	    $big_endian = 1;
	} elsif ($o eq "-littleendian") {
	    $big_endian = 0;
	}
	# miscellaneous, presentation
	elsif ($o eq "-nobanner") {
	    $banner = 0;
	} elsif ($o eq "-banner") {
	    $banner = 1;
	}
	elsif ($o eq "-main") {
	    $do_main = 1;
	} elsif ($o eq "-nomain") {
	    $do_main = 0;
	} elsif ($o eq "-src") {
	    $show_src = 1;
	}
	elsif ($o =~ /^-stash=(.*)/) {
	    my $pkg = $1;
	    no strict 'refs';
	    eval "require $pkg" unless defined %{$pkg.'::'};
	    push @render_packs, $pkg;
	}
	# line-style options
	elsif (exists $style{substr($o, 1)}) {
	    $stylename = substr($o, 1);
	    set_style_standard($stylename);
	} else {
	    warn "Option $o unrecognized";
	}
    }
    return (@args);
}

sub compile {
    my (@args) = compileOpts(@_);
    return sub {
	my @newargs = compileOpts(@_); # accept new rendering options
	warn "disregarding non-options: @newargs\n" if @newargs;

	for my $objname (@args) {
	    next unless $objname; # skip null args to avoid noisy responses

	    if ($objname eq "BEGIN") {
		concise_specials("BEGIN", $order,
				 B::begin_av->isa("B::AV") ?
				 B::begin_av->ARRAY : ());
	    } elsif ($objname eq "INIT") {
		concise_specials("INIT", $order,
				 B::init_av->isa("B::AV") ?
				 B::init_av->ARRAY : ());
	    } elsif ($objname eq "CHECK") {
		concise_specials("CHECK", $order,
				 B::check_av->isa("B::AV") ?
				 B::check_av->ARRAY : ());
	    } elsif ($objname eq "UNITCHECK") {
		concise_specials("UNITCHECK", $order,
				 B::unitcheck_av->isa("B::AV") ?
				 B::unitcheck_av->ARRAY : ());
	    } elsif ($objname eq "END") {
		concise_specials("END", $order,
				 B::end_av->isa("B::AV") ?
				 B::end_av->ARRAY : ());
	    }
	    else {
		# convert function names to subrefs
		my $objref;
		if (ref $objname) {
		    print $walkHandle "B::Concise::compile($objname)\n"
			if $banner;
		    $objref = $objname;
		} else {
		    $objname = "main::" . $objname unless $objname =~ /::/;
		    print $walkHandle "$objname:\n";
		    no strict 'refs';
		    unless (exists &$objname) {
			print $walkHandle "err: unknown function ($objname)\n";
			return;
		    }
		    $objref = \&$objname;
		}
		concise_subref($order, $objref, $objname);
	    }
	}
	for my $pkg (@render_packs) {
	    no strict 'refs';
	    concise_stashref($order, \%{$pkg.'::'});
	}

	if (!@args or $do_main or @render_packs) {
	    print $walkHandle "main program:\n" if $do_main;
	    concise_main($order);
	}
	return @args;	# something
    }
}

my %labels;
my $lastnext;	# remembers op-chain, used to insert gotos

my %opclass = ('OP' => "0", 'UNOP' => "1", 'BINOP' => "2", 'LOGOP' => "|",
	       'LISTOP' => "@", 'PMOP' => "/", 'SVOP' => "\$", 'GVOP' => "*",
	       'PVOP' => '"', 'LOOP' => "{", 'COP' => ";", 'PADOP' => "#");

no warnings 'qw'; # "Possible attempt to put comments..."; use #7
my @linenoise =
  qw'#  () sc (  @? 1  $* gv *{ m$ m@ m% m? p/ *$ $  $# & a& pt \\ s\\ rf bl
     `  *? <> ?? ?/ r/ c/ // qr s/ /c y/ =  @= C  sC Cp sp df un BM po +1 +I
     -1 -I 1+ I+ 1- I- ** *  i* /  i/ %$ i% x  +  i+ -  i- .  "  << >> <  i<
     >  i> <= i, >= i. == i= != i! <? i? s< s> s, s. s= s! s? b& b^ b| -0 -i
     !  ~  a2 si cs rd sr e^ lg sq in %x %o ab le ss ve ix ri sf FL od ch cy
     uf lf uc lc qm @  [f [  @[ eh vl ky dl ex %  ${ @{ uk pk st jn )  )[ a@
     a% sl +] -] [- [+ so rv GS GW MS MW .. f. .f && || ^^ ?: &= |= -> s{ s}
     v} ca wa di rs ;; ;  ;d }{ {  }  {} f{ it {l l} rt }l }n }r dm }g }e ^o
     ^c ^| ^# um bm t~ u~ ~d DB db ^s se ^g ^r {w }w pf pr ^O ^K ^R ^W ^d ^v
     ^e ^t ^k t. fc ic fl .s .p .b .c .l .a .h g1 s1 g2 s2 ?. l? -R -W -X -r
     -w -x -e -o -O -z -s -M -A -C -S -c -b -f -d -p -l -u -g -k -t -T -B cd
     co cr u. cm ut r. l@ s@ r@ mD uD oD rD tD sD wD cD f$ w$ p$ sh e$ k$ g3
     g4 s4 g5 s5 T@ C@ L@ G@ A@ S@ Hg Hc Hr Hw Mg Mc Ms Mr Sg Sc So rq do {e
     e} {t t} g6 G6 6e g7 G7 7e g8 G8 8e g9 G9 9e 6s 7s 8s 9s 6E 7E 8E 9E Pn
     Pu GP SP EP Gn Gg GG SG EG g0 c$ lk t$ ;s n> // /= CO';

my $chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

sub op_flags { # common flags (see BASOP.op_flags in op.h)
    my($x) = @_;
    my(@v);
    push @v, "v" if ($x & 3) == 1;
    push @v, "s" if ($x & 3) == 2;
    push @v, "l" if ($x & 3) == 3;
    push @v, "K" if $x & 4;
    push @v, "P" if $x & 8;
    push @v, "R" if $x & 16;
    push @v, "M" if $x & 32;
    push @v, "S" if $x & 64;
    push @v, "*" if $x & 128;
    return join("", @v);
}

sub base_n {
    my $x = shift;
    return "-" . base_n(-$x) if $x < 0;
    my $str = "";
    do { $str .= substr($chars, $x % $base, 1) } while $x = int($x / $base);
    $str = reverse $str if $big_endian;
    return $str;
}

my %sequence_num;
my $seq_max = 1;

sub reset_sequence {
    # reset the sequence
    %sequence_num = ();
    $seq_max = 1;
    $lastnext = 0;
}

sub seq {
    my($op) = @_;
    return "-" if not exists $sequence_num{$$op};
    return base_n($sequence_num{$$op});
}

sub walk_topdown {
    my($op, $sub, $level) = @_;
    $sub->($op, $level);
    if ($op->flags & OPf_KIDS) {
	for (my $kid = $op->first; $$kid; $kid = $kid->sibling) {
	    walk_topdown($kid, $sub, $level + 1);
	}
    }
    elsif (class($op) eq "PMOP") {
	my $maybe_root = $op->pmreplroot;
	if (ref($maybe_root) and $maybe_root->isa("B::OP")) {
	    # It really is the root of the replacement, not something
	    # else stored here for lack of space elsewhere
	    walk_topdown($maybe_root, $sub, $level + 1);
	}
    }
}

sub walklines {
    my($ar, $level) = @_;
    for my $l (@$ar) {
	if (ref($l) eq "ARRAY") {
	    walklines($l, $level + 1);
	} else {
	    $l->concise($level);
	}
    }
}

sub walk_exec {
    my($top, $level) = @_;
    my %opsseen;
    my @lines;
    my @todo = ([$top, \@lines]);
    while (@todo and my($op, $targ) = @{shift @todo}) {
	for (; $$op; $op = $op->next) {
	    last if $opsseen{$$op}++;
	    push @$targ, $op;
	    my $name = $op->name;
	    if (class($op) eq "LOGOP") {
		my $ar = [];
		push @$targ, $ar;
		push @todo, [$op->other, $ar];
	    } elsif ($name eq "subst" and $ {$op->pmreplstart}) {
		my $ar = [];
		push @$targ, $ar;
		push @todo, [$op->pmreplstart, $ar];
	    } elsif ($name =~ /^enter(loop|iter)$/) {
		if ($] > 5.009) {
		    $labels{${$op->nextop}} = "NEXT";
		    $labels{${$op->lastop}} = "LAST";
		    $labels{${$op->redoop}} = "REDO";
		} else {
		    $labels{$op->nextop->seq} = "NEXT";
		    $labels{$op->lastop->seq} = "LAST";
		    $labels{$op->redoop->seq} = "REDO";		
		}
	    }
	}
    }
    walklines(\@lines, 0);
}

# The structure of this routine is purposely modeled after op.c's peep()
sub sequence {
    my($op) = @_;
    my $oldop = 0;
    return if class($op) eq "NULL" or exists $sequence_num{$$op};
    for (; $$op; $op = $op->next) {
	last if exists $sequence_num{$$op};
	my $name = $op->name;
	if ($name =~ /^(null|scalar|lineseq|scope)$/) {
	    next if $oldop and $ {$op->next};
	} else {
	    $sequence_num{$$op} = $seq_max++;
	    if (class($op) eq "LOGOP") {
		my $other = $op->other;
		$other = $other->next while $other->name eq "null";
		sequence($other);
	    } elsif (class($op) eq "LOOP") {
		my $redoop = $op->redoop;
		$redoop = $redoop->next while $redoop->name eq "null";
		sequence($redoop);
		my $nextop = $op->nextop;
		$nextop = $nextop->next while $nextop->name eq "null";
		sequence($nextop);
		my $lastop = $op->lastop;
		$lastop = $lastop->next while $lastop->name eq "null";
		sequence($lastop);
	    } elsif ($name eq "subst" and $ {$op->pmreplstart}) {
		my $replstart = $op->pmreplstart;
		$replstart = $replstart->next while $replstart->name eq "null";
		sequence($replstart);
	    }
	}
	$oldop = $op;
    }
}

sub fmt_line {    # generate text-line for op.
    my($hr, $op, $text, $level) = @_;

    $_->($hr, $op, \$text, \$level, $stylename) for @callbacks;

    return '' if $hr->{SKIP};	# suppress line if a callback said so
    return '' if $hr->{goto} and $hr->{goto} eq '-';	# no goto nowhere

    # spec: (?(text1#varText2)?)
    $text =~ s/\(\?\(([^\#]*?)\#(\w+)([^\#]*?)\)\?\)/
	$hr->{$2} ? $1.$hr->{$2}.$3 : ""/eg;

    # spec: (x(exec_text;basic_text)x)
    $text =~ s/\(x\((.*?);(.*?)\)x\)/$order eq "exec" ? $1 : $2/egs;

    # spec: (*(text)*)
    $text =~ s/\(\*\(([^;]*?)\)\*\)/$1 x $level/egs;

    # spec: (*(text1;text2)*)
    $text =~ s/\(\*\((.*?);(.*?)\)\*\)/$1 x ($level - 1) . $2 x ($level>0)/egs;

    # convert #Var to tag=>val form: Var\t#var
    $text =~ s/\#([A-Z][a-z]+)(\d+)?/\t\u$1\t\L#$1$2/gs;

    # spec: #varN
    $text =~ s/\#([a-zA-Z]+)(\d+)/sprintf("%-$2s", $hr->{$1})/eg;

    $text =~ s/\#([a-zA-Z]+)/$hr->{$1}/eg;	# populate #var's
    $text =~ s/[ \t]*~+[ \t]*/ /g;		# squeeze tildes

    $text = "# $hr->{src}\n$text" if $show_src and $hr->{src};

    chomp $text;
    return "$text\n" if $text ne "";
    return $text; # suppress empty lines
}

our %priv; # used to display each opcode's BASEOP.op_private values

$priv{$_}{128} = "LVINTRO"
  for ("pos", "substr", "vec", "threadsv", "gvsv", "rv2sv", "rv2hv", "rv2gv",
       "rv2av", "rv2arylen", "aelem", "helem", "aslice", "hslice", "padsv",
       "padav", "padhv", "enteriter");
$priv{$_}{64} = "REFC" for ("leave", "leavesub", "leavesublv", "leavewrite");
$priv{"aassign"}{64} = "COMMON";
$priv{"aassign"}{32} = $] < 5.009 ? "PHASH" : "STATE";
$priv{"sassign"}{32} = "STATE";
$priv{"sassign"}{64} = "BKWARD";
$priv{$_}{64} = "RTIME" for ("match", "subst", "substcont", "qr");
@{$priv{"trans"}}{1,2,4,8,16,64} = ("<UTF", ">UTF", "IDENT", "SQUASH", "DEL",
				    "COMPL", "GROWS");
$priv{"repeat"}{64} = "DOLIST";
$priv{"leaveloop"}{64} = "CONT";
@{$priv{$_}}{32,64,96} = ("DREFAV", "DREFHV", "DREFSV")
  for (qw(rv2gv rv2sv padsv aelem helem));
$priv{$_}{16} = "STATE" for ("padav", "padhv", "padsv");
@{$priv{"entersub"}}{16,32,64} = ("DBG","TARG","NOMOD");
@{$priv{$_}}{4,8,128} = ("INARGS","AMPER","NO()") for ("entersub", "rv2cv");
$priv{"gv"}{32} = "EARLYCV";
$priv{"aelem"}{16} = $priv{"helem"}{16} = "LVDEFER";
$priv{$_}{16} = "OURINTR" for ("gvsv", "rv2sv", "rv2av", "rv2hv", "r2gv",
	"enteriter");
$priv{$_}{16} = "TARGMY"
  for (map(($_,"s$_"),"chop", "chomp"),
       map(($_,"i_$_"), "postinc", "postdec", "multiply", "divide", "modulo",
	   "add", "subtract", "negate"), "pow", "concat", "stringify",
       "left_shift", "right_shift", "bit_and", "bit_xor", "bit_or",
       "complement", "atan2", "sin", "cos", "rand", "exp", "log", "sqrt",
       "int", "hex", "oct", "abs", "length", "index", "rindex", "sprintf",
       "ord", "chr", "crypt", "quotemeta", "join", "push", "unshift", "flock",
       "chdir", "chown", "chroot", "unlink", "chmod", "utime", "rename",
       "link", "symlink", "mkdir", "rmdir", "wait", "waitpid", "system",
       "exec", "kill", "getppid", "getpgrp", "setpgrp", "getpriority",
       "setpriority", "time", "sleep");
$priv{$_}{4} = "REVERSED" for ("enteriter", "iter");
@{$priv{"const"}}{4,8,16,32,64,128} = ("SHORT","STRICT","ENTERED",'$[',"BARE","WARN");
$priv{"flip"}{64} = $priv{"flop"}{64} = "LINENUM";
$priv{"list"}{64} = "GUESSED";
$priv{"delete"}{64} = "SLICE";
$priv{"exists"}{64} = "SUB";
@{$priv{"sort"}}{1,2,4,8,16,32,64} = ("NUM", "INT", "REV", "INPLACE","DESC","QSORT","STABLE");
$priv{"threadsv"}{64} = "SVREFd";
@{$priv{$_}}{16,32,64,128} = ("INBIN","INCR","OUTBIN","OUTCR")
  for ("open", "backtick");
$priv{"exit"}{128} = "VMS";
$priv{$_}{2} = "FTACCESS"
  for ("ftrread", "ftrwrite", "ftrexec", "fteread", "ftewrite", "fteexec");
$priv{"entereval"}{2} = "HAS_HH";
if ($] >= 5.009) {
  # Stacked filetests are post 5.8.x
  $priv{$_}{4} = "FTSTACKED"
    for ("ftrread", "ftrwrite", "ftrexec", "fteread", "ftewrite", "fteexec",
         "ftis", "fteowned", "ftrowned", "ftzero", "ftsize", "ftmtime",
	 "ftatime", "ftctime", "ftsock", "ftchr", "ftblk", "ftfile", "ftdir",
	 "ftpipe", "ftlink", "ftsuid", "ftsgid", "ftsvtx", "fttty", "fttext",
	 "ftbinary");
  # Lexical $_ is post 5.8.x
  $priv{$_}{2} = "GREPLEX"
    for ("mapwhile", "mapstart", "grepwhile", "grepstart");
}

our %hints; # used to display each COP's op_hints values

# strict refs, subs, vars
@hints{2,512,1024} = ('$', '&', '*');
# integers, locale, bytes, arybase
@hints{1,4,8,16,32} = ('i', 'l', 'b', '[');
# block scope, localise %^H, $^OPEN (in), $^OPEN (out)
@hints{256,131072,262144,524288} = ('{','%','<','>');
# overload new integer, float, binary, string, re
@hints{4096,8192,16384,32768,65536} = ('I', 'F', 'B', 'S', 'R');
# taint and eval
@hints{1048576,2097152} = ('T', 'E');
# filetest access, UTF-8
@hints{4194304,8388608} = ('X', 'U');

sub _flags {
    my($hash, $x) = @_;
    my @s;
    for my $flag (sort {$b <=> $a} keys %$hash) {
	if ($hash->{$flag} and $x & $flag and $x >= $flag) {
	    $x -= $flag;
	    push @s, $hash->{$flag};
	}
    }
    push @s, $x if $x;
    return join(",", @s);
}

sub private_flags {
    my($name, $x) = @_;
    _flags($priv{$name}, $x);
}

sub hints_flags {
    my($x) = @_;
    _flags(\%hints, $x);
}

sub concise_sv {
    my($sv, $hr, $preferpv) = @_;
    $hr->{svclass} = class($sv);
    $hr->{svclass} = "UV"
      if $hr->{svclass} eq "IV" and $sv->FLAGS & SVf_IVisUV;
    Carp::cluck("bad concise_sv: $sv") unless $sv and $$sv;
    $hr->{svaddr} = sprintf("%#x", $$sv);
    if ($hr->{svclass} eq "GV" && $sv->isGV_with_GP()) {
	my $gv = $sv;
	my $stash = $gv->STASH->NAME; if ($stash eq "main") {
	    $stash = "";
	} else {
	    $stash = $stash . "::";
	}
	$hr->{svval} = "*$stash" . $gv->SAFENAME;
	return "*$stash" . $gv->SAFENAME;
    } else {
	if ($] >= 5.011) {
	    while (class($sv) eq "IV" && $sv->FLAGS & SVf_ROK) {
		$hr->{svval} .= "\\";
		$sv = $sv->RV;
	    }
	} else {
	    while (class($sv) eq "RV") {
		$hr->{svval} .= "\\";
		$sv = $sv->RV;
	    }
	}
	if (class($sv) eq "SPECIAL") {
	    $hr->{svval} .= ["Null", "sv_undef", "sv_yes", "sv_no"]->[$$sv];
	} elsif ($preferpv && $sv->FLAGS & SVf_POK) {
	    $hr->{svval} .= cstring($sv->PV);
	} elsif ($sv->FLAGS & SVf_NOK) {
	    $hr->{svval} .= $sv->NV;
	} elsif ($sv->FLAGS & SVf_IOK) {
	    $hr->{svval} .= $sv->int_value;
	} elsif ($sv->FLAGS & SVf_POK) {
	    $hr->{svval} .= cstring($sv->PV);
	} elsif (class($sv) eq "HV") {
	    $hr->{svval} .= 'HASH';
	}

	$hr->{svval} = 'undef' unless defined $hr->{svval};
	my $out = $hr->{svclass};
	return $out .= " $hr->{svval}" ; 
    }
}

my %srclines;

sub fill_srclines {
    my $fullnm = shift;
    if ($fullnm eq '-e') {
	$srclines{$fullnm} = [ $fullnm, "-src not supported for -e" ];
	return;
    }
    open (my $fh, '<', $fullnm)
	or warn "# $fullnm: $!, (chdirs not supported by this feature yet)\n"
	and return;
    my @l = <$fh>;
    chomp @l;
    unshift @l, $fullnm; # like @{_<$fullnm} in debug, array starts at 1
    $srclines{$fullnm} = \@l;
}

sub concise_op {
    my ($op, $level, $format) = @_;
    my %h;
    $h{exname} = $h{name} = $op->name;
    $h{NAME} = uc $h{name};
    $h{class} = class($op);
    $h{extarg} = $h{targ} = $op->targ;
    $h{extarg} = "" unless $h{extarg};
    if ($h{name} eq "null" and $h{targ}) {
	# targ holds the old type
	$h{exname} = "ex-" . substr(ppname($h{targ}), 3);
	$h{extarg} = "";
    } elsif ($op->name =~ /^leave(sub(lv)?|write)?$/) {
	# targ potentially holds a reference count
	if ($op->private & 64) {
	    my $refs = "ref" . ($h{targ} != 1 ? "s" : "");
	    $h{targarglife} = $h{targarg} = "$h{targ} $refs";
	}
    } elsif ($h{targ}) {
	my $padname = (($curcv->PADLIST->ARRAY)[0]->ARRAY)[$h{targ}];
	if (defined $padname and class($padname) ne "SPECIAL") {
	    $h{targarg}  = $padname->PVX;
	    if ($padname->FLAGS & SVf_FAKE) {
		if ($] < 5.009) {
		    $h{targarglife} = "$h{targarg}:FAKE";
		} else {
		    # These changes relate to the jumbo closure fix.
		    # See changes 19939 and 20005
		    my $fake = '';
		    $fake .= 'a'
		   	if $padname->PARENT_FAKELEX_FLAGS & PAD_FAKELEX_ANON;
		    $fake .= 'm'
		   	if $padname->PARENT_FAKELEX_FLAGS & PAD_FAKELEX_MULTI;
		    $fake .= ':' . $padname->PARENT_PAD_INDEX
			if $curcv->CvFLAGS & CVf_ANON;
		    $h{targarglife} = "$h{targarg}:FAKE:$fake";
		}
	    }
	    else {
		my $intro = $padname->COP_SEQ_RANGE_LOW - $cop_seq_base;
		my $finish = int($padname->COP_SEQ_RANGE_HIGH) - $cop_seq_base;
		$finish = "end" if $finish == 999999999 - $cop_seq_base;
		$h{targarglife} = "$h{targarg}:$intro,$finish";
	    }
	} else {
	    $h{targarglife} = $h{targarg} = "t" . $h{targ};
	}
    }
    $h{arg} = "";
    $h{svclass} = $h{svaddr} = $h{svval} = "";
    if ($h{class} eq "PMOP") {
	my $precomp = $op->precomp;
	if (defined $precomp) {
	    $precomp = cstring($precomp); # Escape literal control sequences
 	    $precomp = "/$precomp/";
	} else {
	    $precomp = "";
	}
	my $pmreplroot = $op->pmreplroot;
	my $pmreplstart;
	if (ref($pmreplroot) eq "B::GV") {
	    # with C<@stash_array = split(/pat/, str);>,
	    #  *stash_array is stored in /pat/'s pmreplroot.
	    $h{arg} = "($precomp => \@" . $pmreplroot->NAME . ")";
	} elsif (!ref($pmreplroot) and $pmreplroot) {
	    # same as the last case, except the value is actually a
	    # pad offset for where the GV is kept (this happens under
	    # ithreads)
	    my $gv = (($curcv->PADLIST->ARRAY)[1]->ARRAY)[$pmreplroot];
	    $h{arg} = "($precomp => \@" . $gv->NAME . ")";
	} elsif ($ {$op->pmreplstart}) {
	    undef $lastnext;
	    $pmreplstart = "replstart->" . seq($op->pmreplstart);
	    $h{arg} = "(" . join(" ", $precomp, $pmreplstart) . ")";
	} else {
	    $h{arg} = "($precomp)";
	}
    } elsif ($h{class} eq "PVOP" and $h{name} ne "trans") {
	$h{arg} = '("' . $op->pv . '")';
	$h{svval} = '"' . $op->pv . '"';
    } elsif ($h{class} eq "COP") {
	my $label = $op->label;
	$h{coplabel} = $label;
	$label = $label ? "$label: " : "";
	my $loc = $op->file;
	my $pathnm = $loc;
	$loc =~ s[.*/][];
	my $ln = $op->line;
	$loc .= ":$ln";
	my($stash, $cseq) = ($op->stash->NAME, $op->cop_seq - $cop_seq_base);
	my $arybase = $op->arybase;
	$arybase = $arybase ? ' $[=' . $arybase : "";
	$h{arg} = "($label$stash $cseq $loc$arybase)";
	if ($show_src) {
	    fill_srclines($pathnm) unless exists $srclines{$pathnm};
	    # Would love to retain Jim's use of // but this code needs to be
	    # portable to 5.8.x
	    my $line = $srclines{$pathnm}[$ln];
	    $line = "-src unavailable under -e" unless defined $line;
	    $h{src} = "$ln: $line";
	}
    } elsif ($h{class} eq "LOOP") {
	$h{arg} = "(next->" . seq($op->nextop) . " last->" . seq($op->lastop)
	  . " redo->" . seq($op->redoop) . ")";
    } elsif ($h{class} eq "LOGOP") {
	undef $lastnext;
	$h{arg} = "(other->" . seq($op->other) . ")";
    }
    elsif ($h{class} eq "SVOP" or $h{class} eq "PADOP") {
	unless ($h{name} eq 'aelemfast' and $op->flags & OPf_SPECIAL) {
	    my $idx = ($h{class} eq "SVOP") ? $op->targ : $op->padix;
	    my $preferpv = $h{name} eq "method_named";
	    if ($h{class} eq "PADOP" or !${$op->sv}) {
		my $sv = (($curcv->PADLIST->ARRAY)[1]->ARRAY)[$idx];
		$h{arg} = "[" . concise_sv($sv, \%h, $preferpv) . "]";
		$h{targarglife} = $h{targarg} = "";
	    } else {
		$h{arg} = "(" . concise_sv($op->sv, \%h, $preferpv) . ")";
	    }
	}
    }
    $h{seq} = $h{hyphseq} = seq($op);
    $h{seq} = "" if $h{seq} eq "-";
    if ($] > 5.009) {
	$h{opt} = $op->opt;
	$h{label} = $labels{$$op};
    } else {
	$h{seqnum} = $op->seq;
	$h{label} = $labels{$op->seq};
    }
    $h{next} = $op->next;
    $h{next} = (class($h{next}) eq "NULL") ? "(end)" : seq($h{next});
    $h{nextaddr} = sprintf("%#x", $ {$op->next});
    $h{sibaddr} = sprintf("%#x", $ {$op->sibling});
    $h{firstaddr} = sprintf("%#x", $ {$op->first}) if $op->can("first");
    $h{lastaddr} = sprintf("%#x", $ {$op->last}) if $op->can("last");

    $h{classsym} = $opclass{$h{class}};
    $h{flagval} = $op->flags;
    $h{flags} = op_flags($op->flags);
    $h{privval} = $op->private;
    $h{private} = private_flags($h{name}, $op->private);
    if ($op->can("hints")) {
      $h{hintsval} = $op->hints;
      $h{hints} = hints_flags($h{hintsval});
    } else {
      $h{hintsval} = $h{hints} = '';
    }
    $h{addr} = sprintf("%#x", $$op);
    $h{typenum} = $op->type;
    $h{noise} = $linenoise[$op->type];

    return fmt_line(\%h, $op, $format, $level);
}

sub B::OP::concise {
    my($op, $level) = @_;
    if ($order eq "exec" and $lastnext and $$lastnext != $$op) {
	# insert a 'goto' line
	my $synth = {"seq" => seq($lastnext), "class" => class($lastnext),
		     "addr" => sprintf("%#x", $$lastnext),
		     "goto" => seq($lastnext), # simplify goto '-' removal
	     };
	print $walkHandle fmt_line($synth, $op, $gotofmt, $level+1);
    }
    $lastnext = $op->next;
    print $walkHandle concise_op($op, $level, $format);
}

# B::OP::terse (see Terse.pm) now just calls this
sub b_terse {
    my($op, $level) = @_;

    # This isn't necessarily right, but there's no easy way to get
    # from an OP to the right CV. This is a limitation of the
    # ->terse() interface style, and there isn't much to do about
    # it. In particular, we can die in concise_op if the main pad
    # isn't long enough, or has the wrong kind of entries, compared to
    # the pad a sub was compiled with. The fix for that would be to
    # make a backwards compatible "terse" format that never even
    # looked at the pad, just like the old B::Terse. I don't think
    # that's worth the effort, though.
    $curcv = main_cv unless $curcv;

    if ($order eq "exec" and $lastnext and $$lastnext != $$op) {
	# insert a 'goto'
	my $h = {"seq" => seq($lastnext), "class" => class($lastnext),
		 "addr" => sprintf("%#x", $$lastnext)};
	print # $walkHandle
	    fmt_line($h, $op, $style{"terse"}[1], $level+1);
    }
    $lastnext = $op->next;
    print # $walkHandle 
	concise_op($op, $level, $style{"terse"}[0]);
}

sub tree {
    my $op = shift;
    my $level = shift;
    my $style = $tree_decorations[$tree_style];
    my($space, $single, $kids, $kid, $nokid, $last, $lead, $size) = @$style;
    my $name = concise_op($op, $level, $treefmt);
    if (not $op->flags & OPf_KIDS) {
	return $name . "\n";
    }
    my @lines;
    for (my $kid = $op->first; $$kid; $kid = $kid->sibling) {
	push @lines, tree($kid, $level+1);
    }
    my $i;
    for ($i = $#lines; substr($lines[$i], 0, 1) eq " "; $i--) {
	$lines[$i] = $space . $lines[$i];
    }
    if ($i > 0) {
	$lines[$i] = $last . $lines[$i];
	while ($i-- > 1) {
	    if (substr($lines[$i], 0, 1) eq " ") {
		$lines[$i] = $nokid . $lines[$i];
	    } else {
		$lines[$i] = $kid . $lines[$i];
	    }
	}
	$lines[$i] = $kids . $lines[$i];
    } else {
	$lines[0] = $single . $lines[0];
    }
    return("$name$lead" . shift @lines,
           map(" " x (length($name)+$size) . $_, @lines));
}

# *** Warning: fragile kludge ahead ***
# Because the B::* modules run in the same interpreter as the code
# they're compiling, their presence tends to distort the view we have of
# the code we're looking at. In particular, perl gives sequence numbers
# to COPs. If the program we're looking at were run on its own, this
# would start at 1. Because all of B::Concise and all the modules it
# uses are compiled first, though, by the time we get to the user's
# program the sequence number is already pretty high, which could be
# distracting if you're trying to tell OPs apart. Therefore we'd like to
# subtract an offset from all the sequence numbers we display, to
# restore the simpler view of the world. The trick is to know what that
# offset will be, when we're still compiling B::Concise!  If we
# hardcoded a value, it would have to change every time B::Concise or
# other modules we use do. To help a little, what we do here is compile
# a little code at the end of the module, and compute the base sequence
# number for the user's program as being a small offset later, so all we
# have to worry about are changes in the offset.

# [For 5.8.x and earlier perl is generating sequence numbers for all ops,
#  and using them to reference labels]


# When you say "perl -MO=Concise -e '$a'", the output should look like:

# 4  <@> leave[t1] vKP/REFC ->(end)
# 1     <0> enter ->2
 #^ smallest OP sequence number should be 1
# 2     <;> nextstate(main 1 -e:1) v ->3
 #                         ^ smallest COP sequence number should be 1
# -     <1> ex-rv2sv vK/1 ->4
# 3        <$> gvsv(*a) s ->4

# If the second of the marked numbers there isn't 1, it means you need
# to update the corresponding magic number in the next line.
# Remember, this needs to stay the last things in the module.

# Why is this different for MacOS?  Does it matter?
my $cop_seq_mnum = $^O eq 'MacOS' ? 12 : 11;
$cop_seq_base = svref_2object(eval 'sub{0;}')->START->cop_seq + $cop_seq_mnum;

1;

__END__

=head1 NAME

B::Concise - Walk Perl syntax tree, printing concise info about ops

=head1 SYNOPSIS

    perl -MO=Concise[,OPTIONS] foo.pl

    use B::Concise qw(set_style add_callback);

=head1 DESCRIPTION

This compiler backend prints the internal OPs of a Perl program's syntax
tree in one of several space-efficient text formats suitable for debugging
the inner workings of perl or other compiler backends. It can print OPs in
the order they appear in the OP tree, in the order they will execute, or
in a text approximation to their tree structure, and the format of the
information displayed is customizable. Its function is similar to that of
perl's B<-Dx> debugging flag or the B<B::Terse> module, but it is more
sophisticated and flexible.

=head1 EXAMPLE

Here's two outputs (or 'renderings'), using the -exec and -basic
(i.e. default) formatting conventions on the same code snippet.

    % perl -MO=Concise,-exec -e '$a = $b + 42'
    1  <0> enter
    2  <;> nextstate(main 1 -e:1) v
    3  <#> gvsv[*b] s
    4  <$> const[IV 42] s
 *  5  <2> add[t3] sK/2
    6  <#> gvsv[*a] s
    7  <2> sassign vKS/2
    8  <@> leave[1 ref] vKP/REFC

In this -exec rendering, each opcode is executed in the order shown.
The add opcode, marked with '*', is discussed in more detail.

The 1st column is the op's sequence number, starting at 1, and is
displayed in base 36 by default.  Here they're purely linear; the
sequences are very helpful when looking at code with loops and
branches.

The symbol between angle brackets indicates the op's type, for
example; <2> is a BINOP, <@> a LISTOP, and <#> is a PADOP, which is
used in threaded perls. (see L</"OP class abbreviations">).

The opname, as in B<'add[t1]'>, may be followed by op-specific
information in parentheses or brackets (ex B<'[t1]'>).

The op-flags (ex B<'sK/2'>) are described in (L</"OP flags
abbreviations">).

    % perl -MO=Concise -e '$a = $b + 42'
    8  <@> leave[1 ref] vKP/REFC ->(end)
    1     <0> enter ->2
    2     <;> nextstate(main 1 -e:1) v ->3
    7     <2> sassign vKS/2 ->8
 *  5        <2> add[t1] sK/2 ->6
    -           <1> ex-rv2sv sK/1 ->4
    3              <$> gvsv(*b) s ->4
    4           <$> const(IV 42) s ->5
    -        <1> ex-rv2sv sKRM*/1 ->7
    6           <$> gvsv(*a) s ->7

The default rendering is top-down, so they're not in execution order.
This form reflects the way the stack is used to parse and evaluate
expressions; the add operates on the two terms below it in the tree.

Nullops appear as C<ex-opname>, where I<opname> is an op that has been
optimized away by perl.  They're displayed with a sequence-number of
'-', because they are not executed (they don't appear in previous
example), they're printed here because they reflect the parse.

The arrow points to the sequence number of the next op; they're not
displayed in -exec mode, for obvious reasons.

Note that because this rendering was done on a non-threaded perl, the
PADOPs in the previous examples are now SVOPs, and some (but not all)
of the square brackets have been replaced by round ones.  This is a
subtle feature to provide some visual distinction between renderings
on threaded and un-threaded perls.


=head1 OPTIONS

Arguments that don't start with a hyphen are taken to be the names of
subroutines to render; if no such functions are specified, the main
body of the program (outside any subroutines, and not including use'd
or require'd files) is rendered.  Passing C<BEGIN>, C<UNITCHECK>,
C<CHECK>, C<INIT>, or C<END> will cause all of the corresponding
special blocks to be printed.  Arguments must follow options.

Options affect how things are rendered (ie printed).  They're presented
here by their visual effect, 1st being strongest.  They're grouped
according to how they interrelate; within each group the options are
mutually exclusive (unless otherwise stated).

=head2 Options for Opcode Ordering

These options control the 'vertical display' of opcodes.  The display
'order' is also called 'mode' elsewhere in this document.

=over 4

=item B<-basic>

Print OPs in the order they appear in the OP tree (a preorder
traversal, starting at the root). The indentation of each OP shows its
level in the tree, and the '->' at the end of the line indicates the
next opcode in execution order.  This mode is the default, so the flag
is included simply for completeness.

=item B<-exec>

Print OPs in the order they would normally execute (for the majority
of constructs this is a postorder traversal of the tree, ending at the
root). In most cases the OP that usually follows a given OP will
appear directly below it; alternate paths are shown by indentation. In
cases like loops when control jumps out of a linear path, a 'goto'
line is generated.

=item B<-tree>

Print OPs in a text approximation of a tree, with the root of the tree
at the left and 'left-to-right' order of children transformed into
'top-to-bottom'. Because this mode grows both to the right and down,
it isn't suitable for large programs (unless you have a very wide
terminal).

=back

=head2 Options for Line-Style

These options select the line-style (or just style) used to render
each opcode, and dictates what info is actually printed into each line.

=over 4

=item B<-concise>

Use the author's favorite set of formatting conventions. This is the
default, of course.

=item B<-terse>

Use formatting conventions that emulate the output of B<B::Terse>. The
basic mode is almost indistinguishable from the real B<B::Terse>, and the
exec mode looks very similar, but is in a more logical order and lacks
curly brackets. B<B::Terse> doesn't have a tree mode, so the tree mode
is only vaguely reminiscent of B<B::Terse>.

=item B<-linenoise>

Use formatting conventions in which the name of each OP, rather than being
written out in full, is represented by a one- or two-character abbreviation.
This is mainly a joke.

=item B<-debug>

Use formatting conventions reminiscent of B<B::Debug>; these aren't
very concise at all.

=item B<-env>

Use formatting conventions read from the environment variables
C<B_CONCISE_FORMAT>, C<B_CONCISE_GOTO_FORMAT>, and C<B_CONCISE_TREE_FORMAT>.

=back

=head2 Options for tree-specific formatting

=over 4

=item B<-compact>

Use a tree format in which the minimum amount of space is used for the
lines connecting nodes (one character in most cases). This squeezes out
a few precious columns of screen real estate.

=item B<-loose>

Use a tree format that uses longer edges to separate OP nodes. This format
tends to look better than the compact one, especially in ASCII, and is
the default.

=item B<-vt>

Use tree connecting characters drawn from the VT100 line-drawing set.
This looks better if your terminal supports it.

=item B<-ascii>

Draw the tree with standard ASCII characters like C<+> and C<|>. These don't
look as clean as the VT100 characters, but they'll work with almost any
terminal (or the horizontal scrolling mode of less(1)) and are suitable
for text documentation or email. This is the default.

=back

These are pairwise exclusive, i.e. compact or loose, vt or ascii.

=head2 Options controlling sequence numbering

=over 4

=item B<-base>I<n>

Print OP sequence numbers in base I<n>. If I<n> is greater than 10, the
digit for 11 will be 'a', and so on. If I<n> is greater than 36, the digit
for 37 will be 'A', and so on until 62. Values greater than 62 are not
currently supported. The default is 36.

=item B<-bigendian>

Print sequence numbers with the most significant digit first. This is the
usual convention for Arabic numerals, and the default.

=item B<-littleendian>

Print seqence numbers with the least significant digit first.  This is
obviously mutually exclusive with bigendian.

=back

=head2 Other options

=over 4

=item B<-src>

With this option, the rendering of each statement (starting with the
nextstate OP) will be preceded by the 1st line of source code that
generates it.  For example:

    1  <0> enter
    # 1: my $i;
    2  <;> nextstate(main 1 junk.pl:1) v:{
    3  <0> padsv[$i:1,10] vM/LVINTRO
    # 3: for $i (0..9) {
    4  <;> nextstate(main 3 junk.pl:3) v:{
    5  <0> pushmark s
    6  <$> const[IV 0] s
    7  <$> const[IV 9] s
    8  <{> enteriter(next->j last->m redo->9)[$i:1,10] lKS
    k  <0> iter s
    l  <|> and(other->9) vK/1
    # 4:     print "line ";
    9      <;> nextstate(main 2 junk.pl:4) v
    a      <0> pushmark s
    b      <$> const[PV "line "] s
    c      <@> print vK
    # 5:     print "$i\n";
    ...

=item B<-stash="somepackage">

With this, "somepackage" will be required, then the stash is
inspected, and each function is rendered.

=back

The following options are pairwise exclusive.

=over 4

=item B<-main>

Include the main program in the output, even if subroutines were also
specified.  This rendering is normally suppressed when a subroutine
name or reference is given.

=item B<-nomain>

This restores the default behavior after you've changed it with '-main'
(it's not normally needed).  If no subroutine name/ref is given, main is
rendered, regardless of this flag.

=item B<-nobanner>

Renderings usually include a banner line identifying the function name
or stringified subref.  This suppresses the printing of the banner.

TBC: Remove the stringified coderef; while it provides a 'cookie' for
each function rendered, the cookies used should be 1,2,3.. not a
random hex-address.  It also complicates string comparison of two
different trees.

=item B<-banner>

restores default banner behavior.

=item B<-banneris> => subref

TBC: a hookpoint (and an option to set it) for a user-supplied
function to produce a banner appropriate for users needs.  It's not
ideal, because the rendering-state variables, which are a natural
candidate for use in concise.t, are unavailable to the user.

=back

=head2 Option Stickiness

If you invoke Concise more than once in a program, you should know that
the options are 'sticky'.  This means that the options you provide in
the first call will be remembered for the 2nd call, unless you
re-specify or change them.

=head1 ABBREVIATIONS

The concise style uses symbols to convey maximum info with minimal
clutter (like hex addresses).  With just a little practice, you can
start to see the flowers, not just the branches, in the trees.

=head2 OP class abbreviations

These symbols appear before the op-name, and indicate the
B:: namespace that represents the ops in your Perl code.

    0      OP (aka BASEOP)  An OP with no children
    1      UNOP             An OP with one child
    2      BINOP            An OP with two children
    |      LOGOP            A control branch OP
    @      LISTOP           An OP that could have lots of children
    /      PMOP             An OP with a regular expression
    $      SVOP             An OP with an SV
    "      PVOP             An OP with a string
    {      LOOP             An OP that holds pointers for a loop
    ;      COP              An OP that marks the start of a statement
    #      PADOP            An OP with a GV on the pad

=head2 OP flags abbreviations

OP flags are either public or private.  The public flags alter the
behavior of each opcode in consistent ways, and are represented by 0
or more single characters.

    v      OPf_WANT_VOID    Want nothing (void context)
    s      OPf_WANT_SCALAR  Want single value (scalar context)
    l      OPf_WANT_LIST    Want list of any length (list context)
                            Want is unknown
    K      OPf_KIDS         There is a firstborn child.
    P      OPf_PARENS       This operator was parenthesized.
                             (Or block needs explicit scope entry.)
    R      OPf_REF          Certified reference.
                             (Return container, not containee).
    M      OPf_MOD          Will modify (lvalue).
    S      OPf_STACKED      Some arg is arriving on the stack.
    *      OPf_SPECIAL      Do something weird for this op (see op.h)

Private flags, if any are set for an opcode, are displayed after a '/'

    8  <@> leave[1 ref] vKP/REFC ->(end)
    7     <2> sassign vKS/2 ->8

They're opcode specific, and occur less often than the public ones, so
they're represented by short mnemonics instead of single-chars; see
F<op.h> for gory details, or try this quick 2-liner:

  $> perl -MB::Concise -de 1
  DB<1> |x \%B::Concise::priv

=head1 FORMATTING SPECIFICATIONS

For each line-style ('concise', 'terse', 'linenoise', etc.) there are
3 format-specs which control how OPs are rendered.

The first is the 'default' format, which is used in both basic and exec
modes to print all opcodes.  The 2nd, goto-format, is used in exec
mode when branches are encountered.  They're not real opcodes, and are
inserted to look like a closing curly brace.  The tree-format is tree
specific.

When a line is rendered, the correct format-spec is copied and scanned
for the following items; data is substituted in, and other
manipulations like basic indenting are done, for each opcode rendered.

There are 3 kinds of items that may be populated; special patterns,
#vars, and literal text, which is copied verbatim.  (Yes, it's a set
of s///g steps.)

=head2 Special Patterns

These items are the primitives used to perform indenting, and to
select text from amongst alternatives.

=over 4

=item B<(x(>I<exec_text>B<;>I<basic_text>B<)x)>

Generates I<exec_text> in exec mode, or I<basic_text> in basic mode.

=item B<(*(>I<text>B<)*)>

Generates one copy of I<text> for each indentation level.

=item B<(*(>I<text1>B<;>I<text2>B<)*)>

Generates one fewer copies of I<text1> than the indentation level, followed
by one copy of I<text2> if the indentation level is more than 0.

=item B<(?(>I<text1>B<#>I<var>I<Text2>B<)?)>

If the value of I<var> is true (not empty or zero), generates the
value of I<var> surrounded by I<text1> and I<Text2>, otherwise
nothing.

=item B<~>

Any number of tildes and surrounding whitespace will be collapsed to
a single space.

=back

=head2 # Variables

These #vars represent opcode properties that you may want as part of
your rendering.  The '#' is intended as a private sigil; a #var's
value is interpolated into the style-line, much like "read $this".

These vars take 3 forms:

=over 4

=item B<#>I<var>

A property named 'var' is assumed to exist for the opcodes, and is
interpolated into the rendering.

=item B<#>I<var>I<N>

Generates the value of I<var>, left justified to fill I<N> spaces.
Note that this means while you can have properties 'foo' and 'foo2',
you cannot render 'foo2', but you could with 'foo2a'.  You would be
wise not to rely on this behavior going forward ;-)

=item B<#>I<Var>

This ucfirst form of #var generates a tag-value form of itself for
display; it converts '#Var' into a 'Var => #var' style, which is then
handled as described above.  (Imp-note: #Vars cannot be used for
conditional-fills, because the => #var transform is done after the check
for #Var's value).

=back

The following variables are 'defined' by B::Concise; when they are
used in a style, their respective values are plugged into the
rendering of each opcode.

Only some of these are used by the standard styles, the others are
provided for you to delve into optree mechanics, should you wish to
add a new style (see L</add_style> below) that uses them.  You can
also add new ones using L</add_callback>.

=over 4

=item B<#addr>

The address of the OP, in hexadecimal.

=item B<#arg>

The OP-specific information of the OP (such as the SV for an SVOP, the
non-local exit pointers for a LOOP, etc.) enclosed in parentheses.

=item B<#class>

The B-determined class of the OP, in all caps.

=item B<#classsym>

A single symbol abbreviating the class of the OP.

=item B<#coplabel>

The label of the statement or block the OP is the start of, if any.

=item B<#exname>

The name of the OP, or 'ex-foo' if the OP is a null that used to be a foo.

=item B<#extarg>

The target of the OP, or nothing for a nulled OP.

=item B<#firstaddr>

The address of the OP's first child, in hexadecimal.

=item B<#flags>

The OP's flags, abbreviated as a series of symbols.

=item B<#flagval>

The numeric value of the OP's flags.

=item B<#hints>

The COP's hint flags, rendered with abbreviated names if possible. An empty
string if this is not a COP. Here are the symbols used:

    $ strict refs
    & strict subs
    * strict vars
    i integers
    l locale
    b bytes
    [ arybase
    { block scope
    % localise %^H
    < open in
    > open out
    I overload int
    F overload float
    B overload binary
    S overload string
    R overload re
    T taint
    E eval
    X filetest access
    U utf-8

=item B<#hintsval>

The numeric value of the COP's hint flags, or an empty string if this is not
a COP.

=item B<#hyphseq>

The sequence number of the OP, or a hyphen if it doesn't have one.

=item B<#label>

'NEXT', 'LAST', or 'REDO' if the OP is a target of one of those in exec
mode, or empty otherwise.

=item B<#lastaddr>

The address of the OP's last child, in hexadecimal.

=item B<#name>

The OP's name.

=item B<#NAME>

The OP's name, in all caps.

=item B<#next>

The sequence number of the OP's next OP.

=item B<#nextaddr>

The address of the OP's next OP, in hexadecimal.

=item B<#noise>

A one- or two-character abbreviation for the OP's name.

=item B<#private>

The OP's private flags, rendered with abbreviated names if possible.

=item B<#privval>

The numeric value of the OP's private flags.

=item B<#seq>

The sequence number of the OP. Note that this is a sequence number
generated by B::Concise.

=item B<#seqnum>

5.8.x and earlier only. 5.9 and later do not provide this.

The real sequence number of the OP, as a regular number and not adjusted
to be relative to the start of the real program. (This will generally be
a fairly large number because all of B<B::Concise> is compiled before
your program is).

=item B<#opt>

Whether or not the op has been optimised by the peephole optimiser.

Only available in 5.9 and later.

=item B<#sibaddr>

The address of the OP's next youngest sibling, in hexadecimal.

=item B<#svaddr>

The address of the OP's SV, if it has an SV, in hexadecimal.

=item B<#svclass>

The class of the OP's SV, if it has one, in all caps (e.g., 'IV').

=item B<#svval>

The value of the OP's SV, if it has one, in a short human-readable format.

=item B<#targ>

The numeric value of the OP's targ.

=item B<#targarg>

The name of the variable the OP's targ refers to, if any, otherwise the
letter t followed by the OP's targ in decimal.

=item B<#targarglife>

Same as B<#targarg>, but followed by the COP sequence numbers that delimit
the variable's lifetime (or 'end' for a variable in an open scope) for a
variable.

=item B<#typenum>

The numeric value of the OP's type, in decimal.

=back

=head1 One-Liner Command tips

=over 4

=item perl -MO=Concise,bar foo.pl

Renders only bar() from foo.pl.  To see main, drop the ',bar'.  To see
both, add ',-main'

=item perl -MDigest::MD5=md5 -MO=Concise,md5 -e1

Identifies md5 as an XS function.  The export is needed so that BC can
find it in main.

=item perl -MPOSIX -MO=Concise,_POSIX_ARG_MAX -e1

Identifies _POSIX_ARG_MAX as a constant sub, optimized to an IV.
Although POSIX isn't entirely consistent across platforms, this is
likely to be present in virtually all of them.

=item perl -MPOSIX -MO=Concise,a -e 'print _POSIX_SAVED_IDS'

This renders a print statement, which includes a call to the function.
It's identical to rendering a file with a use call and that single
statement, except for the filename which appears in the nextstate ops.

=item perl -MPOSIX -MO=Concise,a -e 'sub a{_POSIX_SAVED_IDS}'

This is B<very> similar to previous, only the first two ops differ.  This
subroutine rendering is more representative, insofar as a single main
program will have many subs.

=item perl -MB::Concise -e 'B::Concise::compile("-exec","-src", \%B::Concise::)->()'

This renders all functions in the B::Concise package with the source
lines.  It eschews the O framework so that the stashref can be passed
directly to B::Concise::compile().  See -stash option for a more
convenient way to render a package.

=back

=head1 Using B::Concise outside of the O framework

The common (and original) usage of B::Concise was for command-line
renderings of simple code, as given in EXAMPLE.  But you can also use
B<B::Concise> from your code, and call compile() directly, and
repeatedly.  By doing so, you can avoid the compile-time only
operation of O.pm, and even use the debugger to step through
B::Concise::compile() itself.

Once you're doing this, you may alter Concise output by adding new
rendering styles, and by optionally adding callback routines which
populate new variables, if such were referenced from those (just
added) styles.  

=head2 Example: Altering Concise Renderings

    use B::Concise qw(set_style add_callback);
    add_style($yourStyleName => $defaultfmt, $gotofmt, $treefmt);
    add_callback
      ( sub {
            my ($h, $op, $format, $level, $stylename) = @_;
            $h->{variable} = some_func($op);
        });
    $walker = B::Concise::compile(@options,@subnames,@subrefs);
    $walker->();

=head2 set_style()

B<set_style> accepts 3 arguments, and updates the three format-specs
comprising a line-style (basic-exec, goto, tree).  It has one minor
drawback though; it doesn't register the style under a new name.  This
can become an issue if you render more than once and switch styles.
Thus you may prefer to use add_style() and/or set_style_standard()
instead.

=head2 set_style_standard($name)

This restores one of the standard line-styles: C<terse>, C<concise>,
C<linenoise>, C<debug>, C<env>, into effect.  It also accepts style
names previously defined with add_style().

=head2 add_style()

This subroutine accepts a new style name and three style arguments as
above, and creates, registers, and selects the newly named style.  It is
an error to re-add a style; call set_style_standard() to switch between
several styles.

=head2 add_callback()

If your newly minted styles refer to any new #variables, you'll need
to define a callback subroutine that will populate (or modify) those
variables.  They are then available for use in the style you've
chosen.

The callbacks are called for each opcode visited by Concise, in the
same order as they are added.  Each subroutine is passed five
parameters.

  1. A hashref, containing the variable names and values which are
     populated into the report-line for the op
  2. the op, as a B<B::OP> object
  3. a reference to the format string
  4. the formatting (indent) level
  5. the selected stylename

To define your own variables, simply add them to the hash, or change
existing values if you need to.  The level and format are passed in as
references to scalars, but it is unlikely that they will need to be
changed or even used.

=head2 Running B::Concise::compile()

B<compile> accepts options as described above in L</OPTIONS>, and
arguments, which are either coderefs, or subroutine names.

It constructs and returns a $treewalker coderef, which when invoked,
traverses, or walks, and renders the optrees of the given arguments to
STDOUT.  You can reuse this, and can change the rendering style used
each time; thereafter the coderef renders in the new style.

B<walk_output> lets you change the print destination from STDOUT to
another open filehandle, or into a string passed as a ref (unless
you've built perl with -Uuseperlio).

    my $walker = B::Concise::compile('-terse','aFuncName', \&aSubRef);  # 1
    walk_output(\my $buf);
    $walker->();			# 1 renders -terse
    set_style_standard('concise');	# 2
    $walker->();			# 2 renders -concise
    $walker->(@new);			# 3 renders whatever
    print "3 different renderings: terse, concise, and @new: $buf\n";

When $walker is called, it traverses the subroutines supplied when it
was created, and renders them using the current style.  You can change
the style afterwards in several different ways:

  1. call C<compile>, altering style or mode/order
  2. call C<set_style_standard>
  3. call $walker, passing @new options

Passing new options to the $walker is the easiest way to change
amongst any pre-defined styles (the ones you add are automatically
recognized as options), and is the only way to alter rendering order
without calling compile again.  Note however that rendering state is
still shared amongst multiple $walker objects, so they must still be
used in a coordinated manner.

=head2 B::Concise::reset_sequence()

This function (not exported) lets you reset the sequence numbers (note
that they're numbered arbitrarily, their goal being to be human
readable).  Its purpose is mostly to support testing, i.e. to compare
the concise output from two identical anonymous subroutines (but
different instances).  Without the reset, B::Concise, seeing that
they're separate optrees, generates different sequence numbers in
the output.

=head2 Errors

Errors in rendering (non-existent function-name, non-existent coderef)
are written to the STDOUT, or wherever you've set it via
walk_output().

Errors using the various *style* calls, and bad args to walk_output(),
result in die().  Use an eval if you wish to catch these errors and
continue processing.

=head1 AUTHOR

Stephen McCamant, E<lt>smcc@CSUA.Berkeley.EDUE<gt>.

=cut
