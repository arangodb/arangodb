# $Id: SAX.pm,v 1.29 2007/06/27 09:09:12 grant Exp $

package XML::SAX;

use strict;
use vars qw($VERSION @ISA @EXPORT_OK);

$VERSION = '0.16';

use Exporter ();
@ISA = ('Exporter');

@EXPORT_OK = qw(Namespaces Validation);

use File::Basename qw(dirname);
use File::Spec ();
use Symbol qw(gensym);
use XML::SAX::ParserFactory (); # loaded for simplicity

use constant PARSER_DETAILS => "ParserDetails.ini";

use constant Namespaces => "http://xml.org/sax/features/namespaces";
use constant Validation => "http://xml.org/sax/features/validation";

my $known_parsers = undef;

# load_parsers takes the ParserDetails.ini file out of the same directory
# that XML::SAX is in, and looks at it. Format in POD below

=begin EXAMPLE

[XML::SAX::PurePerl]
http://xml.org/sax/features/namespaces = 1
http://xml.org/sax/features/validation = 0
# a comment

# blank lines ignored

[XML::SAX::AnotherParser]
http://xml.org/sax/features/namespaces = 0
http://xml.org/sax/features/validation = 1

=end EXAMPLE

=cut

sub load_parsers {
    my $class = shift;
    my $dir = shift;
    
    # reset parsers
    $known_parsers = [];
    
    # get directory from wherever XML::SAX is installed
    if (!$dir) {
        $dir = $INC{'XML/SAX.pm'};
        $dir = dirname($dir);
    }
    
    my $fh = gensym();
    if (!open($fh, File::Spec->catfile($dir, "SAX", PARSER_DETAILS))) {
        XML::SAX->do_warn("could not find " . PARSER_DETAILS . " in $dir/SAX\n");
        return $class;
    }

    $known_parsers = $class->_parse_ini_file($fh);

    return $class;
}

sub _parse_ini_file {
    my $class = shift;
    my ($fh) = @_;

    my @config;
    
    my $lineno = 0;
    while (defined(my $line = <$fh>)) {
        $lineno++;
        my $original = $line;
        # strip whitespace
        $line =~ s/\s*$//m;
        $line =~ s/^\s*//m;
        # strip comments
        $line =~ s/[#;].*$//m;
        # ignore blanks
        next if $line =~ /^$/m;
        
        # heading
        if ($line =~ /^\[\s*(.*)\s*\]$/m) {
            push @config, { Name => $1 };
            next;
        }
        
        # instruction
        elsif ($line =~ /^(.*?)\s*?=\s*(.*)$/) {
            unless(@config) {
                push @config, { Name => '' };
            }
            $config[-1]{Features}{$1} = $2;
        }

        # not whitespace, comment, or instruction
        else {
            die "Invalid line in ini: $lineno\n>>> $original\n";
        }
    }

    return \@config;
}

sub parsers {
    my $class = shift;
    if (!$known_parsers) {
        $class->load_parsers();
    }
    return $known_parsers;
}

sub remove_parser {
    my $class = shift;
    my ($parser_module) = @_;

    if (!$known_parsers) {
        $class->load_parsers();
    }
    
    @$known_parsers = grep { $_->{Name} ne $parser_module } @$known_parsers;

    return $class;
}
 
sub add_parser {
    my $class = shift;
    my ($parser_module) = @_;

    if (!$known_parsers) {
        $class->load_parsers();
    }
    
    # first load module, then query features, then push onto known_parsers,
    
    my $parser_file = $parser_module;
    $parser_file =~ s/::/\//g;
    $parser_file .= ".pm";

    require $parser_file;

    my @features = $parser_module->supported_features();
    
    my $new = { Name => $parser_module };
    foreach my $feature (@features) {
        $new->{Features}{$feature} = 1;
    }

    # If exists in list already, move to end.
    my $done = 0;
    my $pos = undef;
    for (my $i = 0; $i < @$known_parsers; $i++) {
        my $p = $known_parsers->[$i];
        if ($p->{Name} eq $parser_module) {
            $pos = $i;
        }
    }
    if (defined $pos) {
        splice(@$known_parsers, $pos, 1);
        push @$known_parsers, $new;
        $done++;
    }

    # Otherwise (not in list), add at end of list.
    if (!$done) {
        push @$known_parsers, $new;
    }
    
    return $class;
}

sub save_parsers {
    my $class = shift;
    
    # get directory from wherever XML::SAX is installed
    my $dir = $INC{'XML/SAX.pm'};
    $dir = dirname($dir);
    
    my $file = File::Spec->catfile($dir, "SAX", PARSER_DETAILS);
    chmod 0644, $file;
    unlink($file);
    
    my $fh = gensym();
    open($fh, ">$file") ||
        die "Cannot write to $file: $!";

    foreach my $p (@$known_parsers) {
        print $fh "[$p->{Name}]\n";
        foreach my $key (keys %{$p->{Features}}) {
            print $fh "$key = $p->{Features}{$key}\n";
        }
        print $fh "\n";
    }

    print $fh "\n";

    close $fh;

    return $class;
}

sub do_warn {
    my $class = shift;
    # Don't output warnings if running under Test::Harness
    warn(@_) unless $ENV{HARNESS_ACTIVE};
}

1;
__END__

=head1 NAME

XML::SAX - Simple API for XML

=head1 SYNOPSIS

  use XML::SAX;
  
  # get a list of known parsers
  my $parsers = XML::SAX->parsers();
  
  # add/update a parser
  XML::SAX->add_parser(q(XML::SAX::PurePerl));

  # remove parser
  XML::SAX->remove_parser(q(XML::SAX::Foodelberry));

  # save parsers
  XML::SAX->save_parsers();

=head1 DESCRIPTION

XML::SAX is a SAX parser access API for Perl. It includes classes
and APIs required for implementing SAX drivers, along with a factory
class for returning any SAX parser installed on the user's system.

=head1 USING A SAX2 PARSER

The factory class is XML::SAX::ParserFactory. Please see the
documentation of that module for how to instantiate a SAX parser:
L<XML::SAX::ParserFactory>. However if you don't want to load up
another manual page, here's a short synopsis:

  use XML::SAX::ParserFactory;
  use XML::SAX::XYZHandler;
  my $handler = XML::SAX::XYZHandler->new();
  my $p = XML::SAX::ParserFactory->parser(Handler => $handler);
  $p->parse_uri("foo.xml");
  # or $p->parse_string("<foo/>") or $p->parse_file($fh);

This will automatically load a SAX2 parser (defaulting to
XML::SAX::PurePerl if no others are found) and return it to you.

In order to learn how to use SAX to parse XML, you will need to read
L<XML::SAX::Intro> and for reference, L<XML::SAX::Specification>.

=head1 WRITING A SAX2 PARSER

The first thing to remember in writing a SAX2 parser is to subclass
XML::SAX::Base. This will make your life infinitely easier, by providing
a number of methods automagically for you. See L<XML::SAX::Base> for more
details.

When writing a SAX2 parser that is compatible with XML::SAX, you need
to inform XML::SAX of the presence of that driver when you install it.
In order to do that, XML::SAX contains methods for saving the fact that
the parser exists on your system to a "INI" file, which is then loaded
to determine which parsers are installed.

The best way to do this is to follow these rules:

=over 4

=item * Add XML::SAX as a prerequisite in Makefile.PL:

  WriteMakefile(
      ...
      PREREQ_PM => { 'XML::SAX' => 0 },
      ...
  );

Alternatively you may wish to check for it in other ways that will
cause more than just a warning.

=item * Add the following code snippet to your Makefile.PL:

  sub MY::install {
    package MY;
    my $script = shift->SUPER::install(@_);
    if (ExtUtils::MakeMaker::prompt(
      "Do you want to modify ParserDetails.ini?", 'Y')
      =~ /^y/i) {
      $script =~ s/install :: (.*)$/install :: $1 install_sax_driver/m;
      $script .= <<"INSTALL";
  
  install_sax_driver :
  \t\@\$(PERL) -MXML::SAX -e "XML::SAX->add_parser(q(\$(NAME)))->save_parsers()"
  
  INSTALL
    }
    return $script;
  }

Note that you should check the output of this - \$(NAME) will use the name of
your distribution, which may not be exactly what you want. For example XML::LibXML
has a driver called XML::LibXML::SAX::Generator, which is used in place of
\$(NAME) in the above.

=item * Add an XML::SAX test:

A test file should be added to your t/ directory containing something like the
following:

  use Test;
  BEGIN { plan tests => 3 }
  use XML::SAX;
  use XML::SAX::PurePerl::DebugHandler;
  XML::SAX->add_parser(q(XML::SAX::MyDriver));
  local $XML::SAX::ParserPackage = 'XML::SAX::MyDriver';
  eval {
    my $handler = XML::SAX::PurePerl::DebugHandler->new();
    ok($handler);
    my $parser = XML::SAX::ParserFactory->parser(Handler => $handler);
    ok($parser);
    ok($parser->isa('XML::SAX::MyDriver');
    $parser->parse_string("<tag/>");
    ok($handler->{seen}{start_element});
  };

=back

=head1 EXPORTS

By default, XML::SAX exports nothing into the caller's namespace. However you
can request the symbols C<Namespaces> and C<Validation> which are the
URIs for those features, allowing an easier way to request those features
via ParserFactory:

  use XML::SAX qw(Namespaces Validation);
  my $factory = XML::SAX::ParserFactory->new();
  $factory->require_feature(Namespaces);
  $factory->require_feature(Validation);
  my $parser = $factory->parser();

=head1 AUTHOR

Current maintainer: Grant McLean, grantm@cpan.org

Originally written by:

Matt Sergeant, matt@sergeant.org

Kip Hampton, khampton@totalcinema.com

Robin Berjon, robin@knowscape.com

=head1 LICENSE

This is free software, you may use it and distribute it under
the same terms as Perl itself.

=head1 SEE ALSO

L<XML::SAX::Base> for writing SAX Filters and Parsers

L<XML::SAX::PurePerl> for an XML parser written in 100%
pure perl.

L<XML::SAX::Exception> for details on exception handling

=cut

