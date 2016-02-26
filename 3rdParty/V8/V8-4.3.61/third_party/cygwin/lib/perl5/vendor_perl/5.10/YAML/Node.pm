package YAML::Node;
use strict; use warnings;
use YAML::Base; use base 'YAML::Base';
use YAML::Tag;

our @EXPORT = qw(ynode);

sub ynode {
    my $self;
    if (ref($_[0]) eq 'HASH') {
	$self = tied(%{$_[0]});
    }
    elsif (ref($_[0]) eq 'ARRAY') {
	$self = tied(@{$_[0]});
    }
    else {
	$self = tied($_[0]);
    }
    return (ref($self) =~ /^yaml_/) ? $self : undef;
}

sub new {
    my ($class, $node, $tag) = @_;
    my $self;
    $self->{NODE} = $node;
    my (undef, $type) = $class->node_info($node);
    $self->{KIND} = (not defined $type) ? 'scalar' :
                    ($type eq 'ARRAY') ? 'sequence' :
		    ($type eq 'HASH') ? 'mapping' :
		    $class->die("Can't create YAML::Node from '$type'");
    tag($self, ($tag || ''));
    if ($self->{KIND} eq 'scalar') {
	yaml_scalar->new($self, $_[1]);
	return \ $_[1];
    }
    my $package = "yaml_" . $self->{KIND};    
    $package->new($self)
}

sub node { $_->{NODE} }
sub kind { $_->{KIND} }
sub tag {
    my ($self, $value) = @_;
    if (defined $value) {
       	$self->{TAG} = YAML::Tag->new($value);
	return $self;
    }
    else {
       return $self->{TAG};
    }
}
sub keys {
    my ($self, $value) = @_;
    if (defined $value) {
       	$self->{KEYS} = $value;
	return $self;
    }
    else {
       return $self->{KEYS};
    }
}

#==============================================================================
package yaml_scalar;
@yaml_scalar::ISA = qw(YAML::Node);

sub new {
    my ($class, $self) = @_;
    tie $_[2], $class, $self;
}

sub TIESCALAR {
    my ($class, $self) = @_;
    bless $self, $class;
    $self
}

sub FETCH {
    my ($self) = @_;
    $self->{NODE}
}

sub STORE {
    my ($self, $value) = @_;
    $self->{NODE} = $value
}

#==============================================================================
package yaml_sequence;
@yaml_sequence::ISA = qw(YAML::Node);

sub new {
    my ($class, $self) = @_;
    my $new;
    tie @$new, $class, $self;
    $new
}

sub TIEARRAY {
    my ($class, $self) = @_;
    bless $self, $class
}

sub FETCHSIZE {
    my ($self) = @_;
    scalar @{$self->{NODE}};
}

sub FETCH {
    my ($self, $index) = @_;
    $self->{NODE}[$index]
}

sub STORE {
    my ($self, $index, $value) = @_;
    $self->{NODE}[$index] = $value
}

sub undone {
    die "Not implemented yet"; # XXX
}

*STORESIZE = *POP = *PUSH = *SHIFT = *UNSHIFT = *SPLICE = *DELETE = *EXISTS = 
*STORESIZE = *POP = *PUSH = *SHIFT = *UNSHIFT = *SPLICE = *DELETE = *EXISTS = 
*undone; # XXX Must implement before release

#==============================================================================
package yaml_mapping;
@yaml_mapping::ISA = qw(YAML::Node);

sub new {
    my ($class, $self) = @_;
    @{$self->{KEYS}} = sort keys %{$self->{NODE}}; 
    my $new;
    tie %$new, $class, $self;
    $new
}

sub TIEHASH {
    my ($class, $self) = @_;
    bless $self, $class
}

sub FETCH {
    my ($self, $key) = @_;
    if (exists $self->{NODE}{$key}) {
	return (grep {$_ eq $key} @{$self->{KEYS}}) 
	       ? $self->{NODE}{$key} : undef;
    }
    return $self->{HASH}{$key};
}

sub STORE {
    my ($self, $key, $value) = @_;
    if (exists $self->{NODE}{$key}) {
	$self->{NODE}{$key} = $value;
    }
    elsif (exists $self->{HASH}{$key}) {
	$self->{HASH}{$key} = $value;
    }
    else {
	if (not grep {$_ eq $key} @{$self->{KEYS}}) {
	    push(@{$self->{KEYS}}, $key);
	}
	$self->{HASH}{$key} = $value;
    }
    $value
}

sub DELETE {
    my ($self, $key) = @_;
    my $return;
    if (exists $self->{NODE}{$key}) {
	$return = $self->{NODE}{$key};
    }
    elsif (exists $self->{HASH}{$key}) {
	$return = delete $self->{NODE}{$key};
    }
    for (my $i = 0; $i < @{$self->{KEYS}}; $i++) {
	if ($self->{KEYS}[$i] eq $key) {
	    splice(@{$self->{KEYS}}, $i, 1);
	}
    }
    return $return;
}

sub CLEAR {
    my ($self) = @_;
    @{$self->{KEYS}} = ();
    %{$self->{HASH}} = ();
}

sub FIRSTKEY {
    my ($self) = @_;
    $self->{ITER} = 0;
    $self->{KEYS}[0]
}

sub NEXTKEY {
    my ($self) = @_;
    $self->{KEYS}[++$self->{ITER}]
}

sub EXISTS {
    my ($self, $key) = @_;
    exists $self->{NODE}{$key}
}

1;

__END__

=head1 NAME

YAML::Node - A generic data node that encapsulates YAML information

=head1 SYNOPSIS

    use YAML;
    use YAML::Node;
    
    my $ynode = YAML::Node->new({}, 'ingerson.com/fruit');
    %$ynode = qw(orange orange apple red grape green);
    print Dump $ynode;

yields:

    --- !ingerson.com/fruit
    orange: orange
    apple: red
    grape: green

=head1 DESCRIPTION

A generic node in YAML is similar to a plain hash, array, or scalar node
in Perl except that it must also keep track of its type. The type is a
URI called the YAML type tag.

YAML::Node is a class for generating and manipulating these containers.
A YAML node (or ynode) is a tied hash, array or scalar. In most ways it
behaves just like the plain thing. But you can assign and retrieve and
YAML type tag URI to it. For the hash flavor, you can also assign the
order that the keys will be retrieved in. By default a ynode will offer
its keys in the same order that they were assigned.

YAML::Node has a class method call new() that will return a ynode. You
pass it a regular node and an optional type tag. After that you can
use it like a normal Perl node, but when you YAML::Dump it, the magical
properties will be honored.

This is how you can control the sort order of hash keys during a YAML
serialization. By default, YAML sorts keys alphabetically. But notice
in the above example that the keys were Dumped in the same order they
were assigned.

YAML::Node exports a function called ynode(). This function returns the tied object so that you can call special methods on it like ->keys().

keys() works like this:

    use YAML;
    use YAML::Node;
    
    %$node = qw(orange orange apple red grape green);
    $ynode = YAML::Node->new($node);
    ynode($ynode)->keys(['grape', 'apple']);
    print Dump $ynode;

produces:

    ---
    grape: green
    apple: red

It tells the ynode which keys and what order to use.

ynodes will play a very important role in how programs use YAML. They
are the foundation of how a Perl class can marshall the Loading and
Dumping of its objects.

The upcoming versions of YAML.pm will have much more information on this.

=head1 AUTHOR

Ingy döt Net <ingy@cpan.org>

=head1 COPYRIGHT

Copyright (c) 2006. Ingy döt Net. All rights reserved.

Copyright (c) 2002. Brian Ingerson. All rights reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut
