#
# Documentation at the __END__
#

package Alias;

require 5.004;
require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader);
@EXPORT = qw(alias attr);
@EXPORT_OK = qw(const);

$VERSION = $VERSION = '2.32';

use Carp;

bootstrap Alias;

$Alias::KeyFilter = "";
$Alias::AttrPrefix = "";
$Alias::Deref = "";            # don't deref objects

sub alias {
  croak "Need even number of args" if @_ % 2;
  my($pkg) = caller;              # for namespace soundness
  while (@_) {
    # *foo = \*bar works in 5.002
    *{"$pkg\:\:$_[0]"} = (defined($_[1]) and ref($_[1])) ? $_[1] : \$_[1];
    shift; shift;
  }
}

# alias the elements of hashref
# same as alias %{$_[0]}, but also localizes the aliases and
# returns the hashref
sub attr;

alias const => \&alias;           # alias the alias :-)


1;
__END__

=head1 NAME

alias - declare symbolic aliases for perl data

attr  - auto-declare hash attributes for convenient access

const - define compile-time scalar constants


=head1 SYNOPSIS

    use Alias qw(alias const attr);
    alias TEN => $ten, Ten => \$ten, Ten => \&ten,
          Ten => \@ten, Ten => \%ten, TeN => \*ten;
    {
       local @Ten;
       my $ten = [1..10];
       alias Ten => $ten;   # local @Ten
    }

    const pi => 3.14, ten => 10;

    package Foo;
    use Alias;
    sub new { bless {foo => 1, _bar => [2, 3]}, $_[0] }
    sub a_method {
       my $s = attr shift;
       # $foo, @_bar are now local aliases for
       # $_[0]{foo}, @{$_[0]{_bar}} etc.
    }

    sub b_method {
      local $Alias::KeyFilter = "_";
      local $Alias::AttrPrefix = "main::";
      my $s = attr shift;
      # local @::_bar is now available, ($foo, $::foo are not)
    }

    sub c_method {
      local $Alias::KeyFilter = sub { $_ = shift; return (/^_/ ? 1 : 0) };
      local $Alias::AttrPrefix = sub {
                                       $_ = shift;
				       s/^_(.+)$/main::$1/;
				       return $_
				     };
      my $s = attr shift;
      # local @::bar is now available, ($foo, $::foo are not)
    }


=head1 DESCRIPTION

Provides general mechanisms for aliasing perl data for convenient access.

This module works by putting some values on the symbol table with
user-supplied names.  Values that are references will get dereferenced into
their base types.  This means that a value of C<[1,2,3]> with a name of
"foo" will be made available as C<@foo>, not C<$foo>.

The exception to this rule is the default behavior of the C<attr> function,
which will not dereference values which are blessed references (aka
objects).  See L<$Alias::Deref> for how to change this default behavior.

=head2 Functions

=over 4

=item alias

Given a list of name => value pairs, declares aliases
in the C<caller>s namespace. If the value supplied is a reference, the
alias is created for the underlying value instead of the reference
itself (there is no need to use this module to alias references--they
are automatically "aliased" on assignment).  This allows the user to
alias most of the basic types.

If the value supplied is a scalar compile-time constant, the aliases 
become read-only. Any attempt to write to them will fail with a run time
error. 

Aliases can be dynamically scoped by pre-declaring the target variable as
C<local>.  Using C<attr> for this purpose is more convenient, and
recommended.

=item attr

Given a hash reference, aliases the values of the hash to the names that
correspond to the keys.  It always returns the supplied value.  The aliases
are local to the enclosing block. If any of the values are unblessed
references, they are available as their dereferenced types.  Thus the action
is similar to saying:

    alias %{$_[0]}

but, in addition, also localizes the aliases, and does not dereference
objects.  Dereferencing of objects can be forced by setting the C<Deref>
option.  See L<$Alias::Deref>.

This can be used for convenient access to hash values and hash-based object
attributes.  

Note that this makes available the semantics of C<local> subroutines and
methods.  That makes for some nifty possibilities.  We could make truly
private methods by putting anonymous subs within an object.  These subs
would be available within methods where we use C<attr>, and will not
be visible to the outside world as normal methods.  We could forbid 
recursion in methods by always putting an empty sub in the object hash 
with the same key as the method name. This would be useful where a method 
has to run code from other modules, but cannot be certain whether that 
module will call it back again.

The default behavior is to create aliases for all the entries in the hash,
in the callers namespace.  This can be controlled by setting a few options.
See L<Configuration Variables> for details.

=item const

This is simply a function alias for C<alias>, described above.  Provided on
demand at C<use> time, since it reads better for constant declarations.
Note that hashes and arrays cannot be so C<const>rained.

=back

=head2 Configuration Variables

The following configuration variables can be used to control the behavior of
the C<attr> function.  They are typically set after the C<use Alias;>
statement.  Another typical usage is to C<local>ize them in a block so that
their values are only effective within that block.

=over 4

=item $Alias::KeyFilter

Specifies the key prefix used for determining which hash entries will be
interned by C<attr>.  Can be a CODE reference, in which case it will be
called with the key, and the boolean return value will determine if
that hash entry is a candidate attribute.

=item $Alias::AttrPrefix

