package fields;

require 5.005;
use strict;
no strict 'refs';
unless( eval q{require warnings::register; warnings::register->import; 1} ) {
    *warnings::warnif = sub { 
        require Carp;
        Carp::carp(@_);
    }
}
use vars qw(%attr $VERSION);

$VERSION = '2.13';

# constant.pm is slow
sub PUBLIC     () { 2**0  }
sub PRIVATE    () { 2**1  }
sub INHERITED  () { 2**2  }
sub PROTECTED  () { 2**3  }


# The %attr hash holds the attributes of the currently assigned fields
# per class.  The hash is indexed by class names and the hash value is
# an array reference.  The first element in the array is the lowest field
# number not belonging to a base class.  The remaining elements' indices
# are the field numbers.  The values are integer bit masks, or undef
# in the case of base class private fields (which occupy a slot but are
# otherwise irrelevant to the class).

sub import {
    my $class = shift;
    return unless @_;
    my $package = caller(0);
    # avoid possible typo warnings
    %{"$package\::FIELDS"} = () unless %{"$package\::FIELDS"};
    my $fields = \%{"$package\::FIELDS"};
    my $fattr = ($attr{$package} ||= [1]);
    my $next = @$fattr;

    # Quiet pseudo-hash deprecation warning for uses of fields::new.
    bless \%{"$package\::FIELDS"}, 'pseudohash';

    if ($next > $fattr->[0]
        and ($fields->{$_[0]} || 0) >= $fattr->[0])
    {
        # There are already fields not belonging to base classes.
        # Looks like a possible module reload...
        $next = $fattr->[0];
    }
    foreach my $f (@_) {
        my $fno = $fields->{$f};

        # Allow the module to be reloaded so long as field positions
        # have not changed.
        if ($fno and $fno != $next) {
            require Carp;
            if ($fno < $fattr->[0]) {
              if ($] < 5.006001) {
                warn("Hides field '$f' in base class") if $^W;
              } else {
                warnings::warnif("Hides field '$f' in base class") ;
              }
            } else {
                Carp::croak("Field name '$f' already in use");
            }
        }
        $fields->{$f} = $next;
        $fattr->[$next] = ($f =~ /^_/) ? PRIVATE : PUBLIC;
        $next += 1;
    }
    if (@$fattr > $next) {
        # Well, we gave them the benefit of the doubt by guessing the
        # module was reloaded, but they appear to be declaring fields
        # in more than one place.  We can't be sure (without some extra
        # bookkeeping) that the rest of the fields will be declared or
        # have the same positions, so punt.
        require Carp;
        Carp::croak ("Reloaded module must declare all fields at once");
    }
}

sub inherit {
    require base;
    goto &base::inherit_fields;
}

sub _dump  # sometimes useful for debugging
{
    for my $pkg (sort keys %attr) {
        print "\n$pkg";
        if (@{"$pkg\::ISA"}) {
            print " (", join(", ", @{"$pkg\::ISA"}), ")";
        }
        print "\n";
        my $fields = \%{"$pkg\::FIELDS"};
        for my $f (sort {$fields->{$a} <=> $fields->{$b}} keys %$fields) {
            my $no = $fields->{$f};
            print "   $no: $f";
            my $fattr = $attr{$pkg}[$no];
            if (defined $fattr) {
                my @a;
                push(@a, "public")    if $fattr & PUBLIC;
                push(@a, "private")   if $fattr & PRIVATE;
                push(@a, "inherited") if $fattr & INHERITED;
                print "\t(", join(", ", @a), ")";
            }
            print "\n";
        }
    }
}

if ($] < 5.009) {
  *new = sub {
    my $class = shift;
    $class = ref $class if ref $class;
    return bless [\%{$class . "::FIELDS"}], $class;
  }
} else {
  *new = sub {
    my $class = shift;
    $class = ref $class if ref $class;
    require Hash::Util;
    my $self = bless {}, $class;

    # The lock_keys() prototype won't work since we require Hash::Util :(
    &Hash::Util::lock_keys(\%$self, _accessible_keys($class));
    return $self;
  }
}

sub _accessible_keys {
    my ($class) = @_;
    return (
        keys %{$class.'::FIELDS'},
        map(_accessible_keys($_), @{$class.'::ISA'}),
    );
}

