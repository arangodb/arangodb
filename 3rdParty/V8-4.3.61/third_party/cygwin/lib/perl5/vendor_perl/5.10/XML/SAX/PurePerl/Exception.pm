# $Id: Exception.pm,v 1.2 2001/11/14 11:07:25 matt Exp $

package XML::SAX::PurePerl::Exception;

use strict;

use overload '""' => "stringify";

use vars qw/$StackTrace/;

$StackTrace = $ENV{XML_DEBUG} || 0;

sub throw {
    my $class = shift;
    die $class->new(@_);
}

sub new {
    my $class = shift;
    my %opts = @_;
    die "Invalid options" unless exists $opts{Message};
    
    if ($opts{reader}) {
        return bless { Message => $opts{Message},
                        Exception => undef, # not sure what this is for!!!
                        ColumnNumber => $opts{reader}->column,
                        LineNumber => $opts{reader}->line,
                        PublicId => $opts{reader}->public_id,
                        SystemId => $opts{reader}->system_id,
                        $StackTrace ? (StackTrace => stacktrace()) : (),
                    }, $class;
    }
    return bless { Message => $opts{Message},
                    Exception => undef, # not sure what this is for!!!
                  }, $class;
}

sub stringify {
    my $self = shift;
    local $^W;
    return $self->{Message} . " [Ln: " . $self->{LineNumber} . 
                ", Col: " . $self->{ColumnNumber} . "]" .
                ($StackTrace ? stackstring($self->{StackTrace}) : "") . "\n";
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

