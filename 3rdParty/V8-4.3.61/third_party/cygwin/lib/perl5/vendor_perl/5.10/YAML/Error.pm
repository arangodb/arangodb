package YAML::Error;
use strict; use warnings;
use YAML::Base; use base 'YAML::Base';

field 'code';
field 'type' => 'Error';
field 'line';
field 'document';
field 'arguments' => [];

my ($error_messages, %line_adjust);

sub format_message {
    my $self = shift;
    my $output = 'YAML ' . $self->type . ': ';
    my $code = $self->code;
    if ($error_messages->{$code}) {
        $code = sprintf($error_messages->{$code}, @{$self->arguments});
    }
    $output .= $code . "\n";

    $output .= '   Code: ' . $self->code . "\n"
        if defined $self->code;
    $output .= '   Line: ' . $self->line . "\n"
        if defined $self->line;
    $output .= '   Document: ' . $self->document . "\n"
        if defined $self->document;
    return $output;
}

sub error_messages {
    $error_messages;
}

%$error_messages = map {s/^\s+//;$_} split "\n", <<'...';
YAML_PARSE_ERR_BAD_CHARS
  Invalid characters in stream. This parser only supports printable ASCII
YAML_PARSE_ERR_NO_FINAL_NEWLINE
  Stream does not end with newline character
YAML_PARSE_ERR_BAD_MAJOR_VERSION
  Can't parse a %s document with a 1.0 parser
YAML_PARSE_WARN_BAD_MINOR_VERSION
  Parsing a %s document with a 1.0 parser
YAML_PARSE_WARN_MULTIPLE_DIRECTIVES
  '%s directive used more than once'
YAML_PARSE_ERR_TEXT_AFTER_INDICATOR
  No text allowed after indicator
YAML_PARSE_ERR_NO_ANCHOR
  No anchor for alias '*%s'
YAML_PARSE_ERR_NO_SEPARATOR
  Expected separator '---'
YAML_PARSE_ERR_SINGLE_LINE
  Couldn't parse single line value
YAML_PARSE_ERR_BAD_ANCHOR
  Invalid anchor
YAML_DUMP_ERR_INVALID_INDENT
  Invalid Indent width specified: '%s'
YAML_LOAD_USAGE
  usage: YAML::Load($yaml_stream_scalar)
YAML_PARSE_ERR_BAD_NODE
  Can't parse node
YAML_PARSE_ERR_BAD_EXPLICIT
  Unsupported explicit transfer: '%s'
YAML_DUMP_USAGE_DUMPCODE
  Invalid value for DumpCode: '%s'
YAML_LOAD_ERR_FILE_INPUT
  Couldn't open %s for input:\n%s
YAML_DUMP_ERR_FILE_CONCATENATE
  Can't concatenate to YAML file %s
YAML_DUMP_ERR_FILE_OUTPUT
  Couldn't open %s for output:\n%s
YAML_DUMP_ERR_NO_HEADER
  With UseHeader=0, the node must be a plain hash or array
YAML_DUMP_WARN_BAD_NODE_TYPE
  Can't perform serialization for node type: '%s'
YAML_EMIT_WARN_KEYS
  Encountered a problem with 'keys':\n%s
YAML_DUMP_WARN_DEPARSE_FAILED
  Deparse failed for CODE reference
YAML_DUMP_WARN_CODE_DUMMY
  Emitting dummy subroutine for CODE reference
YAML_PARSE_ERR_MANY_EXPLICIT
  More than one explicit transfer
YAML_PARSE_ERR_MANY_IMPLICIT
  More than one implicit request
YAML_PARSE_ERR_MANY_ANCHOR
  More than one anchor
YAML_PARSE_ERR_ANCHOR_ALIAS
  Can't define both an anchor and an alias
YAML_PARSE_ERR_BAD_ALIAS
  Invalid alias
YAML_PARSE_ERR_MANY_ALIAS
  More than one alias
YAML_LOAD_ERR_NO_CONVERT
  Can't convert implicit '%s' node to explicit '%s' node
YAML_LOAD_ERR_NO_DEFAULT_VALUE
  No default value for '%s' explicit transfer
YAML_LOAD_ERR_NON_EMPTY_STRING
  Only the empty string can be converted to a '%s'
YAML_LOAD_ERR_BAD_MAP_TO_SEQ
  Can't transfer map as sequence. Non numeric key '%s' encountered.
YAML_DUMP_ERR_BAD_GLOB
  '%s' is an invalid value for Perl glob
YAML_DUMP_ERR_BAD_REGEXP
  '%s' is an invalid value for Perl Regexp
YAML_LOAD_ERR_BAD_MAP_ELEMENT
  Invalid element in map
YAML_LOAD_WARN_DUPLICATE_KEY
  Duplicate map key found. Ignoring.
YAML_LOAD_ERR_BAD_SEQ_ELEMENT
  Invalid element in sequence
YAML_PARSE_ERR_INLINE_MAP
  Can't parse inline map
YAML_PARSE_ERR_INLINE_SEQUENCE
  Can't parse inline sequence
YAML_PARSE_ERR_BAD_DOUBLE
  Can't parse double quoted string
YAML_PARSE_ERR_BAD_SINGLE
  Can't parse single quoted string
YAML_PARSE_ERR_BAD_INLINE_IMPLICIT
  Can't parse inline implicit value '%s'
YAML_PARSE_ERR_BAD_IMPLICIT
  Unrecognized implicit value '%s'
YAML_PARSE_ERR_INDENTATION
  Error. Invalid indentation level
YAML_PARSE_ERR_INCONSISTENT_INDENTATION
  Inconsistent indentation level
YAML_LOAD_WARN_UNRESOLVED_ALIAS
  Can't resolve alias *%s
YAML_LOAD_WARN_NO_REGEXP_IN_REGEXP
  No 'REGEXP' element for Perl regexp
YAML_LOAD_WARN_BAD_REGEXP_ELEM
  Unknown element '%s' in Perl regexp
YAML_LOAD_WARN_GLOB_NAME
  No 'NAME' element for Perl glob
YAML_LOAD_WARN_PARSE_CODE
  Couldn't parse Perl code scalar: %s
YAML_LOAD_WARN_CODE_DEPARSE
  Won't parse Perl code unless $YAML::LoadCode is set
YAML_EMIT_ERR_BAD_LEVEL
  Internal Error: Bad level detected
YAML_PARSE_WARN_AMBIGUOUS_TAB
  Amibiguous tab converted to spaces
YAML_LOAD_WARN_BAD_GLOB_ELEM
  Unknown element '%s' in Perl glob
YAML_PARSE_ERR_ZERO_INDENT
  Can't use zero as an indentation width
YAML_LOAD_WARN_GLOB_IO
  Can't load an IO filehandle. Yet!!!
...

%line_adjust = map {($_, 1)} 
  qw(YAML_PARSE_ERR_BAD_MAJOR_VERSION
     YAML_PARSE_WARN_BAD_MINOR_VERSION 
     YAML_PARSE_ERR_TEXT_AFTER_INDICATOR 
     YAML_PARSE_ERR_NO_ANCHOR 
     YAML_PARSE_ERR_MANY_EXPLICIT
     YAML_PARSE_ERR_MANY_IMPLICIT
     YAML_PARSE_ERR_MANY_ANCHOR
     YAML_PARSE_ERR_ANCHOR_ALIAS
     YAML_PARSE_ERR_BAD_ALIAS
     YAML_PARSE_ERR_MANY_ALIAS
     YAML_LOAD_ERR_NO_CONVERT
     YAML_LOAD_ERR_NO_DEFAULT_VALUE
     YAML_LOAD_ERR_NON_EMPTY_STRING
     YAML_LOAD_ERR_BAD_MAP_TO_SEQ
     YAML_LOAD_ERR_BAD_STR_TO_INT
     YAML_LOAD_ERR_BAD_STR_TO_DATE
     YAML_LOAD_ERR_BAD_STR_TO_TIME
     YAML_LOAD_WARN_DUPLICATE_KEY
     YAML_PARSE_ERR_INLINE_MAP
     YAML_PARSE_ERR_INLINE_SEQUENCE
     YAML_PARSE_ERR_BAD_DOUBLE
     YAML_PARSE_ERR_BAD_SINGLE
     YAML_PARSE_ERR_BAD_INLINE_IMPLICIT
     YAML_PARSE_ERR_BAD_IMPLICIT
     YAML_LOAD_WARN_NO_REGEXP_IN_REGEXP
     YAML_LOAD_WARN_BAD_REGEXP_ELEM
     YAML_LOAD_WARN_REGEXP_CREATE
     YAML_LOAD_WARN_GLOB_NAME
     YAML_LOAD_WARN_PARSE_CODE
     YAML_LOAD_WARN_CODE_DEPARSE
     YAML_LOAD_WARN_BAD_GLOB_ELEM
     YAML_PARSE_ERR_ZERO_INDENT
    );

package YAML::Warning;
use base 'YAML::Error';

1;

__END__

=head1 NAME

YAML::Error - Error formatting class for YAML modules

=head1 SYNOPSIS

    $self->die('YAML_PARSE_ERR_NO_ANCHOR', $alias);
    $self->warn('YAML_LOAD_WARN_DUPLICATE_KEY');

=head1 DESCRIPTION

This module provides a C<die> and a C<warn> facility.

=head1 AUTHOR

Ingy döt Net <ingy@cpan.org>

=head1 COPYRIGHT

Copyright (c) 2006. Ingy döt Net. All rights reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut
