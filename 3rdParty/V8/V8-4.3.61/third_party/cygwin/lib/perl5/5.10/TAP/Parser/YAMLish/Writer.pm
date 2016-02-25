package TAP::Parser::YAMLish::Writer;

use strict;

use vars qw{$VERSION};

$VERSION = '3.10';

my $ESCAPE_CHAR = qr{ [ \x00-\x1f \" ] }x;
my $ESCAPE_KEY  = qr{ (?: ^\W ) | $ESCAPE_CHAR }x;

my @UNPRINTABLE = qw(
  z    x01  x02  x03  x04  x05  x06  a
  x08  t    n    v    f    r    x0e  x0f
  x10  x11  x12  x13  x14  x15  x16  x17
  x18  x19  x1a  e    x1c  x1d  x1e  x1f
);

# Create an empty TAP::Parser::YAMLish::Writer object
sub new {
    my $class = shift;
    bless {}, $class;
}

sub write {
    my $self = shift;

    die "Need something to write"
      unless @_;

    my $obj = shift;
    my $out = shift || \*STDOUT;

    die "Need a reference to something I can write to"
      unless ref $out;

    $self->{writer} = $self->_make_writer($out);

    $self->_write_obj( '---', $obj );
    $self->_put('...');

    delete $self->{writer};
}

sub _make_writer {
    my $self = shift;
    my $out  = shift;

    my $ref = ref $out;

    if ( 'CODE' eq $ref ) {
        return $out;
    }
    elsif ( 'ARRAY' eq $ref ) {
        return sub { push @$out, shift };
    }
    elsif ( 'SCALAR' eq $ref ) {
        return sub { $$out .= shift() . "\n" };
    }
    elsif ( 'GLOB' eq $ref || 'IO::Handle' eq $ref ) {
        return sub { print $out shift(), "\n" };
    }

    die "Can't write to $out";
}

sub _put {
    my $self = shift;
    $self->{writer}->( join '', @_ );
}

sub _enc_scalar {
    my $self = shift;
    my $val  = shift;
    my $rule = shift;

    return '~' unless defined $val;

    if ( $val =~ /$rule/ ) {
        $val =~ s/\\/\\\\/g;
        $val =~ s/"/\\"/g;
        $val =~ s/ ( [\x00-\x1f] ) / '\\' . $UNPRINTABLE[ ord($1) ] /gex;
        return qq{"$val"};
    }

    if ( length($val) == 0 or $val =~ /\s/ ) {
        $val =~ s/'/''/;
        return "'$val'";
    }

    return $val;
}

sub _write_obj {
    my $self   = shift;
    my $prefix = shift;
    my $obj    = shift;
    my $indent = shift || 0;

    if ( my $ref = ref $obj ) {
        my $pad = '  ' x $indent;
        if ( 'HASH' eq $ref ) {
            if ( keys %$obj ) {
                $self->_put($prefix);
                for my $key ( sort keys %$obj ) {
                    my $value = $obj->{$key};
                    $self->_write_obj(
                        $pad . $self->_enc_scalar( $key, $ESCAPE_KEY ) . ':',
                        $value, $indent + 1
                    );
                }
            }
            else {
                $self->_put( $prefix, ' {}' );
            }
        }
        elsif ( 'ARRAY' eq $ref ) {
            if (@$obj) {
                $self->_put($prefix);
                for my $value (@$obj) {
                    $self->_write_obj(
                        $pad . '-', $value,
                        $indent + 1
                    );
                }
            }
            else {
                $self->_put( $prefix, ' []' );
            }
        }
        else {
            die "Don't know how to enocde $ref";
        }
    }
    else {
        $self->_put( $prefix, ' ', $self->_enc_scalar( $obj, $ESCAPE_CHAR ) );
    }
}

1;

__END__

=pod

=head1 NAME

TAP::Parser::YAMLish::Writer - Write YAMLish data

=head1 VERSION

Version 3.10

=head1 SYNOPSIS

    use TAP::Parser::YAMLish::Writer;
    
    my $data = {
        one => 1,
        two => 2,
        three => [ 1, 2, 3 ],
    };
    
    my $yw = TAP::Parser::YAMLish::Writer->new;
    
    # Write to an array...
    $yw->write( $data, \@some_array );
    
    # ...an open file handle...
    $yw->write( $data, $some_file_handle );
    
    # ...a string ...
    $yw->write( $data, \$some_string );
    
    # ...or a closure
    $yw->write( $data, sub {
        my $line = shift;
        print "$line\n";
    } );

=head1 DESCRIPTION

Encodes a scalar, hash reference or array reference as YAMLish.

=head1 METHODS

=head2 Class Methods

=head3 C<new>

 my $writer = TAP::Parser::YAMLish::Writer->new;

The constructor C<new> creates and returns an empty
C<TAP::Parser::YAMLish::Writer> object.

=head2 Instance Methods

=head3 C<write>

 $writer->write($obj, $output );

Encode a scalar, hash reference or array reference as YAML.

    my $writer = sub {
        my $line = shift;
        print SOMEFILE "$line\n";
    };
    
    my $data = {
        one => 1,
        two => 2,
        three => [ 1, 2, 3 ],
    };
    
    my $yw = TAP::Parser::YAMLish::Writer->new;
    $yw->write( $data, $writer );


The C< $output > argument may be:

=over

=item * a reference to a scalar to append YAML to

=item * the handle of an open file

=item * a reference to an array into which YAML will be pushed

=item * a code reference

=back

If you supply a code reference the subroutine will be called once for
each line of output with the line as its only argument. Passed lines
will have no trailing newline.

=head1 AUTHOR

Andy Armstrong, <andy@hexten.net>

=head1 SEE ALSO

L<YAML::Tiny>, L<YAML>, L<YAML::Syck>, L<Config::Tiny>, L<CSS::Tiny>,
L<http://use.perl.org/~Alias/journal/29427>

=head1 COPYRIGHT

Copyright 2007-2008 Andy Armstrong.

This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

The full text of the license can be found in the
LICENSE file included with this module.

=cut

