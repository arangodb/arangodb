# $Id: DocumentLocator.pm,v 1.3 2005/10/14 20:31:20 matt Exp $

package XML::SAX::DocumentLocator;
use strict;

sub new {
    my $class = shift;
    my %object;
    tie %object, $class, @_;

    return bless \%object, $class;
}

sub TIEHASH {
    my $class = shift;
    my ($pubmeth, $sysmeth, $linemeth, $colmeth, $encmeth, $xmlvmeth) = @_;
    return bless { 
        pubmeth => $pubmeth,
        sysmeth => $sysmeth,
        linemeth => $linemeth,
        colmeth => $colmeth,
        encmeth => $encmeth,
        xmlvmeth => $xmlvmeth,
    }, $class;
}

sub FETCH {
    my ($self, $key) = @_;
    my $method;
    if ($key eq 'PublicId') {
        $method = $self->{pubmeth};
    }
    elsif ($key eq 'SystemId') {
        $method = $self->{sysmeth};
    }
    elsif ($key eq 'LineNumber') {
        $method = $self->{linemeth};
    }
    elsif ($key eq 'ColumnNumber') {
        $method = $self->{colmeth};
    }
    elsif ($key eq 'Encoding') {
        $method = $self->{encmeth};
    }
    elsif ($key eq 'XMLVersion') {
        $method = $self->{xmlvmeth};
    }
    if ($method) {
        my $value = $method->($key);
        return $value;
    }
    return undef;
}

sub EXISTS {
    my ($self, $key) = @_;
    if ($key =~ /^(PublicId|SystemId|LineNumber|ColumnNumber|Encoding|XMLVersion)$/) {
        return 1;
    }
    return 0;
}

sub STORE {
    my ($self, $key, $value) = @_;
}

sub DELETE {
    my ($self, $key) = @_;
}

sub CLEAR {
    my ($self) = @_;
}

sub FIRSTKEY {
    my ($self) = @_;
    # assignment resets.
    $self->{keys} = {
        PublicId => 1,
        SystemId => 1,
        LineNumber => 1,
        ColumnNumber => 1,
        Encoding => 1,
        XMLVersion => 1,
    };
    return each %{$self->{keys}};
}

sub NEXTKEY {
    my ($self, $lastkey) = @_;
    return each %{$self->{keys}};
}

1;
__END__

=head1 NAME

XML::SAX::DocumentLocator - Helper class for document locators

=head1 SYNOPSIS

  my $locator = XML::SAX::DocumentLocator->new(
      sub { $object->get_public_id },
      sub { $object->get_system_id },
      sub { $reader->current_line },
      sub { $reader->current_column },
      sub { $reader->get_encoding },
      sub { $reader->get_xml_version },
  );

=head1 DESCRIPTION

This module gives you a tied hash reference that calls the
specified closures when asked for PublicId, SystemId,
LineNumber and ColumnNumber.

It is useful for writing SAX Parsers so that you don't have
to constantly update the line numbers in a hash reference on
the object you pass to set_document_locator(). See the source
code for XML::SAX::PurePerl for a usage example.

=head1 API

There is only 1 method: C<new>. Simply pass it a list of
closures that when called will return the PublicId, the
SystemId, the LineNumber, the ColumnNumber, the Encoding 
and the XMLVersion respectively.

The closures are passed a single parameter, the key being
requested. But you're free to ignore that.

=cut

