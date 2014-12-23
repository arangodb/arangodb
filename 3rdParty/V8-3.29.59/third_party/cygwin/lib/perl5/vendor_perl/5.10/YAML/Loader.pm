package YAML::Loader;
use strict; use warnings;
use YAML::Base;
use base 'YAML::Loader::Base';
use YAML::Types;

# Context constants
use constant LEAF => 1;
use constant COLLECTION => 2;
use constant VALUE => "\x07YAML\x07VALUE\x07";
use constant COMMENT => "\x07YAML\x07COMMENT\x07";

# Common YAML character sets
my $ESCAPE_CHAR = '[\\x00-\\x08\\x0b-\\x0d\\x0e-\\x1f]';
my $FOLD_CHAR = '>';
my $LIT_CHAR = '|';    
my $LIT_CHAR_RX = "\\$LIT_CHAR";    

sub load {
    my $self = shift;
    $self->stream($_[0] || '');
    return $self->_parse();
}

# Top level function for parsing. Parse each document in order and
# handle processing for YAML headers.
sub _parse {
    my $self = shift;
    my (%directives, $preface);
    $self->{stream} =~ s|\015\012|\012|g;
    $self->{stream} =~ s|\015|\012|g;
    $self->line(0);
    $self->die('YAML_PARSE_ERR_BAD_CHARS') 
      if $self->stream =~ /$ESCAPE_CHAR/;
    $self->die('YAML_PARSE_ERR_NO_FINAL_NEWLINE') 
      if length($self->stream) and 
         $self->{stream} !~ s/(.)\n\Z/$1/s;
    $self->lines([split /\x0a/, $self->stream, -1]);
    $self->line(1);
    # Throw away any comments or blanks before the header (or start of
    # content for headerless streams)
    $self->_parse_throwaway_comments();
    $self->document(0);
    $self->documents([]);
    # Add an "assumed" header if there is no header and the stream is
    # not empty (after initial throwaways).
    if (not $self->eos) {
        if ($self->lines->[0] !~ /^---(\s|$)/) {
            unshift @{$self->lines}, '---';
            $self->{line}--;
        }
    }

    # Main Loop. Parse out all the top level nodes and return them.
    while (not $self->eos) {
        $self->anchor2node({});
        $self->{document}++;
        $self->done(0);
        $self->level(0);
        $self->offset->[0] = -1;

        if ($self->lines->[0] =~ /^---\s*(.*)$/) {
            my @words = split /\s+/, $1;
            %directives = ();
            while (@words && $words[0] =~ /^#(\w+):(\S.*)$/) {
                my ($key, $value) = ($1, $2);
                shift(@words);
                if (defined $directives{$key}) {
                    $self->warn('YAML_PARSE_WARN_MULTIPLE_DIRECTIVES',
                      $key, $self->document);
                    next;
                }
                $directives{$key} = $value;
            }
            $self->preface(join ' ', @words);
        }
        else {
            $self->die('YAML_PARSE_ERR_NO_SEPARATOR');
        }

        if (not $self->done) {
            $self->_parse_next_line(COLLECTION);
        }
        if ($self->done) {
            $self->{indent} = -1;
            $self->content('');
        }

        $directives{YAML} ||= '1.0';
        $directives{TAB} ||= 'NONE';
        ($self->{major_version}, $self->{minor_version}) = 
          split /\./, $directives{YAML}, 2;
        $self->die('YAML_PARSE_ERR_BAD_MAJOR_VERSION', $directives{YAML})
          if $self->major_version ne '1';
        $self->warn('YAML_PARSE_WARN_BAD_MINOR_VERSION', $directives{YAML})
          if $self->minor_version ne '0';
        $self->die('Unrecognized TAB policy')
          unless $directives{TAB} =~ /^(NONE|\d+)(:HARD)?$/;

        push @{$self->documents}, $self->_parse_node();
    }
    return wantarray ? @{$self->documents} : $self->documents->[-1];
}

# This function is the dispatcher for parsing each node. Every node
# recurses back through here. (Inlines are an exception as they have
# their own sub-parser.)
sub _parse_node {
    my $self = shift;
    my $preface = $self->preface;
    $self->preface('');
    my ($node, $type, $indicator, $escape, $chomp) = ('') x 5;
    my ($anchor, $alias, $explicit, $implicit, $class) = ('') x 5;
    ($anchor, $alias, $explicit, $implicit, $preface) = 
      $self->_parse_qualifiers($preface);
    if ($anchor) {
        $self->anchor2node->{$anchor} = CORE::bless [], 'YAML-anchor2node';
    }
    $self->inline('');
    while (length $preface) {
        my $line = $self->line - 1;
        if ($preface =~ s/^($FOLD_CHAR|$LIT_CHAR_RX)(-|\+)?\d*\s*//) { 
            $indicator = $1;
            $chomp = $2 if defined($2);
        }
        else {
            $self->die('YAML_PARSE_ERR_TEXT_AFTER_INDICATOR') if $indicator;
            $self->inline($preface);
            $preface = '';
        }
    }
    if ($alias) {
        $self->die('YAML_PARSE_ERR_NO_ANCHOR', $alias)
          unless defined $self->anchor2node->{$alias};
        if (ref($self->anchor2node->{$alias}) ne 'YAML-anchor2node') {
            $node = $self->anchor2node->{$alias};
        }
        else {
            $node = do {my $sv = "*$alias"};
            push @{$self->anchor2node->{$alias}}, [\$node, $self->line]; 
        }
    }
    elsif (length $self->inline) {
        $node = $self->_parse_inline(1, $implicit, $explicit);
        if (length $self->inline) {
            $self->die('YAML_PARSE_ERR_SINGLE_LINE'); 
        }
    }
    elsif ($indicator eq $LIT_CHAR) {
        $self->{level}++;
        $node = $self->_parse_block($chomp);
        $node = $self->_parse_implicit($node) if $implicit;
        $self->{level}--; 
    }
    elsif ($indicator eq $FOLD_CHAR) {
        $self->{level}++;
        $node = $self->_parse_unfold($chomp);
        $node = $self->_parse_implicit($node) if $implicit;
        $self->{level}--;
    }
    else {
        $self->{level}++;
        $self->offset->[$self->level] ||= 0;
        if ($self->indent == $self->offset->[$self->level]) {
            if ($self->content =~ /^-( |$)/) {
                $node = $self->_parse_seq($anchor);
            }
            elsif ($self->content =~ /(^\?|\:( |$))/) {
                $node = $self->_parse_mapping($anchor);
            }
            elsif ($preface =~ /^\s*$/) {
                $node = $self->_parse_implicit('');
            }
            else {
                $self->die('YAML_PARSE_ERR_BAD_NODE');
            }
        }
        else {
            $node = undef;
        }
        $self->{level}--;
    }
    $#{$self->offset} = $self->level;

    if ($explicit) {
        if ($class) {
            if (not ref $node) {
                my $copy = $node;
                undef $node;
                $node = \$copy;
            }
            CORE::bless $node, $class;
        }
        else {
            $node = $self->_parse_explicit($node, $explicit);
        }
    }
    if ($anchor) {
        if (ref($self->anchor2node->{$anchor}) eq 'YAML-anchor2node') {
            # XXX Can't remember what this code actually does
            for my $ref (@{$self->anchor2node->{$anchor}}) {
                ${$ref->[0]} = $node;
                $self->warn('YAML_LOAD_WARN_UNRESOLVED_ALIAS',
                    $anchor, $ref->[1]);
            }
        }
        $self->anchor2node->{$anchor} = $node;
    }
    return $node;
}

# Preprocess the qualifiers that may be attached to any node.
sub _parse_qualifiers {
    my $self = shift;
    my ($preface) = @_;
    my ($anchor, $alias, $explicit, $implicit, $token) = ('') x 5;
    $self->inline('');
    while ($preface =~ /^[&*!]/) {
        my $line = $self->line - 1;
        if ($preface =~ s/^\!(\S+)\s*//) {
            $self->die('YAML_PARSE_ERR_MANY_EXPLICIT') if $explicit;
            $explicit = $1;
        }
        elsif ($preface =~ s/^\!\s*//) {
            $self->die('YAML_PARSE_ERR_MANY_IMPLICIT') if $implicit;
            $implicit = 1;
        }
        elsif ($preface =~ s/^\&([^ ,:]+)\s*//) {
            $token = $1;
            $self->die('YAML_PARSE_ERR_BAD_ANCHOR') 
              unless $token =~ /^[a-zA-Z0-9]+$/;
            $self->die('YAML_PARSE_ERR_MANY_ANCHOR') if $anchor;
            $self->die('YAML_PARSE_ERR_ANCHOR_ALIAS') if $alias;
            $anchor = $token;
        }
        elsif ($preface =~ s/^\*([^ ,:]+)\s*//) {
            $token = $1;
            $self->die('YAML_PARSE_ERR_BAD_ALIAS')
              unless $token =~ /^[a-zA-Z0-9]+$/;
            $self->die('YAML_PARSE_ERR_MANY_ALIAS') if $alias;
            $self->die('YAML_PARSE_ERR_ANCHOR_ALIAS') if $anchor;
            $alias = $token;
        }
    }
    return ($anchor, $alias, $explicit, $implicit, $preface); 
}

# Morph a node to it's explicit type  
sub _parse_explicit {
    my $self = shift;
    my ($node, $explicit) = @_;
    my ($type, $class);
    if ($explicit =~ /^\!?perl\/(hash|array|ref|scalar)(?:\:(\w(\w|\:\:)*)?)?$/) {
        ($type, $class) = (($1 || ''), ($2 || ''));

        # FIXME # die unless uc($type) eq ref($node) ?

        if ( $type eq "ref" ) {
            $self->die('YAML_LOAD_ERR_NO_DEFAULT_VALUE', 'XXX', $explicit)
            unless exists $node->{VALUE()} and scalar(keys %$node) == 1;

            my $value = $node->{VALUE()};
            $node = \$value;
        }
        
        if ( $type eq "scalar" and length($class) and !ref($node) ) {
            my $value = $node;
            $node = \$value;
        }

        if ( length($class) ) {
            CORE::bless($node, $class);
        }

        return $node;
    }
    if ($explicit =~ m{^!?perl/(glob|regexp|code)(?:\:(\w(\w|\:\:)*)?)?$}) {
        ($type, $class) = (($1 || ''), ($2 || ''));
        my $type_class = "YAML::Type::$type";
        no strict 'refs';
        if ($type_class->can('yaml_load')) {
            return $type_class->yaml_load($node, $class, $self);
        }
        else {
            $self->die('YAML_LOAD_ERR_NO_CONVERT', 'XXX', $explicit);
        }
    }
    # This !perl/@Foo and !perl/$Foo are deprecated but still parsed
    elsif ($YAML::TagClass->{$explicit} ||
           $explicit =~ m{^perl/(\@|\$)?([a-zA-Z](\w|::)+)$}
          ) {
        $class = $YAML::TagClass->{$explicit} || $2;
        if ($class->can('yaml_load')) {
            require YAML::Node;
            return $class->yaml_load(YAML::Node->new($node, $explicit));
        }
        else {
            if (ref $node) {
                return CORE::bless $node, $class;
            }
            else {
                return CORE::bless \$node, $class;
            }
        }
    }
    elsif (ref $node) {
        require YAML::Node;
        return YAML::Node->new($node, $explicit);
    }
    else {
        # XXX This is likely wrong. Failing test:
        # --- !unknown 'scalar value'
        return $node;
    }
}

# Parse a YAML mapping into a Perl hash
sub _parse_mapping {
    my $self = shift;
    my ($anchor) = @_;
    my $mapping = {};
    $self->anchor2node->{$anchor} = $mapping;
    my $key;
    while (not $self->done and $self->indent == $self->offset->[$self->level]) {
        # If structured key:
        if ($self->{content} =~ s/^\?\s*//) {
            $self->preface($self->content);
            $self->_parse_next_line(COLLECTION);
            $key = $self->_parse_node();
            $key = "$key";
        }
        # If "default" key (equals sign) 
        elsif ($self->{content} =~ s/^\=\s*//) {
            $key = VALUE;
        }
        # If "comment" key (slash slash)
        elsif ($self->{content} =~ s/^\=\s*//) {
            $key = COMMENT;
        }
        # Regular scalar key:
        else {
            $self->inline($self->content);
            $key = $self->_parse_inline();
            $key = "$key";
            $self->content($self->inline);
            $self->inline('');
        }
            
        unless ($self->{content} =~ s/^:\s*//) {
            $self->die('YAML_LOAD_ERR_BAD_MAP_ELEMENT');
        }
        $self->preface($self->content);
        my $line = $self->line;
        $self->_parse_next_line(COLLECTION);
        my $value = $self->_parse_node();
        if (exists $mapping->{$key}) {
            $self->warn('YAML_LOAD_WARN_DUPLICATE_KEY');
        }
        else {
            $mapping->{$key} = $value;
        }
    }
    return $mapping;
}

# Parse a YAML sequence into a Perl array
sub _parse_seq {
    my $self = shift;
    my ($anchor) = @_;
    my $seq = [];
    $self->anchor2node->{$anchor} = $seq;
    while (not $self->done and $self->indent == $self->offset->[$self->level]) {
        if ($self->content =~ /^-(?: (.*))?$/) {
            $self->preface(defined($1) ? $1 : '');
        }
        else {
            $self->die('YAML_LOAD_ERR_BAD_SEQ_ELEMENT');
        }
        if ($self->preface =~ /^(\s*)(\w.*\:(?: |$).*)$/) {
            $self->indent($self->offset->[$self->level] + 2 + length($1));
            $self->content($2);
            $self->level($self->level + 1);
            $self->offset->[$self->level] = $self->indent;
            $self->preface('');
            push @$seq, $self->_parse_mapping('');
            $self->{level}--;
            $#{$self->offset} = $self->level;
        }
        else {
            $self->_parse_next_line(COLLECTION);
            push @$seq, $self->_parse_node();
        }
    }
    return $seq;
}

# Parse an inline value. Since YAML supports inline collections, this is
# the top level of a sub parsing.
sub _parse_inline {
    my $self = shift;
    my ($top, $top_implicit, $top_explicit) = (@_, '', '', '');
    $self->{inline} =~ s/^\s*(.*)\s*$/$1/; # OUCH - mugwump
    my ($node, $anchor, $alias, $explicit, $implicit) = ('') x 5;
    ($anchor, $alias, $explicit, $implicit, $self->{inline}) = 
      $self->_parse_qualifiers($self->inline);
    if ($anchor) {
        $self->anchor2node->{$anchor} = CORE::bless [], 'YAML-anchor2node';
    }
    $implicit ||= $top_implicit;
    $explicit ||= $top_explicit;
    ($top_implicit, $top_explicit) = ('', '');
    if ($alias) {
        $self->die('YAML_PARSE_ERR_NO_ANCHOR', $alias)
          unless defined $self->anchor2node->{$alias};
        if (ref($self->anchor2node->{$alias}) ne 'YAML-anchor2node') {
            $node = $self->anchor2node->{$alias};
        }
        else {
            $node = do {my $sv = "*$alias"};
            push @{$self->anchor2node->{$alias}}, [\$node, $self->line]; 
        }
    }
    elsif ($self->inline =~ /^\{/) {
        $node = $self->_parse_inline_mapping($anchor);
    }
    elsif ($self->inline =~ /^\[/) {
        $node = $self->_parse_inline_seq($anchor);
    }
    elsif ($self->inline =~ /^"/) {
        $node = $self->_parse_inline_double_quoted();
        $node = $self->_unescape($node);
        $node = $self->_parse_implicit($node) if $implicit;
    }
    elsif ($self->inline =~ /^'/) {
        $node = $self->_parse_inline_single_quoted();
        $node = $self->_parse_implicit($node) if $implicit;
    }
    else {
        if ($top) {
            $node = $self->inline;
            $self->inline('');
        }
        else {
            $node = $self->_parse_inline_simple();
        }
        $node = $self->_parse_implicit($node) unless $explicit;
    }
    if ($explicit) {
        $node = $self->_parse_explicit($node, $explicit);
    }
    if ($anchor) {
        if (ref($self->anchor2node->{$anchor}) eq 'YAML-anchor2node') {
            for my $ref (@{$self->anchor2node->{$anchor}}) {
                ${$ref->[0]} = $node;
                $self->warn('YAML_LOAD_WARN_UNRESOLVED_ALIAS',
                    $anchor, $ref->[1]);
            }
        }
        $self->anchor2node->{$anchor} = $node;
    }
    return $node;
}

# Parse the inline YAML mapping into a Perl hash
sub _parse_inline_mapping {
    my $self = shift;
    my ($anchor) = @_;
    my $node = {};
    $self->anchor2node->{$anchor} = $node;

    $self->die('YAML_PARSE_ERR_INLINE_MAP')
      unless $self->{inline} =~ s/^\{\s*//;
    while (not $self->{inline} =~ s/^\s*\}//) {
        my $key = $self->_parse_inline();
        $self->die('YAML_PARSE_ERR_INLINE_MAP')
          unless $self->{inline} =~ s/^\: \s*//;
        my $value = $self->_parse_inline();
        if (exists $node->{$key}) {
            $self->warn('YAML_LOAD_WARN_DUPLICATE_KEY');
        }
        else {
            $node->{$key} = $value;
        }
        next if $self->inline =~ /^\s*\}/;
        $self->die('YAML_PARSE_ERR_INLINE_MAP')
          unless $self->{inline} =~ s/^\,\s*//;
    }
    return $node;
}

# Parse the inline YAML sequence into a Perl array
sub _parse_inline_seq {
    my $self = shift;
    my ($anchor) = @_;
    my $node = [];
    $self->anchor2node->{$anchor} = $node;

    $self->die('YAML_PARSE_ERR_INLINE_SEQUENCE')
      unless $self->{inline} =~ s/^\[\s*//;
    while (not $self->{inline} =~ s/^\s*\]//) {
        my $value = $self->_parse_inline();
        push @$node, $value;
        next if $self->inline =~ /^\s*\]/;
        $self->die('YAML_PARSE_ERR_INLINE_SEQUENCE') 
          unless $self->{inline} =~ s/^\,\s*//;
    }
    return $node;
}

# Parse the inline double quoted string.
sub _parse_inline_double_quoted {
    my $self = shift;
    my $node;
    if ($self->inline =~ /^"((?:\\"|[^"])*)"\s*(.*)$/) {
        $node = $1;
        $self->inline($2);
        $node =~ s/\\"/"/g;
    }
    else {
        $self->die('YAML_PARSE_ERR_BAD_DOUBLE');
    }
    return $node;
}


# Parse the inline single quoted string.
sub _parse_inline_single_quoted {
    my $self = shift;
    my $node;
    if ($self->inline =~ /^'((?:''|[^'])*)'\s*(.*)$/) {
        $node = $1;
        $self->inline($2);
        $node =~ s/''/'/g;
    }
    else {
        $self->die('YAML_PARSE_ERR_BAD_SINGLE');
    }
    return $node;
}

# Parse the inline unquoted string and do implicit typing.
sub _parse_inline_simple {
    my $self = shift;
    my $value;
    if ($self->inline =~ /^(|[^!@#%^&*].*?)(?=[\[\]\{\},]|, |: |- |:\s*$|$)/) {
        $value = $1;
        substr($self->{inline}, 0, length($1)) = '';
    }
    else {
        $self->die('YAML_PARSE_ERR_BAD_INLINE_IMPLICIT', $value);
    }
    return $value;
}

sub _parse_implicit {
    my $self = shift;
    my ($value) = @_;
    $value =~ s/\s*$//;
    return $value if $value eq '';
    return undef if $value =~ /^~$/;
    return $value
      unless $value =~ /^[\@\`\^]/ or
             $value =~ /^[\-\?]\s/;
    $self->die('YAML_PARSE_ERR_BAD_IMPLICIT', $value);
}

# Unfold a YAML multiline scalar into a single string.
sub _parse_unfold {
    my $self = shift;
    my ($chomp) = @_;
    my $node = '';
    my $space = 0;
    while (not $self->done and $self->indent == $self->offset->[$self->level]) {
        $node .= $self->content. "\n";
        $self->_parse_next_line(LEAF);
    }
    $node =~ s/^(\S.*)\n(?=\S)/$1 /gm;
    $node =~ s/^(\S.*)\n(\n+\S)/$1$2/gm;
    $node =~ s/\n*\Z// unless $chomp eq '+';
    $node .= "\n" unless $chomp;
    return $node;
}

# Parse a YAML block style scalar. This is like a Perl here-document.
sub _parse_block {
    my $self = shift;
    my ($chomp) = @_;
    my $node = '';
    while (not $self->done and $self->indent == $self->offset->[$self->level]) {
        $node .= $self->content . "\n";
        $self->_parse_next_line(LEAF);
    }
    return $node if '+' eq $chomp;
    $node =~ s/\n*\Z/\n/;
    $node =~ s/\n\Z// if $chomp eq '-';
    return $node;
}

# Handle Perl style '#' comments. Comments must be at the same indentation
# level as the collection line following them.
sub _parse_throwaway_comments {
    my $self = shift;
    while (@{$self->lines} and
           $self->lines->[0] =~ m{^\s*(\#|$)}
          ) {
        shift @{$self->lines};
        $self->{line}++;
    }
    $self->eos($self->{done} = not @{$self->lines});
}

# This is the routine that controls what line is being parsed. It gets called
# once for each line in the YAML stream.
#
# This routine must:
# 1) Skip past the current line
# 2) Determine the indentation offset for a new level
# 3) Find the next _content_ line
#   A) Skip over any throwaways (Comments/blanks)
#   B) Set $self->indent, $self->content, $self->line
# 4) Expand tabs appropriately  
sub _parse_next_line {
    my $self = shift;
    my ($type) = @_;
    my $level = $self->level;
    my $offset = $self->offset->[$level];
    $self->die('YAML_EMIT_ERR_BAD_LEVEL') unless defined $offset;
    shift @{$self->lines};
    $self->eos($self->{done} = not @{$self->lines});
    return if $self->eos;
    $self->{line}++;

    # Determine the offset for a new leaf node
    if ($self->preface =~
        qr/(?:^|\s)(?:$FOLD_CHAR|$LIT_CHAR_RX)(?:-|\+)?(\d*)\s*$/
       ) {
        $self->die('YAML_PARSE_ERR_ZERO_INDENT')
          if length($1) and $1 == 0;
        $type = LEAF;
        if (length($1)) {
            $self->offset->[$level + 1] = $offset + $1;
        }
        else {
            # First get rid of any comments.
            while (@{$self->lines} && ($self->lines->[0] =~ /^\s*#/)) {
                $self->lines->[0] =~ /^( *)/ or die;
                last unless length($1) <= $offset;
                shift @{$self->lines};
                $self->{line}++;
            }
            $self->eos($self->{done} = not @{$self->lines});
            return if $self->eos;
            if ($self->lines->[0] =~ /^( *)\S/ and length($1) > $offset) {
                $self->offset->[$level+1] = length($1);
            }
            else {
                $self->offset->[$level+1] = $offset + 1;
            }
        }
        $offset = $self->offset->[++$level];
    }
    # Determine the offset for a new collection level
    elsif ($type == COLLECTION and 
           $self->preface =~ /^(\s*(\!\S*|\&\S+))*\s*$/) {
        $self->_parse_throwaway_comments();
        if ($self->eos) {
            $self->offset->[$level+1] = $offset + 1;
            return;
        }
        else {
            $self->lines->[0] =~ /^( *)\S/ or die;
            if (length($1) > $offset) {
                $self->offset->[$level+1] = length($1);
            }
            else {
                $self->offset->[$level+1] = $offset + 1;
            }
        }
        $offset = $self->offset->[++$level];
    }
        
    if ($type == LEAF) {
        while (@{$self->lines} and
               $self->lines->[0] =~ m{^( *)(\#)} and
               length($1) < $offset
              ) {
            shift @{$self->lines};
            $self->{line}++;
        }
        $self->eos($self->{done} = not @{$self->lines});
    }
    else {
        $self->_parse_throwaway_comments();
    }
    return if $self->eos; 
    
    if ($self->lines->[0] =~ /^---(\s|$)/) {
        $self->done(1);
        return;
    }
    if ($type == LEAF and 
        $self->lines->[0] =~ /^ {$offset}(.*)$/
       ) {
        $self->indent($offset);
        $self->content($1);
    }
    elsif ($self->lines->[0] =~ /^\s*$/) {
        $self->indent($offset);
        $self->content('');
    }
    else {
        $self->lines->[0] =~ /^( *)(\S.*)$/;
        while ($self->offset->[$level] > length($1)) {
            $level--;
        }
        $self->die('YAML_PARSE_ERR_INCONSISTENT_INDENTATION') 
          if $self->offset->[$level] != length($1);
        $self->indent(length($1));
        $self->content($2);
    }
    $self->die('YAML_PARSE_ERR_INDENTATION')
      if $self->indent - $offset > 1;
}

#==============================================================================
# Utility subroutines.
#==============================================================================

# Printable characters for escapes
my %unescapes = 
  (
   0 => "\x00", a => "\x07", t => "\x09",
   n => "\x0a", v => "\x0b", f => "\x0c",
   r => "\x0d", e => "\x1b", '\\' => '\\',
  );
   
# Transform all the backslash style escape characters to their literal meaning
sub _unescape {
    my $self = shift;
    my ($node) = @_;
    $node =~ s/\\([never\\fart0]|x([0-9a-fA-F]{2}))/
              (length($1)>1)?pack("H2",$2):$unescapes{$1}/gex;
    return $node;
}

1;

__END__

=head1 NAME

YAML::Loader - YAML class for loading Perl objects to YAML

=head1 SYNOPSIS

    use YAML::Loader;
    my $loader = YAML::Loader->new;
    my $hash = $loader->load(<<'...');
    foo: bar
    ...

=head1 DESCRIPTION

YAML::Loader is the module that YAML.pm used to deserialize YAML to Perl
objects. It is fully object oriented and usable on its own.

=head1 AUTHOR

Ingy döt Net <ingy@cpan.org>

=head1 COPYRIGHT

Copyright (c) 2006. Ingy döt Net. All rights reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut
