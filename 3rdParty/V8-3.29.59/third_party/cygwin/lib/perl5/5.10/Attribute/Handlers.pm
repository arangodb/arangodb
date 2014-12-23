package Attribute::Handlers;
use 5.006;
use Carp;
use warnings;
use strict;
use vars qw($VERSION $AUTOLOAD);
$VERSION = '0.80';
# $DB::single=1;

my %symcache;
sub findsym {
	my ($pkg, $ref, $type) = @_;
	return $symcache{$pkg,$ref} if $symcache{$pkg,$ref};
	$type ||= ref($ref);
	my $found;
	no strict 'refs';
        foreach my $sym ( values %{$pkg."::"} ) {
	    use strict;
	    next unless ref ( \$sym ) eq 'GLOB';
            return $symcache{$pkg,$ref} = \$sym
		if *{$sym}{$type} && *{$sym}{$type} == $ref;
	}
}

my %validtype = (
	VAR	=> [qw[SCALAR ARRAY HASH]],
        ANY	=> [qw[SCALAR ARRAY HASH CODE]],
        ""	=> [qw[SCALAR ARRAY HASH CODE]],
        SCALAR	=> [qw[SCALAR]],
        ARRAY	=> [qw[ARRAY]],
        HASH	=> [qw[HASH]],
        CODE	=> [qw[CODE]],
);
my %lastattr;
my @declarations;
my %raw;
my %phase;
my %sigil = (SCALAR=>'$', ARRAY=>'@', HASH=>'%');
my $global_phase = 0;
my %global_phases = (
	BEGIN	=> 0,
	CHECK	=> 1,
	INIT	=> 2,
	END	=> 3,
);
my @global_phases = qw(BEGIN CHECK INIT END);

sub _usage_AH_ {
	croak "Usage: use $_[0] autotie => {AttrName => TieClassName,...}";
}

my $qual_id = qr/^[_a-z]\w*(::[_a-z]\w*)*$/i;

sub import {
    my $class = shift @_;
    return unless $class eq "Attribute::Handlers";
    while (@_) {
	my $cmd = shift;
        if ($cmd =~ /^autotie((?:ref)?)$/) {
	    my $tiedata = ($1 ? '$ref, ' : '') . '@$data';
            my $mapping = shift;
	    _usage_AH_ $class unless ref($mapping) eq 'HASH';
	    while (my($attr, $tieclass) = each %$mapping) {
                $tieclass =~ s/^([_a-z]\w*(::[_a-z]\w*)*)(.*)/$1/is;
		my $args = $3||'()';
		_usage_AH_ $class unless $attr =~ $qual_id
		                 && $tieclass =~ $qual_id
		                 && eval "use base q\0$tieclass\0; 1";
	        if ($tieclass->isa('Exporter')) {
		    local $Exporter::ExportLevel = 2;
		    $tieclass->import(eval $args);
	        }
		$attr =~ s/__CALLER__/caller(1)/e;
		$attr = caller()."::".$attr unless $attr =~ /::/;
	        eval qq{
	            sub $attr : ATTR(VAR) {
			my (\$ref, \$data) = \@_[2,4];
			my \$was_arrayref = ref \$data eq 'ARRAY';
			\$data = [ \$data ] unless \$was_arrayref;
			my \$type = ref(\$ref)||"value (".(\$ref||"<undef>").")";
			 (\$type eq 'SCALAR')? tie \$\$ref,'$tieclass',$tiedata
			:(\$type eq 'ARRAY') ? tie \@\$ref,'$tieclass',$tiedata
			:(\$type eq 'HASH')  ? tie \%\$ref,'$tieclass',$tiedata
			: die "Can't autotie a \$type\n"
	            } 1
	        } or die "Internal error: $@";
	    }
        }
        else {
            croak "Can't understand $_"; 
        }
    }
}
sub _resolve_lastattr {
	return unless $lastattr{ref};
	my $sym = findsym @lastattr{'pkg','ref'}
		or die "Internal error: $lastattr{pkg} symbol went missing";
	my $name = *{$sym}{NAME};
	warn "Declaration of $name attribute in package $lastattr{pkg} may clash with future reserved word\n"
		if $^W and $name !~ /[A-Z]/;
	foreach ( @{$validtype{$lastattr{type}}} ) {
		no strict 'refs';
		*{"$lastattr{pkg}::_ATTR_${_}_${name}"} = $lastattr{ref};
	}
	%lastattr = ();
}

sub AUTOLOAD {
	return if $AUTOLOAD =~ /::DESTROY$/;
	my ($class) = $AUTOLOAD =~ m/(.*)::/g;
	$AUTOLOAD =~ m/_ATTR_(.*?)_(.*)/ or
	    croak "Can't locate class method '$AUTOLOAD' via package '$class'";
	croak "Attribute handler '$2' doesn't handle $1 attributes";
}

my $builtin = qr/lvalue|method|locked|unique|shared/;

sub _gen_handler_AH_() {
	return sub {
	    _resolve_lastattr;
	    my ($pkg, $ref, @attrs) = @_;
	    my (undef, $filename, $linenum) = caller 2;
	    foreach (@attrs) {
		my ($attr, $data) = /^([a-z_]\w*)(?:[(](.*)[)])?$/is or next;
		if ($attr eq 'ATTR') {
			no strict 'refs';
			$data ||= "ANY";
			$raw{$ref} = $data =~ s/\s*,?\s*RAWDATA\s*,?\s*//;
			$phase{$ref}{BEGIN} = 1
				if $data =~ s/\s*,?\s*(BEGIN)\s*,?\s*//;
			$phase{$ref}{INIT} = 1
				if $data =~ s/\s*,?\s*(INIT)\s*,?\s*//;
			$phase{$ref}{END} = 1
				if $data =~ s/\s*,?\s*(END)\s*,?\s*//;
			$phase{$ref}{CHECK} = 1
				if $data =~ s/\s*,?\s*(CHECK)\s*,?\s*//
				|| ! keys %{$phase{$ref}};
			# Added for cleanup to not pollute next call.
			(%lastattr = ()),
			croak "Can't have two ATTR specifiers on one subroutine"
				if keys %lastattr;
			croak "Bad attribute type: ATTR($data)"
				unless $validtype{$data};
			%lastattr=(pkg=>$pkg,ref=>$ref,type=>$data);
		}
		else {
			my $type = ref $ref;
			my $handler = $pkg->can("_ATTR_${type}_${attr}");
			next unless $handler;
		        my $decl = [$pkg, $ref, $attr, $data,
				    $raw{$handler}, $phase{$handler}, $filename, $linenum];
			foreach my $gphase (@global_phases) {
			    _apply_handler_AH_($decl,$gphase)
				if $global_phases{$gphase} <= $global_phase;
			}
			if ($global_phase != 0) {
				# if _gen_handler_AH_ is being called after 
				# CHECK it's for a lexical, so make sure
				# it didn't want to run anything later
			
				local $Carp::CarpLevel = 2;
				carp "Won't be able to apply END handler"
					if $phase{$handler}{END};
			}
			else {
				push @declarations, $decl
			}
		}
		$_ = undef;
	    }
	    return grep {defined && !/$builtin/} @attrs;
	}
}

{
    no strict 'refs';
    *{"Attribute::Handlers::UNIVERSAL::MODIFY_${_}_ATTRIBUTES"} =
	_gen_handler_AH_ foreach @{$validtype{ANY}};
}
push @UNIVERSAL::ISA, 'Attribute::Handlers::UNIVERSAL'
       unless grep /^Attribute::Handlers::UNIVERSAL$/, @UNIVERSAL::ISA;

