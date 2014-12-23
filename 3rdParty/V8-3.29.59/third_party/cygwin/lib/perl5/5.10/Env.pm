package Env;

our $VERSION = '1.00';

=head1 NAME

Env - perl module that imports environment variables as scalars or arrays

=head1 SYNOPSIS

    use Env;
    use Env qw(PATH HOME TERM);
    use Env qw($SHELL @LD_LIBRARY_PATH);

=head1 DESCRIPTION

Perl maintains environment variables in a special hash named C<%ENV>.  For
when this access method is inconvenient, the Perl module C<Env> allows
environment variables to be treated as scalar or array variables.

The C<Env::import()> function ties environment variables with suitable
names to global Perl variables with the same names.  By default it
ties all existing environment variables (C<keys %ENV>) to scalars.  If
the C<import> function receives arguments, it takes them to be a list of
variables to tie; it's okay if they don't yet exist. The scalar type
prefix '$' is inferred for any element of this list not prefixed by '$'
or '@'. Arrays are implemented in terms of C<split> and C<join>, using
C<$Config::Config{path_sep}> as the delimiter.

After an environment variable is tied, merely use it like a normal variable.
You may access its value 

    @path = split(/:/, $PATH);
    print join("\n", @LD_LIBRARY_PATH), "\n";

or modify it

    $PATH .= ":.";
    push @LD_LIBRARY_PATH, $dir;

however you'd like. Bear in mind, however, that each access to a tied array
variable requires splitting the environment variable's string anew.

The code:

    use Env qw(@PATH);
    push @PATH, '.';

is equivalent to:

    use Env qw(PATH);
    $PATH .= ":.";

except that if C<$ENV{PATH}> started out empty, the second approach leaves
it with the (odd) value "C<:.>", but the first approach leaves it with "C<.>".

To remove a tied environment variable from
the environment, assign it the undefined value

    undef $PATH;
    undef @LD_LIBRARY_PATH;

=head1 LIMITATIONS

On VMS systems, arrays tied to environment variables are read-only. Attempting
to change anything will cause a warning.

=head1 AUTHOR

Chip Salzenberg E<lt>F<chip@fin.uucp>E<gt>
and
Gregor N. Purdy E<lt>F<gregor@focusresearch.com>E<gt>

=cut

sub import {
    my ($callpack) = caller(0);
    my $pack = shift;
    my @vars = grep /^[\$\@]?[A-Za-z_]\w*$/, (@_ ? @_ : keys(%ENV));
    return unless @vars;

    @vars = map { m/^[\$\@]/ ? $_ : '$'.$_ } @vars;

    eval "package $callpack; use vars qw(" . join(' ', @vars) . ")";
    die $@ if $@;
    foreach (@vars) {
	my ($type, $name) = m/^([\$\@])(.*)$/;
	if ($type eq '$') {
	    tie ${"${callpack}::$name"}, Env, $name;
	} else {
	    if ($^O eq 'VMS') {
		tie @{"${callpack}::$name"}, Env::Array::VMS, $name;
	    } else {
		tie @{"${callpack}::$name"}, Env::Array, $name;
	    }
	}
    }
}

sub TIESCALAR {
    bless \($_[1]);
}

sub FETCH {
    my ($self) = @_;
    $ENV{$$self};
}

sub STORE {
    my ($self, $value) = @_;
    if (defined($value)) {
	$ENV{$$self} = $value;
    } else {
	delete $ENV{$$self};
    }
}

######################################################################

package Env::Array;
 
use Config;
use Tie::Array;

@ISA = qw(Tie::Array);

my $sep = $Config::Config{path_sep};

sub TIEARRAY {
    bless \($_[1]);
}

sub FETCHSIZE {
    my ($self) = @_;
    my @temp = split($sep, $ENV{$$self});
    return scalar(@temp);
}

sub STORESIZE {
    my ($self, $size) = @_;
    my @temp = split($sep, $ENV{$$self});
    $#temp = $size - 1;
    $ENV{$$self} = join($sep, @temp);
}

sub CLEAR {
    my ($self) = @_;
    $ENV{$$self} = '';
}

sub FETCH {
    my ($self, $index) = @_;
    return (split($sep, $ENV{$$self}))[$index];
}

sub STORE {
    my ($self, $index, $value) = @_;
    my @temp = split($sep, $ENV{$$self});
    $temp[$index] = $value;
    $ENV{$$self} = join($sep, @temp);
    return $value;
}

sub PUSH {
    my $self = shift;
    my @temp = split($sep, $ENV{$$self});
    push @temp, @_;
    $ENV{$$self} = join($sep, @temp);
    return scalar(@temp);
}

sub POP {
    my ($self) = @_;
    my @temp = split($sep, $ENV{$$self});
    my $result = pop @temp;
    $ENV{$$self} = join($sep, @temp);
    return $result;
}

sub UNSHIFT {
    my $self = shift;
    my @temp = split($sep, $ENV{$$self});
    my $result = unshift @temp, @_;
    $ENV{$$self} = join($sep, @temp);
    return $result;
}

sub SHIFT {
    my ($self) = @_;
    my @temp = split($sep, $ENV{$$self});
    my $result = shift @temp;
    $ENV{$$self} = join($sep, @temp);
    return $result;
}

sub SPLICE {
    my $self = shift;
    my $offset = shift;
    my $length = shift;
    my @temp = split($sep, $ENV{$$self});
    if (wantarray) {
	my @result = splice @temp, $self, $offset, $length, @_;
	$ENV{$$self} = join($sep, @temp);
	return @result;
    } else {
	my $result = scalar splice @temp, $offset, $length, @_;
	$ENV{$$self} = join($sep, @temp);
	return $result;
    }
}

######################################################################

package Env::Array::VMS;
use Tie::Array;

@ISA = qw(Tie::Array);
 
sub TIEARRAY {
    bless \($_[1]);
}

sub FETCHSIZE {
    my ($self) = @_;
    my $i = 0;
    while ($i < 127 and defined $ENV{$$self . ';' . $i}) { $i++; };
    return $i;
}

sub FETCH {
    my ($self, $index) = @_;
    return $ENV{$$self . ';' . $index};
}

1;
