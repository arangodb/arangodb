package Safe;

use 5.003_11;
use strict;

$Safe::VERSION = "2.16";

# *** Don't declare any lexicals above this point ***
#
# This function should return a closure which contains an eval that can't
# see any lexicals in scope (apart from __ExPr__ which is unavoidable)

sub lexless_anon_sub {
		 # $_[0] is package;
		 # $_[1] is strict flag;
    my $__ExPr__ = $_[2];   # must be a lexical to create the closure that
			    # can be used to pass the value into the safe
			    # world

    # Create anon sub ref in root of compartment.
    # Uses a closure (on $__ExPr__) to pass in the code to be executed.
    # (eval on one line to keep line numbers as expected by caller)
    eval sprintf
    'package %s; %s strict; sub { @_=(); eval q[my $__ExPr__;] . $__ExPr__; }',
		$_[0], $_[1] ? 'use' : 'no';
}

use Carp;
BEGIN { eval q{
    use Carp::Heavy;
} }

use Opcode 1.01, qw(
    opset opset_to_ops opmask_add
    empty_opset full_opset invert_opset verify_opset
    opdesc opcodes opmask define_optag opset_to_hex
);

*ops_to_opset = \&opset;   # Temporary alias for old Penguins


my $default_root  = 0;
# share *_ and functions defined in universal.c
# Don't share stuff like *UNIVERSAL:: otherwise code from the
# compartment can 0wn functions in UNIVERSAL
my $default_share = [qw[
    *_
    &PerlIO::get_layers
    &UNIVERSAL::isa
    &UNIVERSAL::can
    &UNIVERSAL::VERSION
    &utf8::is_utf8
    &utf8::valid
    &utf8::encode
    &utf8::decode
    &utf8::upgrade
    &utf8::downgrade
    &utf8::native_to_unicode
    &utf8::unicode_to_native
    $version::VERSION
    $version::CLASS
    @version::ISA
], ($] >= 5.008001 && qw[
    &Regexp::DESTROY
]), ($] >= 5.010 && qw[
    &re::is_regexp
    &re::regname
    &re::regnames
    &re::regnames_count
    &Tie::Hash::NamedCapture::FETCH
    &Tie::Hash::NamedCapture::STORE
    &Tie::Hash::NamedCapture::DELETE
    &Tie::Hash::NamedCapture::CLEAR
    &Tie::Hash::NamedCapture::EXISTS
    &Tie::Hash::NamedCapture::FIRSTKEY
    &Tie::Hash::NamedCapture::NEXTKEY
    &Tie::Hash::NamedCapture::SCALAR
    &Tie::Hash::NamedCapture::flags
    &UNIVERSAL::DOES
    &version::()
    &version::new
    &version::(""
    &version::stringify
    &version::(0+
    &version::numify
    &version::normal
    &version::(cmp
    &version::(<=>
    &version::vcmp
    &version::(bool
    &version::boolean
    &version::(nomethod
    &version::noop
    &version::is_alpha
    &version::qv
]), ($] >= 5.011 && qw[
    &re::regexp_pattern
])];

sub new {
    my($class, $root, $mask) = @_;
    my $obj = {};
    bless $obj, $class;

    if (defined($root)) {
	croak "Can't use \"$root\" as root name"
	    if $root =~ /^main\b/ or $root !~ /^\w[:\w]*$/;
	$obj->{Root}  = $root;
	$obj->{Erase} = 0;
    }
    else {
	$obj->{Root}  = "Safe::Root".$default_root++;
	$obj->{Erase} = 1;
    }

    # use permit/deny methods instead till interface issues resolved
    # XXX perhaps new Safe 'Root', mask => $mask, foo => bar, ...;
    croak "Mask parameter to new no longer supported" if defined $mask;
    $obj->permit_only(':default');

    # We must share $_ and @_ with the compartment or else ops such
    # as split, length and so on won't default to $_ properly, nor
    # will passing argument to subroutines work (via @_). In fact,
    # for reasons I don't completely understand, we need to share
    # the whole glob *_ rather than $_ and @_ separately, otherwise
    # @_ in non default packages within the compartment don't work.
    $obj->share_from('main', $default_share);
    Opcode::_safe_pkg_prep($obj->{Root}) if($Opcode::VERSION > 1.04);
    return $obj;
}

sub DESTROY {
    my $obj = shift;
    $obj->erase('DESTROY') if $obj->{Erase};
}

sub erase {
    my ($obj, $action) = @_;
    my $pkg = $obj->root();
    my ($stem, $leaf);

    no strict 'refs';
    $pkg = "main::$pkg\::";	# expand to full symbol table name
    ($stem, $leaf) = $pkg =~ m/(.*::)(\w+::)$/;

    # The 'my $foo' is needed! Without it you get an
    # 'Attempt to free unreferenced scalar' warning!
    my $stem_symtab = *{$stem}{HASH};

    #warn "erase($pkg) stem=$stem, leaf=$leaf";
    #warn " stem_symtab hash ".scalar(%$stem_symtab)."\n";
	# ", join(', ', %$stem_symtab),"\n";

#    delete $stem_symtab->{$leaf};

    my $leaf_glob   = $stem_symtab->{$leaf};
    my $leaf_symtab = *{$leaf_glob}{HASH};
#    warn " leaf_symtab ", join(', ', %$leaf_symtab),"\n";
    %$leaf_symtab = ();
    #delete $leaf_symtab->{'__ANON__'};
    #delete $leaf_symtab->{'foo'};
    #delete $leaf_symtab->{'main::'};
#    my $foo = undef ${"$stem\::"}{"$leaf\::"};

    if ($action and $action eq 'DESTROY') {
        delete $stem_symtab->{$leaf};
    } else {
        $obj->share_from('main', $default_share);
    }
    1;
}


sub reinit {
    my $obj= shift;
    $obj->erase;
    $obj->share_redo;
}

sub root {
    my $obj = shift;
    croak("Safe root method now read-only") if @_;
    return $obj->{Root};
}


sub mask {
    my $obj = shift;
    return $obj->{Mask} unless @_;
    $obj->deny_only(@_);
}

# v1 compatibility methods
sub trap   { shift->deny(@_)   }
sub untrap { shift->permit(@_) }

sub deny {
    my $obj = shift;
    $obj->{Mask} |= opset(@_);
}
sub deny_only {
    my $obj = shift;
    $obj->{Mask} = opset(@_);
}

sub permit {
    my $obj = shift;
    # XXX needs testing
    $obj->{Mask} &= invert_opset opset(@_);
}
sub permit_only {
    my $obj = shift;
    $obj->{Mask} = invert_opset opset(@_);
}


sub dump_mask {
    my $obj = shift;
    print opset_to_hex($obj->{Mask}),"\n";
}



sub share {
    my($obj, @vars) = @_;
    $obj->share_from(scalar(caller), \@vars);
}

sub share_from {
    my $obj = shift;
    my $pkg = shift;
    my $vars = shift;
    my $no_record = shift || 0;
    my $root = $obj->root();
    croak("vars not an array ref") unless ref $vars eq 'ARRAY';
    no strict 'refs';
    # Check that 'from' package actually exists
    croak("Package \"$pkg\" does not exist")
	unless keys %{"$pkg\::"};
    my $arg;
    foreach $arg (@$vars) {
	# catch some $safe->share($var) errors:
	my ($var, $type);
	$type = $1 if ($var = $arg) =~ s/^(\W)//;
	# warn "share_from $pkg $type $var";
	*{$root."::$var"} = (!$type)       ? \&{$pkg."::$var"}
			  : ($type eq '&') ? \&{$pkg."::$var"}
			  : ($type eq '$') ? \${$pkg."::$var"}
			  : ($type eq '@') ? \@{$pkg."::$var"}
			  : ($type eq '%') ? \%{$pkg."::$var"}
			  : ($type eq '*') ?  *{$pkg."::$var"}
			  : croak(qq(Can't share "$type$var" of unknown type));
    }
    $obj->share_record($pkg, $vars) unless $no_record or !$vars;
}

sub share_record {
    my $obj = shift;
    my $pkg = shift;
    my $vars = shift;
    my $shares = \%{$obj->{Shares} ||= {}};
    # Record shares using keys of $obj->{Shares}. See reinit.
    @{$shares}{@$vars} = ($pkg) x @$vars if @$vars;
}
sub share_redo {
    my $obj = shift;
    my $shares = \%{$obj->{Shares} ||= {}};
    my($var, $pkg);
    while(($var, $pkg) = each %$shares) {
	# warn "share_redo $pkg\:: $var";
	$obj->share_from($pkg,  [ $var ], 1);
    }
}
sub share_forget {
    delete shift->{Shares};
}

sub varglob {
    my ($obj, $var) = @_;
    no strict 'refs';
    return *{$obj->root()."::$var"};
}


sub reval {
    my ($obj, $expr, $strict) = @_;
    my $root = $obj->{Root};

    my $evalsub = lexless_anon_sub($root,$strict, $expr);
    return Opcode::_safe_call_sv($root, $obj->{Mask}, $evalsub);
}

sub rdo {
    my ($obj, $file) = @_;
    my $root = $obj->{Root};

    my $evalsub = eval
	    sprintf('package %s; sub { @_ = (); do $file }', $root);
    return Opcode::_safe_call_sv($root, $obj->{Mask}, $evalsub);
}


1;

__END__

=head1 NAME

Safe - Compile and execute code in restricted compartments

=head1 SYNOPSIS

  use Safe;

  $compartment = new Safe;

  $compartment->permit(qw(time sort :browse));

  $result = $compartment->reval($unsafe_code);

=head1 DESCRIPTION

The Safe extension module allows the creation of compartments
in which perl code can be evaluated. Each compartment has

=over 8

=item a new namespace

The "root" of the namespace (i.e. "main::") is changed to a
different package and code evaluated in the compartment cannot
refer to variables outside this namespace, even with run-time
glob lookups and other tricks.

Code which is compiled outside the compartment can choose to place
variables into (or I<share> variables with) the compartment's namespace
and only that data will be visible to code evaluated in the
compartment.

By default, the only variables shared with compartments are the
"underscore" variables $_ and @_ (and, technically, the less frequently
used %_, the _ filehandle and so on). This is because otherwise perl
operators which default to $_ will not work and neither will the
assignment of arguments to @_ on subroutine entry.

=item an operator mask

Each compartment has an associated "operator mask". Recall that
perl code is compiled into an internal format before execution.
Evaluating perl code (e.g. via "eval" or "do 'file'") causes
the code to be compiled into an internal format and then,
provided there was no error in the compilation, executed.
Code evaluated in a compartment compiles subject to the
compartment's operator mask. Attempting to evaluate code in a
compartment which contains a masked operator will cause the
compilation to fail with an error. The code will not be executed.

The default operator mask for a newly created compartment is
the ':default' optag.

It is important that you read the L<Opcode> module documentation
for more information, especially for detailed definitions of opnames,
optags and opsets.

Since it is only at the compilation stage that the operator mask
applies, controlled access to potentially unsafe operations can
be achieved by having a handle to a wrapper subroutine (written
outside the compartment) placed into the compartment. For example,

    $cpt = new Safe;
    sub wrapper {
        # vet arguments and perform potentially unsafe operations
    }
    $cpt->share('&wrapper');

=back


=head1 WARNING

The authors make B<no warranty>, implied or otherwise, about the
suitability of this software for safety or security purposes.

The authors shall not in any case be liable for special, incidental,
consequential, indirect or other similar damages arising from the use
of this software.

Your mileage will vary. If in any doubt B<do not use it>.


=head2 RECENT CHANGES

The interface to the Safe module has changed quite dramatically since
version 1 (as supplied with Perl5.002). Study these pages carefully if
you have code written to use Safe version 1 because you will need to
makes changes.


=head2 Methods in class Safe

To create a new compartment, use

    $cpt = new Safe;

Optional argument is (NAMESPACE), where NAMESPACE is the root namespace
to use for the compartment (defaults to "Safe::Root0", incremented for
each new compartment).

Note that version 1.00 of the Safe module supported a second optional
parameter, MASK.  That functionality has been withdrawn pending deeper
consideration. Use the permit and deny methods described below.

The following methods can then be used on the compartment
object returned by the above constructor. The object argument
is implicit in each case.


=over 8

=item permit (OP, ...)

Permit the listed operators to be used when compiling code in the
compartment (in I<addition> to any operators already permitted).

You can list opcodes by names, or use a tag name; see
L<Opcode/"Predefined Opcode Tags">.

=item permit_only (OP, ...)

Permit I<only> the listed operators to be used when compiling code in
the compartment (I<no> other operators are permitted).

=item deny (OP, ...)

Deny the listed operators from being used when compiling code in the
compartment (other operators may still be permitted).

=item deny_only (OP, ...)

Deny I<only> the listed operators from being used when compiling code
in the compartment (I<all> other operators will be permitted).

=item trap (OP, ...)

=item untrap (OP, ...)

The trap and untrap methods are synonyms for deny and permit
respectfully.

=item share (NAME, ...)

This shares the variable(s) in the argument list with the compartment.
This is almost identical to exporting variables using the L<Exporter>
module.

Each NAME must be the B<name> of a non-lexical variable, typically
with the leading type identifier included. A bareword is treated as a
function name.

Examples of legal names are '$foo' for a scalar, '@foo' for an
array, '%foo' for a hash, '&foo' or 'foo' for a subroutine and '*foo'
for a glob (i.e.  all symbol table entries associated with "foo",
including scalar, array, hash, sub and filehandle).

Each NAME is assumed to be in the calling package. See share_from
for an alternative method (which share uses).

=item share_from (PACKAGE, ARRAYREF)

This method is similar to share() but allows you to explicitly name the
package that symbols should be shared from. The symbol names (including
type characters) are supplied as an array reference.

    $safe->share_from('main', [ '$foo', '%bar', 'func' ]);


=item varglob (VARNAME)

This returns a glob reference for the symbol table entry of VARNAME in
the package of the compartment. VARNAME must be the B<name> of a
variable without any leading type marker. For example,

    $cpt = new Safe 'Root';
    $Root::foo = "Hello world";
    # Equivalent version which doesn't need to know $cpt's package name:
    ${$cpt->varglob('foo')} = "Hello world";


=item reval (STRING)

This evaluates STRING as perl code inside the compartment.

The code can only see the compartment's namespace (as returned by the
B<root> method). The compartment's root package appears to be the
C<main::> package to the code inside the compartment.

Any attempt by the code in STRING to use an operator which is not permitted
by the compartment will cause an error (at run-time of the main program
but at compile-time for the code in STRING).  The error is of the form
"'%s' trapped by operation mask...".

If an operation is trapped in this way, then the code in STRING will
not be executed. If such a trapped operation occurs or any other
compile-time or return error, then $@ is set to the error message, just
as with an eval().

If there is no error, then the method returns the value of the last
expression evaluated, or a return statement may be used, just as with
subroutines and B<eval()>. The context (list or scalar) is determined
by the caller as usual.

This behaviour differs from the beta distribution of the Safe extension
where earlier versions of perl made it hard to mimic the return
behaviour of the eval() command and the context was always scalar.

Some points to note:

If the entereval op is permitted then the code can use eval "..." to
'hide' code which might use denied ops. This is not a major problem
since when the code tries to execute the eval it will fail because the
opmask is still in effect. However this technique would allow clever,
and possibly harmful, code to 'probe' the boundaries of what is
possible.

Any string eval which is executed by code executing in a compartment,
or by code called from code executing in a compartment, will be eval'd
in the namespace of the compartment. This is potentially a serious
problem.

Consider a function foo() in package pkg compiled outside a compartment
but shared with it. Assume the compartment has a root package called
'Root'. If foo() contains an eval statement like eval '$foo = 1' then,
normally, $pkg::foo will be set to 1.  If foo() is called from the
compartment (by whatever means) then instead of setting $pkg::foo, the
eval will actually set $Root::pkg::foo.

This can easily be demonstrated by using a module, such as the Socket
module, which uses eval "..." as part of an AUTOLOAD function. You can
'use' the module outside the compartment and share an (autoloaded)
function with the compartment. If an autoload is triggered by code in
the compartment, or by any code anywhere that is called by any means
from the compartment, then the eval in the Socket module's AUTOLOAD
function happens in the namespace of the compartment. Any variables
created or used by the eval'd code are now under the control of
the code in the compartment.

A similar effect applies to I<all> runtime symbol lookups in code
called from a compartment but not compiled within it.



=item rdo (FILENAME)

This evaluates the contents of file FILENAME inside the compartment.
See above documentation on the B<reval> method for further details.

=item root (NAMESPACE)

This method returns the name of the package that is the root of the
compartment's namespace.

Note that this behaviour differs from version 1.00 of the Safe module
where the root module could be used to change the namespace. That
functionality has been withdrawn pending deeper consideration.

=item mask (MASK)

This is a get-or-set method for the compartment's operator mask.

With no MASK argument present, it returns the current operator mask of
the compartment.

With the MASK argument present, it sets the operator mask for the
compartment (equivalent to calling the deny_only method).

=back


=head2 Some Safety Issues

This section is currently just an outline of some of the things code in
a compartment might do (intentionally or unintentionally) which can
have an effect outside the compartment.

=over 8

=item Memory

Consuming all (or nearly all) available memory.

=item CPU

Causing infinite loops etc.

=item Snooping

Copying private information out of your system. Even something as
simple as your user name is of value to others. Much useful information
could be gleaned from your environment variables for example.

=item Signals

Causing signals (especially SIGFPE and SIGALARM) to affect your process.

Setting up a signal handler will need to be carefully considered
and controlled.  What mask is in effect when a signal handler
gets called?  If a user can get an imported function to get an
exception and call the user's signal handler, does that user's
restricted mask get re-instated before the handler is called?
Does an imported handler get called with its original mask or
the user's one?

=item State Changes

Ops such as chdir obviously effect the process as a whole and not just
the code in the compartment. Ops such as rand and srand have a similar
but more subtle effect.

=back

=head2 AUTHOR

Originally designed and implemented by Malcolm Beattie.

Reworked to use the Opcode module and other changes added by Tim Bunce.

Currently maintained by the Perl 5 Porters, <perl5-porters@perl.org>.

=cut