Specifies a prefix to prepend to the names of localized attributes created
by C<attr>.  Can be a CODE reference, in which case it will be called with
the key, and the result will determine the full name of the attribute.  The
value can have embedded package delimiters ("::" or "'"), which cause the
attributes to be interned in that namespace instead of the C<caller>s own
namespace. For example, setting it to "main::" makes C<use strict 'vars';>
somewhat more palatable (since we can refer to the attributes as C<$::foo>,
etc., without actually declaring the attributes).

=item $Alias::Deref

Controls the implicit dereferencing behavior of C<attr>.  If it is set to
"" or 0, C<attr> will not dereference blessed references.  If it is a true
value (anything but "", 0, or a CODE reference), all references will be
made available as their dereferenced types, including values that may be
objects.  The default is "".

This option can be used as a filter if it is set to a CODE reference, in
which case it will be called with the key and the value (whenever the value
happens to be a reference), and the boolean return value will determine if
that particular reference must be dereferenced.


=back

=head2 Exports

=over 4

=item alias

=item attr

=back

=head1 EXAMPLES

Run these code snippets and observe the results to become more familiar
with the features of this module.

    use Alias qw(alias const attr);
    $ten = 10;
    alias TEN => $ten, Ten => \$ten, Ten => \&ten,
    	  Ten => \@ten, Ten => \%ten;
    alias TeN => \*ten;  # same as *TeN = *ten

    # aliasing basic types
    $ten = 20;   
    print "$TEN|$Ten|$ten\n";   # should print "20|20|20"
    sub ten { print "10\n"; }
    @ten = (1..10);
    %ten = (a..j);
    &Ten;                       # should print "10"
    print @Ten, "|", %Ten, "\n";

    # this will fail at run time
    const _TEN_ => 10;
    eval { $_TEN_ = 20 };
    print $@ if $@;

    # dynamically scoped aliases
    @DYNAMIC = qw(m n o);
    {
       my $tmp = [ qw(a b c d) ];
       local @DYNAMIC;
       alias DYNAMIC => $tmp, PERM => $tmp;

       $DYNAMIC[2] = 'zzz';
       # prints "abzzzd|abzzzd|abzzzd"
       print @$tmp, "|", @DYNAMIC, "|", @PERM, "\n";

       @DYNAMIC = qw(p q r);
       # prints "pqr|pqr|pqr"
       print @$tmp, "|", @DYNAMIC, "|", @PERM, "\n";
    }

    # prints "mno|pqr"
    print @DYNAMIC, "|", @PERM, "\n";

    # named closures
    my($lex) = 'abcd';
    $closure = sub { print $lex, "\n" };
    alias NAMEDCLOSURE => \&$closure;
    NAMEDCLOSURE();            # prints "abcd"
    $lex = 'pqrs';
    NAMEDCLOSURE();            # prints "pqrs"

    # hash/object attributes
    package Foo;
    use Alias;
    sub new { 
      bless 
	{ foo => 1, 
          bar => [2,3], 
          buz => { a => 4},
          privmeth => sub { "private" },
          easymeth => sub { die "to recurse or to die, is the question" },
        }, $_[0]; 
    }

    sub easymeth {
      my $s = attr shift;    # localizes $foo, @bar, %buz etc with values
      eval { $s->easymeth }; # should fail
      print $@ if $@;

      # prints "1|2|3|a|4|private|"
      print join '|', $foo, @bar, %buz, $s->privmeth, "\n";
    }

    $foo = 6;
    @bar = (7,8);
    %buz = (b => 9);
    Foo->new->easymeth;       # this will not recurse endlessly

    # prints "6|7|8|b|9|"
    print join '|', $foo, @bar, %buz, "\n";

    # this should fail at run-time
    eval { Foo->new->privmeth };
    print $@ if $@;


=head1 NOTES

It is worth repeating that the aliases created by C<alias> and C<const> will
be created in the C<caller>s namespace (we can use the C<AttrPrefix> option to
specify a different namespace for C<attr>).  If that namespace happens to be
C<local>ized, the aliases created will be local to that block.  C<attr>
localizes the aliases for us.

Remember that references will be available as their dereferenced types.

Aliases cannot be lexical, since, by neccessity, they live on the
symbol table. 

Lexicals can be aliased. Note that this provides a means of reversing the
action of anonymous type generators C<\>, C<[]> and C<{}>.  This allows us
to anonymously construct data or code and give it a symbol-table presence
when we choose.

Any occurrence of C<::> or C<'> in names will be treated as package
qualifiers, and the value will be interned in that namespace.

Remember that aliases are very much like references, only we don't
have to dereference them as often.  Which means we won't have to
pound on the dollars so much.

We can dynamically make subroutines and named closures with this scheme.

It is possible to alias packages, but that might be construed as
abuse.

Using this module will dramatically reduce noise characters in 
object-oriented perl code.


=head1 BUGS

C<use strict 'vars';> is not very usable, since we B<depend> so much
on the symbol table.  You can declare the attributes with C<use vars> to
avoid warnings.  Setting C<$Alias::AttrPrefix> to "main::" is one way
to avoid C<use vars> and frustration.

Tied variables cannot be aliased properly, yet.


=head1 VERSION

Version 2.32       30 Apr 1999


=head1 AUTHOR

Gurusamy Sarathy                gsar@umich.edu

Copyright (c) 1995-99 Gurusamy Sarathy. All rights reserved.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=head1 SEE ALSO

perl(1)

=cut
