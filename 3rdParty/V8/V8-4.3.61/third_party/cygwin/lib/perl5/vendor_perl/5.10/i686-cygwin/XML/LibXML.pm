# $Id: LibXML.pm 709 2008-01-29 21:01:32Z pajas $

package XML::LibXML;

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS
            $skipDTD $skipXMLDeclaration $setTagCompression
            $MatchCB $ReadCB $OpenCB $CloseCB 
            );
use Carp;

use XML::LibXML::Common qw(:encoding :libxml);

use constant XML_XMLNS_NS => 'http://www.w3.org/2000/xmlns/';
use constant XML_XML_NS => 'http://www.w3.org/XML/1998/namespace';

use XML::LibXML::NodeList;
use XML::LibXML::XPathContext;
use IO::Handle; # for FH reads called as methods

BEGIN {

$VERSION = "1.66"; # VERSION TEMPLATE: DO NOT CHANGE
require Exporter;
require DynaLoader;
@ISA = qw(DynaLoader Exporter);

#-------------------------------------------------------------------------#
# export information                                                      #
#-------------------------------------------------------------------------#
%EXPORT_TAGS = (
                all => [qw(
                           XML_ELEMENT_NODE
                           XML_ATTRIBUTE_NODE
                           XML_TEXT_NODE
                           XML_CDATA_SECTION_NODE
                           XML_ENTITY_REF_NODE
                           XML_ENTITY_NODE
                           XML_PI_NODE
                           XML_COMMENT_NODE
                           XML_DOCUMENT_NODE
                           XML_DOCUMENT_TYPE_NODE
                           XML_DOCUMENT_FRAG_NODE
                           XML_NOTATION_NODE
                           XML_HTML_DOCUMENT_NODE
                           XML_DTD_NODE
                           XML_ELEMENT_DECL
                           XML_ATTRIBUTE_DECL
                           XML_ENTITY_DECL
                           XML_NAMESPACE_DECL
                           XML_XINCLUDE_END
                           XML_XINCLUDE_START
                           encodeToUTF8
                           decodeFromUTF8
		           XML_XMLNS_NS
		           XML_XML_NS
                          )],
                libxml => [qw(
                           XML_ELEMENT_NODE
                           XML_ATTRIBUTE_NODE
                           XML_TEXT_NODE
                           XML_CDATA_SECTION_NODE
                           XML_ENTITY_REF_NODE
                           XML_ENTITY_NODE
                           XML_PI_NODE
                           XML_COMMENT_NODE
                           XML_DOCUMENT_NODE
                           XML_DOCUMENT_TYPE_NODE
                           XML_DOCUMENT_FRAG_NODE
                           XML_NOTATION_NODE
                           XML_HTML_DOCUMENT_NODE
                           XML_DTD_NODE
                           XML_ELEMENT_DECL
                           XML_ATTRIBUTE_DECL
                           XML_ENTITY_DECL
                           XML_NAMESPACE_DECL
                           XML_XINCLUDE_END
                           XML_XINCLUDE_START
                          )],
                encoding => [qw(
                                encodeToUTF8
                                decodeFromUTF8
                               )],
		ns => [qw(
		           XML_XMLNS_NS
		           XML_XML_NS		    
		 )],
               );

@EXPORT_OK = (
              @{$EXPORT_TAGS{all}},
             );

@EXPORT = (
           @{$EXPORT_TAGS{all}},
          );

#-------------------------------------------------------------------------#
# initialization of the global variables                                  #
#-------------------------------------------------------------------------#
$skipDTD            = 0;
$skipXMLDeclaration = 0;
$setTagCompression  = 0;

$MatchCB = undef;
$ReadCB  = undef;
$OpenCB  = undef;
$CloseCB = undef;

#-------------------------------------------------------------------------#
# bootstrapping                                                           #
#-------------------------------------------------------------------------#
bootstrap XML::LibXML $VERSION;
undef &AUTOLOAD;

} # BEGIN

#-------------------------------------------------------------------------#
# test exact version (up to patch-level)                                  #
#-------------------------------------------------------------------------#
{
  my ($runtime_version) = LIBXML_RUNTIME_VERSION() =~ /^(\d+)/;
  if ( $runtime_version < LIBXML_VERSION ) {
    warn "Warning: XML::LibXML compiled against libxml2 ".LIBXML_VERSION.
      ", but runtime libxml2 is older $runtime_version\n";
  }
}

#-------------------------------------------------------------------------#
# parser constructor                                                      #
#-------------------------------------------------------------------------#
sub new {
    my $class = shift;
    my %options = @_;
    if ( not exists $options{XML_LIBXML_KEEP_BLANKS} ) {
        $options{XML_LIBXML_KEEP_BLANKS} = 1;
    }

    if ( defined $options{catalog} ) {
        $class->load_catalog( $options{catalog} );
        delete $options{catalog};
    }

    my $self = bless \%options, $class;
    if ( defined $options{Handler} ) {
        $self->set_handler( $options{Handler} );
    }

    $self->{XML_LIBXML_EXT_DTD} = 1;
    $self->{_State_} = 0;
    return $self;
}

#-------------------------------------------------------------------------#
# Threads support methods                                                 #
#-------------------------------------------------------------------------#

# threads doc says CLONE's API may change in future, which would break
# an XS method prototype
sub CLONE { XML::LibXML::_CLONE( $_[0] ) }

#-------------------------------------------------------------------------#
# DOM Level 2 document constructor                                        #
#-------------------------------------------------------------------------#

sub createDocument {
   my $self = shift;
   if (!@_ or $_[0] =~ m/^\d\.\d$/) {
     # for backward compatibility
     return XML::LibXML::Document->new(@_);
   }
   else {
     # DOM API: createDocument(namespaceURI, qualifiedName, doctype?)
     my $doc = XML::LibXML::Document-> new;
     my $el = $doc->createElementNS(shift, shift);
     $doc->setDocumentElement($el);
     $doc->setExternalSubset(shift) if @_;
     return $doc;
   }
}

#-------------------------------------------------------------------------#
# callback functions                                                      #
#-------------------------------------------------------------------------#

sub input_callbacks {
    my $self     = shift;
    my $icbclass = shift;

    if ( defined $icbclass ) {
        $self->{XML_LIBXML_CALLBACK_STACK} = $icbclass;
    }
    return $self->{XML_LIBXML_CALLBACK_STACK};
}

sub match_callback {
    my $self = shift;
    if ( ref $self ) {
        if ( scalar @_ ) {
            $self->{XML_LIBXML_MATCH_CB} = shift;
            $self->{XML_LIBXML_CALLBACK_STACK} = undef;
        }
        return $self->{XML_LIBXML_MATCH_CB};
    }
    else {
        $MatchCB = shift if scalar @_;
        return $MatchCB;
    }
}

sub read_callback {
    my $self = shift;
    if ( ref $self ) {
        if ( scalar @_ ) {
            $self->{XML_LIBXML_READ_CB} = shift;
            $self->{XML_LIBXML_CALLBACK_STACK} = undef;
        }
        return $self->{XML_LIBXML_READ_CB};
    }
    else {
        $ReadCB = shift if scalar @_;
        return $ReadCB;
    }
}

sub close_callback {
    my $self = shift;
    if ( ref $self ) {
        if ( scalar @_ ) {
            $self->{XML_LIBXML_CLOSE_CB} = shift;
            $self->{XML_LIBXML_CALLBACK_STACK} = undef;
        }
        return $self->{XML_LIBXML_CLOSE_CB};
    }
    else {
        $CloseCB = shift if scalar @_;
        return $CloseCB;
    }
}

sub open_callback {
    my $self = shift;
    if ( ref $self ) {
        if ( scalar @_ ) {
            $self->{XML_LIBXML_OPEN_CB} = shift;
            $self->{XML_LIBXML_CALLBACK_STACK} = undef;
        }
        return $self->{XML_LIBXML_OPEN_CB};
    }
    else {
        $OpenCB = shift if scalar @_;
        return $OpenCB;
    }
}

sub callbacks {
    my $self = shift;
    if ( ref $self ) {
        if (@_) {
            my ($match, $open, $read, $close) = @_;
            @{$self}{qw(XML_LIBXML_MATCH_CB XML_LIBXML_OPEN_CB XML_LIBXML_READ_CB XML_LIBXML_CLOSE_CB)} = ($match, $open, $read, $close);
            $self->{XML_LIBXML_CALLBACK_STACK} = undef;
        }
        else {
            return @{$self}{qw(XML_LIBXML_MATCH_CB XML_LIBXML_OPEN_CB XML_LIBXML_READ_CB XML_LIBXML_CLOSE_CB)};
        }
    }
    else {
        if (@_) {
           ( $MatchCB, $OpenCB, $ReadCB, $CloseCB ) = @_;
        }
        else {
            return ( $MatchCB, $OpenCB, $ReadCB, $CloseCB );
        }
    }
}

#-------------------------------------------------------------------------#
# member variable manipulation                                            #
#-------------------------------------------------------------------------#
sub validation {
    my $self = shift;
    $self->{XML_LIBXML_VALIDATION} = shift if scalar @_;
    return $self->{XML_LIBXML_VALIDATION};
}

sub recover {
    my $self = shift;
    $self->{XML_LIBXML_RECOVER} = shift if scalar @_;
    return $self->{XML_LIBXML_RECOVER};
}

sub recover_silently {
    my $self = shift;
    my $arg = shift;
    (($arg == 1) ? $self->recover(2) : $self->recover($arg)) if defined($arg);
    return ($self->recover() == 2) ? 1 : 0;
}

sub expand_entities {
    my $self = shift;
    $self->{XML_LIBXML_EXPAND_ENTITIES} = shift if scalar @_;
    return $self->{XML_LIBXML_EXPAND_ENTITIES};
}

sub keep_blanks {
    my $self = shift;
    $self->{XML_LIBXML_KEEP_BLANKS} = shift if scalar @_;
    return $self->{XML_LIBXML_KEEP_BLANKS};
}

sub pedantic_parser {
    my $self = shift;
    $self->{XML_LIBXML_PEDANTIC} = shift if scalar @_;
    return $self->{XML_LIBXML_PEDANTIC};
}

sub line_numbers {
    my $self = shift;
    $self->{XML_LIBXML_LINENUMBERS} = shift if scalar @_;
    return $self->{XML_LIBXML_LINENUMBERS};
}

sub no_network {
    my $self = shift;
    $self->{XML_LIBXML_NONET} = shift if scalar @_;
    return $self->{XML_LIBXML_NONET};
}

sub load_ext_dtd {
    my $self = shift;
    $self->{XML_LIBXML_EXT_DTD} = shift if scalar @_;
    return $self->{XML_LIBXML_EXT_DTD};
}

sub complete_attributes {
    my $self = shift;
    $self->{XML_LIBXML_COMPLETE_ATTR} = shift if scalar @_;
    return $self->{XML_LIBXML_COMPLETE_ATTR};
}

sub expand_xinclude  {
    my $self = shift;
    $self->{XML_LIBXML_EXPAND_XINCLUDE} = shift if scalar @_;
    return $self->{XML_LIBXML_EXPAND_XINCLUDE};
}

sub base_uri {
    my $self = shift;
    $self->{XML_LIBXML_BASE_URI} = shift if scalar @_;
    return $self->{XML_LIBXML_BASE_URI};
}

sub gdome_dom {
    my $self = shift;
    $self->{XML_LIBXML_GDOME} = shift if scalar @_;
    return $self->{XML_LIBXML_GDOME};
}

sub clean_namespaces {
    my $self = shift;
    $self->{XML_LIBXML_NSCLEAN} = shift if scalar @_;
    return $self->{XML_LIBXML_NSCLEAN};
}

#-------------------------------------------------------------------------#
# set the optional SAX(2) handler                                         #
#-------------------------------------------------------------------------#
sub set_handler {
    my $self = shift;
    if ( defined $_[0] ) {
        $self->{HANDLER} = $_[0];

        $self->{SAX_ELSTACK} = [];
        $self->{SAX} = {State => 0};
    }
    else {
        # undef SAX handling
        $self->{SAX_ELSTACK} = [];
        delete $self->{HANDLER};
        delete $self->{SAX};
    }
}

#-------------------------------------------------------------------------#
# helper functions                                                        #
#-------------------------------------------------------------------------#
sub _auto_expand {
    my ( $self, $result, $uri ) = @_;

    $result->setBaseURI( $uri ) if defined $uri;

    if ( defined $self->{XML_LIBXML_EXPAND_XINCLUDE}
         and  $self->{XML_LIBXML_EXPAND_XINCLUDE} == 1 ) {
        $self->{_State_} = 1;
        eval { $self->processXIncludes($result); };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
            $self->_cleanup_callbacks();
            $result = undef;
            croak $err;
        }
    }
    return $result;
}

sub _init_callbacks {
    my $self = shift;
    my $icb = $self->{XML_LIBXML_CALLBACK_STACK};
    
    unless ( defined $icb ) {
        $self->{XML_LIBXML_CALLBACK_STACK} = XML::LibXML::InputCallback->new();
        $icb = $self->{XML_LIBXML_CALLBACK_STACK};
    }

    my $mcb = $self->match_callback();
    my $ocb = $self->open_callback();
    my $rcb = $self->read_callback();
    my $ccb = $self->close_callback();

    if ( defined $mcb and defined $ocb and defined $rcb and defined $ccb ) {
        $icb->register_callbacks( [$mcb, $ocb, $rcb, $ccb] );
    }
    
    $icb->init_callbacks();
}

sub _cleanup_callbacks {
    my $self = shift;
    $self->{XML_LIBXML_CALLBACK_STACK}->cleanup_callbacks();
    my $mcb = $self->match_callback();
    $self->{XML_LIBXML_CALLBACK_STACK}->unregister_callbacks( [$mcb] );
}

sub __read {
    read($_[0], $_[1], $_[2]);
}

sub __write {
    if ( ref( $_[0] ) ) {
        $_[0]->write( $_[1], $_[2] );
    }
    else {
        $_[0]->write( $_[1] );
    }
}

# currently this is only used in the XInlcude processor
# but in the future, all parsing functions should turn to
# the new libxml2 parsing API internally and this will
# become handy
sub _parser_options {
  my ($self,$opts)=@_;
  $opts = {} unless ref $opts;
  my $flags = 0;
  $flags |=     1 if  exists $opts->{recover} ? $opts->{recover} : $self->recover;
  $flags |=     2 if  exists $opts->{expand_entities} ? $opts->{expand_entities} : $self->expand_entities;
  $flags |=     4 if  exists $opts->{load_ext_dtd} ? $opts->{load_ext_dtd} : $self->load_ext_dtd;
  $flags |=     8 if  exists $opts->{complete_attributes} ? $opts->{complete_attributes} : $self->complete_attributes;
  $flags |=    16 if  exists $opts->{validation} ? $opts->{validation} : $self->validation;
  $flags |=    32 if  $opts->{suppress_errors};
  $flags |=    64 if  $opts->{suppress_warnings};
  $flags |=   128 if  exists $opts->{pedantic_parser} ? $opts->{pedantic_parser} : $self->pedantic_parser;
  $flags |=   256 if  exists $opts->{no_blanks} ? $opts->{no_blanks} : !$self->keep_blanks();
  $flags |=  1024 if  exists $opts->{expand_xinclude} ? $opts->{expand_xinclude} : $self->expand_xinclude;
  $flags |=  2048 if  exists $opts->{no_network} ? $opts->{no_network} : $self->no_network;
  $flags |=  8192 if  exists $opts->{clean_namespaces} ? $opts->{clean_namespaces} : $self->clean_namespaces;
  $flags |= 16384 if  $opts->{no_cdata};
  $flags |= 32768 if  $opts->{no_xinclude_nodes};
  return ($flags);
}


#-------------------------------------------------------------------------#
# parsing functions                                                       #
#-------------------------------------------------------------------------#
# all parsing functions handle normal as SAX parsing at the same time.
# note that SAX parsing is handled incomplete! use XML::LibXML::SAX for
# complete parsing sequences
#-------------------------------------------------------------------------#
sub parse_string {
    my $self = shift;
    croak("parse_string is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};

    unless ( defined $_[0] and length $_[0] ) {
        croak("Empty String");
    }

    $self->{_State_} = 1;
    my $result;

    $self->_init_callbacks();

    if ( defined $self->{SAX} ) {
        my $string = shift;
        $self->{SAX_ELSTACK} = [];
        
        eval { $result = $self->_parse_sax_string($string); };

        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
	    chomp $err;
            $self->_cleanup_callbacks();
            croak $err;
        }
    }
    else {
        eval { $result = $self->_parse_string( @_ ); };

        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
	    chomp $err;
            $self->_cleanup_callbacks();
            croak $err;
        }

        $result = $self->_auto_expand( $result, $self->{XML_LIBXML_BASE_URI} );
    }
    $self->_cleanup_callbacks();

    return $result;
}

sub parse_fh {
    my $self = shift;
    croak("parse_fh is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};
    $self->{_State_} = 1;
    my $result;

    $self->_init_callbacks();

    if ( defined $self->{SAX} ) {
        $self->{SAX_ELSTACK} = [];
        eval { $self->_parse_sax_fh( @_ );  };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
	    chomp $err;
            $self->_cleanup_callbacks();
            croak $err;
        }
    }
    else {
        eval { $result = $self->_parse_fh( @_ ); };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
	    chomp $err;
            $self->_cleanup_callbacks();
            croak $err;
        }

        $result = $self->_auto_expand( $result, $self->{XML_LIBXML_BASE_URI} );
    }

    $self->_cleanup_callbacks();

    return $result;
}

sub parse_file {
    my $self = shift;
    croak("parse_file is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};
    $self->{_State_} = 1;
    my $result;

    $self->_init_callbacks();

    if ( defined $self->{SAX} ) {
        $self->{SAX_ELSTACK} = [];
        eval { $self->_parse_sax_file( @_ );  };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
	    chomp $err;
            $self->_cleanup_callbacks();
            croak $err;
        }
    }
    else {
        eval { $result = $self->_parse_file(@_); };
        my $err = $@;
        $self->{_State_} = 0;
        if ($err) {
	    chomp $err;
            $self->_cleanup_callbacks();
            croak $err;
        }

        $result = $self->_auto_expand( $result );
    }
    $self->_cleanup_callbacks();

    return $result;
}

sub parse_xml_chunk {
    my $self = shift;
    # max 2 parameter:
    # 1: the chunk
    # 2: the encoding of the string
    croak("parse_xml_chunk is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};    my $result;

    unless ( defined $_[0] and length $_[0] ) {
        croak("Empty String");
    }

    $self->{_State_} = 1;

    $self->_init_callbacks();

    if ( defined $self->{SAX} ) {
        eval {
            $self->_parse_sax_xml_chunk( @_ );

            # this is required for XML::GenericChunk.
            # in normal case is_filter is not defined, an thus the parsing
            # will be terminated. in case of a SAX filter the parsing is not
            # finished at that state. therefore we must not reset the parsing
            unless ( $self->{IS_FILTER} ) {
                $result = $self->{HANDLER}->end_document();
            }
        };
    }
    else {
        eval { $result = $self->_parse_xml_chunk( @_ ); };
    }

    $self->_cleanup_callbacks();

    my $err = $@;
    $self->{_State_} = 0;
    if ($err) {
        chomp $err;
        croak $err;
    }

    return $result;
}

sub parse_balanced_chunk {
    my $self = shift;
    $self->_init_callbacks();
    my $rv;
    eval {
        $rv = $self->parse_xml_chunk( @_ );
    };
    my $err = $@;
    $self->_cleanup_callbacks();
    if ( $err ) {
        chomp $err;
        croak $err;
    }
    return $rv
}

# java style
sub processXIncludes {
    my $self = shift;
    my $doc = shift;
    my $opts = shift;
    my $options = $self->_parser_options($opts);
    if ( $self->{_State_} != 1 ) {
        $self->_init_callbacks();
    }
    my $rv;
    eval {
        $rv = $self->_processXIncludes($doc || " ", $options);
    };
    my $err = $@;
    if ( $self->{_State_} != 1 ) {
        $self->_cleanup_callbacks();
    }

    if ( $err ) {
        chomp $err;
        croak $err;
    }
    return $rv;
}

# perl style
sub process_xincludes {
    my $self = shift;
    my $doc = shift;
    my $opts = shift;
    my $options = $self->_parser_options($opts);

    my $rv;
    $self->_init_callbacks();
    eval {
        $rv = $self->_processXIncludes($doc || " ", $options);
    };
    my $err = $@;
    $self->_cleanup_callbacks();
    if ( $err ) {
        chomp $err;
        croak $@;
    }
    return $rv;
}

#-------------------------------------------------------------------------#
# HTML parsing functions                                                  #
#-------------------------------------------------------------------------#

sub _html_options {
  my ($self,$opts)=@_;
  $opts = {} unless ref $opts;
  #  return (undef,undef) unless ref $opts;
  my $flags = 0;
  $flags |=     1 if exists $opts->{recover} ? $opts->{recover} : $self->recover;
  $flags |=    32 if $opts->{suppress_errors};
  $flags |=    64 if $opts->{suppress_warnings};
  $flags |=   128 if exists $opts->{pedantic_parser} ? $opts->{pedantic_parser} : $self->pedantic_parser;
  $flags |=   256 if exists $opts->{no_blanks} ? $opts->{no_blanks} : !$self->keep_blanks;
  $flags |=  2048 if exists $opts->{no_network} ? $opts->{no_network} : !$self->no_network;
  return ($opts->{URI},$opts->{encoding},$flags);
}

sub parse_html_string {
    my ($self,$str,$opts) = @_;
    croak("parse_html_string is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};

    unless ( defined $str and length $str ) {
        croak("Empty String");
    }
    $self->{_State_} = 1;
    my $result;

    $self->_init_callbacks();
    eval { 
      $result = $self->_parse_html_string( $str,
					   $self->_html_options($opts)
					  ); 
    };
    my $err = $@;
    $self->{_State_} = 0;
    if ($err) {
      chomp $err;
      $self->_cleanup_callbacks();
      croak $err;
    }

    $self->_cleanup_callbacks();

    return $result;
}

sub parse_html_file {
    my ($self,$file,$opts) = @_;
    croak("parse_html_file is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};
    $self->{_State_} = 1;
    my $result;

    $self->_init_callbacks();
    eval { $result = $self->_parse_html_file($file,
					     $self->_html_options($opts)
					    ); };
    my $err = $@;
    $self->{_State_} = 0;
    if ($err) {
      chomp $err;
      $self->_cleanup_callbacks();
      croak $err;
    }
    
    $self->_cleanup_callbacks();

    return $result;
}

sub parse_html_fh {
    my ($self,$fh,$opts) = @_;
    croak("parse_html_fh is not a class method! Create a parser object with XML::LibXML->new first!") unless ref $self;
    croak("parse already in progress") if $self->{_State_};
    $self->{_State_} = 1;

    my $result;
    $self->_init_callbacks();
    eval { $result = $self->_parse_html_fh( $fh, 
					    $self->_html_options($opts)
					   ); };
    my $err = $@;
    $self->{_State_} = 0;
    if ($err) {
      chomp $err;
      $self->_cleanup_callbacks();
      croak $err;
    }
    $self->_cleanup_callbacks();

    return $result;
}

#-------------------------------------------------------------------------#
# push parser interface                                                   #
#-------------------------------------------------------------------------#
sub init_push {
    my $self = shift;

    if ( defined $self->{CONTEXT} ) {
        delete $self->{CONTEXT};
    }

    if ( defined $self->{SAX} ) {
        $self->{CONTEXT} = $self->_start_push(1);
    }
    else {
        $self->{CONTEXT} = $self->_start_push(0);
    }
}

sub push {
    my $self = shift;

    $self->_init_callbacks();
    
    if ( not defined $self->{CONTEXT} ) {
        $self->init_push();
    }

    eval {
        foreach ( @_ ) {
            $self->_push( $self->{CONTEXT}, $_ );
        }
    };
    my $err = $@;
    $self->_cleanup_callbacks();
    if ( $err ) {
        chomp $err;
        croak $err;
    }
}

# this function should be promoted!
# the reason is because libxml2 uses xmlParseChunk() for this purpose!
sub parse_chunk {
    my $self = shift;
    my $chunk = shift;
    my $terminate = shift;

    if ( not defined $self->{CONTEXT} ) {
        $self->init_push();
    }

    if ( defined $chunk and length $chunk ) {
        $self->_push( $self->{CONTEXT}, $chunk );
    }

    if ( $terminate ) {
        return $self->finish_push();
    }
}


sub finish_push {
    my $self = shift;
    my $restore = shift || 0;
    return undef unless defined $self->{CONTEXT};

    my $retval;

    if ( defined $self->{SAX} ) {
        eval {
            $self->_end_sax_push( $self->{CONTEXT} );
            $retval = $self->{HANDLER}->end_document( {} );
        };
    }
    else {
        eval { $retval = $self->_end_push( $self->{CONTEXT}, $restore ); };
    }
    delete $self->{CONTEXT};
    my $err = $@;
    if ( $err ) {
        chomp $err;
        croak( $err );
    }
    return $retval;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Node Interface                                             #
#-------------------------------------------------------------------------#
package XML::LibXML::Node;

sub isSupported {
    my $self    = shift;
    my $feature = shift;
    return $self->can($feature) ? 1 : 0;
}

sub getChildNodes { my $self = shift; return $self->childNodes(); }

sub childNodes {
    my $self = shift;
    my @children = $self->_childNodes();
    return wantarray ? @children : XML::LibXML::NodeList->new_from_ref(\@children , 1);
}

sub attributes {
    my $self = shift;
    my @attr = $self->_attributes();
    return wantarray ? @attr : XML::LibXML::NamedNodeMap->new( @attr );
}


sub findnodes {
    my ($node, $xpath) = @_;
    my @nodes = $node->_findnodes($xpath);
    if (wantarray) {
        return @nodes;
    }
    else {
        return XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
    }
}

sub findvalue {
    my ($node, $xpath) = @_;
    my $res;
    eval {
        $res = $node->find($xpath);
    };
    if  ( $@ ) {
        die $@;
    }
    return $res->to_literal->value;
}

sub find {
    my ($node, $xpath) = @_;
    my ($type, @params) = $node->_find($xpath);
    if ($type) {
        return $type->new(@params);
    }
    return undef;
}

sub setOwnerDocument {
    my ( $self, $doc ) = @_;
    $doc->adoptNode( $self );
}

sub toStringC14N {
    my ($self, $comments, $xpath) = (shift, shift, shift);
    return $self->_toStringC14N( $comments || 0,
				 (defined $xpath ? $xpath : undef),
				 0,
				 undef );
}
sub toStringEC14N {
    my ($self, $comments, $xpath, $inc_prefix_list) = @_;
    if (defined($inc_prefix_list) and !UNIVERSAL::isa($inc_prefix_list,'ARRAY')) {
      croak("toStringEC14N: inclusive_prefix_list must be undefined or ARRAY");
    }
    return $self->_toStringC14N( $comments || 0,
				 (defined $xpath ? $xpath : undef),
				 1,
				 (defined $inc_prefix_list ? $inc_prefix_list : undef));
}

*serialize_c14n = \&toStringC14N;
*serialize_exc_c14n = \&toStringEC14N;

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Document Interface                                         #
#-------------------------------------------------------------------------#
package XML::LibXML::Document;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Node');

sub actualEncoding {
  my $doc = shift;
  my $enc = $doc->encoding;
  return (defined $enc and length $enc) ? $enc : 'UTF-8';
}

sub setDocumentElement {
    my $doc = shift;
    my $element = shift;

    my $oldelem = $doc->documentElement;
    if ( defined $oldelem ) {
        $doc->removeChild($oldelem);
    }

    $doc->_setDocumentElement($element);
}

sub toString {
    my $self = shift;
    my $flag = shift;

    my $retval = "";

    if ( defined $XML::LibXML::skipXMLDeclaration
         and $XML::LibXML::skipXMLDeclaration == 1 ) {
        foreach ( $self->childNodes ){
            next if $_->nodeType == XML::LibXML::XML_DTD_NODE()
                    and $XML::LibXML::skipDTD;
            $retval .= $_->toString;
        }
    }
    else {
        $flag ||= 0 unless defined $flag;
        $retval =  $self->_toString($flag);
    }

    return $retval;
}

sub serialize {
    my $self = shift;
    return $self->toString( @_ );
}

#-------------------------------------------------------------------------#
# bad style xinclude processing                                           #
#-------------------------------------------------------------------------#
sub process_xinclude {
    my $self = shift;
    my $opts = shift;
    XML::LibXML->new->processXIncludes( $self, $opts );
}

sub insertProcessingInstruction {
    my $self   = shift;
    my $target = shift;
    my $data   = shift;

    my $pi     = $self->createPI( $target, $data );
    my $root   = $self->documentElement;

    if ( defined $root ) {
        # this is actually not correct, but i guess it's what the user
        # intends
        $self->insertBefore( $pi, $root );
    }
    else {
        # if no documentElement was found we just append the PI
        $self->appendChild( $pi );
    }
}

sub insertPI {
    my $self = shift;
    $self->insertProcessingInstruction( @_ );
}

#-------------------------------------------------------------------------#
# DOM L3 Document functions.
# added after robins implicit feature requst
#-------------------------------------------------------------------------#
*getElementsByTagName = \&XML::LibXML::Element::getElementsByTagName;
*getElementsByTagNameNS = \&XML::LibXML::Element::getElementsByTagNameNS;
*getElementsByLocalName = \&XML::LibXML::Element::getElementsByLocalName;

1;

#-------------------------------------------------------------------------#
# XML::LibXML::DocumentFragment Interface                                 #
#-------------------------------------------------------------------------#
package XML::LibXML::DocumentFragment;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Node');

sub toString {
    my $self = shift;
    my $retval = "";
    if ( $self->hasChildNodes() ) {
        foreach my $n ( $self->childNodes() ) {
            $retval .= $n->toString(@_);
        }
    }
    return $retval;
}

*serialize = \&toString;

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Element Interface                                          #
#-------------------------------------------------------------------------#
package XML::LibXML::Element;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Node');
use XML::LibXML qw(:ns :libxml);
use Carp;

sub setNamespace {
    my $self = shift;
    my $n = $self->nodeName;
    if ( $self->_setNamespace(@_) ){
        if ( scalar @_ < 3 || $_[2] == 1 ){
            $self->setNodeName( $n );
        }
        return 1;
    }
    return 0;
}

sub getAttribute {
    my $self = shift;
    my $name = $_[0];
    if ( $name =~ /^xmlns(?::|$)/ ) {
        # user wants to get a namespace ...
        (my $prefix = $name )=~s/^xmlns:?//;
	$self->_getNamespaceDeclURI($prefix);
    }
    else {
        $self->_getAttribute(@_);
    }
}

sub setAttribute {
    my ( $self, $name, $value ) = @_;
    if ( $name =~ /^xmlns(?::|$)/ ) {
      # user wants to set the special attribute for declaring XML namespace ...

      # this is fine but not exactly DOM conformant behavior, btw (according to DOM we should
      # probably declare an attribute which looks like XML namespace declaration
      # but isn't)
      (my $nsprefix = $name )=~s/^xmlns:?//;
      my $nn = $self->nodeName;
      if ( $nn =~ /^\Q${nsprefix}\E:/ ) {
	# the element has the same prefix
	$self->setNamespaceDeclURI($nsprefix,$value) ||
	  $self->setNamespace($value,$nsprefix,1);
        ##
        ## We set the namespace here.
        ## This is helpful, as in:
        ##
        ## |  $e = XML::LibXML::Element->new('foo:bar');
        ## |  $e->setAttribute('xmlns:foo','http://yoyodine')
        ##
      }
      else {
	# just modify the namespace
	$self->setNamespaceDeclURI($nsprefix, $value) ||
	  $self->setNamespace($value,$nsprefix,0);
      }
    }
    else {
        $self->_setAttribute($name, $value);
    }
}

sub getAttributeNS {
    my $self = shift;
    my ($nsURI, $name) = @_;
    croak("invalid attribute name") if !defined($name) or $name eq q{};
    if ( defined($nsURI) and $nsURI eq XML_XMLNS_NS ) {
	$self->_getNamespaceDeclURI($name eq 'xmlns' ? undef : $name);
    }
    else {
        $self->_getAttributeNS(@_);
    }
}

sub setAttributeNS {
  my ($self, $nsURI, $qname, $value)=@_;
  unless (defined $qname and length $qname) {
    croak("bad name");
  }
  if (defined($nsURI) and $nsURI eq XML_XMLNS_NS) {
    if ($qname !~ /^xmlns(?::|$)/) {
      croak("NAMESPACE ERROR: Namespace declartions must have the prefix 'xmlns'");
    }
    $self->setAttribute($qname,$value); # see implementation above
    return;
  }
  if ($qname=~/:/ and not (defined($nsURI) and length($nsURI))) {
    croak("NAMESPACE ERROR: Attribute without a prefix cannot be in a namespace");
  }
  if ($qname=~/^xmlns(?:$|:)/) {
    croak("NAMESPACE ERROR: 'xmlns' prefix and qualified-name are reserved for the namespace ".XML_XMLNS_NS);
  }
  if ($qname=~/^xml:/ and not (defined $nsURI and $nsURI eq XML_XML_NS)) {
    croak("NAMESPACE ERROR: 'xml' prefix is reserved for the namespace ".XML_XML_NS);
  }
  $self->_setAttributeNS( defined $nsURI ? $nsURI : undef, $qname, $value );
}

sub getElementsByTagName {
    my ( $node , $name ) = @_;
    my $xpath = $name eq '*' ? "descendant::*" : "descendant::*[name()='$name']";
    my @nodes = $node->_findnodes($xpath);
    return wantarray ? @nodes : XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
}

sub  getElementsByTagNameNS {
    my ( $node, $nsURI, $name ) = @_;
    my $xpath;
    if ( $name eq '*' ) {
      if ( $nsURI eq '*' ) {
	$xpath = "descendant::*";
      } else {
	$xpath = "descendant::*[namespace-uri()='$nsURI']";
      }
    } elsif ( $nsURI eq '*' ) {
      $xpath = "descendant::*[local-name()='$name']";
    } else {
      $xpath = "descendant::*[local-name()='$name' and namespace-uri()='$nsURI']";
    }
    my @nodes = $node->_findnodes($xpath);
    return wantarray ? @nodes : XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
}

sub getElementsByLocalName {
    my ( $node,$name ) = @_;
    my $xpath;
    if ($name eq '*') {
      $xpath = "descendant::*";
    } else {
      $xpath = "descendant::*[local-name()='$name']";
    }
    my @nodes = $node->_findnodes($xpath);
    return wantarray ? @nodes : XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
}

sub getChildrenByTagName {
    my ( $node, $name ) = @_;
    my @nodes;
    if ($name eq '*') {
      @nodes = grep { $_->nodeType == XML_ELEMENT_NODE() }
	$node->childNodes();
    } else {
      @nodes = grep { $_->nodeName eq $name } $node->childNodes();
    }
    return wantarray ? @nodes : XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
}

sub getChildrenByLocalName {
    my ( $node, $name ) = @_;
    my @nodes;
    if ($name eq '*') {
      @nodes = grep { $_->nodeType == XML_ELEMENT_NODE() }
	$node->childNodes();
    } else {
      @nodes = grep { $_->nodeType == XML_ELEMENT_NODE() and
		      $_->localName eq $name } $node->childNodes();
    }
    return wantarray ? @nodes : XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
}

sub getChildrenByTagNameNS {
    my ( $node, $nsURI, $name ) = @_;
    my @nodes = $node->_getChildrenByTagNameNS($nsURI,$name);
    return wantarray ? @nodes : XML::LibXML::NodeList->new_from_ref(\@nodes, 1);
}

sub appendWellBalancedChunk {
    my ( $self, $chunk ) = @_;

    my $local_parser = XML::LibXML->new();
    my $frag = $local_parser->parse_xml_chunk( $chunk );

    $self->appendChild( $frag );
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Text Interface                                             #
#-------------------------------------------------------------------------#
package XML::LibXML::Text;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Node');

sub attributes { return undef; }

sub deleteDataString {
    my $node = shift;
    my $string = shift;
    my $all    = shift;
    my $data = $node->nodeValue();
    $string =~ s/([\\\*\+\^\{\}\&\?\[\]\(\)\$\%\@])/\\$1/g;
    if ( $all ) {
        $data =~ s/$string//g;
    }
    else {
        $data =~ s/$string//;
    }
    $node->setData( $data );
}

sub replaceDataString {
    my ( $node, $left, $right,$all ) = @_;

    #ashure we exchange the strings and not expressions!
    $left  =~ s/([\\\*\+\^\{\}\&\?\[\]\(\)\$\%\@])/\\$1/g;
    my $datastr = $node->nodeValue();
    if ( $all ) {
        $datastr =~ s/$left/$right/g;
    }
    else{
        $datastr =~ s/$left/$right/;
    }
    $node->setData( $datastr );
}

sub replaceDataRegEx {
    my ( $node, $leftre, $rightre, $flags ) = @_;
    return unless defined $leftre;
    $rightre ||= "";

    my $datastr = $node->nodeValue();
    my $restr   = "s/" . $leftre . "/" . $rightre . "/";
    $restr .= $flags if defined $flags;

    eval '$datastr =~ '. $restr;

    $node->setData( $datastr );
}

1;

package XML::LibXML::Comment;

use vars qw(@ISA);
@ISA = ('XML::LibXML::Text');

1;

package XML::LibXML::CDATASection;

use vars qw(@ISA);
@ISA     = ('XML::LibXML::Text');

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Attribute Interface                                        #
#-------------------------------------------------------------------------#
package XML::LibXML::Attr;
use vars qw( @ISA ) ;
@ISA = ('XML::LibXML::Node') ;

sub setNamespace {
    my ($self,$href,$prefix) = @_;
    my $n = $self->nodeName;
    if ( $self->_setNamespace($href,$prefix) ) {
        $self->setNodeName($n);
        return 1;
    }

    return 0;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Dtd Interface                                              #
#-------------------------------------------------------------------------#
# this is still under construction
#
package XML::LibXML::Dtd;
use vars qw( @ISA );
@ISA = ('XML::LibXML::Node');

1;

#-------------------------------------------------------------------------#
# XML::LibXML::PI Interface                                               #
#-------------------------------------------------------------------------#
package XML::LibXML::PI;
use vars qw( @ISA );
@ISA = ('XML::LibXML::Node');

sub setData {
    my $pi = shift;

    my $string = "";
    if ( scalar @_ == 1 ) {
        $string = shift;
    }
    else {
        my %h = @_;
        $string = join " ", map {$_.'="'.$h{$_}.'"'} keys %h;
    }

    # the spec says any char but "?>" [17]
    $pi->_setData( $string ) unless  $string =~ /\?>/;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::Namespace Interface                                        #
#-------------------------------------------------------------------------#
package XML::LibXML::Namespace;

# this is infact not a node!
sub prefix { return "xmlns"; }
sub getPrefix { return "xmlns"; }
sub getNamespaceURI { return "http://www.w3.org/2000/xmlns/" };

sub getNamespaces { return (); }

sub nodeName {
  my $self = shift;
  my $nsP  = $self->localname;
  return ( defined($nsP) && length($nsP) ) ? "xmlns:$nsP" : "xmlns";
}
sub name    { goto &nodeName }
sub getName { goto &nodeName }

sub isEqualNode {
    my ( $self, $ref ) = @_;
    if ( ref($ref) eq "XML::LibXML::Namespace" ) {
        return $self->_isEqual($ref);
    }
    return 0;
}

sub isSameNode {
    my ( $self, $ref ) = @_;
    if ( $$self == $$ref ){
        return 1;
    }
    return 0;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::NamedNodeMap Interface                                     #
#-------------------------------------------------------------------------#
package XML::LibXML::NamedNodeMap;

use XML::LibXML::Common qw(:libxml);

sub new {
    my $class = shift;
    my $self = bless { Nodes => [@_] }, $class;
    $self->{NodeMap} = { map { $_->nodeName => $_ } @_ };
    return $self;
}

sub length     { return scalar( @{$_[0]->{Nodes}} ); }
sub nodes      { return $_[0]->{Nodes}; }
sub item       { $_[0]->{Nodes}->[$_[1]]; }

sub getNamedItem {
    my $self = shift;
    my $name = shift;

    return $self->{NodeMap}->{$name};
}

sub setNamedItem {
    my $self = shift;
    my $node = shift;

    my $retval;
    if ( defined $node ) {
        if ( scalar @{$self->{Nodes}} ) {
            my $name = $node->nodeName();
            if ( $node->nodeType() == XML_NAMESPACE_DECL ) {
                return;
            }
            if ( defined $self->{NodeMap}->{$name} ) {
                if ( $node->isSameNode( $self->{NodeMap}->{$name} ) ) {
                    return;
                }
                $retval = $self->{NodeMap}->{$name}->replaceNode( $node );
            }
            else {
                $self->{Nodes}->[0]->addSibling($node);
            }

            $self->{NodeMap}->{$name} = $node;
            push @{$self->{Nodes}}, $node;
        }
        else {
            # not done yet
            # can this be properly be done???
            warn "not done yet\n";
        }
    }
    return $retval;
}

sub removeNamedItem {
    my $self = shift;
    my $name = shift;
    my $retval;
    if ( $name =~ /^xmlns/ ) {
        warn "not done yet\n";
    }
    elsif ( exists $self->{NodeMap}->{$name} ) {
        $retval = $self->{NodeMap}->{$name};
        $retval->unbindNode;
        delete $self->{NodeMap}->{$name};
        $self->{Nodes} = [grep {not($retval->isSameNode($_))} @{$self->{Nodes}}];
    }

    return $retval;
}

sub getNamedItemNS {
    my $self = shift;
    my $nsURI = shift;
    my $name = shift;
    return undef;
}

sub setNamedItemNS {
    my $self = shift;
    my $nsURI = shift;
    my $node = shift;
    return undef;
}

sub removeNamedItemNS {
    my $self = shift;
    my $nsURI = shift;
    my $name = shift;
    return undef;
}

1;

package XML::LibXML::_SAXParser;

# this is pseudo class!!! and it will be removed as soon all functions
# moved to XS level

use XML::SAX::Exception;

# these functions will use SAX exceptions as soon i know how things really work
sub warning {
    my ( $parser, $message, $line, $col ) = @_;
    my $error = XML::SAX::Exception::Parse->new( LineNumber   => $line,
                                                 ColumnNumber => $col,
                                                 Message      => $message, );
    $parser->{HANDLER}->warning( $error );
}

sub error {
    my ( $parser, $message, $line, $col ) = @_;

    my $error = XML::SAX::Exception::Parse->new( LineNumber   => $line,
                                                 ColumnNumber => $col,
                                                 Message      => $message, );
    $parser->{HANDLER}->error( $error );
}

sub fatal_error {
    my ( $parser, $message, $line, $col ) = @_;
    my $error = XML::SAX::Exception::Parse->new( LineNumber   => $line,
                                                 ColumnNumber => $col,
                                                 Message      => $message, );
    $parser->{HANDLER}->fatal_error( $error );
}

1;

package XML::LibXML::RelaxNG;

sub new {
    my $class = shift;
    my %args = @_;

    my $self = undef;
    if ( defined $args{location} ) {
        $self = $class->parse_location( $args{location} );
    }
    elsif ( defined $args{string} ) {
        $self = $class->parse_buffer( $args{string} );
    }
    elsif ( defined $args{DOM} ) {
        $self = $class->parse_document( $args{DOM} );
    }

    return $self;
}

1;

package XML::LibXML::Schema;

sub new {
    my $class = shift;
    my %args = @_;

    my $self = undef;
    if ( defined $args{location} ) {
        $self = $class->parse_location( $args{location} );
    }
    elsif ( defined $args{string} ) {
        $self = $class->parse_buffer( $args{string} );
    }

    return $self;
}

1;

#-------------------------------------------------------------------------#
# XML::LibXML::InputCallback Interface                                    #
#-------------------------------------------------------------------------#
package XML::LibXML::InputCallback;

use vars qw($_CUR_CB @_GLOBAL_CALLBACKS @_CB_STACK);

$_CUR_CB = undef;

@_GLOBAL_CALLBACKS = ();
@_CB_STACK = ();

#-------------------------------------------------------------------------#
# global callbacks                                                        #
#-------------------------------------------------------------------------#
sub _callback_match {
    my $uri = shift;
    my $retval = 0;

    # loop through the callbacks and and find the first matching
    # The callbacks are stored in execution order (reverse stack order)
    # any new global callbacks are shifted to the callback stack.
    foreach my $cb ( @_GLOBAL_CALLBACKS ) {

        # callbacks have to return 1, 0 or undef, while 0 and undef 
        # are handled the same way. 
        # in fact, if callbacks return other values, the global match 
        # assumes silently that the callback failed.

        $retval = $cb->[0]->($uri);

        if ( defined $retval and $retval == 1 ) {
            # make the other callbacks use this callback
            $_CUR_CB = $cb;
            unshift @_CB_STACK, $cb; 
            last;
        }
    }

    return $retval;
}

sub _callback_open {
    my $uri = shift;
    my $retval = undef;
    
    # the open callback has to return a defined value. 
    # if one works on files this can be a file handle. But 
    # depending on the needs of the callback it also can be a 
    # database handle or a integer labeling a certain dataset.

    if ( defined $_CUR_CB ) {
        $retval = $_CUR_CB->[1]->( $uri );
        
        # reset the callbacks, if one callback cannot open an uri
        if ( not defined $retval or $retval == 0 ) {
            shift @_CB_STACK;
            $_CUR_CB = $_CB_STACK[0];
        }
    }
    
    return $retval;
}

sub _callback_read {
    my $fh = shift;
    my $buflen = shift;

    my $retval = undef;

    if ( defined $_CUR_CB ) {
        $retval = $_CUR_CB->[2]->( $fh, $buflen );
    }

    return $retval;
}

sub _callback_close {
    my $fh = shift;
    my $retval = 0;
    
    if ( defined $_CUR_CB ) {
        $retval = $_CUR_CB->[3]->( $fh );
        shift @_CB_STACK;
        $_CUR_CB = $_CB_STACK[0];
    }

    return $retval;
}

#-------------------------------------------------------------------------#
# member functions and methods                                            #
#-------------------------------------------------------------------------#

sub new {
    my $CLASS = shift;
    return bless {'_CALLBACKS' => []}, $CLASS;
}

# add a callback set to the callback stack
# synopsis: $icb->register_callbacks( [$match_cb, $open_cb, $read_cb, $close_cb] );
sub register_callbacks {
    my $self = shift;
    my $cbset = shift;
    
    # test if callback set is complete
    if ( ref $cbset eq "ARRAY" and scalar( @$cbset ) == 4 ) {
        unshift @{$self->{_CALLBACKS}}, $cbset;
    }
}

# remove a callback set to the callback stack
# if a callback set is passed, this function will check for the match function
sub unregister_callbacks {
    my $self = shift;
    my $cbset = shift;
    if ( ref $cbset eq "ARRAY" and scalar( @$cbset ) == 4 ) {
        $self->{_CALLBACKS} = [grep { $_->[0] != $cbset->[0] } @{$self->{_CALLBACKS}}];
    }
    else {
        shift @{$self->{_CALLBACKS}};
    }
}

# make libxml2 use the callbacks 
sub init_callbacks {
    my $self = shift;

    $_CUR_CB           = undef;
    @_CB_STACK         = ();

    @_GLOBAL_CALLBACKS = @{ $self->{_CALLBACKS} };
    
    if ( defined $XML::LibXML::match_cb and 
         defined $XML::LibXML::open_cb  and 
         defined $XML::LibXML::read_cb  and 
         defined $XML::LibXML::close_cb ) {
        push @_GLOBAL_CALLBACKS, [$XML::LibXML::match_cb,
                                  $XML::LibXML::open_cb,
                                  $XML::LibXML::read_cb,
                                  $XML::LibXML::close_cb];
    }

    $self->lib_init_callbacks();
}

# reset libxml2's callbacks
sub cleanup_callbacks {
    my $self = shift;
    
    $_CUR_CB           = undef;
    @_GLOBAL_CALLBACKS = ();
    @_CB_STACK         = ();

    $self->lib_cleanup_callbacks();
}

1;

__END__