sub _apply_handler_AH_ {
	my ($declaration, $phase) = @_;
	my ($pkg, $ref, $attr, $data, $raw, $handlerphase, $filename, $linenum) = @$declaration;
	return unless $handlerphase->{$phase};
	# print STDERR "Handling $attr on $ref in $phase with [$data]\n";
	my $type = ref $ref;
	my $handler = "_ATTR_${type}_${attr}";
	my $sym = findsym($pkg, $ref);
	$sym ||= $type eq 'CODE' ? 'ANON' : 'LEXICAL';
	no warnings;
	if (!$raw && defined($data)) {
	    if ($data ne '') {
		my $evaled = eval("package $pkg; no warnings; no strict;
				   local \$SIG{__WARN__}=sub{die}; [$data]");
		$data = $evaled unless $@;
	    }
	    else { $data = undef }
	}
	$pkg->$handler($sym,
		       (ref $sym eq 'GLOB' ? *{$sym}{ref $ref}||$ref : $ref),
		       $attr,
		       $data,
		       $phase,
		       $filename,
		       $linenum,
		      );
	return 1;
}

{
        no warnings 'void';
        CHECK {
               $global_phase++;
               _resolve_lastattr;
               _apply_handler_AH_($_,'CHECK') foreach @declarations;
        }

        INIT {
                $global_phase++;
                _apply_handler_AH_($_,'INIT') foreach @declarations
        }
}

END { $global_phase++; _apply_handler_AH_($_,'END') foreach @declarations }

1;
__END__

=head1 NAME

Attribute::Handlers - Simpler definition of attribute handlers

=head1 VERSION

This document describes version 0.79 of Attribute::Handlers,
released November 25, 2007.

=head1 SYNOPSIS

	package MyClass;
	require v5.6.0;
	use Attribute::Handlers;
	no warnings 'redefine';


	sub Good : ATTR(SCALAR) {
		my ($package, $symbol, $referent, $attr, $data) = @_;

		# Invoked for any scalar variable with a :Good attribute,
		# provided the variable was declared in MyClass (or
		# a derived class) or typed to MyClass.

		# Do whatever to $referent here (executed in CHECK phase).
		...
	}

	sub Bad : ATTR(SCALAR) {
		# Invoked for any scalar variable with a :Bad attribute,
		# provided the variable was declared in MyClass (or
		# a derived class) or typed to MyClass.
		...
	}

	sub Good : ATTR(ARRAY) {
		# Invoked for any array variable with a :Good attribute,
		# provided the variable was declared in MyClass (or
		# a derived class) or typed to MyClass.
		...
	}

	sub Good : ATTR(HASH) {
		# Invoked for any hash variable with a :Good attribute,
		# provided the variable was declared in MyClass (or
		# a derived class) or typed to MyClass.
		...
	}

	sub Ugly : ATTR(CODE) {
		# Invoked for any subroutine declared in MyClass (or a 
		# derived class) with an :Ugly attribute.
		...
	}

	sub Omni : ATTR {
		# Invoked for any scalar, array, hash, or subroutine
		# with an :Omni attribute, provided the variable or
		# subroutine was declared in MyClass (or a derived class)
		# or the variable was typed to MyClass.
		# Use ref($_[2]) to determine what kind of referent it was.
		...
	}


	use Attribute::Handlers autotie => { Cycle => Tie::Cycle };

	my $next : Cycle(['A'..'Z']);


=head1 DESCRIPTION

This module, when inherited by a package, allows that package's class to
define attribute handler subroutines for specific attributes. Variables
and subroutines subsequently defined in that package, or in packages
derived from that package may be given attributes with the same names as
the attribute handler subroutines, which will then be called in one of
the compilation phases (i.e. in a C<BEGIN>, C<CHECK>, C<INIT>, or C<END>
block). (C<UNITCHECK> blocks don't correspond to a global compilation
phase, so they can't be specified here.)

To create a handler, define it as a subroutine with the same name as
the desired attribute, and declare the subroutine itself with the  
attribute C<:ATTR>. For example:

    package LoudDecl;
    use Attribute::Handlers;

    sub Loud :ATTR {
	my ($package, $symbol, $referent, $attr, $data, $phase, $filename, $linenum) = @_;
	print STDERR
	    ref($referent), " ",
	    *{$symbol}{NAME}, " ",
	    "($referent) ", "was just declared ",
	    "and ascribed the ${attr} attribute ",
	    "with data ($data)\n",
	    "in phase $phase\n",
	    "in file $filename at line $linenum\n";
    }

This creates a handler for the attribute C<:Loud> in the class LoudDecl.
Thereafter, any subroutine declared with a C<:Loud> attribute in the class
LoudDecl:

	package LoudDecl;

	sub foo: Loud {...}

causes the above handler to be invoked, and passed:

=over

=item [0]

the name of the package into which it was declared;

=item [1]

a reference to the symbol table entry (typeglob) containing the subroutine;

=item [2]

a reference to the subroutine;

=item [3]

the name of the attribute;

=item [4]

any data associated with that attribute;

=item [5]

the name of the phase in which the handler is being invoked;

=item [6]

the filename in which the handler is being invoked;

=item [7]

the line number in this file.

=back

Likewise, declaring any variables with the C<:Loud> attribute within the
package:

        package LoudDecl;

        my $foo :Loud;
        my @foo :Loud;
        my %foo :Loud;

will cause the handler to be called with a similar argument list (except,
of course, that C<$_[2]> will be a reference to the variable).

The package name argument will typically be the name of the class into
which the subroutine was declared, but it may also be the name of a derived
class (since handlers are inherited).

If a lexical variable is given an attribute, there is no symbol table to 
which it belongs, so the symbol table argument (C<$_[1]>) is set to the
string C<'LEXICAL'> in that case. Likewise, ascribing an attribute to
an anonymous subroutine results in a symbol table argument of C<'ANON'>.

The data argument passes in the value (if any) associated with the
attribute. For example, if C<&foo> had been declared:

        sub foo :Loud("turn it up to 11, man!") {...}

then a reference to an array containing the string
C<"turn it up to 11, man!"> would be passed as the last argument.

Attribute::Handlers makes strenuous efforts to convert
the data argument (C<$_[4]>) to a useable form before passing it to
the handler (but see L<"Non-interpretive attribute handlers">).
If those efforts succeed, the interpreted data is passed in an array
reference; if they fail, the raw data is passed as a string.
For example, all of these:

    sub foo :Loud(till=>ears=>are=>bleeding) {...}
    sub foo :Loud(qw/till ears are bleeding/) {...}
    sub foo :Loud(qw/my, ears, are, bleeding/) {...}
    sub foo :Loud(till,ears,are,bleeding) {...}

causes it to pass C<['till','ears','are','bleeding']> as the handler's
data argument. While:

    sub foo :Loud(['till','ears','are','bleeding']) {...}

causes it to pass C<[ ['till','ears','are','bleeding'] ]>; the array
reference specified in the data being passed inside the standard
array reference indicating successful interpretation.

However, if the data can't be parsed as valid Perl, then
it is passed as an uninterpreted string. For example:

    sub foo :Loud(my,ears,are,bleeding) {...}
    sub foo :Loud(qw/my ears are bleeding) {...}

cause the strings C<'my,ears,are,bleeding'> and
C<'qw/my ears are bleeding'> respectively to be passed as the
data argument.

If no value is associated with the attribute, C<undef> is passed.

=head2 Typed lexicals

Regardless of the package in which it is declared, if a lexical variable is
ascribed an attribute, the handler that is invoked is the one belonging to
the package to which it is typed. For example, the following declarations:

        package OtherClass;

        my LoudDecl $loudobj : Loud;
        my LoudDecl @loudobjs : Loud;
        my LoudDecl %loudobjex : Loud;

causes the LoudDecl::Loud handler to be invoked (even if OtherClass also
defines a handler for C<:Loud> attributes).


=head2 Type-specific attribute handlers

If an attribute handler is declared and the C<:ATTR> specifier is
given the name of a built-in type (C<SCALAR>, C<ARRAY>, C<HASH>, or C<CODE>),
the handler is only applied to declarations of that type. For example,
the following definition:

        package LoudDecl;

        sub RealLoud :ATTR(SCALAR) { print "Yeeeeow!" }

creates an attribute handler that applies only to scalars:


        package Painful;
        use base LoudDecl;

        my $metal : RealLoud;           # invokes &LoudDecl::RealLoud
        my @metal : RealLoud;           # error: unknown attribute
        my %metal : RealLoud;           # error: unknown attribute
        sub metal : RealLoud {...}      # error: unknown attribute

You can, of course, declare separate handlers for these types as well
(but you'll need to specify C<no warnings 'redefine'> to do it quietly):

        package LoudDecl;
        use Attribute::Handlers;
        no warnings 'redefine';

        sub RealLoud :ATTR(SCALAR) { print "Yeeeeow!" }
        sub RealLoud :ATTR(ARRAY) { print "Urrrrrrrrrr!" }
        sub RealLoud :ATTR(HASH) { print "Arrrrrgggghhhhhh!" }
        sub RealLoud :ATTR(CODE) { croak "Real loud sub torpedoed" }

You can also explicitly indicate that a single handler is meant to be
used for all types of referents like so:

        package LoudDecl;
        use Attribute::Handlers;

        sub SeriousLoud :ATTR(ANY) { warn "Hearing loss imminent" }

(I.e. C<ATTR(ANY)> is a synonym for C<:ATTR>).


=head2 Non-interpretive attribute handlers

Occasionally the strenuous efforts Attribute::Handlers makes to convert
the data argument (C<$_[4]>) to a useable form before passing it to
the handler get in the way.

You can turn off that eagerness-to-help by declaring
an attribute handler with the keyword C<RAWDATA>. For example:

        sub Raw          : ATTR(RAWDATA) {...}
        sub Nekkid       : ATTR(SCALAR,RAWDATA) {...}
        sub Au::Naturale : ATTR(RAWDATA,ANY) {...}

Then the handler makes absolutely no attempt to interpret the data it
receives and simply passes it as a string:

        my $power : Raw(1..100);        # handlers receives "1..100"

=head2 Phase-specific attribute handlers

By default, attribute handlers are called at the end of the compilation
phase (in a C<CHECK> block). This seems to be optimal in most cases because
most things that can be defined are defined by that point but nothing has
been executed.

However, it is possible to set up attribute handlers that are called at
other points in the program's compilation or execution, by explicitly
stating the phase (or phases) in which you wish the attribute handler to
be called. For example:

        sub Early    :ATTR(SCALAR,BEGIN) {...}
        sub Normal   :ATTR(SCALAR,CHECK) {...}
        sub Late     :ATTR(SCALAR,INIT) {...}
        sub Final    :ATTR(SCALAR,END) {...}
        sub Bookends :ATTR(SCALAR,BEGIN,END) {...}

As the last example indicates, a handler may be set up to be (re)called in
two or more phases. The phase name is passed as the handler's final argument.

Note that attribute handlers that are scheduled for the C<BEGIN> phase
are handled as soon as the attribute is detected (i.e. before any
subsequently defined C<BEGIN> blocks are executed).


=head2 Attributes as C<tie> interfaces

Attributes make an excellent and intuitive interface through which to tie
variables. For example:

        use Attribute::Handlers;
        use Tie::Cycle;

        sub UNIVERSAL::Cycle : ATTR(SCALAR) {
                my ($package, $symbol, $referent, $attr, $data, $phase) = @_;
                $data = [ $data ] unless ref $data eq 'ARRAY';
                tie $$referent, 'Tie::Cycle', $data;
        }

        # and thereafter...

        package main;

        my $next : Cycle('A'..'Z');     # $next is now a tied variable

        while (<>) {
                print $next;
        }

Note that, because the C<Cycle> attribute receives its arguments in the
C<$data> variable, if the attribute is given a list of arguments, C<$data>
will consist of a single array reference; otherwise, it will consist of the
single argument directly. Since Tie::Cycle requires its cycling values to
be passed as an array reference, this means that we need to wrap
non-array-reference arguments in an array constructor:

        $data = [ $data ] unless ref $data eq 'ARRAY';

Typically, however, things are the other way around: the tieable class expects
its arguments as a flattened list, so the attribute looks like:

        sub UNIVERSAL::Cycle : ATTR(SCALAR) {
                my ($package, $symbol, $referent, $attr, $data, $phase) = @_;
                my @data = ref $data eq 'ARRAY' ? @$data : $data;
                tie $$referent, 'Tie::Whatever', @data;
        }


This software pattern is so widely applicable that Attribute::Handlers
provides a way to automate it: specifying C<'autotie'> in the
C<use Attribute::Handlers> statement. So, the cycling example,
could also be written:

        use Attribute::Handlers autotie => { Cycle => 'Tie::Cycle' };

        # and thereafter...

        package main;

        my $next : Cycle(['A'..'Z']);     # $next is now a tied variable

        while (<>) {
                print $next;

Note that we now have to pass the cycling values as an array reference,
since the C<autotie> mechanism passes C<tie> a list of arguments as a list
(as in the Tie::Whatever example), I<not> as an array reference (as in
the original Tie::Cycle example at the start of this section).

The argument after C<'autotie'> is a reference to a hash in which each key is
the name of an attribute to be created, and each value is the class to which
variables ascribed that attribute should be tied.

Note that there is no longer any need to import the Tie::Cycle module --
Attribute::Handlers takes care of that automagically. You can even pass
arguments to the module's C<import> subroutine, by appending them to the
class name. For example:

	use Attribute::Handlers
		autotie => { Dir => 'Tie::Dir qw(DIR_UNLINK)' };

If the attribute name is unqualified, the attribute is installed in the
current package. Otherwise it is installed in the qualifier's package:

        package Here;

        use Attribute::Handlers autotie => {
                Other::Good => Tie::SecureHash, # tie attr installed in Other::
                        Bad => Tie::Taxes,      # tie attr installed in Here::
            UNIVERSAL::Ugly => Software::Patent # tie attr installed everywhere
        };

Autoties are most commonly used in the module to which they actually tie, 
and need to export their attributes to any module that calls them. To
facilitate this, Attribute::Handlers recognizes a special "pseudo-class" --
C<__CALLER__>, which may be specified as the qualifier of an attribute:

        package Tie::Me::Kangaroo:Down::Sport;

        use Attribute::Handlers autotie => { '__CALLER__::Roo' => __PACKAGE__ };

This causes Attribute::Handlers to define the C<Roo> attribute in the package
that imports the Tie::Me::Kangaroo:Down::Sport module.

Note that it is important to quote the __CALLER__::Roo identifier because
a bug in perl 5.8 will refuse to parse it and cause an unknown error.

=head3 Passing the tied object to C<tie>

Occasionally it is important to pass a reference to the object being tied
to the TIESCALAR, TIEHASH, etc. that ties it. 

The C<autotie> mechanism supports this too. The following code:

	use Attribute::Handlers autotieref => { Selfish => Tie::Selfish };
	my $var : Selfish(@args);

has the same effect as:

	tie my $var, 'Tie::Selfish', @args;

But when C<"autotieref"> is used instead of C<"autotie">:

	use Attribute::Handlers autotieref => { Selfish => Tie::Selfish };
	my $var : Selfish(@args);

the effect is to pass the C<tie> call an extra reference to the variable
being tied:

        tie my $var, 'Tie::Selfish', \$var, @args;



=head1 EXAMPLES

If the class shown in L<SYNOPSIS> were placed in the MyClass.pm
module, then the following code:

        package main;
        use MyClass;

        my MyClass $slr :Good :Bad(1**1-1) :Omni(-vorous);

        package SomeOtherClass;
        use base MyClass;

        sub tent { 'acle' }

        sub fn :Ugly(sister) :Omni('po',tent()) {...}
        my @arr :Good :Omni(s/cie/nt/);
        my %hsh :Good(q/bye/) :Omni(q/bus/);


would cause the following handlers to be invoked:

        # my MyClass $slr :Good :Bad(1**1-1) :Omni(-vorous);

        MyClass::Good:ATTR(SCALAR)( 'MyClass',          # class
                                    'LEXICAL',          # no typeglob
                                    \$slr,              # referent
                                    'Good',             # attr name
                                    undef               # no attr data
                                    'CHECK',            # compiler phase
                                  );

        MyClass::Bad:ATTR(SCALAR)( 'MyClass',           # class
                                   'LEXICAL',           # no typeglob
                                   \$slr,               # referent
                                   'Bad',               # attr name
                                   0                    # eval'd attr data
                                   'CHECK',             # compiler phase
                                 );

        MyClass::Omni:ATTR(SCALAR)( 'MyClass',          # class
                                    'LEXICAL',          # no typeglob
                                    \$slr,              # referent
                                    'Omni',             # attr name
                                    '-vorous'           # eval'd attr data
                                    'CHECK',            # compiler phase
                                  );


        # sub fn :Ugly(sister) :Omni('po',tent()) {...}

        MyClass::UGLY:ATTR(CODE)( 'SomeOtherClass',     # class
                                  \*SomeOtherClass::fn, # typeglob
                                  \&SomeOtherClass::fn, # referent
                                  'Ugly',               # attr name
                                  'sister'              # eval'd attr data
                                  'CHECK',              # compiler phase
                                );

        MyClass::Omni:ATTR(CODE)( 'SomeOtherClass',     # class
                                  \*SomeOtherClass::fn, # typeglob
                                  \&SomeOtherClass::fn, # referent
                                  'Omni',               # attr name
                                  ['po','acle']         # eval'd attr data
                                  'CHECK',              # compiler phase
                                );


        # my @arr :Good :Omni(s/cie/nt/);

        MyClass::Good:ATTR(ARRAY)( 'SomeOtherClass',    # class
                                   'LEXICAL',           # no typeglob
                                   \@arr,               # referent
                                   'Good',              # attr name
                                   undef                # no attr data
                                   'CHECK',             # compiler phase
                                 );

        MyClass::Omni:ATTR(ARRAY)( 'SomeOtherClass',    # class
                                   'LEXICAL',           # no typeglob
                                   \@arr,               # referent
                                   'Omni',              # attr name
                                   ""                   # eval'd attr data 
                                   'CHECK',             # compiler phase
                                 );


        # my %hsh :Good(q/bye) :Omni(q/bus/);
                                  
        MyClass::Good:ATTR(HASH)( 'SomeOtherClass',     # class
                                  'LEXICAL',            # no typeglob
                                  \%hsh,                # referent
                                  'Good',               # attr name
                                  'q/bye'               # raw attr data
                                  'CHECK',              # compiler phase
                                );
                        
        MyClass::Omni:ATTR(HASH)( 'SomeOtherClass',     # class
                                  'LEXICAL',            # no typeglob
                                  \%hsh,                # referent
                                  'Omni',               # attr name
                                  'bus'                 # eval'd attr data
                                  'CHECK',              # compiler phase
                                );


Installing handlers into UNIVERSAL, makes them...err..universal.
For example:

        package Descriptions;
        use Attribute::Handlers;

        my %name;
        sub name { return $name{$_[2]}||*{$_[1]}{NAME} }

        sub UNIVERSAL::Name :ATTR {
                $name{$_[2]} = $_[4];
        }

        sub UNIVERSAL::Purpose :ATTR {
                print STDERR "Purpose of ", &name, " is $_[4]\n";
        }

        sub UNIVERSAL::Unit :ATTR {
                print STDERR &name, " measured in $_[4]\n";
        }

Let's you write:

        use Descriptions;

        my $capacity : Name(capacity)
                     : Purpose(to store max storage capacity for files)
                     : Unit(Gb);


        package Other;

        sub foo : Purpose(to foo all data before barring it) { }

        # etc.


=head1 DIAGNOSTICS

=over

=item C<Bad attribute type: ATTR(%s)>

An attribute handler was specified with an C<:ATTR(I<ref_type>)>, but the
type of referent it was defined to handle wasn't one of the five permitted:
C<SCALAR>, C<ARRAY>, C<HASH>, C<CODE>, or C<ANY>.

=item C<Attribute handler %s doesn't handle %s attributes>

A handler for attributes of the specified name I<was> defined, but not
for the specified type of declaration. Typically encountered whe trying
to apply a C<VAR> attribute handler to a subroutine, or a C<SCALAR>
attribute handler to some other type of variable.

=item C<Declaration of %s attribute in package %s may clash with future reserved word>

A handler for an attributes with an all-lowercase name was declared. An
attribute with an all-lowercase name might have a meaning to Perl
itself some day, even though most don't yet. Use a mixed-case attribute
name, instead.

=item C<Can't have two ATTR specifiers on one subroutine>

You just can't, okay?
Instead, put all the specifications together with commas between them
in a single C<ATTR(I<specification>)>.

=item C<Can't autotie a %s>

You can only declare autoties for types C<"SCALAR">, C<"ARRAY">, and
C<"HASH">. They're the only things (apart from typeglobs -- which are
not declarable) that Perl can tie.

=item C<Internal error: %s symbol went missing>

Something is rotten in the state of the program. An attributed
subroutine ceased to exist between the point it was declared and the point
at which its attribute handler(s) would have been called.

=item C<Won't be able to apply END handler>

You have defined an END handler for an attribute that is being applied
to a lexical variable.  Since the variable may not be available during END
this won't happen.

=back

=head1 AUTHOR

Damian Conway (damian@conway.org). The maintainer of this module is now Rafael
Garcia-Suarez (rgarciasuarez@gmail.com).

=head1 BUGS

There are undoubtedly serious bugs lurking somewhere in code this funky :-)
Bug reports and other feedback are most welcome.

=head1 COPYRIGHT

         Copyright (c) 2001, Damian Conway. All Rights Reserved.
       This module is free software. It may be used, redistributed
           and/or modified under the same terms as Perl itself.
