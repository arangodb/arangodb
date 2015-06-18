package XML::SAX::Exception;

use strict;

use overload '""' => "stringify",
    'fallback' => 1;

use vars qw/$StackTrace $VERSION/;
$VERSION = '1.01';
use Carp;

$StackTrace = $ENV{XML_DEBUG} || 0;

# Other exception classes:

@XML::SAX::Exception::NotRecognized::ISA = ('XML::SAX::Exception');
@XML::SAX::Exception::NotSupported::ISA = ('XML::SAX::Exception');
@XML::SAX::Exception::Parse::ISA = ('XML::SAX::Exception');


sub throw {
    my $class = shift;
    if (ref($class)) {
        die $class;
    }
    die $class->new(@_);
}

sub new {
    my $class = shift;
    my %opts = @_;
    confess "Invalid options: " . join(', ', keys %opts) unless exists $opts{Message};
    
    bless { ($StackTrace ? (StackTrace => stacktrace()) : ()), %opts },
        $class;
}

sub stringify {
    my $self = shift;
    local $^W;
    my $error;
    if (exists $self->{LineNumber}) {
        $error = $self->{Message} . " [Ln: " . $self->{LineNumber} . 
                ", Col: " . $self->{ColumnNumber} . "]";
    }
    else {
        $error = $self->{Message};
    }
    if ($StackTrace) {
        $error .= stackstring($self->{StackTrace});
    }
    $error .= "\n";
    return $error;
}

sub stacktrace {
    my $i = 2;
    my @fulltrace;
    while (my @trace = caller($i++)) {
        my %hash;
        @hash{qw(Package Filename Line)} = @trace[0..2];
        push @fulltrace, \%hash;
    }
    return \@fulltrace;
}

sub stackstring {
    my $stacktrace = shift;
    my $string = "\nFrom:\n";
    foreach my $current (@$stacktrace) {
        $string .= $current->{Filename} . " Line: " . $current->{Line} . "\n";
    }
    return $string;
}

1;

__END__

=head1 NAME

XML::SAX::Exception - Exception classes for XML::SAX

=head1 SYNOPSIS

  throw XML::SAX::Exception::NotSupported(
          Message => "The foo feature is not supported",
          );

=head1 DESCRIPTION

This module is the base class for all SAX Exceptions, those defined in
the spec as well as those that one may create for one's own SAX errors.

There are three subclasses included, corresponding to those of the SAX
spec:

  XML::SAX::Exception::NotSupported
  XML::SAX::Exception::NotRecognized
  XML::SAX::Exception::Parse

Use them wherever you want, and as much as possible when you encounter
such errors. SAX is meant to use exceptions as much as possible to 
flag problems.

=head1 CREATING NEW EXCEPTION CLASSES

All you need to do to create a new exception class is:

  @XML::SAX::Exception::MyException::ISA = ('XML::SAX::Exception')

The given package doesn't need to exist, it'll behave correctly this 
way. If your exception refines an existing exception class, then you
may also inherit from that instead of from the base class.

=head1 THROWING EXCEPTIONS

This is as simple as exemplified in the SYNOPSIS. In fact, there's 
nothing more to know. All you have to do is:

  throw XML::SAX::Exception::MyException( Message => 'Something went wrong' );

and voila, you've thrown an exception which can be caught in an eval block.

=cut

