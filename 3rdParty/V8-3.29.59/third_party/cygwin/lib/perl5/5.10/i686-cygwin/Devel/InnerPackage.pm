package Devel::InnerPackage;

use strict;
use base qw(Exporter);
use vars qw($VERSION @EXPORT_OK);

$VERSION = '0.3';
@EXPORT_OK = qw(list_packages);

=pod

=head1 NAME


Devel::InnerPackage - find all the inner packages of a package

=head1 SYNOPSIS

    use Foo::Bar;
    use Devel::InnerPackage qw(list_packages);

    my @inner_packages = list_packages('Foo::Bar');


=head1 DESCRIPTION


Given a file like this


    package Foo::Bar;

    sub foo {}


    package Foo::Bar::Quux;

    sub quux {}

    package Foo::Bar::Quirka;

    sub quirka {}

    1;

then

    list_packages('Foo::Bar');

will return

    Foo::Bar::Quux
    Foo::Bar::Quirka

=head1 METHODS

=head2 list_packages <package name>

Return a list of all inner packages of that package.

=cut

sub list_packages {
            my $pack = shift; $pack .= "::" unless $pack =~ m!::$!;

            no strict 'refs';
            my @packs;
            my @stuff = grep !/^(main|)::$/, keys %{$pack};
            for my $cand (grep /::$/, @stuff)
            {
                $cand =~ s!::$!!;
                my @children = list_packages($pack.$cand);
    
                push @packs, "$pack$cand" unless $cand =~ /^::/ ||
                    !__PACKAGE__->_loaded($pack.$cand); # or @children;
                push @packs, @children;
            }
            return grep {$_ !~ /::(::ISA::CACHE|SUPER)/} @packs;
}

### XXX this is an inlining of the Class-Inspector->loaded()
### method, but inlined to remove the dependency.
sub _loaded {
       my ($class, $name) = @_;

    no strict 'refs';

       # Handle by far the two most common cases
       # This is very fast and handles 99% of cases.
       return 1 if defined ${"${name}::VERSION"};
       return 1 if defined @{"${name}::ISA"};

       # Are there any symbol table entries other than other namespaces
       foreach ( keys %{"${name}::"} ) {
               next if substr($_, -2, 2) eq '::';
               return 1 if defined &{"${name}::$_"};
       }

       # No functions, and it doesn't have a version, and isn't anything.
       # As an absolute last resort, check for an entry in %INC
       my $filename = join( '/', split /(?:'|::)/, $name ) . '.pm';
       return 1 if defined $INC{$filename};

       '';
}


=head1 AUTHOR

Simon Wistow <simon@thegestalt.org>

=head1 COPYING

Copyright, 2005 Simon Wistow

Distributed under the same terms as Perl itself.

=head1 BUGS

None known.

=cut 





1;
