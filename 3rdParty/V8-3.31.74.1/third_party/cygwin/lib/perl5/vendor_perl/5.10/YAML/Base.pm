package YAML::Base;
use strict; use warnings;
use base 'Exporter';

our @EXPORT = qw(field XXX);

sub new {
    my $class = shift;
    $class = ref($class) || $class;
    my $self = bless {}, $class;
    while (@_) {
        my $method = shift;
        $self->$method(shift);
    }
    return $self;
}

# Use lexical subs to reduce pollution of private methods by base class.
my ($_new_error, $_info, $_scalar_info, $parse_arguments, $default_as_code);

sub XXX {
    require Data::Dumper;
    CORE::die(Data::Dumper::Dumper(@_));
}

my %code = (
    sub_start =>
      "sub {\n",
    set_default =>
      "  \$_[0]->{%s} = %s\n    unless exists \$_[0]->{%s};\n",
    init =>
      "  return \$_[0]->{%s} = do { my \$self = \$_[0]; %s }\n" .
      "    unless \$#_ > 0 or defined \$_[0]->{%s};\n",
    return_if_get =>
      "  return \$_[0]->{%s} unless \$#_ > 0;\n",
    set =>
      "  \$_[0]->{%s} = \$_[1];\n",
    sub_end => 
      "  return \$_[0]->{%s};\n}\n",
);

sub field {
    my $package = caller;
    my ($args, @values) = &$parse_arguments(
        [ qw(-package -init) ],
        @_,
    );
    my ($field, $default) = @values;
    $package = $args->{-package} if defined $args->{-package};
    return if defined &{"${package}::$field"};
    my $default_string =
        ( ref($default) eq 'ARRAY' and not @$default )
        ? '[]'
        : (ref($default) eq 'HASH' and not keys %$default )
          ? '{}'
          : &$default_as_code($default);

    my $code = $code{sub_start};
    if ($args->{-init}) {
        my $fragment = $code{init};
        $code .= sprintf $fragment, $field, $args->{-init}, ($field) x 4;
    }
    $code .= sprintf $code{set_default}, $field, $default_string, $field
      if defined $default;
    $code .= sprintf $code{return_if_get}, $field;
    $code .= sprintf $code{set}, $field;
    $code .= sprintf $code{sub_end}, $field;

    my $sub = eval $code;
    die $@ if $@;
    no strict 'refs';
    *{"${package}::$field"} = $sub;
    return $code if defined wantarray;
}

sub die {
    my $self = shift;
    my $error = $self->$_new_error(@_);
    $error->type('Error');
    Carp::croak($error->format_message);
}

sub warn {
    my $self = shift;
    return unless $^W;
    my $error = $self->$_new_error(@_);
    $error->type('Warning');
    Carp::cluck($error->format_message);
}

# This code needs to be refactored to be simpler and more precise, and no,
# Scalar::Util doesn't DWIM.
#
# Can't handle:
# * blessed regexp
sub node_info {
    my $self = shift;
    my $stringify = $_[1] || 0;
    my ($class, $type, $id) =
        ref($_[0])
        ? $stringify
          ? &$_info("$_[0]")
          : do {
              require overload;
              my @info = &$_info(overload::StrVal($_[0]));
              if (ref($_[0]) eq 'Regexp') {
                  @info[0, 1] = (undef, 'REGEXP');
              }
              @info;
          }
        : &$_scalar_info($_[0]);
    ($class, $type, $id) = &$_scalar_info("$_[0]")
        unless $id;
    return wantarray ? ($class, $type, $id) : $id;
}

#-------------------------------------------------------------------------------
$_info = sub {
    return (($_[0]) =~ qr{^(?:(.*)\=)?([^=]*)\(([^\(]*)\)$}o);
};

$_scalar_info = sub {
    my $id = 'undef';
    if (defined $_[0]) {
        \$_[0] =~ /\((\w+)\)$/o or CORE::die();
        $id = "$1-S";
    }
    return (undef, undef, $id);
};

$_new_error = sub {
    require Carp;
    my $self = shift;
    require YAML::Error;

    my $code = shift || 'unknown error';
    my $error = YAML::Error->new(code => $code);
    $error->line($self->line) if $self->can('line');
    $error->document($self->document) if $self->can('document');
    $error->arguments([@_]);
    return $error;
};
    
$parse_arguments = sub {
    my $paired_arguments = shift || []; 
    my ($args, @values) = ({}, ());
    my %pairs = map { ($_, 1) } @$paired_arguments;
    while (@_) {
        my $elem = shift;
        if (defined $elem and defined $pairs{$elem} and @_) {
            $args->{$elem} = shift;
        }
        else {
            push @values, $elem;
        }
    }
    return wantarray ? ($args, @values) : $args;        
};

$default_as_code = sub {
    no warnings 'once';
    require Data::Dumper;
    local $Data::Dumper::Sortkeys = 1;
    my $code = Data::Dumper::Dumper(shift);
    $code =~ s/^\$VAR1 = //;
    $code =~ s/;$//;
    return $code;
};

1;

__END__

=head1 NAME

YAML::Base - Base class for YAML classes

=head1 SYNOPSIS

    package YAML::Something;
    use YAML::Base -base;

=head1 DESCRIPTION

YAML::Base is the parent of all YAML classes.

=head1 AUTHOR

Ingy döt Net <ingy@cpan.org>

=head1 COPYRIGHT

Copyright (c) 2006. Ingy döt Net. All rights reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut
