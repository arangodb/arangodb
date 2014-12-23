package XML::LibXML::Reader;
use XML::LibXML;

use strict;
use warnings;

use vars qw ($VERSION);
$VERSION = "1.66"; # VERSION TEMPLATE: DO NOT CHANGE

use Carp;
use 5.008_000;
use base qw(Exporter);
use constant {
    XML_READER_TYPE_NONE => 0,
    XML_READER_TYPE_ELEMENT => 1,
    XML_READER_TYPE_ATTRIBUTE => 2,
    XML_READER_TYPE_TEXT => 3,
    XML_READER_TYPE_CDATA => 4,
    XML_READER_TYPE_ENTITY_REFERENCE => 5,
    XML_READER_TYPE_ENTITY => 6,
    XML_READER_TYPE_PROCESSING_INSTRUCTION => 7,
    XML_READER_TYPE_COMMENT => 8,
    XML_READER_TYPE_DOCUMENT => 9,
    XML_READER_TYPE_DOCUMENT_TYPE => 10,
    XML_READER_TYPE_DOCUMENT_FRAGMENT => 11,
    XML_READER_TYPE_NOTATION => 12,
    XML_READER_TYPE_WHITESPACE => 13,
    XML_READER_TYPE_SIGNIFICANT_WHITESPACE => 14,
    XML_READER_TYPE_END_ELEMENT => 15,
    XML_READER_TYPE_END_ENTITY => 16,
    XML_READER_TYPE_XML_DECLARATION => 17,

    XML_READER_NONE      => -1,
    XML_READER_START     =>  0,
    XML_READER_ELEMENT   =>  1,
    XML_READER_END       =>  2,
    XML_READER_EMPTY     =>  3,
    XML_READER_BACKTRACK =>  4,
    XML_READER_DONE      =>  5,
    XML_READER_ERROR     =>  6
};
use vars qw( @EXPORT @EXPORT_OK %EXPORT_TAGS );

BEGIN {

%EXPORT_TAGS = (
  types =>
  [qw(
    XML_READER_TYPE_NONE
    XML_READER_TYPE_ELEMENT
    XML_READER_TYPE_ATTRIBUTE
    XML_READER_TYPE_TEXT
    XML_READER_TYPE_CDATA
    XML_READER_TYPE_ENTITY_REFERENCE
    XML_READER_TYPE_ENTITY
    XML_READER_TYPE_PROCESSING_INSTRUCTION
    XML_READER_TYPE_COMMENT
    XML_READER_TYPE_DOCUMENT
    XML_READER_TYPE_DOCUMENT_TYPE
    XML_READER_TYPE_DOCUMENT_FRAGMENT
    XML_READER_TYPE_NOTATION
    XML_READER_TYPE_WHITESPACE
    XML_READER_TYPE_SIGNIFICANT_WHITESPACE
    XML_READER_TYPE_END_ELEMENT
    XML_READER_TYPE_END_ENTITY
    XML_READER_TYPE_XML_DECLARATION
    )],
  states =>
  [qw(
    XML_READER_NONE
    XML_READER_START
    XML_READER_ELEMENT
    XML_READER_END
    XML_READER_EMPTY
    XML_READER_BACKTRACK
    XML_READER_DONE
    XML_READER_ERROR
   )]
);
@EXPORT    = (@{$EXPORT_TAGS{types}},@{$EXPORT_TAGS{states}});
@EXPORT_OK = @EXPORT;
$EXPORT_TAGS{all}=\@EXPORT_OK;
}

{
  my %flags = (
    recover => 1,		 # recover on errors
    expand_entities => 2,	 # substitute entities
    load_ext_dtd => 4,		 # load the external subset
    complete_attributes => 8,	 # default DTD attributes
    validation => 16,		 # validate with the DTD
    suppress_errors => 32,	 # suppress error reports
    suppress_warnings => 64,	 # suppress warning reports
    pedantic_parser => 128,	 # pedantic error reporting
    no_blanks => 256,		 # remove blank nodes
    expand_xinclude => 1024,	 # Implement XInclude substitition
    xinclude => 1024,		 # ... alias
    no_network => 2048,		 # Forbid network access
    clean_namespaces => 8192,    # remove redundant namespaces declarations
    no_cdata => 16384,		 # merge CDATA as text nodes
    no_xinclude_nodes => 32768,	 # do not generate XINCLUDE START/END nodes
  );
  sub _parser_options {
    my ($opts) = @_;

    # currently dictionaries break XML::LibXML memory management
    my $no_dict = 4096;
    my $flags = $no_dict;     # safety precaution

    my ($key, $value);
    while (($key,$value) = each %$opts) {
      my $f = $flags{ $key };
      if (defined $f) {
	if ($value) {
	  $flags |= $f
	} else {
	  $flags &= ~$f;
	}
      }
    }
    return $flags;
  }
  my %props = (
    load_ext_dtd => 1,		 # load the external subset
    complete_attributes => 2,	 # default DTD attributes
    validation => 3,		 # validate with the DTD
    expand_entities => 4,	 # substitute entities
  );
  sub getParserProp {
    my ($self, $name) = @_;
    my $prop = $props{$name};
    return undef unless defined $prop;
    return $self->_getParserProp($prop);
  }
  sub setParserProp {
    my $self = shift;
    my %args = map { ref($_) eq 'HASH' ? (%$_) : $_ } @_;
    my ($key, $value);
    while (($key,$value) = each %args) {
      my $prop = $props{ $key };
      $self->_setParserProp($prop,$value);
    }
    return;
  }

  my (%string_pool,%rng_pool,%xsd_pool); # used to preserve data passed to the reader
  sub new {
    my ($class) = shift;
    my %args = map { ref($_) eq 'HASH' ? (%$_) : $_ } @_;
    my $encoding = $args{encoding};
    my $URI = $args{URI};
    my $options = _parser_options(\%args);

    my $self = undef;
    if ( defined $args{location} ) {
      $self = $class->_newForFile( $args{location}, $encoding, $options );
    }
    elsif ( defined $args{string} ) {
      $self = $class->_newForString( $args{string}, $URI, $encoding, $options );
      $string_pool{$self} = \$args{string};
    }
    elsif ( defined $args{IO} ) {
      $self = $class->_newForIO( $args{IO}, $URI, $encoding, $options  );
    }
    elsif ( defined $args{DOM} ) {
      croak("DOM must be a XML::LibXML::Document node")
	unless UNIVERSAL::isa($args{DOM}, 'XML::LibXML::Document');
      $self = $class->_newForDOM( $args{DOM} );
    }
    elsif ( defined $args{FD} ) {
      my $fd = fileno($args{FD});
      $self = $class->_newForFd( $fd, $URI, $encoding, $options  );
    }
    else {
      croak("XML::LibXML::Reader->new: specify location, string, IO, DOM, or FD");
    }
    if ($args{RelaxNG}) {
      if (ref($args{RelaxNG})) {
	$rng_pool{$self} = \$args{RelaxNG};
	$self->_setRelaxNG($args{RelaxNG});
      } else {
	$self->_setRelaxNGFile($args{RelaxNG});
      }
    }
    if ($args{Schema}) {
      if (ref($args{Schema})) {
	$xsd_pool{$self} = \$args{Schema};
	$self->_setXSD($args{Schema});
      } else {
	$self->_setXSDFile($args{Schema});
      }
    }
    return $self;
  }
  sub DESTROY {
    my $self = shift;
    delete $string_pool{$self};
    delete $rng_pool{$self};
    delete $xsd_pool{$self};
    $self->_DESTROY;
  }
}
sub close {
    my ($reader) = @_;
    # _close return -1 on failure, 0 on success
    # perl close returns 0 on failure, 1 on success
    return $reader->_close == 0 ? 1 : 0;
}

sub preservePattern {
  my $reader=shift;
  my ($pattern,$ns_map)=@_;
  if (ref($ns_map) eq 'HASH') {
    # translate prefix=>URL hash to a (URL,prefix) list
    $reader->_preservePattern($pattern,[reverse %$ns_map]);
  } else {
    $reader->_preservePattern(@_);
  }
}

1;
__END__
