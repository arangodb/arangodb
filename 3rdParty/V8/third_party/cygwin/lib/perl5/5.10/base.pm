package base;

use strict 'vars';
use vars qw($VERSION);
$VERSION = '2.14';
$VERSION = eval $VERSION;

# constant.pm is slow
sub SUCCESS () { 1 }

sub PUBLIC     () { 2**0  }
sub PRIVATE    () { 2**1  }
sub INHERITED  () { 2**2  }
sub PROTECTED  () { 2**3  }


my $Fattr = \%fields::attr;

sub has_fields {
    my($base) = shift;
    my $fglob = ${"$base\::"}{FIELDS};
    return( ($fglob && 'GLOB' eq ref($fglob) && *$fglob{HASH}) ? 1 : 0 );
}

sub has_version {
    my($base) = shift;
    my $vglob = ${$base.'::'}{VERSION};
    return( ($vglob && *$vglob{SCALAR}) ? 1 : 0 );
}

sub has_attr {
    my($proto) = shift;
    my($class) = ref $proto || $proto;
    return exists $Fattr->{$class};
}

sub get_attr {
    $Fattr->{$_[0]} = [1] unless $Fattr->{$_[0]};
    return $Fattr->{$_[0]};
}

if ($] < 5.009) {
    *get_fields = sub {
        # Shut up a possible typo warning.
        () = \%{$_[0].'::FIELDS'};
        my $f = \%{$_[0].'::FIELDS'};

        # should be centralized in fields? perhaps
        # fields::mk_FIELDS_be_OK. Peh. As long as %{ $package . '::FIELDS' }
        # is used here anyway, it doesn't matter.
        bless $f, 'pseudohash' if (ref($f) ne 'pseudohash');

        return $f;
    }
}
else {
    *get_fields = sub {
        # Shut up a possible typo warning.
        () = \%{$_[0].'::FIELDS'};
        return \%{$_[0].'::FIELDS'};
    }
}

sub import {
    my $class = shift;

    return SUCCESS unless @_;

    # List of base classes from which we will inherit %FIELDS.
    my $fields_base;

    my $inheritor = caller(0);
    my @isa_classes;

    my @bases;
    foreach my $base (@_) {
        if ( $inheritor eq $base ) {
            warn "Class '$inheritor' tried to inherit from itself\n";
        }

        next if grep $_->isa($base), ($inheritor, @bases);

        if (has_version($base)) {
            ${$base.'::VERSION'} = '-1, set by base.pm' 
              unless defined ${$base.'::VERSION'};
        }
        else {
            my $sigdie;
            {
                local $SIG{__DIE__};
                eval "require $base";
                # Only ignore "Can't locate" errors from our eval require.
                # Other fatal errors (syntax etc) must be reported.
                die if $@ && $@ !~ /^Can't locate .*? at \(eval /;
                unless (%{"$base\::"}) {
                    require Carp;
                    local $" = " ";
                    Carp::croak(<<ERROR);
Base class package "$base" is empty.
    (Perhaps you need to 'use' the module which defines that package first,
    or make that module available in \@INC (\@INC contains: @INC).
ERROR
                }
                $sigdie = $SIG{__DIE__} || undef;
            }
            # Make sure a global $SIG{__DIE__} makes it out of the localization.
            $SIG{__DIE__} = $sigdie if defined $sigdie;
            ${$base.'::VERSION'} = "-1, set by base.pm"
              unless defined ${$base.'::VERSION'};
        }
        push @bases, $base;

        if ( has_fields($base) || has_attr($base) ) {
            # No multiple fields inheritance *suck*
            if ($fields_base) {
                require Carp;
                Carp::croak("Can't multiply inherit fields");
            } else {
                $fields_base = $base;
            }
        }
    }
    # Save this until the end so it's all or nothing if the above loop croaks.
    push @{"$inheritor\::ISA"}, @isa_classes;

    push @{"$inheritor\::ISA"}, @bases;

    if( defined $fields_base ) {
        inherit_fields($inheritor, $fields_base);
    }
}


sub inherit_fields {
    my($derived, $base) = @_;

    return SUCCESS unless $base;

    my $battr = get_attr($base);
    my $dattr = get_attr($derived);
    my $dfields = get_fields($derived);
    my $bfields = get_fields($base);

    $dattr->[0] = @$battr;

    if( keys %$dfields ) {
        warn <<"END";
$derived is inheriting from $base but already has its own fields!
This will cause problems.  Be sure you use base BEFORE declaring fields.
END

    }

    # Iterate through the base's fields adding all the non-private
    # ones to the derived class.  Hang on to the original attribute
    # (Public, Private, etc...) and add Inherited.
    # This is all too complicated to do efficiently with add_fields().
    while (my($k,$v) = each %$bfields) {
        my $fno;
        if ($fno = $dfields->{$k} and $fno != $v) {
            require Carp;
            Carp::croak ("Inherited fields can't override existing fields");
        }

        if( $battr->[$v] & PRIVATE ) {
            $dattr->[$v] = PRIVATE | INHERITED;
        }
        else {
            $dattr->[$v] = INHERITED | $battr->[$v];
            $dfields->{$k} = $v;
        }
    }

    foreach my $idx (1..$#{$battr}) {
        next if defined $dattr->[$idx];
        $dattr->[$idx] = $battr->[$idx] & INHERITED;
    }
}


1;

__END__

=head1 NAME

base - Establish an ISA relationship with base classes at compile time

=head1 SYNOPSIS

    package Baz;
    use base qw(Foo Bar);

=head1 DESCRIPTION

Unless you are using the C<fields> pragma, consider this module discouraged
in favor of the lighter-weight C<parent>.

Allows you to both load one or more modules, while setting up inheritance from
those modules at the same time.  Roughly similar in effect to

    package Baz;
    BEGIN {
        require Foo;
        require Bar;
        push @ISA, qw(Foo Bar);
    }

C<base> employs some heuristics to determine if a module has already been
loaded, if it has it doesn't try again. If C<base> tries to C<require> the
module it will not die if it cannot find the module's file, but will die on any
other error. After all this, should your base class be empty, containing no
symbols, it will die. This is useful for inheriting from classes in the same
file as yourself, like so:

        package Foo;
        sub exclaim { "I can have such a thing?!" }
        
        package Bar;
        use base "Foo";

If $VERSION is not detected even after loading it, <base> will define $VERSION
in the base package, setting it to the string C<-1, set by base.pm>.

C<base> will also initialize the fields if one of the base classes has it.
Multiple inheritance of fields is B<NOT> supported, if two or more base classes
each have inheritable fields the 'base' pragma will croak. See L<fields>,
L<public> and L<protected> for a description of this feature.

The base class' C<import> method is B<not> called.


=head1 DIAGNOSTICS

=over 4

=item Base class package "%s" is empty.

base.pm was unable to require the base package, because it was not
found in your path.

=item Class 'Foo' tried to inherit from itself

Attempting to inherit from yourself generates a warning.

    use Foo;
    use base 'Foo';

=back

=head1 HISTORY

This module was introduced with Perl 5.004_04.

=head1 CAVEATS

Due to the limitations of the implementation, you must use
base I<before> you declare any of your own fields.


=head1 SEE ALSO

L<fields>

=cut