sub phash {
    die "Pseudo-hashes have been removed from Perl" if $] >= 5.009;
    my $h;
    my $v;
    if (@_) {
       if (ref $_[0] eq 'ARRAY') {
           my $a = shift;
           @$h{@$a} = 1 .. @$a;
           if (@_) {
               $v = shift;
               unless (! @_ and ref $v eq 'ARRAY') {
                   require Carp;
                   Carp::croak ("Expected at most two array refs\n");
               }
           }
       }
       else {
           if (@_ % 2) {
               require Carp;
               Carp::croak ("Odd number of elements initializing pseudo-hash\n");
           }
           my $i = 0;
           @$h{grep ++$i % 2, @_} = 1 .. @_ / 2;
           $i = 0;
           $v = [grep $i++ % 2, @_];
       }
    }
    else {
       $h = {};
       $v = [];
    }
    [ $h, @$v ];

}

1;

__END__

=head1 NAME

fields - compile-time class fields

=head1 SYNOPSIS

    {
        package Foo;
        use fields qw(foo bar _Foo_private);
        sub new {
            my Foo $self = shift;
            unless (ref $self) {
                $self = fields::new($self);
                $self->{_Foo_private} = "this is Foo's secret";
            }
            $self->{foo} = 10;
            $self->{bar} = 20;
            return $self;
        }
    }

    my $var = Foo->new;
    $var->{foo} = 42;

    # this will generate an error
    $var->{zap} = 42;

    # subclassing
    {
        package Bar;
        use base 'Foo';
        use fields qw(baz _Bar_private);        # not shared with Foo
        sub new {
            my $class = shift;
            my $self = fields::new($class);
            $self->SUPER::new();                # init base fields
            $self->{baz} = 10;                  # init own fields
            $self->{_Bar_private} = "this is Bar's secret";
            return $self;
        }
    }

=head1 DESCRIPTION

The C<fields> pragma enables compile-time verified class fields.

NOTE: The current implementation keeps the declared fields in the %FIELDS
hash of the calling package, but this may change in future versions.
Do B<not> update the %FIELDS hash directly, because it must be created
at compile-time for it to be fully useful, as is done by this pragma.

B<Only valid for perl before 5.9.0:>

If a typed lexical variable holding a reference is used to access a
hash element and a package with the same name as the type has
declared class fields using this pragma, then the operation is
turned into an array access at compile time.


The related C<base> pragma will combine fields from base classes and any
fields declared using the C<fields> pragma.  This enables field
inheritance to work properly.

Field names that start with an underscore character are made private to
the class and are not visible to subclasses.  Inherited fields can be
overridden but will generate a warning if used together with the C<-w>
switch.

B<Only valid for perls before 5.9.0:>

The effect of all this is that you can have objects with named
fields which are as compact and as fast arrays to access. This only
works as long as the objects are accessed through properly typed
variables. If the objects are not typed, access is only checked at
run time.


The following functions are supported:

=over 4

=item new

B< perl before 5.9.0: > fields::new() creates and blesses a
pseudo-hash comprised of the fields declared using the C<fields>
pragma into the specified class.

B< perl 5.9.0 and higher: > fields::new() creates and blesses a
restricted-hash comprised of the fields declared using the C<fields>
pragma into the specified class.

This function is usable with or without pseudo-hashes.  It is the
recommended way to construct a fields-based object.

This makes it possible to write a constructor like this:

    package Critter::Sounds;
    use fields qw(cat dog bird);

    sub new {
        my $self = shift;
        $self = fields::new($self) unless ref $self;
        $self->{cat} = 'meow';                          # scalar element
        @$self{'dog','bird'} = ('bark','tweet');        # slice
        return $self;
    }

=item phash

B< before perl 5.9.0: > 

fields::phash() can be used to create and initialize a plain (unblessed)
pseudo-hash.  This function should always be used instead of creating
pseudo-hashes directly.

If the first argument is a reference to an array, the pseudo-hash will
be created with keys from that array.  If a second argument is supplied,
it must also be a reference to an array whose elements will be used as
the values.  If the second array contains less elements than the first,
the trailing elements of the pseudo-hash will not be initialized.
This makes it particularly useful for creating a pseudo-hash from
subroutine arguments:

    sub dogtag {
       my $tag = fields::phash([qw(name rank ser_num)], [@_]);
    }

fields::phash() also accepts a list of key-value pairs that will
be used to construct the pseudo hash.  Examples:

    my $tag = fields::phash(name => "Joe",
                            rank => "captain",
                            ser_num => 42);

    my $pseudohash = fields::phash(%args);

B< perl 5.9.0 and higher: >

Pseudo-hashes have been removed from Perl as of 5.10.  Consider using
restricted hashes or fields::new() instead.  Using fields::phash()
will cause an error.

=back

=head1 SEE ALSO

L<base>

=cut
