# $Id: ParserFactory.pm,v 1.13 2002/11/19 18:25:47 matt Exp $

package XML::SAX::ParserFactory;

use strict;
use vars qw($VERSION);

$VERSION = '1.01';

use Symbol qw(gensym);
use XML::SAX;
use XML::SAX::Exception;

sub new {
    my $class = shift;
    my %params = @_; # TODO : Fix this in spec.
    my $self = bless \%params, $class;
    $self->{KnownParsers} = XML::SAX->parsers();
    return $self;
}

sub parser {
    my $self = shift;
    my @parser_params = @_;
    if (!ref($self)) {
        $self = $self->new();
    }
    
    my $parser_class = $self->_parser_class();

    my $version = '';
    if ($parser_class =~ s/\s*\(([\d\.]+)\)\s*$//) {
        $version = " $1";
    }

    {
        no strict 'refs';
        if (!keys %{"${parser_class}::"}) {
            eval "use $parser_class $version;";
        }
    }

    return $parser_class->new(@parser_params);
}

sub require_feature {
    my $self = shift;
    my ($feature) = @_;
    $self->{RequiredFeatures}{$feature}++;
    return $self;
}

sub _parser_class {
    my $self = shift;

    # First try ParserPackage
    if ($XML::SAX::ParserPackage) {
        return $XML::SAX::ParserPackage;
    }

    # Now check if required/preferred is there
    if ($self->{RequiredFeatures}) {
        my %required = %{$self->{RequiredFeatures}};
        # note - we never go onto the next try (ParserDetails.ini),
        # because if we can't provide the requested feature
        # we need to throw an exception.
        PARSER:
        foreach my $parser (reverse @{$self->{KnownParsers}}) {
            foreach my $feature (keys %required) {
                if (!exists $parser->{Features}{$feature}) {
                    next PARSER;
                }
            }
            # got here - all features must exist!
            return $parser->{Name};
        }
        # TODO : should this be NotSupported() ?
        throw XML::SAX::Exception (
                Message => "Unable to provide required features",
            );
    }

    # Next try SAX.ini
    for my $dir (@INC) {
        my $fh = gensym();
        if (open($fh, "$dir/SAX.ini")) {
            my $param_list = XML::SAX->_parse_ini_file($fh);
            my $params = $param_list->[0]->{Features};
            if ($params->{ParserPackage}) {
                return $params->{ParserPackage};
            }
            else {
                # we have required features (or nothing?)
                PARSER:
                foreach my $parser (reverse @{$self->{KnownParsers}}) {
                    foreach my $feature (keys %$params) {
                        if (!exists $parser->{Features}{$feature}) {
                            next PARSER;
                        }
                    }
                    return $parser->{Name};
                }
                XML::SAX->do_warn("Unable to provide SAX.ini required features. Using fallback\n");
            } 
            last; # stop after first INI found
        }
    }

    if (@{$self->{KnownParsers}}) {
        return $self->{KnownParsers}[-1]{Name};
    }
    else {
        return "XML::SAX::PurePerl"; # backup plan!
    }
}

1;
__END__

=head1 NAME

XML::SAX::ParserFactory - Obtain a SAX parser

=head1 SYNOPSIS

  use XML::SAX::ParserFactory;
  use XML::SAX::XYZHandler;
  my $handler = XML::SAX::XYZHandler->new();
  my $p = XML::SAX::ParserFactory->parser(Handler => $handler);
  $p->parse_uri("foo.xml");
  # or $p->parse_string("<foo/>") or $p->parse_file($fh);

=head1 DESCRIPTION

XML::SAX::ParserFactory is a factory class for providing an application
with a Perl SAX2 XML parser. It is akin to DBI - a front end for other
parser classes. Each new SAX2 parser installed will register itself
with XML::SAX, and then it will become available to all applications
that use XML::SAX::ParserFactory to obtain a SAX parser.

Unlike DBI however, XML/SAX parsers almost all work alike (especially
if they subclass XML::SAX::Base, as they should), so rather than
specifying the parser you want in the call to C<parser()>, XML::SAX
has several ways to automatically choose which parser to use:

=over 4

=item * $XML::SAX::ParserPackage

If this package variable is set, then this package is C<require()>d
and an instance of this package is returned by calling the C<new()>
class method in that package. If it cannot be loaded or there is
an error, an exception will be thrown. The variable can also contain
a version number:

  $XML::SAX::ParserPackage = "XML::SAX::Expat (0.72)";

And the number will be treated as a minimum version number.

=item * Required features

It is possible to require features from the parsers. For example, you
may wish for a parser that supports validation via a DTD. To do that,
use the following code:

  use XML::SAX::ParserFactory;
  my $factory = XML::SAX::ParserFactory->new();
  $factory->require_feature('http://xml.org/sax/features/validation');
  my $parser = $factory->parser(...);

Alternatively, specify the required features in the call to the
ParserFactory constructor:

  my $factory = XML::SAX::ParserFactory->new(
          RequiredFeatures => {
               'http://xml.org/sax/features/validation' => 1,
               }
          );

If the features you have asked for are unavailable (for example the
user might not have a validating parser installed), then an
exception will be thrown.

The list of known parsers is searched in reverse order, so it will
always return the last installed parser that supports all of your
requested features (Note: this is subject to change if someone
comes up with a better way of making this work).

=item * SAX.ini

ParserFactory will search @INC for a file called SAX.ini, which
is in a simple format:

  # a comment looks like this,
  ; or like this, and are stripped anywhere in the file
  key = value # SAX.in contains key/value pairs.

All whitespace is non-significant.

This file can contain either a line:

  ParserPackage = MyParserModule (1.02)

Where MyParserModule is the module to load and use for the parser,
and the number in brackets is a minimum version to load.

Or you can list required features:

  http://xml.org/sax/features/validation = 1

And each feature with a true value will be required.

=item * Fallback

If none of the above works, the last parser installed on the user's
system will be used. The XML::SAX package ships with a pure perl
XML parser, XML::SAX::PurePerl, so that there will always be a
fallback parser.

=back

=head1 AUTHOR

Matt Sergeant, matt@sergeant.org

=head1 LICENSE

This is free software, you may use it and distribute it under the same
terms as Perl itself.

=cut

