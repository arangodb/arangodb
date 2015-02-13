#      B.pm
#
#      Copyright (c) 1996, 1997, 1998 Malcolm Beattie
#
#      You may distribute under the terms of either the GNU General Public
#      License or the Artistic License, as specified in the README file.
#
package B;

our $VERSION = '1.20';

use XSLoader ();
require Exporter;
@ISA = qw(Exporter);

# walkoptree_slow comes from B.pm (you are there),
# walkoptree comes from B.xs
@EXPORT_OK = qw(minus_c ppname save_BEGINs
		class peekop cast_I32 cstring cchar hash threadsv_names
		main_root main_start main_cv svref_2object opnumber
		sub_generation amagic_generation perlstring
		walkoptree_slow walkoptree walkoptree_exec walksymtable
		parents comppadlist sv_undef compile_stats timing_info
		begin_av init_av check_av end_av regex_padav dowarn defstash
		curstash warnhook diehook inc_gv @optype @specialsv_name
		);
push @EXPORT_OK, qw(unitcheck_av) if $] > 5.009;

sub OPf_KIDS ();
use strict;
@B::SV::ISA = 'B::OBJECT';
@B::NULL::ISA = 'B::SV';
@B::PV::ISA = 'B::SV';
@B::IV::ISA = 'B::SV';
@B::NV::ISA = 'B::SV';
# RV is eliminated with 5.11.0, but effectively is a specialisation of IV now.
@B::RV::ISA = $] >= 5.011 ? 'B::IV' : 'B::SV';
@B::PVIV::ISA = qw(B::PV B::IV);
@B::PVNV::ISA = qw(B::PVIV B::NV);
@B::PVMG::ISA = 'B::PVNV';
@B::REGEXP::ISA = 'B::PVMG' if $] >= 5.011;
# Change in the inheritance hierarchy post 5.9.0
@B::PVLV::ISA = $] > 5.009 ? 'B::GV' : 'B::PVMG';
# BM is eliminated post 5.9.5, but effectively is a specialisation of GV now.
@B::BM::ISA = $] > 5.009005 ? 'B::GV' : 'B::PVMG';
@B::AV::ISA = 'B::PVMG';
@B::GV::ISA = 'B::PVMG';
@B::HV::ISA = 'B::PVMG';
@B::CV::ISA = 'B::PVMG';
@B::IO::ISA = 'B::PVMG';
@B::FM::ISA = 'B::CV';

@B::OP::ISA = 'B::OBJECT';
@B::UNOP::ISA = 'B::OP';
@B::BINOP::ISA = 'B::UNOP';
@B::LOGOP::ISA = 'B::UNOP';
@B::LISTOP::ISA = 'B::BINOP';
@B::SVOP::ISA = 'B::OP';
@B::PADOP::ISA = 'B::OP';
@B::PVOP::ISA = 'B::OP';
@B::LOOP::ISA = 'B::LISTOP';
@B::PMOP::ISA = 'B::LISTOP';
@B::COP::ISA = 'B::OP';

@B::SPECIAL::ISA = 'B::OBJECT';

@B::optype = qw(OP UNOP BINOP LOGOP LISTOP PMOP SVOP PADOP PVOP LOOP COP);
# bytecode.pl contained the following comment:
# Nullsv *must* come first in the following so that the condition
# ($$sv == 0) can continue to be used to test (sv == Nullsv).
@B::specialsv_name = qw(Nullsv &PL_sv_undef &PL_sv_yes &PL_sv_no
			(SV*)pWARN_ALL (SV*)pWARN_NONE (SV*)pWARN_STD);

{
    # Stop "-w" from complaining about the lack of a real B::OBJECT class
    package B::OBJECT;
}

sub B::GV::SAFENAME {
  my $name = (shift())->NAME;

  # The regex below corresponds to the isCONTROLVAR macro
  # from toke.c

  $name =~ s/^([\cA-\cZ\c\\c[\c]\c?\c_\c^])/"^".
	chr( utf8::unicode_to_native( 64 ^ ord($1) ))/e;

  # When we say unicode_to_native we really mean ascii_to_native,
  # which matters iff this is a non-ASCII platform (EBCDIC).

  return $name;
}

sub B::IV::int_value {
  my ($self) = @_;
  return (($self->FLAGS() & SVf_IVisUV()) ? $self->UVX : $self->IV);
}

sub B::NULL::as_string() {""}
sub B::IV::as_string()   {goto &B::IV::int_value}
sub B::PV::as_string()   {goto &B::PV::PV}

my $debug;
my $op_count = 0;
my @parents = ();

sub debug {
    my ($class, $value) = @_;
    $debug = $value;
    walkoptree_debug($value);
}

sub class {
    my $obj = shift;
    my $name = ref $obj;
    $name =~ s/^.*:://;
    return $name;
}

sub parents { \@parents }

# For debugging
sub peekop {
    my $op = shift;
    return sprintf("%s (0x%x) %s", class($op), $$op, $op->name);
}

sub walkoptree_slow {
    my($op, $method, $level) = @_;
    $op_count++; # just for statistics
    $level ||= 0;
    warn(sprintf("walkoptree: %d. %s\n", $level, peekop($op))) if $debug;
    $op->$method($level) if $op->can($method);
    if ($$op && ($op->flags & OPf_KIDS)) {
	my $kid;
	unshift(@parents, $op);
	for ($kid = $op->first; $$kid; $kid = $kid->sibling) {
	    walkoptree_slow($kid, $method, $level + 1);
	}
	shift @parents;
    }
    if (class($op) eq 'PMOP'
	&& ref($op->pmreplroot)
	&& ${$op->pmreplroot}
	&& $op->pmreplroot->isa( 'B::OP' ))
    {
	unshift(@parents, $op);
	walkoptree_slow($op->pmreplroot, $method, $level + 1);
	shift @parents;
    }
}

sub compile_stats {
    return "Total number of OPs processed: $op_count\n";
}

sub timing_info {
    my ($sec, $min, $hr) = localtime;
    my ($user, $sys) = times;
    sprintf("%02d:%02d:%02d user=$user sys=$sys",
	    $hr, $min, $sec, $user, $sys);
}

my %symtable;

sub clearsym {
    %symtable = ();
}

sub savesym {
    my ($obj, $value) = @_;
#    warn(sprintf("savesym: sym_%x => %s\n", $$obj, $value)); # debug
    $symtable{sprintf("sym_%x", $$obj)} = $value;
}

sub objsym {
    my $obj = shift;
    return $symtable{sprintf("sym_%x", $$obj)};
}

sub walkoptree_exec {
    my ($op, $method, $level) = @_;
    $level ||= 0;
    my ($sym, $ppname);
    my $prefix = "    " x $level;
    for (; $$op; $op = $op->next) {
	$sym = objsym($op);
	if (defined($sym)) {
	    print $prefix, "goto $sym\n";
	    return;
	}
	savesym($op, sprintf("%s (0x%lx)", class($op), $$op));
	$op->$method($level);
	$ppname = $op->name;
	if ($ppname =~
	    /^(d?or(assign)?|and(assign)?|mapwhile|grepwhile|entertry|range|cond_expr)$/)
	{
	    print $prefix, uc($1), " => {\n";
	    walkoptree_exec($op->other, $method, $level + 1);
	    print $prefix, "}\n";
	} elsif ($ppname eq "match" || $ppname eq "subst") {
	    my $pmreplstart = $op->pmreplstart;
	    if ($$pmreplstart) {
		print $prefix, "PMREPLSTART => {\n";
		walkoptree_exec($pmreplstart, $method, $level + 1);
		print $prefix, "}\n";
	    }
	} elsif ($ppname eq "substcont") {
	    print $prefix, "SUBSTCONT => {\n";
	    walkoptree_exec($op->other->pmreplstart, $method, $level + 1);
	    print $prefix, "}\n";
	    $op = $op->other;
	} elsif ($ppname eq "enterloop") {
	    print $prefix, "REDO => {\n";
	    walkoptree_exec($op->redoop, $method, $level + 1);
	    print $prefix, "}\n", $prefix, "NEXT => {\n";
	    walkoptree_exec($op->nextop, $method, $level + 1);
	    print $prefix, "}\n", $prefix, "LAST => {\n";
	    walkoptree_exec($op->lastop,  $method, $level + 1);
	    print $prefix, "}\n";
	} elsif ($ppname eq "subst") {
	    my $replstart = $op->pmreplstart;
	    if ($$replstart) {
		print $prefix, "SUBST => {\n";
		walkoptree_exec($replstart, $method, $level + 1);
		print $prefix, "}\n";
	    }
	}
    }
}

sub walksymtable {
    my ($symref, $method, $recurse, $prefix) = @_;
    my $sym;
    my $ref;
    my $fullname;
    no strict 'refs';
    $prefix = '' unless defined $prefix;
    while (($sym, $ref) = each %$symref) {
        $fullname = "*main::".$prefix.$sym;
	if ($sym =~ /::$/) {
	    $sym = $prefix . $sym;
	    if ($sym ne "main::" && $sym ne "<none>::" && &$recurse($sym)) {
               walksymtable(\%$fullname, $method, $recurse, $sym);
	    }
	} else {
           svref_2object(\*$fullname)->$method();
	}
    }
}

{
    package B::Section;
    my $output_fh;
    my %sections;

    sub new {
	my ($class, $section, $symtable, $default) = @_;
	$output_fh ||= FileHandle->new_tmpfile;
	my $obj = bless [-1, $section, $symtable, $default], $class;
	$sections{$section} = $obj;
	return $obj;
    }

    sub get {
	my ($class, $section) = @_;
	return $sections{$section};
    }

    sub add {
	my $section = shift;
	while (defined($_ = shift)) {
	    print $output_fh "$section->[1]\t$_\n";
	    $section->[0]++;
	}
    }

    sub index {
	my $section = shift;
	return $section->[0];
    }

    sub name {
	my $section = shift;
	return $section->[1];
    }

    sub symtable {
	my $section = shift;
	return $section->[2];
    }

    sub default {
	my $section = shift;
	return $section->[3];
    }

    sub output {
	my ($section, $fh, $format) = @_;
	my $name = $section->name;
	my $sym = $section->symtable || {};
	my $default = $section->default;

	seek($output_fh, 0, 0);
	while (<$output_fh>) {
	    chomp;
	    s/^(.*?)\t//;
	    if ($1 eq $name) {
		s{(s\\_[0-9a-f]+)} {
		    exists($sym->{$1}) ? $sym->{$1} : $default;
		}ge;
		printf $fh $format, $_;
	    }
	}
    }
}

XSLoader::load 'B';

1;

__END__

=head1 NAME

B - The Perl Compiler

=head1 SYNOPSIS

	use B;

=head1 DESCRIPTION

The C<B> module supplies classes which allow a Perl program to delve
into its own innards. It is the module used to implement the
"backends" of the Perl compiler. Usage of the compiler does not
require knowledge of this module: see the F<O> module for the
user-visible part. The C<B> module is of use to those who want to
write new compiler backends. This documentation assumes that the
reader knows a fair amount about perl's internals including such
things as SVs, OPs and the internal symbol table and syntax tree
of a program.

=head1 OVERVIEW

The C<B> module contains a set of utility functions for querying the
current state of the Perl interpreter; typically these functions
return objects from the B::SV and B::OP classes, or their derived
classes.  These classes in turn define methods for querying the
resulting objects about their own internal state.

=head1 Utility Functions

The C<B> module exports a variety of functions: some are simple
utility functions, others provide a Perl program with a way to
get an initial "handle" on an internal object.

=head2 Functions Returning C<B::SV>, C<B::AV>, C<B::HV>, and C<B::CV> objects

For descriptions of the class hierarchy of these objects and the
methods that can be called on them, see below, L<"OVERVIEW OF
CLASSES"> and L<"SV-RELATED CLASSES">.

=over 4

=item sv_undef

Returns the SV object corresponding to the C variable C<sv_undef>.

=item sv_yes

Returns the SV object corresponding to the C variable C<sv_yes>.

=item sv_no

Returns the SV object corresponding to the C variable C<sv_no>.

=item svref_2object(SVREF)

Takes a reference to any Perl value, and turns the referred-to value
into an object in the appropriate B::OP-derived or B::SV-derived
class. Apart from functions such as C<main_root>, this is the primary
way to get an initial "handle" on an internal perl data structure
which can then be followed with the other access methods.

The returned object will only be valid as long as the underlying OPs
and SVs continue to exist. Do not attempt to use the object after the
underlying structures are freed.

=item amagic_generation

Returns the SV object corresponding to the C variable C<amagic_generation>.

=item init_av

Returns the AV object (i.e. in class B::AV) representing INIT blocks.

=item check_av

Returns the AV object (i.e. in class B::AV) representing CHECK blocks.

=item unitcheck_av

Returns the AV object (i.e. in class B::AV) representing UNITCHECK blocks.

=item begin_av

Returns the AV object (i.e. in class B::AV) representing BEGIN blocks.

=item end_av

Returns the AV object (i.e. in class B::AV) representing END blocks.

=item comppadlist

Returns the AV object (i.e. in class B::AV) of the global comppadlist.

=item regex_padav

Only when perl was compiled with ithreads.

=item main_cv

Return the (faked) CV corresponding to the main part of the Perl
program.

=back

=head2 Functions for Examining the Symbol Table

=over 4

=item walksymtable(SYMREF, METHOD, RECURSE, PREFIX)

Walk the symbol table starting at SYMREF and call METHOD on each
symbol (a B::GV object) visited.  When the walk reaches package
symbols (such as "Foo::") it invokes RECURSE, passing in the symbol
name, and only recurses into the package if that sub returns true.

PREFIX is the name of the SYMREF you're walking.

For example:

  # Walk CGI's symbol table calling print_subs on each symbol.
  # Recurse only into CGI::Util::
  walksymtable(\%CGI::, 'print_subs', sub { $_[0] eq 'CGI::Util::' },
               'CGI::');

print_subs() is a B::GV method you have declared. Also see L<"B::GV
Methods">, below.

=back

=head2 Functions Returning C<B::OP> objects or for walking op trees

For descriptions of the class hierarchy of these objects and the
methods that can be called on them, see below, L<"OVERVIEW OF
CLASSES"> and L<"OP-RELATED CLASSES">.

=over 4

=item main_root

Returns the root op (i.e. an object in the appropriate B::OP-derived
class) of the main part of the Perl program.

=item main_start

Returns the starting op of the main part of the Perl program.

=item walkoptree(OP, METHOD)

Does a tree-walk of the syntax tree based at OP and calls METHOD on
each op it visits. Each node is visited before its children. If
C<walkoptree_debug> (see below) has been called to turn debugging on then
the method C<walkoptree_debug> is called on each op before METHOD is
called.

=item walkoptree_debug(DEBUG)

Returns the current debugging flag for C<walkoptree>. If the optional
DEBUG argument is non-zero, it sets the debugging flag to that. See
the description of C<walkoptree> above for what the debugging flag
does.

=back

=head2 Miscellaneous Utility Functions

=over 4

=item ppname(OPNUM)

Return the PP function name (e.g. "pp_add") of op number OPNUM.

=item hash(STR)

Returns a string in the form "0x..." representing the value of the
internal hash function used by perl on string STR.

=item cast_I32(I)

Casts I to the internal I32 type used by that perl.

=item minus_c

Does the equivalent of the C<-c> command-line option. Obviously, this
is only useful in a BEGIN block or else the flag is set too late.

=item cstring(STR)

Returns a double-quote-surrounded escaped version of STR which can
be used as a string in C source code.

=item perlstring(STR)

Returns a double-quote-surrounded escaped version of STR which can
be used as a string in Perl source code.

=item class(OBJ)

Returns the class of an object without the part of the classname
preceding the first C<"::">. This is used to turn C<"B::UNOP"> into
C<"UNOP"> for example.

=item threadsv_names

In a perl compiled for threads, this returns a list of the special
per-thread threadsv variables.

=back

=head2 Exported utility variabiles

=over 4

=item @optype

  my $op_type = $optype[$op_type_num];

A simple mapping of the op type number to its type (like 'COP' or 'BINOP').

=item @specialsv_name

  my $sv_name = $specialsv_name[$sv_index];

Certain SV types are considered 'special'.  They're represented by
B::SPECIAL and are referred to by a number from the specialsv_list.
This array maps that number back to the name of the SV (like 'Nullsv'
or '&PL_sv_undef').

=back


=head1 OVERVIEW OF CLASSES

The C structures used by Perl's internals to hold SV and OP
information (PVIV, AV, HV, ..., OP, SVOP, UNOP, ...) are modelled on a
class hierarchy and the C<B> module gives access to them via a true
object hierarchy. Structure fields which point to other objects
(whether types of SV or types of OP) are represented by the C<B>
module as Perl objects of the appropriate class.

The bulk of the C<B> module is the methods for accessing fields of
these structures.

Note that all access is read-only.  You cannot modify the internals by
using this module. Also, note that the B::OP and B::SV objects created
by this module are only valid for as long as the underlying objects
exist; their creation doesn't increase the reference counts of the
underlying objects. Trying to access the fields of a freed object will
give incomprehensible results, or worse.

=head2 SV-RELATED CLASSES

B::IV, B::NV, B::RV, B::PV, B::PVIV, B::PVNV, B::PVMG, B::BM (5.9.5 and
earlier), B::PVLV, B::AV, B::HV, B::CV, B::GV, B::FM, B::IO. These classes
correspond in the obvious way to the underlying C structures of similar names.
The inheritance hierarchy mimics the underlying C "inheritance". For the
5.10.x branch, (I<ie> 5.10.0, 5.10.1 I<etc>) this is:

                           B::SV
                             |
                +------------+------------+------------+
                |            |            |            |
              B::PV        B::IV        B::NV        B::RV
                  \         /           /
                   \       /           /
                    B::PVIV           /
                         \           /
                          \         /
                           \       /
                            B::PVNV
                               |
                               |
                            B::PVMG
                               |
                   +-----+-----+-----+-----+
                   |     |     |     |     |
                 B::AV B::GV B::HV B::CV B::IO
                         |           |
                         |           |
                      B::PVLV      B::FM

For 5.9.0 and earlier, PVLV is a direct subclass of PVMG, and BM is still
present as a distinct type, so the base of this diagram is


                               |
                               |
                            B::PVMG
                               |
            +------+-----+-----+-----+-----+-----+
            |      |     |     |     |     |     |
         B::PVLV B::BM B::AV B::GV B::HV B::CV B::IO
                                           |
                                           |
                                         B::FM

For 5.11.0 and later, B::RV is abolished, and IVs can be used to store
references, and a new type B::REGEXP is introduced, giving this structure:

                           B::SV
                             |
                +------------+------------+
                |            |            |
              B::PV        B::IV        B::NV
                  \         /           /
                   \       /           /
                    B::PVIV           /
                         \           /
                          \         /
                           \       /
                            B::PVNV
                               |
                               |
                            B::PVMG
                               |
           +-------+-------+---+---+-------+-------+
           |       |       |       |       |       |
         B::AV   B::GV   B::HV   B::CV   B::IO B::REGEXP
                   |               |
                   |               |
                B::PVLV          B::FM


Access methods correspond to the underlying C macros for field access,
usually with the leading "class indication" prefix removed (Sv, Av,
Hv, ...). The leading prefix is only left in cases where its removal
would cause a clash in method name. For example, C<GvREFCNT> stays
as-is since its abbreviation would clash with the "superclass" method
C<REFCNT> (corresponding to the C function C<SvREFCNT>).

=head2 B::SV Methods

=over 4

=item REFCNT

=item FLAGS

=item object_2svref

Returns a reference to the regular scalar corresponding to this
B::SV object. In other words, this method is the inverse operation
to the svref_2object() subroutine. This scalar and other data it points
at should be considered read-only: modifying them is neither safe nor
guaranteed to have a sensible effect.

=back

=head2 B::IV Methods

=over 4

=item IV

Returns the value of the IV, I<interpreted as
a signed integer>. This will be misleading
if C<FLAGS & SVf_IVisUV>. Perhaps you want the
C<int_value> method instead?

=item IVX

=item UVX

=item int_value

This method returns the value of the IV as an integer.
It differs from C<IV> in that it returns the correct
value regardless of whether it's stored signed or
unsigned.

=item needs64bits

=item packiv

=back

=head2 B::NV Methods

=over 4

=item NV

=item NVX

=back

=head2 B::RV Methods

=over 4

=item RV

=back

=head2 B::PV Methods

=over 4

=item PV

This method is the one you usually want. It constructs a
string using the length and offset information in the struct:
for ordinary scalars it will return the string that you'd see
from Perl, even if it contains null characters.

=item RV

Same as B::RV::RV, except that it will die() if the PV isn't
a reference.

=item PVX

This method is less often useful. It assumes that the string
stored in the struct is null-terminated, and disregards the
length information.

It is the appropriate method to use if you need to get the name
of a lexical variable from a padname array. Lexical variable names
are always stored with a null terminator, and the length field
(SvCUR) is overloaded for other purposes and can't be relied on here.

=back

=head2 B::PVMG Methods

=over 4

=item MAGIC

=item SvSTASH

=back

=head2 B::MAGIC Methods

=over 4

=item MOREMAGIC

=item precomp

Only valid on r-magic, returns the string that generated the regexp.

=item PRIVATE

=item TYPE

=item FLAGS

=item OBJ

Will die() if called on r-magic.

=item PTR

=item REGEX

Only valid on r-magic, returns the integer value of the REGEX stored
in the MAGIC.

=back

=head2 B::PVLV Methods

=over 4

=item TARGOFF

=item TARGLEN

=item TYPE

=item TARG

=back

=head2 B::BM Methods

=over 4

=item USEFUL

=item PREVIOUS

=item RARE

=item TABLE

=back

=head2 B::GV Methods

=over 4

=item is_empty

This method returns TRUE if the GP field of the GV is NULL.

=item NAME

=item SAFENAME

This method returns the name of the glob, but if the first
character of the name is a control character, then it converts
it to ^X first, so that *^G would return "^G" rather than "\cG".

It's useful if you want to print out the name of a variable.
If you restrict yourself to globs which exist at compile-time
then the result ought to be unambiguous, because code like
C<${"^G"} = 1> is compiled as two ops - a constant string and
a dereference (rv2gv) - so that the glob is created at runtime.

If you're working with globs at runtime, and need to disambiguate
*^G from *{"^G"}, then you should use the raw NAME method.

=item STASH

=item SV

=item IO

=item FORM

=item AV

=item HV

=item EGV

=item CV

=item CVGEN

=item LINE

=item FILE

=item FILEGV

=item GvREFCNT

=item FLAGS

=back

=head2 B::IO Methods

=over 4

=item LINES

=item PAGE

=item PAGE_LEN

=item LINES_LEFT

=item TOP_NAME

=item TOP_GV

=item FMT_NAME

=item FMT_GV

=item BOTTOM_NAME

=item BOTTOM_GV

=item SUBPROCESS

=item IoTYPE

=item IoFLAGS

=item IsSTD

Takes one arguments ( 'stdin' | 'stdout' | 'stderr' ) and returns true
if the IoIFP of the object is equal to the handle whose name was
passed as argument ( i.e. $io->IsSTD('stderr') is true if
IoIFP($io) == PerlIO_stdin() ).

=back

=head2 B::AV Methods

=over 4

=item FILL

=item MAX

=item ARRAY

=item ARRAYelt

Like C<ARRAY>, but takes an index as an argument to get only one element,
rather than a list of all of them.

=item OFF

This method is deprecated if running under Perl 5.8, and is no longer present
if running under Perl 5.9

=item AvFLAGS

This method returns the AV specific flags. In Perl 5.9 these are now stored
in with the main SV flags, so this method is no longer present.

=back

=head2 B::CV Methods

=over 4

=item STASH

=item START

=item ROOT

=item GV

=item FILE

=item DEPTH

=item PADLIST

=item OUTSIDE

=item OUTSIDE_SEQ

=item XSUB

=item XSUBANY

For constant subroutines, returns the constant SV returned by the subroutine.

=item CvFLAGS

=item const_sv

=back

=head2 B::HV Methods

=over 4

=item FILL

=item MAX

=item KEYS

=item RITER

=item NAME

=item ARRAY

=item PMROOT

This method is not present if running under Perl 5.9, as the PMROOT
information is no longer stored directly in the hash.

=back

=head2 OP-RELATED CLASSES

C<B::OP>, C<B::UNOP>, C<B::BINOP>, C<B::LOGOP>, C<B::LISTOP>, C<B::PMOP>,
C<B::SVOP>, C<B::PADOP>, C<B::PVOP>, C<B::LOOP>, C<B::COP>.

These classes correspond in the obvious way to the underlying C
structures of similar names. The inheritance hierarchy mimics the
underlying C "inheritance":

                                 B::OP
                                   |
                   +---------------+--------+--------+-------+
                   |               |        |        |       |
                B::UNOP          B::SVOP B::PADOP  B::COP  B::PVOP
                 ,'  `-.
                /       `--.
           B::BINOP     B::LOGOP
               |
               |
           B::LISTOP
             ,' `.
            /     \
        B::LOOP B::PMOP

Access methods correspond to the underlying C structre field names,
with the leading "class indication" prefix (C<"op_">) removed.

=head2 B::OP Methods

These methods get the values of similarly named fields within the OP
data structure.  See top of C<op.h> for more info.

=over 4

=item next

=item sibling

=item name

This returns the op name as a string (e.g. "add", "rv2av").

=item ppaddr

This returns the function name as a string (e.g. "PL_ppaddr[OP_ADD]",
"PL_ppaddr[OP_RV2AV]").

=item desc

This returns the op description from the global C PL_op_desc array
(e.g. "addition" "array deref").

=item targ

=item type

=item opt

=item flags

=item private

=item spare

=back

=head2 B::UNOP METHOD

=over 4

=item first

=back

=head2 B::BINOP METHOD

=over 4

=item last

=back

=head2 B::LOGOP METHOD

=over 4

=item other

=back

=head2 B::LISTOP METHOD

=over 4

=item children

=back

=head2 B::PMOP Methods

=over 4

=item pmreplroot

=item pmreplstart

=item pmnext

Only up to Perl 5.9.4

=item pmregexp

=item pmflags

=item extflags

Since Perl 5.9.5

=item precomp

=item pmoffset

Only when perl was compiled with ithreads.

=back

=head2 B::SVOP METHOD

=over 4

=item sv

=item gv

=back

=head2 B::PADOP METHOD

=over 4

=item padix

=back

=head2 B::PVOP METHOD

=over 4

=item pv

=back

=head2 B::LOOP Methods

=over 4

=item redoop

=item nextop

=item lastop

=back

=head2 B::COP Methods

=over 4

=item label

=item stash

=item stashpv

=item file

=item cop_seq

=item arybase

=item line

=item warnings

=item io

=item hints

=item hints_hash

=back


=head1 AUTHOR

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>

=cut
