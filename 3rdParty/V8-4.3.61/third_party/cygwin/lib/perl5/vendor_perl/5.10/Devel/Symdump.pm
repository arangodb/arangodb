package Devel::Symdump;

use 5.003;
use Carp ();
use strict;
use vars qw($Defaults $VERSION *ENTRY $MAX_RECURSION);

$VERSION = '2.08';
$MAX_RECURSION = 97;

$Defaults = {
	     'RECURS'   => 0,
	     'AUTOLOAD' => {
			    'packages'	=> 1,
			    'scalars'	=> 1,
			    'arrays'	=> 1,
			    'hashes'	=> 1,
			    'functions'	=> 1,
			    'ios'	=> 1,
			    'unknowns'	=> 1,
			   },
             'SEEN' => {},
	    };

sub rnew {
    my($class,@packages) = @_;
    no strict "refs";
    my $self = bless {%${"$class\::Defaults"}}, $class;
    $self->{RECURS}++;
    $self->_doit(@packages);
}

sub new {
    my($class,@packages) = @_;
    no strict "refs";
    my $self = bless {%${"$class\::Defaults"}}, $class;
    $self->_doit(@packages);
}

sub _doit {
    my($self,@packages) = @_;
    @packages = ("main") unless @packages;
    $self->{RESULT} = $self->_symdump(@packages);
    return $self;
}

sub _symdump {
    my($self,@packages) = @_ ;
    my($key,$val,$num,$pack,@todo,$tmp);
    my $result = {};
    foreach $pack (@packages){
	no strict;
	while (($key,$val) = each(%{*{"$pack\::"}})) {
	    my $gotone = 0;
	    local(*ENTRY) = $val;
	    #### SCALAR ####
	    if (defined $val && defined *ENTRY{SCALAR}) {
		$result->{$pack}{SCALARS}{$key}++;
		$gotone++;
	    }
	    #### ARRAY ####
	    if (defined $val && defined *ENTRY{ARRAY}) {
		$result->{$pack}{ARRAYS}{$key}++;
		$gotone++;
	    }
	    #### HASH ####
	    if (defined $val && defined *ENTRY{HASH} && $key !~ /::/) {
		$result->{$pack}{HASHES}{$key}++;
		$gotone++;
	    }
	    #### PACKAGE ####
	    if (defined $val && defined *ENTRY{HASH} && $key =~ /::$/ &&
                $key ne "main::" && $key ne "<none>::") {
                my($p) = $pack ne "main" ? "$pack\::" : "";
                ($p .= $key) =~ s/::$//;
                $result->{$pack}{PACKAGES}{$p}++;
                $gotone++;
                if (++$self->{SEEN}{*$val} > $Devel::Symdump::MAX_RECURSION){
                    next;
                }
		push @todo, $p;
	    }
	    #### FUNCTION ####
	    if (defined $val && defined *ENTRY{CODE}) {
		$result->{$pack}{FUNCTIONS}{$key}++;
		$gotone++;
	    }

	    #### IO #### had to change after 5.003_10
	    if ($] > 5.003_10){
		if (defined $val && defined *ENTRY{IO}){ # fileno and telldir...
		    $result->{$pack}{IOS}{$key}++;
		    $gotone++;
		}
	    } else {
		#### FILEHANDLE ####
		if (defined fileno(ENTRY)){
		    $result->{$pack}{IOS}{$key}++;
		    $gotone++;
		} elsif (defined telldir(ENTRY)){
		    #### DIRHANDLE ####
		    $result->{$pack}{IOS}{$key}++;
		    $gotone++;
		}
	    }

	    #### SOMETHING ELSE ####
	    unless ($gotone) {
		$result->{$pack}{UNKNOWNS}{$key}++;
	    }
	}
    }

    return (@todo && $self->{RECURS})
		? { %$result, %{$self->_symdump(@todo)} }
		: $result;
}

sub _partdump {
    my($self,$part)=@_;
    my ($pack, @result);
    my $prepend = "";
    foreach $pack (keys %{$self->{RESULT}}){
	$prepend = "$pack\::" unless $part eq 'PACKAGES';
	push @result, map {"$prepend$_"} keys %{$self->{RESULT}{$pack}{$part} || {}};
    }
    return @result;
}

# this is needed so we don't try to AUTOLOAD the DESTROY method
sub DESTROY {}

sub as_string {
    my $self = shift;
    my($type,@m);
    for $type (sort keys %{$self->{'AUTOLOAD'}}) {
	push @m, $type;
	push @m, "\t" . join "\n\t", map {
	    s/([\000-\037\177])/ '^' . pack('c', ord($1) ^ 64) /eg;
	    $_;
	} sort $self->_partdump(uc $type);
    }
    return join "\n", @m;
}

sub as_HTML {
    my $self = shift;
    my($type,@m);
    push @m, "<TABLE>";
    for $type (sort keys %{$self->{'AUTOLOAD'}}) {
	push @m, "<TR><TD valign=top><B>$type</B></TD>";
	push @m, "<TD>" . join ", ", map {
	    s/([\000-\037\177])/ '^' .
		pack('c', ord($1) ^ 64)
		    /eg; $_;
	} sort $self->_partdump(uc $type);
	push @m, "</TD></TR>";
    }
    push @m, "</TABLE>";
    return join "\n", @m;
}

sub diff {
    my($self,$second) = @_;
    my($type,@m);
    for $type (sort keys %{$self->{'AUTOLOAD'}}) {
	my(%first,%second,%all,$symbol);
	foreach $symbol ($self->_partdump(uc $type)){
	    $first{$symbol}++;
	    $all{$symbol}++;
	}
	foreach $symbol ($second->_partdump(uc $type)){
	    $second{$symbol}++;
	    $all{$symbol}++;
	}
	my(@typediff);
	foreach $symbol (sort keys %all){
	    next if $first{$symbol} && $second{$symbol};
	    push @typediff, "- $symbol" unless $second{$symbol};
	    push @typediff, "+ $symbol" unless $first{$symbol};
	}
	foreach (@typediff) {
	    s/([\000-\037\177])/ '^' . pack('c', ord($1) ^ 64) /eg;
	}
	push @m, $type, @typediff if @typediff;
    }
    return join "\n", @m;
}

sub inh_tree {
    my($self) = @_;
    return $self->{INHTREE} if ref $self && defined $self->{INHTREE};
    my($inherited_by) = {};
    my($m)="";
    my(@isa) = grep /\bISA$/, Devel::Symdump->rnew->arrays;
    my $isa;
    foreach $isa (sort @isa) {
	$isa =~ s/::ISA$//;
	my($isaisa);
	no strict 'refs';
	foreach $isaisa (@{"$isa\::ISA"}){
	    $inherited_by->{$isaisa}{$isa}++;
	}
    }
    my $item;
    foreach $item (sort keys %$inherited_by) {
	$m .= "$item\n";
	$m .= _inh_tree($item,$inherited_by);
    }
    $self->{INHTREE} = $m if ref $self;
    $m;
}

sub _inh_tree {
    my($package,$href,$depth) = @_;
    return unless defined $href;
    $depth ||= 0;
    $depth++;
    if ($depth > 100){
	warn "Deep recursion in ISA\n";
	return;
    }
    my($m) = "";
    # print "DEBUG: package[$package]depth[$depth]\n";
    my $i;
    foreach $i (sort keys %{$href->{$package}}) {
	$m .= qq{\t} x $depth;
	$m .= qq{$i\n};
	$m .= _inh_tree($i,$href,$depth);
    }
    $m;
}

sub isa_tree{
    my($self) = @_;
    return $self->{ISATREE} if ref $self && defined $self->{ISATREE};
    my(@isa) = grep /\bISA$/, Devel::Symdump->rnew->arrays;
    my($m) = "";
    my($isa);
    foreach $isa (sort @isa) {
	$isa =~ s/::ISA$//;
	$m .= qq{$isa\n};
	$m .= _isa_tree($isa)
    }
    $self->{ISATREE} = $m if ref $self;
    $m;
}

sub _isa_tree{
    my($package,$depth) = @_;
    $depth ||= 0;
    $depth++;
    if ($depth > 100){
	warn "Deep recursion in ISA\n";
	return;
    }
    my($m) = "";
    # print "DEBUG: package[$package]depth[$depth]\n";
    my $isaisa;
    no strict 'refs';
    foreach $isaisa (@{"$package\::ISA"}) {
	$m .= qq{\t} x $depth;
	$m .= qq{$isaisa\n};
	$m .= _isa_tree($isaisa,$depth);
    }
    $m;
}

AUTOLOAD {
    my($self,@packages) = @_;
    unless (ref $self) {
	$self = $self->new(@packages);
    }
    no strict "vars";
    (my $auto = $AUTOLOAD) =~ s/.*:://;

    $auto =~ s/(file|dir)handles/ios/;
    my $compat = $1;

    unless ($self->{'AUTOLOAD'}{$auto}) {
	Carp::croak("invalid Devel::Symdump method: $auto()");
    }

    my @syms = $self->_partdump(uc $auto);
    if (defined $compat) {
	no strict 'refs';
        local $^W; # bleadperl@26631 introduced an io warning here
	if ($compat eq "file") {
	    @syms = grep { defined(fileno($_)) } @syms;
	} else {
	    @syms = grep { defined(telldir($_)) } @syms;
	}
    }
    return @syms; # make sure now it gets context right
}

1;

__END__

=head1 NAME

Devel::Symdump - dump symbol names or the symbol table

=head1 SYNOPSIS

    # Constructor
    require Devel::Symdump;
    @packs = qw(some_package another_package);
    $obj = Devel::Symdump->new(@packs);        # no recursion
    $obj = Devel::Symdump->rnew(@packs);       # with recursion

    # Methods
    @array = $obj->packages;
    @array = $obj->scalars;
    @array = $obj->arrays;
    @array = $obj->hashes;
    @array = $obj->functions;
    @array = $obj->filehandles;  # deprecated, use ios instead
    @array = $obj->dirhandles;   # deprecated, use ios instead
    @array = $obj->ios;
    @array = $obj->unknowns;     # only perl version < 5.003 had some

    $string = $obj->as_string;
    $string = $obj->as_HTML;
    $string = $obj1->diff($obj2);

    $string = Devel::Symdump->isa_tree;    # or $obj->isa_tree
    $string = Devel::Symdump->inh_tree;    # or $obj->inh_tree

    # Methods with autogenerated objects
    # all of those call new(@packs) internally
    @array = Devel::Symdump->packages(@packs);
    @array = Devel::Symdump->scalars(@packs);
    @array = Devel::Symdump->arrays(@packs);
    @array = Devel::Symdump->hashes(@packs);
    @array = Devel::Symdump->functions(@packs);
    @array = Devel::Symdump->ios(@packs);
    @array = Devel::Symdump->unknowns(@packs);

=head1 DESCRIPTION

This little package serves to access the symbol table of perl.

=over 4

=item C<Devel::Symdump-E<gt>rnew(@packages)>

returns a symbol table object for all subtrees below @packages.
Nested Modules are analyzed recursively. If no package is given as
argument, it defaults to C<main>. That means to get the whole symbol
table, just do a C<rnew> without arguments.

The global variable $Devel::Symdump::MAX_RECURSION limits the
recursion to prevent contention. The default value is set to 97, just
low enough to survive the test suite without a warning about deep
recursion.

=item C<Devel::Symdump-E<gt>new(@packages)>

does not go into recursion and only analyzes the packages that are
given as arguments.

=item packages, scalars, arrays, hashes, functions, ios

The methods packages(), scalars(), arrays(), hashes(), functions(),
ios(), and (for older perls) unknowns() each return an array of fully
qualified symbols of the specified type in all packages that are held
within a Devel::Symdump object, but without the leading C<$>, C<@> or
C<%>. In a scalar context, they will return the number of such
symbols. Unknown symbols are usually either formats or variables that
haven't yet got a defined value.

=item as_string

=item as_HTML

As_string() and as_HTML() return a simple string/HTML representations
of the object.

=item diff

Diff() prints the difference between two Devel::Symdump objects in
human readable form. The format is similar to the one used by the
as_string method.

=item isa_tree

=item inh_tree

Isa_tree() and inh_tree() both return a simple string representation
of the current inheritance tree. The difference between the two
methods is the direction from which the tree is viewed: top-down or
bottom-up. As I'm sure, many users will have different expectation
about what is top and what is bottom, I'll provide an example what
happens when the Socket module is loaded:

=item % print Devel::Symdump-E<gt>inh_tree

    AutoLoader
            DynaLoader
                    Socket
    DynaLoader
            Socket
    Exporter
            Carp
            Config
            Socket

The inh_tree method shows on the left hand side a package name and
indented to the right the packages that use the former.

=item % print Devel::Symdump-E<gt>isa_tree

    Carp
            Exporter
    Config
            Exporter
    DynaLoader
            AutoLoader
    Socket
            Exporter
            DynaLoader
                    AutoLoader

The isa_tree method displays from left to right ISA relationships, so
Socket IS A DynaLoader and DynaLoader IS A AutoLoader. (Actually, they
were at the time this manpage was written)

=back

You may call both methods, isa_tree() and inh_tree(), with an
object. If you do that, the object will store the output and retrieve
it when you call the same method again later. The typical usage would
be to use them as class methods directly though.

=head1 SUBCLASSING

The design of this package is intentionally primitive and allows it to
be subclassed easily. An example of a (maybe) useful subclass is
Devel::Symdump::Export, a package which exports all methods of the
Devel::Symdump package and turns them into functions.

=head1 AUTHORS

Andreas Koenig F<< <andk@cpan.org> >> and Tom Christiansen
F<< <tchrist@perl.com> >>. Based on the old F<dumpvar.pl> by Larry
Wall.

=head1 COPYRIGHT, LICENSE

This module is

Copyright (c) 1995, 1997, 2000, 2002, 2005, 2006 Andreas Koenig C<< <andk@cpan.org> >>.

All rights reserved.

This library is free software;
you may use, redistribute and/or modify it under the same
terms as Perl itself.

=cut


# Local Variables:
# mode: cperl
# cperl-indent-level: 4
# End:
