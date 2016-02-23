package ExtUtils::Constant::Base;

use strict;
use vars qw($VERSION);
use Carp;
use Text::Wrap;
use ExtUtils::Constant::Utils qw(C_stringify perl_stringify);
$VERSION = '0.04';

use constant is_perl56 => ($] < 5.007 && $] > 5.005_50);


=head1 NAME

ExtUtils::Constant::Base - base class for ExtUtils::Constant objects

=head1 SYNOPSIS

    require ExtUtils::Constant::Base;
    @ISA = 'ExtUtils::Constant::Base';

=head1 DESCRIPTION

ExtUtils::Constant::Base provides a base implementation of methods to
generate C code to give fast constant value lookup by named string. Currently
it's mostly used ExtUtils::Constant::XS, which generates the lookup code
for the constant() subroutine found in many XS modules.

=head1 USAGE

ExtUtils::Constant::Base exports no subroutines. The following methods are
available

=over 4

=cut

sub valid_type {
  # Default to assuming that you don't need different types of return data.
  1;
}
sub default_type {
  '';
}

=item header

A method returning a scalar containing definitions needed, typically for a
C header file.

=cut

sub header {
  ''
}

# This might actually be a return statement. Note that you are responsible
# for any space you might need before your value, as it lets to perform
# "tricks" such as "return KEY_" and have strings appended.
sub assignment_clause_for_type;
# In which case this might be an empty string
sub return_statement_for_type {undef};
sub return_statement_for_notdef;
sub return_statement_for_notfound;

# "#if 1" is true to a C pre-processor
sub macro_from_name {
  1;
}

sub macro_from_item {
  1;
}

sub macro_to_ifdef {
    my ($self, $macro) = @_;
    if (ref $macro) {
	return $macro->[0];
    }
    if (defined $macro && $macro ne "" && $macro ne "1") {
	return $macro ? "#ifdef $macro\n" : "#if 0\n";
    }
    return "";
}

sub macro_to_endif {
    my ($self, $macro) = @_;

    if (ref $macro) {
	return $macro->[1];
    }
    if (defined $macro && $macro ne "" && $macro ne "1") {
	return "#endif\n";
    }
    return "";
}

sub name_param {
  'name';
}

# This is possibly buggy, in that it's not mandatory (below, in the main
# C_constant parameters, but is expected to exist here, if it's needed)
# Buggy because if you're definitely pure 8 bit only, and will never be
# presented with your constants in utf8, the default form of C_constant can't
# be told not to do the utf8 version.

sub is_utf8_param {
  'utf8';
}

sub memEQ {
  "!memcmp";
}

=item memEQ_clause args_hashref

A method to return a suitable C C<if> statement to check whether I<name>
is equal to the C variable C<name>. If I<checked_at> is defined, then it
is used to avoid C<memEQ> for short names, or to generate a comment to
highlight the position of the character in the C<switch> statement.

If i<checked_at> is a reference to a scalar, then instead it gives
the characters pre-checked at the beginning, (and the number of chars by
which the C variable name has been advanced. These need to be chopped from
the front of I<name>).

=cut

sub memEQ_clause {
#    if (memEQ(name, "thingy", 6)) {
  # Which could actually be a character comparison or even ""
  my ($self, $args) = @_;
  my ($name, $checked_at, $indent) = @{$args}{qw(name checked_at indent)};
  $indent = ' ' x ($indent || 4);
  my $front_chop;
  if (ref $checked_at) {
    # regexp won't work on 5.6.1 without use utf8; in turn that won't work
    # on 5.005_03.
    substr ($name, 0, length $$checked_at,) = '';
    $front_chop = C_stringify ($$checked_at);
    undef $checked_at;
  }
  my $len = length $name;

  if ($len < 2) {
    return $indent . "{\n"
	if (defined $checked_at and $checked_at == 0) or $len == 0;
    # We didn't switch, drop through to the code for the 2 character string
    $checked_at = 1;
  }

  my $name_param = $self->name_param;

  if ($len < 3 and defined $checked_at) {
    my $check;
    if ($checked_at == 1) {
      $check = 0;
    } elsif ($checked_at == 0) {
      $check = 1;
    }
    if (defined $check) {
      my $char = C_stringify (substr $name, $check, 1);
      # Placate 5.005 with a break in the string. I can't see a good way of
      # getting it to not take [ as introducing an array lookup, even with
      # ${name_param}[$check]
      return $indent . "if ($name_param" . "[$check] == '$char') {\n";
    }
  }
  if (($len == 2 and !defined $checked_at)
     or ($len == 3 and defined ($checked_at) and $checked_at == 2)) {
    my $char1 = C_stringify (substr $name, 0, 1);
    my $char2 = C_stringify (substr $name, 1, 1);
    return $indent .
      "if ($name_param" . "[0] == '$char1' && $name_param" . "[1] == '$char2') {\n";
  }
  if (($len == 3 and defined ($checked_at) and $checked_at == 1)) {
    my $char1 = C_stringify (substr $name, 0, 1);
    my $char2 = C_stringify (substr $name, 2, 1);
    return $indent .
      "if ($name_param" . "[0] == '$char1' && $name_param" . "[2] == '$char2') {\n";
  }

  my $pointer = '^';
  my $have_checked_last = defined ($checked_at) && $len == $checked_at + 1;
  if ($have_checked_last) {
    # Checked at the last character, so no need to memEQ it.
    $pointer = C_stringify (chop $name);
    $len--;
  }

  $name = C_stringify ($name);
  my $memEQ = $self->memEQ();
  my $body = $indent . "if ($memEQ($name_param, \"$name\", $len)) {\n";
  # Put a little ^ under the letter we checked at
  # Screws up for non printable and non-7 bit stuff, but that's too hard to
  # get right.
  if (defined $checked_at) {
    $body .= $indent . "/*      " . (' ' x length $memEQ)
      . (' ' x length $name_param)
      . (' ' x $checked_at) . $pointer
      . (' ' x ($len - $checked_at + length $len)) . "    */\n";
  } elsif (defined $front_chop) {
    $body .= $indent . "/*                $front_chop"
      . (' ' x ($len + 1 + length $len)) . "    */\n";
  }
  return $body;
}

=item dump_names arg_hashref, ITEM...

An internal function to generate the embedded perl code that will regenerate
the constant subroutines.  I<default_type>, I<types> and I<ITEM>s are the
same as for C_constant.  I<indent> is treated as number of spaces to indent
by.  If C<declare_types> is true a C<$types> is always declared in the perl
code generated, if defined and false never declared, and if undefined C<$types>
is only declared if the values in I<types> as passed in cannot be inferred from
I<default_types> and the I<ITEM>s.

=cut

sub dump_names {
  my ($self, $args, @items) = @_;
  my ($default_type, $what, $indent, $declare_types)
    = @{$args}{qw(default_type what indent declare_types)};
  $indent = ' ' x ($indent || 0);

  my $result;
  my (@simple, @complex, %used_types);
  foreach (@items) {
    my $type;
    if (ref $_) {
      $type = $_->{type} || $default_type;
      if ($_->{utf8}) {
        # For simplicity always skip the bytes case, and reconstitute this entry
        # from its utf8 twin.
        next if $_->{utf8} eq 'no';
        # Copy the hashref, as we don't want to mess with the caller's hashref.
        $_ = {%$_};
        unless (is_perl56) {
          utf8::decode ($_->{name});
        } else {
          $_->{name} = pack 'U*', unpack 'U0U*', $_->{name};
        }
        delete $_->{utf8};
      }
    } else {
      $_ = {name=>$_};
      $type = $default_type;
    }
    $used_types{$type}++;
    if ($type eq $default_type
        # grr 5.6.1
        and length $_->{name}
        and length $_->{name} == ($_->{name} =~ tr/A-Za-z0-9_//)
        and !defined ($_->{macro}) and !defined ($_->{value})
        and !defined ($_->{default}) and !defined ($_->{pre})
        and !defined ($_->{post}) and !defined ($_->{def_pre})
        and !defined ($_->{def_post}) and !defined ($_->{weight})) {
      # It's the default type, and the name consists only of A-Za-z0-9_
      push @simple, $_->{name};
    } else {
      push @complex, $_;
    }
  }

  if (!defined $declare_types) {
    # Do they pass in any types we weren't already using?
    foreach (keys %$what) {
      next if $used_types{$_};
      $declare_types++; # Found one in $what that wasn't used.
      last; # And one is enough to terminate this loop
    }
  }
  if ($declare_types) {
    $result = $indent . 'my $types = {map {($_, 1)} qw('
      . join (" ", sort keys %$what) . ")};\n";
  }
  local $Text::Wrap::huge = 'overflow';
  local $Text::Wrap::columns = 80;
  $result .= wrap ($indent . "my \@names = (qw(",
		   $indent . "               ", join (" ", sort @simple) . ")");
  if (@complex) {
    foreach my $item (sort {$a->{name} cmp $b->{name}} @complex) {
      my $name = perl_stringify $item->{name};
      my $line = ",\n$indent            {name=>\"$name\"";
      $line .= ", type=>\"$item->{type}\"" if defined $item->{type};
      foreach my $thing (qw (macro value default pre post def_pre def_post)) {
        my $value = $item->{$thing};
        if (defined $value) {
          if (ref $value) {
            $line .= ", $thing=>[\""
              . join ('", "', map {perl_stringify $_} @$value) . '"]';
          } else {
            $line .= ", $thing=>\"" . perl_stringify($value) . "\"";
          }
        }
      }
      $line .= "}";
      # Ensure that the enclosing C comment doesn't end
      # by turning */  into *" . "/
      $line =~ s!\*\/!\*" . "/!gs;
      # gcc -Wall doesn't like finding /* inside a comment
      $line =~ s!\/\*!/" . "\*!gs;
      $result .= $line;
    }
  }
  $result .= ");\n";

  $result;
}

=item assign arg_hashref, VALUE...

A method to return a suitable assignment clause. If I<type> is aggregate
(eg I<PVN> expects both pointer and length) then there should be multiple
I<VALUE>s for the components. I<pre> and I<post> if defined give snippets
of C code to proceed and follow the assignment. I<pre> will be at the start
of a block, so variables may be defined in it.

=cut
# Hmm. value undef to to NOTDEF? value () to do NOTFOUND?

sub assign {
  my $self = shift;
  my $args = shift;
  my ($indent, $type, $pre, $post, $item)
      = @{$args}{qw(indent type pre post item)};
  $post ||= '';
  my $clause;
  my $close;
  if ($pre) {
    chomp $pre;
    $close = "$indent}\n";
    $clause = $indent . "{\n";
    $indent .= "  ";
    $clause .= "$indent$pre";
    $clause .= ";" unless $pre =~ /;$/;
    $clause .= "\n";
  }
  confess "undef \$type" unless defined $type;
  confess "Can't generate code for type $type"
    unless $self->valid_type($type);

  $clause .= join '', map {"$indent$_\n"}
    $self->assignment_clause_for_type({type=>$type,item=>$item}, @_);
  chomp $post;
  if (length $post) {
    $clause .= "$post";
    $clause .= ";" unless $post =~ /;$/;
    $clause .= "\n";
  }
  my $return = $self->return_statement_for_type($type);
  $clause .= "$indent$return\n" if defined $return;
  $clause .= $close if $close;
  return $clause;
}

=item return_clause arg_hashref, ITEM

A method to return a suitable C<#ifdef> clause. I<ITEM> is a hashref
(as passed to C<C_constant> and C<match_clause>. I<indent> is the number
of spaces to indent, defaulting to 6.

=cut

sub return_clause {

##ifdef thingy
#      *iv_return = thingy;
#      return PERL_constant_ISIV;
##else
#      return PERL_constant_NOTDEF;
##endif
  my ($self, $args, $item) = @_;
  my $indent = $args->{indent};

  my ($name, $value, $default, $pre, $post, $def_pre, $def_post, $type)
    = @$item{qw (name value default pre post def_pre def_post type)};
  $value = $name unless defined $value;
  my $macro = $self->macro_from_item($item);
  $indent = ' ' x ($indent || 6);
  unless (defined $type) {
    # use Data::Dumper; print STDERR Dumper ($item);
    confess "undef \$type";
  }

  ##ifdef thingy
  my $clause = $self->macro_to_ifdef($macro);

  #      *iv_return = thingy;
  #      return PERL_constant_ISIV;
  $clause
    .= $self->assign ({indent=>$indent, type=>$type, pre=>$pre, post=>$post,
		       item=>$item}, ref $value ? @$value : $value);

  if (defined $macro && $macro ne "" && $macro ne "1") {
    ##else
    $clause .= "#else\n";

    #      return PERL_constant_NOTDEF;
    if (!defined $default) {
      my $notdef = $self->return_statement_for_notdef();
      $clause .= "$indent$notdef\n" if defined $notdef;
    } else {
      my @default = ref $default ? @$default : $default;
      $type = shift @default;
      $clause .= $self->assign ({indent=>$indent, type=>$type, pre=>$pre,
				 post=>$post, item=>$item}, @default);
    }
  }
  ##endif
  $clause .= $self->macro_to_endif($macro);

  return $clause;
}

sub match_clause {
  # $offset defined if we have checked an offset.
  my ($self, $args, $item) = @_;
  my ($offset, $indent) = @{$args}{qw(checked_at indent)};
  $indent = ' ' x ($indent || 4);
  my $body = '';
  my ($no, $yes, $either, $name, $inner_indent);
  if (ref $item eq 'ARRAY') {
    ($yes, $no) = @$item;
    $either = $yes || $no;
    confess "$item is $either expecting hashref in [0] || [1]"
      unless ref $either eq 'HASH';
    $name = $either->{name};
  } else {
    confess "$item->{name} has utf8 flag '$item->{utf8}', should be false"
      if $item->{utf8};
    $name = $item->{name};
    $inner_indent = $indent;
  }

  $body .= $self->memEQ_clause ({name => $name, checked_at => $offset,
				 indent => length $indent});
  # If we've been presented with an arrayref for $item, then the user string
  # contains in the range 128-255, and we need to check whether it was utf8
  # (or not).
  # In the worst case we have two named constants, where one's name happens
  # encoded in UTF8 happens to be the same byte sequence as the second's
  # encoded in (say) ISO-8859-1.
  # In this case, $yes and $no both have item hashrefs.
  if ($yes) {
    $body .= $indent . "  if (" . $self->is_utf8_param . ") {\n";
  } elsif ($no) {
    $body .= $indent . "  if (!" . $self->is_utf8_param . ") {\n";
  }
  if ($either) {
    $body .= $self->return_clause ({indent=>4 + length $indent}, $either);
    if ($yes and $no) {
      $body .= $indent . "  } else {\n";
      $body .= $self->return_clause ({indent=>4 + length $indent}, $no);
    }
    $body .= $indent . "  }\n";
  } else {
    $body .= $self->return_clause ({indent=>2 + length $indent}, $item);
  }
  $body .= $indent . "}\n";
}


=item switch_clause arg_hashref, NAMELEN, ITEMHASH, ITEM...

An internal method to generate a suitable C<switch> clause, called by
C<C_constant> I<ITEM>s are in the hash ref format as given in the description
of C<C_constant>, and must all have the names of the same length, given by
I<NAMELEN>.  I<ITEMHASH> is a reference to a hash, keyed by name, values being
the hashrefs in the I<ITEM> list.  (No parameters are modified, and there can
be keys in the I<ITEMHASH> that are not in the list of I<ITEM>s without
causing problems - the hash is passed in to save generating it afresh for
each call).

=cut

sub switch_clause {
  my ($self, $args, $namelen, $items, @items) = @_;
  my ($indent, $comment) = @{$args}{qw(indent comment)};
  $indent = ' ' x ($indent || 2);

  local $Text::Wrap::huge = 'overflow';
  local $Text::Wrap::columns = 80;

  my @names = sort map {$_->{name}} @items;
  my $leader = $indent . '/* ';
  my $follower = ' ' x length $leader;
  my $body = $indent . "/* Names all of length $namelen.  */\n";
  if (defined $comment) {
    $body = wrap ($leader, $follower, $comment) . "\n";
    $leader = $follower;
  }
  my @safe_names = @names;
  foreach (@safe_names) {
    confess sprintf "Name '$_' is length %d, not $namelen", length
      unless length == $namelen;
    # Argh. 5.6.1
    # next unless tr/A-Za-z0-9_//c;
    next if tr/A-Za-z0-9_// == length;
    $_ = '"' . perl_stringify ($_) . '"';
    # Ensure that the enclosing C comment doesn't end
    # by turning */  into *" . "/
    s!\*\/!\*"."/!gs;
    # gcc -Wall doesn't like finding /* inside a comment
    s!\/\*!/"."\*!gs;
  }
  $body .= wrap ($leader, $follower, join (" ", @safe_names) . " */") . "\n";
  # Figure out what to switch on.
  # (RMS, Spread of jump table, Position, Hashref)
  my @best = (1e38, ~0);
  # Prefer the last character over the others. (As it lets us shorten the
  # memEQ clause at no cost).
  foreach my $i ($namelen - 1, 0 .. ($namelen - 2)) {
    my ($min, $max) = (~0, 0);
    my %spread;
    if (is_perl56) {
      # Need proper Unicode preserving hash keys for bytes in range 128-255
      # here too, for some reason. grr 5.6.1 yet again.
      tie %spread, 'ExtUtils::Constant::Aaargh56Hash';
    }
    foreach (@names) {
      my $char = substr $_, $i, 1;
      my $ord = ord $char;
      confess "char $ord is out of range" if $ord > 255;
      $max = $ord if $ord > $max;
      $min = $ord if $ord < $min;
      push @{$spread{$char}}, $_;
      # warn "$_ $char";
    }
    # I'm going to pick the character to split on that minimises the root
    # mean square of the number of names in each case. Normally this should
    # be the one with the most keys, but it may pick a 7 where the 8 has
    # one long linear search. I'm not sure if RMS or just sum of squares is
    # actually better.
    # $max and $min are for the tie-breaker if the root mean squares match.
    # Assuming that the compiler may be building a jump table for the
    # switch() then try to minimise the size of that jump table.
    # Finally use < not <= so that if it still ties the earliest part of
    # the string wins. Because if that passes but the memEQ fails, it may
    # only need the start of the string to bin the choice.
    # I think. But I'm micro-optimising. :-)
    # OK. Trump that. Now favour the last character of the string, before the
    # rest.
    my $ss;
    $ss += @$_ * @$_ foreach values %spread;
    my $rms = sqrt ($ss / keys %spread);
    if ($rms < $best[0] || ($rms == $best[0] && ($max - $min) < $best[1])) {
      @best = ($rms, $max - $min, $i, \%spread);
    }
  }
  confess "Internal error. Failed to pick a switch point for @names"
    unless defined $best[2];
  # use Data::Dumper; print Dumper (@best);
  my ($offset, $best) = @best[2,3];
  $body .= $indent . "/* Offset $offset gives the best switch position.  */\n";

  my $do_front_chop = $offset == 0 && $namelen > 2;
  if ($do_front_chop) {
    $body .= $indent . "switch (*" . $self->name_param() . "++) {\n";
  } else {
    $body .= $indent . "switch (" . $self->name_param() . "[$offset]) {\n";
  }
  foreach my $char (sort keys %$best) {
    confess sprintf "'$char' is %d bytes long, not 1", length $char
      if length ($char) != 1;
    confess sprintf "char %#X is out of range", ord $char if ord ($char) > 255;
    $body .= $indent . "case '" . C_stringify ($char) . "':\n";
    foreach my $thisone (sort {
	# Deal with the case of an item actually being an array ref to 1 or 2
	# hashrefs. Don't assign to $a or $b, as they're aliases to the orignal
	my $l = ref $a eq 'ARRAY' ? ($a->[0] || $->[1]) : $a;
	my $r = ref $b eq 'ARRAY' ? ($b->[0] || $->[1]) : $b;
	# Sort by weight first
	($r->{weight} || 0) <=> ($l->{weight} || 0)
	    # Sort equal weights by name
	    or $l->{name} cmp $r->{name}}
			 # If this looks evil, maybe it is.  $items is a
			 # hashref, and we're doing a hash slice on it
			 @{$items}{@{$best->{$char}}}) {
      # warn "You are here";
      if ($do_front_chop) {
        $body .= $self->match_clause ({indent => 2 + length $indent,
				       checked_at => \$char}, $thisone);
      } else {
        $body .= $self->match_clause ({indent => 2 + length $indent,
				       checked_at => $offset}, $thisone);
      }
    }
    $body .= $indent . "  break;\n";
  }
  $body .= $indent . "}\n";
  return $body;
}

sub C_constant_return_type {
  "static int";
}

sub C_constant_prefix_param {
  '';
}

sub C_constant_prefix_param_defintion {
  '';
}

sub name_param_definition {
  "const char *" . $_[0]->name_param;
}

sub namelen_param {
  'len';
}

sub namelen_param_definition {
  'size_t ' . $_[0]->namelen_param;
}

sub C_constant_other_params {
  '';
}

sub C_constant_other_params_defintion {
  '';
}

=item params WHAT

An "internal" method, subject to change, currently called to allow an
overriding class to cache information that will then be passed into all
the C<*param*> calls. (Yes, having to read the source to make sense of this is
considered a known bug). I<WHAT> is be a hashref of types the constant
function will return. In ExtUtils::Constant::XS this method is used to
returns a hashref keyed IV NV PV SV to show which combination of pointers will
be needed in the C argument list generated by
C_constant_other_params_definition and C_constant_other_params

=cut

sub params {
  '';
}


=item dogfood arg_hashref, ITEM...

An internal function to generate the embedded perl code that will regenerate
the constant subroutines.  Parameters are the same as for C_constant.

Currently the base class does nothing and returns an empty string.

=cut

sub dogfood {
  ''
}

=item normalise_items args, default_type, seen_types, seen_items, ITEM...

Convert the items to a normalised form. For 8 bit and Unicode values converts
the item to an array of 1 or 2 items, both 8 bit and UTF-8 encoded.

=cut

sub normalise_items
{
    my $self = shift;
    my $args = shift;
    my $default_type = shift;
    my $what = shift;
    my $items = shift;
    my @new_items;
    foreach my $orig (@_) {
	my ($name, $item);
      if (ref $orig) {
        # Make a copy which is a normalised version of the ref passed in.
        $name = $orig->{name};
        my ($type, $macro, $value) = @$orig{qw (type macro value)};
        $type ||= $default_type;
        $what->{$type} = 1;
        $item = {name=>$name, type=>$type};

        undef $macro if defined $macro and $macro eq $name;
        $item->{macro} = $macro if defined $macro;
        undef $value if defined $value and $value eq $name;
        $item->{value} = $value if defined $value;
        foreach my $key (qw(default pre post def_pre def_post weight
			    not_constant)) {
          my $value = $orig->{$key};
          $item->{$key} = $value if defined $value;
          # warn "$key $value";
        }
      } else {
        $name = $orig;
        $item = {name=>$name, type=>$default_type};
        $what->{$default_type} = 1;
      }
      warn +(ref ($self) || $self)
	. "doesn't know how to handle values of type $_ used in macro $name"
	  unless $self->valid_type ($item->{type});
      # tr///c is broken on 5.6.1 for utf8, so my original tr/\0-\177//c
      # doesn't work. Upgrade to 5.8
      # if ($name !~ tr/\0-\177//c || $] < 5.005_50) {
      if ($name =~ tr/\0-\177// == length $name || $] < 5.005_50
	 || $args->{disable_utf8_duplication}) {
        # No characters outside 7 bit ASCII.
        if (exists $items->{$name}) {
          die "Multiple definitions for macro $name";
        }
        $items->{$name} = $item;
      } else {
        # No characters outside 8 bit. This is hardest.
        if (exists $items->{$name} and ref $items->{$name} ne 'ARRAY') {
          confess "Unexpected ASCII definition for macro $name";
        }
        # Again, 5.6.1 tr broken, so s/5\.6.*/5\.8\.0/;
        # if ($name !~ tr/\0-\377//c) {
        if ($name =~ tr/\0-\377// == length $name) {
#          if ($] < 5.007) {
#            $name = pack "C*", unpack "U*", $name;
#          }
          $item->{utf8} = 'no';
          $items->{$name}[1] = $item;
          push @new_items, $item;
          # Copy item, to create the utf8 variant.
          $item = {%$item};
        }
        # Encode the name as utf8 bytes.
        unless (is_perl56) {
          utf8::encode($name);
        } else {
#          warn "Was >$name< " . length ${name};
          $name = pack 'C*', unpack 'C*', $name . pack 'U*';
#          warn "Now '${name}' " . length ${name};
        }
        if ($items->{$name}[0]) {
          die "Multiple definitions for macro $name";
        }
        $item->{utf8} = 'yes';
        $item->{name} = $name;
        $items->{$name}[0] = $item;
        # We have need for the utf8 flag.
        $what->{''} = 1;
      }
      push @new_items, $item;
    }
    @new_items;
}

=item C_constant arg_hashref, ITEM...

A function that returns a B<list> of C subroutine definitions that return
the value and type of constants when passed the name by the XS wrapper.
I<ITEM...> gives a list of constant names. Each can either be a string,
which is taken as a C macro name, or a reference to a hash with the following
keys

=over 8

=item name

The name of the constant, as seen by the perl code.

=item type

The type of the constant (I<IV>, I<NV> etc)

=item value

A C expression for the value of the constant, or a list of C expressions if
the type is aggregate. This defaults to the I<name> if not given.

=item macro

The C pre-processor macro to use in the C<#ifdef>. This defaults to the
I<name>, and is mainly used if I<value> is an C<enum>. If a reference an
array is passed then the first element is used in place of the C<#ifdef>
line, and the second element in place of the C<#endif>. This allows
pre-processor constructions such as

    #if defined (foo)
    #if !defined (bar)
    ...
    #endif
    #endif

to be used to determine if a constant is to be defined.

A "macro" 1 signals that the constant is always defined, so the C<#if>/C<#endif>
test is omitted.

=item default

Default value to use (instead of C<croak>ing with "your vendor has not
defined...") to return if the macro isn't defined. Specify a reference to
an array with type followed by value(s).

=item pre

C code to use before the assignment of the value of the constant. This allows
you to use temporary variables to extract a value from part of a C<struct>
and return this as I<value>. This C code is places at the start of a block,
so you can declare variables in it.

=item post

C code to place between the assignment of value (to a temporary) and the
return from the function. This allows you to clear up anything in I<pre>.
Rarely needed.

=item def_pre

=item def_post

Equivalents of I<pre> and I<post> for the default value.

=item utf8

Generated internally. Is zero or undefined if name is 7 bit ASCII,
"no" if the name is 8 bit (and so should only match if SvUTF8() is false),
"yes" if the name is utf8 encoded.

The internals automatically clone any name with characters 128-255 but none
256+ (ie one that could be either in bytes or utf8) into a second entry
which is utf8 encoded.

=item weight

Optional sorting weight for names, to determine the order of
linear testing when multiple names fall in the same case of a switch clause.
Higher comes earlier, undefined defaults to zero.

=back

In the argument hashref, I<package> is the name of the package, and is only
used in comments inside the generated C code. I<subname> defaults to
C<constant> if undefined.

I<default_type> is the type returned by C<ITEM>s that don't specify their
type. It defaults to the value of C<default_type()>. I<types> should be given
either as a comma separated list of types that the C subroutine I<subname>
will generate or as a reference to a hash. I<default_type> will be added to
the list if not present, as will any types given in the list of I<ITEM>s. The
resultant list should be the same list of types that C<XS_constant> is
given. [Otherwise C<XS_constant> and C<C_constant> may differ in the number of
parameters to the constant function. I<indent> is currently unused and
ignored. In future it may be used to pass in information used to change the C
indentation style used.]  The best way to maintain consistency is to pass in a
hash reference and let this function update it.

I<breakout> governs when child functions of I<subname> are generated.  If there
are I<breakout> or more I<ITEM>s with the same length of name, then the code
to switch between them is placed into a function named I<subname>_I<len>, for
example C<constant_5> for names 5 characters long.  The default I<breakout> is
3.  A single C<ITEM> is always inlined.

=cut

# The parameter now BREAKOUT was previously documented as:
#
# I<NAMELEN> if defined signals that all the I<name>s of the I<ITEM>s are of
# this length, and that the constant name passed in by perl is checked and
# also of this length. It is used during recursion, and should be C<undef>
# unless the caller has checked all the lengths during code generation, and
# the generated subroutine is only to be called with a name of this length.
#
# As you can see it now performs this function during recursion by being a
# scalar reference.

sub C_constant {
  my ($self, $args, @items) = @_;
  my ($package, $subname, $default_type, $what, $indent, $breakout) =
    @{$args}{qw(package subname default_type types indent breakout)};
  $package ||= 'Foo';
  $subname ||= 'constant';
  # I'm not using this. But a hashref could be used for full formatting without
  # breaking this API
  # $indent ||= 0;

  my ($namelen, $items);
  if (ref $breakout) {
    # We are called recursively. We trust @items to be normalised, $what to
    # be a hashref, and pinch %$items from our parent to save recalculation.
    ($namelen, $items) = @$breakout;
  } else {
    $items = {};
    if (is_perl56) {
      # Need proper Unicode preserving hash keys.
      require ExtUtils::Constant::Aaargh56Hash;
      tie %$items, 'ExtUtils::Constant::Aaargh56Hash';
    }
    $breakout ||= 3;
    $default_type ||= $self->default_type();
    if (!ref $what) {
      # Convert line of the form IV,UV,NV to hash
      $what = {map {$_ => 1} split /,\s*/, ($what || '')};
      # Figure out what types we're dealing with, and assign all unknowns to the
      # default type
    }
    @items = $self->normalise_items ({}, $default_type, $what, $items, @items);
    # use Data::Dumper; print Dumper @items;
  }
  my $params = $self->params ($what);

  # Probably "static int"
  my ($body, @subs);
  $body = $self->C_constant_return_type($params) . "\n$subname ("
    # Eg "pTHX_ "
    . $self->C_constant_prefix_param_defintion($params)
      # Probably "const char *name"
      . $self->name_param_definition($params);
  # Something like ", STRLEN len"
  $body .= ", " . $self->namelen_param_definition($params)
    unless defined $namelen;
  $body .= $self->C_constant_other_params_defintion($params);
  $body .= ") {\n";

  if (defined $namelen) {
    # We are a child subroutine. Print the simple description
    my $comment = 'When generated this function returned values for the list'
      . ' of names given here.  However, subsequent manual editing may have'
        . ' added or removed some.';
    $body .= $self->switch_clause ({indent=>2, comment=>$comment},
				   $namelen, $items, @items);
  } else {
    # We are the top level.
    $body .= "  /* Initially switch on the length of the name.  */\n";
    $body .= $self->dogfood ({package => $package, subname => $subname,
			      default_type => $default_type, what => $what,
			      indent => $indent, breakout => $breakout},
			     @items);
    $body .= '  switch ('.$self->namelen_param().") {\n";
    # Need to group names of the same length
    my @by_length;
    foreach (@items) {
      push @{$by_length[length $_->{name}]}, $_;
    }
    foreach my $i (0 .. $#by_length) {
      next unless $by_length[$i];	# None of this length
      $body .= "  case $i:\n";
      if (@{$by_length[$i]} == 1) {
        my $only_thing = $by_length[$i]->[0];
        if ($only_thing->{utf8}) {
          if ($only_thing->{utf8} eq 'yes') {
            # With utf8 on flag item is passed in element 0
            $body .= $self->match_clause (undef, [$only_thing]);
          } else {
            # With utf8 off flag item is passed in element 1
            $body .= $self->match_clause (undef, [undef, $only_thing]);
          }
        } else {
          $body .= $self->match_clause (undef, $only_thing);
        }
      } elsif (@{$by_length[$i]} < $breakout) {
        $body .= $self->switch_clause ({indent=>4},
				       $i, $items, @{$by_length[$i]});
      } else {
        # Only use the minimal set of parameters actually needed by the types
        # of the names of this length.
        my $what = {};
        foreach (@{$by_length[$i]}) {
          $what->{$_->{type}} = 1;
          $what->{''} = 1 if $_->{utf8};
        }
        $params = $self->params ($what);
        push @subs, $self->C_constant ({package=>$package,
					subname=>"${subname}_$i",
					default_type => $default_type,
					types => $what, indent => $indent,
					breakout => [$i, $items]},
				       @{$by_length[$i]});
        $body .= "    return ${subname}_$i ("
	  # Eg "aTHX_ "
	  . $self->C_constant_prefix_param($params)
	    # Probably "name"
	    . $self->name_param($params);
	$body .= $self->C_constant_other_params($params);
        $body .= ");\n";
      }
      $body .= "    break;\n";
    }
    $body .= "  }\n";
  }
  my $notfound = $self->return_statement_for_notfound();
  $body .= "  $notfound\n" if $notfound;
  $body .= "}\n";
  return (@subs, $body);
}

1;
__END__

=back

=head1 BUGS

Not everything is documented yet.

Probably others.

=head1 AUTHOR

Nicholas Clark <nick@ccl4.org> based on the code in C<h2xs> by Larry Wall and
others
