# Scalar::Util.pm
#
# Copyright (c) 1997-2006 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package Scalar::Util;

use strict;
use vars qw(@ISA @EXPORT_OK $VERSION);
require Exporter;
require List::Util; # List::Util loads the XS

@ISA       = qw(Exporter);
@EXPORT_OK = qw(blessed dualvar reftype weaken isweak tainted readonly openhandle refaddr isvstring looks_like_number set_prototype);
$VERSION    = "1.19";
$VERSION   = eval $VERSION;

sub export_fail {
  if (grep { /^(weaken|isweak)$/ } @_ ) {
    require Carp;
    Carp::croak("Weak references are not implemented in the version of perl");
  }
  if (grep { /^(isvstring)$/ } @_ ) {
    require Carp;
    Carp::croak("Vstrings are not implemented in the version of perl");
  }
  if (grep { /^(dualvar|set_prototype)$/ } @_ ) {
    require Carp;
    Carp::croak("$1 is only avaliable with the XS version");
  }

  @_;
}

sub openhandle ($) {
  my $fh = shift;
  my $rt = reftype($fh) || '';

  return defined(fileno($fh)) ? $fh : undef
    if $rt eq 'IO';

  if (reftype(\$fh) eq 'GLOB') { # handle  openhandle(*DATA)
    $fh = \(my $tmp=$fh);
  }
  elsif ($rt ne 'GLOB') {
    return undef;
  }

  (tied(*$fh) or defined(fileno($fh)))
    ? $fh : undef;
}

eval <<'ESQ' unless defined &dualvar;

use vars qw(@EXPORT_FAIL);
push @EXPORT_FAIL, qw(weaken isweak dualvar isvstring set_prototype);

# The code beyond here is only used if the XS is not installed

# Hope nobody defines a sub by this name
sub UNIVERSAL::a_sub_not_likely_to_be_here { ref($_[0]) }

sub blessed ($) {
  local($@, $SIG{__DIE__}, $SIG{__WARN__});
  length(ref($_[0]))
    ? eval { $_[0]->a_sub_not_likely_to_be_here }
    : undef
}

sub refaddr($) {
  my $pkg = ref($_[0]) or return undef;
  if (blessed($_[0])) {
    bless $_[0], 'Scalar::Util::Fake';
  }
  else {
    $pkg = undef;
  }
  "$_[0]" =~ /0x(\w+)/;
  my $i = do { local $^W; hex $1 };
  bless $_[0], $pkg if defined $pkg;
  $i;
}

sub reftype ($) {
  local($@, $SIG{__DIE__}, $SIG{__WARN__});
  my $r = shift;
  my $t;

  length($t = ref($r)) or return undef;

  # This eval will fail if the reference is not blessed
  eval { $r->a_sub_not_likely_to_be_here; 1 }
    ? do {
      $t = eval {
	  # we have a GLOB or an IO. Stringify a GLOB gives it's name
	  my $q = *$r;
	  $q =~ /^\*/ ? "GLOB" : "IO";
	}
	or do {
	  # OK, if we don't have a GLOB what parts of
	  # a glob will it populate.
	  # NOTE: A glob always has a SCALAR
	  local *glob = $r;
	  defined *glob{ARRAY} && "ARRAY"
	  or defined *glob{HASH} && "HASH"
	  or defined *glob{CODE} && "CODE"
	  or length(ref(${$r})) ? "REF" : "SCALAR";
	}
    }
    : $t
}

sub tainted {
  local($@, $SIG{__DIE__}, $SIG{__WARN__});
  local $^W = 0;
  eval { kill 0 * $_[0] };
  $@ =~ /^Insecure/;
}

sub readonly {
  return 0 if tied($_[0]) || (ref(\($_[0])) ne "SCALAR");

  local($@, $SIG{__DIE__}, $SIG{__WARN__});
  my $tmp = $_[0];

  !eval { $_[0] = $tmp; 1 };
}

sub looks_like_number {
  local $_ = shift;

  # checks from perlfaq4
  return 0 if !defined($_) or ref($_);
  return 1 if (/^[+-]?\d+$/); # is a +/- integer
  return 1 if (/^([+-]?)(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?$/); # a C float
  return 1 if ($] >= 5.008 and /^(Inf(inity)?|NaN)$/i) or ($] >= 5.006001 and /^Inf$/i);

  0;
}

ESQ

1;

__END__

=head1 NAME

Scalar::Util - A selection of general-utility scalar subroutines

=head1 SYNOPSIS

    use Scalar::Util qw(blessed dualvar isweak readonly refaddr reftype tainted
                        weaken isvstring looks_like_number set_prototype);

=head1 DESCRIPTION

C<Scalar::Util> contains a selection of subroutines that people have
expressed would be nice to have in the perl core, but the usage would
not really be high enough to warrant the use of a keyword, and the size
so small such that being individual extensions would be wasteful.

By default C<Scalar::Util> does not export any subroutines. The
subroutines defined are

=over 4

=item blessed EXPR

If EXPR evaluates to a blessed reference the name of the package
that it is blessed into is returned. Otherwise C<undef> is returned.

   $scalar = "foo";
   $class  = blessed $scalar;           # undef

   $ref    = [];
   $class  = blessed $ref;              # undef

   $obj    = bless [], "Foo";
   $class  = blessed $obj;              # "Foo"

=item dualvar NUM, STRING

Returns a scalar that has the value NUM in a numeric context and the
value STRING in a string context.

    $foo = dualvar 10, "Hello";
    $num = $foo + 2;                    # 12
    $str = $foo . " world";             # Hello world

=item isvstring EXPR

If EXPR is a scalar which was coded as a vstring the result is true.

    $vs   = v49.46.48;
    $fmt  = isvstring($vs) ? "%vd" : "%s"; #true
    printf($fmt,$vs);

=item isweak EXPR

If EXPR is a scalar which is a weak reference the result is true.

    $ref  = \$foo;
    $weak = isweak($ref);               # false
    weaken($ref);
    $weak = isweak($ref);               # true

B<NOTE>: Copying a weak reference creates a normal, strong, reference.

    $copy = $ref;
    $weak = isweak($ref);               # false

=item looks_like_number EXPR

Returns true if perl thinks EXPR is a number. See
L<perlapi/looks_like_number>.

=item openhandle FH

Returns FH if FH may be used as a filehandle and is open, or FH is a tied
handle. Otherwise C<undef> is returned.

    $fh = openhandle(*STDIN);		# \*STDIN
    $fh = openhandle(\*STDIN);		# \*STDIN
    $fh = openhandle(*NOTOPEN);		# undef
    $fh = openhandle("scalar");		# undef
    
=item readonly SCALAR

Returns true if SCALAR is readonly.

    sub foo { readonly($_[0]) }

    $readonly = foo($bar);              # false
    $readonly = foo(0);                 # true

=item refaddr EXPR

If EXPR evaluates to a reference the internal memory address of
the referenced value is returned. Otherwise C<undef> is returned.

    $addr = refaddr "string";           # undef
    $addr = refaddr \$var;              # eg 12345678
    $addr = refaddr [];                 # eg 23456784

    $obj  = bless {}, "Foo";
    $addr = refaddr $obj;               # eg 88123488

=item reftype EXPR

If EXPR evaluates to a reference the type of the variable referenced
is returned. Otherwise C<undef> is returned.

    $type = reftype "string";           # undef
    $type = reftype \$var;              # SCALAR
    $type = reftype [];                 # ARRAY

    $obj  = bless {}, "Foo";
    $type = reftype $obj;               # HASH

=item set_prototype CODEREF, PROTOTYPE

Sets the prototype of the given function, or deletes it if PROTOTYPE is
undef. Returns the CODEREF.

    set_prototype \&foo, '$$';

=item tainted EXPR

Return true if the result of EXPR is tainted

    $taint = tainted("constant");       # false
    $taint = tainted($ENV{PWD});        # true if running under -T

=item weaken REF

REF will be turned into a weak reference. This means that it will not
hold a reference count on the object it references. Also when the reference
count on that object reaches zero, REF will be set to undef.

This is useful for keeping copies of references , but you don't want to
prevent the object being DESTROY-ed at its usual time.

    {
      my $var;
      $ref = \$var;
      weaken($ref);                     # Make $ref a weak reference
    }
    # $ref is now undef

Note that if you take a copy of a scalar with a weakened reference,
the copy will be a strong reference.

    my $var;
    my $foo = \$var;
    weaken($foo);                       # Make $foo a weak reference
    my $bar = $foo;                     # $bar is now a strong reference

This may be less obvious in other situations, such as C<grep()>, for instance
when grepping through a list of weakened references to objects that may have
been destroyed already:

    @object = grep { defined } @object;

This will indeed remove all references to destroyed objects, but the remaining
references to objects will be strong, causing the remaining objects to never
be destroyed because there is now always a strong reference to them in the
@object array.

=back

=head1 KNOWN BUGS

There is a bug in perl5.6.0 with UV's that are >= 1<<31. This will
show up as tests 8 and 9 of dualvar.t failing

=head1 SEE ALSO

L<List::Util>

=head1 COPYRIGHT

Copyright (c) 1997-2006 Graham Barr <gbarr@pobox.com>. All rights reserved.
This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

Except weaken and isweak which are

Copyright (c) 1999 Tuomas J. Lukka <lukka@iki.fi>. All rights reserved.
This program is free software; you can redistribute it and/or modify it
under the same terms as perl itself.

=cut
