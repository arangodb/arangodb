package Test::YAML;
use Test::Base 0.47 -Base;
use lib 'lib';

our $VERSION = '0.57';

our $YAML = 'YAML';

our @EXPORT = qw(
    no_diff
    run_yaml_tests
    run_roundtrip_nyn roundtrip_nyn
    run_load_passes load_passes
    dumper Load Dump LoadFile DumpFile
    XXX
);

delimiters('===', '+++');

sub Dump() { YAML(Dump => @_) }
sub Load() { YAML(Load => @_) }
sub DumpFile() { YAML(DumpFile => @_) }
sub LoadFile() { YAML(LoadFile => @_) }

sub YAML() {
    load_yaml_pm();
    my $meth = shift;
    my $code = $YAML->can($meth) or die "$YAML cannot do $meth";
    goto &$code;
}

sub load_yaml_pm {
    my $file = "$YAML.pm";
    $file =~ s{::}{/}g;
    require $file;
}

sub run_yaml_tests() {
    run {
        my $block = shift;
        &{_get_function($block)}($block) unless 
          _skip_tests_for_now($block) or
          _skip_yaml_tests($block);
    };
}

sub run_roundtrip_nyn() {
    my @options = @_;
    run {
        my $block = shift;
        roundtrip_nyn($block, @options);
    };
}

sub roundtrip_nyn() {
    my $block = shift;
    my $option = shift || '';
    die "'perl' data section required"
        unless exists $block->{perl};
    my @values = eval $block->perl;
    die "roundtrip_nyn eval perl error: $@" if $@;
    my $config = $block->config || '';
    my $result = eval "$config; Dump(\@values)";
    die "roundtrip_nyn YAML::Dump error: $@" if $@;
    if (exists $block->{yaml}) {
        is $result, $block->yaml,
            $block->description . ' (n->y)';
    }
    else {
        pass $block->description . ' (n->y)';
    }
        
    return if exists $block->{no_round_trip} or
        not exists $block->{yaml};

    if ($option eq 'dumper') {
        is dumper(Load($block->yaml)), dumper(@values),
            $block->description . ' (y->n)';
    }
    else {
        is_deeply [Load($block->yaml)], [@values],
            $block->description . ' (y->n)';
    }
}

sub count_roundtrip_nyn() {
    my $block = shift or die "Bad call to count_roundtrip_nyn";
    return 1 if exists $block->{skip_this_for_now};
    my $count = 0;
    $count++ if exists $block->{perl};
    $count++ unless exists $block->{no_round_trip} or
        not exists $block->{yaml};
    die "Invalid test definition" unless $count;
    return $count;
}

sub run_load_passes() {
    run {
        my $block = shift;
        my $yaml = $block->yaml;
        eval { YAML(Load => $yaml) };
        is("$@", "");
    };
}

sub load_passes() {
    my $block = shift;
    my $yaml = $block->yaml;
    eval { YAML(Load => $yaml) };
    is "$@", "", $block->description;
}

sub count_load_passes() {1}

sub dumper() {
    require Data::Dumper;
    $Data::Dumper::Sortkeys = 1;
    $Data::Dumper::Terse = 1;
    $Data::Dumper::Indent = 1;
    return Data::Dumper::Dumper(@_);
}

{
    no warnings;
    sub XXX {
        YAML::Base::XXX(@_);
    }
}

sub _count_tests() {
    my $block = shift or die "Bad call to _count_tests";
    no strict 'refs';
    &{'count_' . _get_function_name($block)}($block);
}

sub _get_function_name() {
    my $block = shift;
    return $block->function || 'roundtrip_nyn';
}

sub _get_function() {
    my $block = shift;
    no strict 'refs';
    \ &{_get_function_name($block)};
}

sub _skip_tests_for_now() {
    my $block = shift;
    if (exists $block->{skip_this_for_now}) {
        _skip_test(
            $block->description,
            _count_tests($block),
        );
        return 1;
    }
    return 0;
}

sub _skip_yaml_tests() {
    my $block = shift;
    if ($block->skip_unless_modules) {
        my @modules = split /[\s\,]+/, $block->skip_unless_modules;
        for my $module (@modules) {
            eval "require $module";
            if ($@) {
                _skip_test(
                    "This test requires the '$module' module",
                    _count_tests($block),
                );
                return 1;
            }
        }
    }
    return 0;
}

sub _skip_test() {
    my ($message, $count) = @_;
    SKIP: {
        skip($message, $count);
    }
}

#-------------------------------------------------------------------------------
package Test::YAML::Filter;
use base 'Test::Base::Filter';

sub yaml_dump {
    Test::YAML::Dump(@_);
}

sub yaml_load {
    Test::YAML::Load(@_);
}

sub Dump { goto &Test::YAML::Dump }
sub Load { goto &Test::YAML::Load }
sub DumpFile { goto &Test::YAML::DumpFile }
sub LoadFile { goto &Test::YAML::LoadFile }

sub yaml_load_or_fail {
    my ($result, $error, $warning) =
      $self->_yaml_load_result_error_warning(@_);
    return $error || $result;
}

sub yaml_load_error_or_warning {
    my ($result, $error, $warning) =
      $self->_yaml_load_result_error_warning(@_);
    return $error || $warning || '';
}

sub perl_eval_error_or_warning {
    my ($result, $error, $warning) =
      $self->_perl_eval_result_error_warning(@_);
    return $error || $warning || '';
}

sub _yaml_load_result_error_warning {
    $self->assert_scalar(@_);
    my $yaml = shift;
    my $warning = '';
    local $SIG{__WARN__} = sub { $warning = join '', @_ };
    my $result = eval {
        $self->yaml_load($yaml);
    };
    return ($result, $@, $warning);
}

sub _perl_eval_result_error_warning {
    $self->assert_scalar(@_);
    my $perl = shift;
    my $warning = '';
    local $SIG{__WARN__} = sub { $warning = join '', @_ };
    my $result = eval $perl;
    return ($result, $@, $warning);
}

1;

=head1 NAME

Test::YAML - Testing Module for YAML Implementations

=head1 SYNOPSIS

    use Test::YAML tests => 1;

    pass;

=head1 DESCRIPTION

Test::YAML is a subclass of Test::Base with YAML specific support.

=head1 AUTHOR

Ingy döt Net <ingy@cpan.org>

=head1 COPYRIGHT

Copyright (c) 2006. Ingy döt Net. All rights reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut
