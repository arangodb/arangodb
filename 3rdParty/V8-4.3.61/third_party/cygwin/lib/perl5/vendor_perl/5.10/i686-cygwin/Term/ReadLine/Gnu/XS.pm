#!/usr/local/bin/perl
#
#	XS.pm : perl function definition for Term::ReadLine::Gnu
#
#	$Id: XS.pm,v 1.23 2008-02-08 00:43:46+09 hiroo Exp $
#
#	Copyright (c) 2008 Hiroo Hayashi.  All rights reserved.
#
#	This program is free software; you can redistribute it and/or
#	modify it under the same terms as Perl itself.

package Term::ReadLine::Gnu::XS;

use Carp;
use strict;
use AutoLoader 'AUTOLOAD';

use vars qw($VERSION);
$VERSION='1.17';	# added for CPAN

# make aliases
use vars qw(%Attribs);
*Attribs = \%Term::ReadLine::Gnu::Attribs;

use vars qw(*read_history);
*read_history = \&read_history_range;

# alias for 8 characters limitation imposed by AutoSplit
use vars qw(*rl_unbind_key *rl_unbind_function *rl_unbind_command
	    *history_list *history_arg_extract);
*rl_unbind_key = \&unbind_key;
*rl_unbind_function = \&unbind_function;
*rl_unbind_command = \&unbind_command;
*history_list = \&hist_list;
*history_arg_extract = \&hist_arg_extract;

# For backward compatibility.  Using these name (*_in_map) is deprecated.
use vars qw(*rl_unbind_function_in_map *rl_unbind_command_in_map);
*rl_unbind_function_in_map = \&unbind_function;
*rl_unbind_command_in_map  = \&unbind_command;

rl_add_defun('history-expand-line',	 \&history_expand_line);
# bind operate-and-get-next to \C-o by default for the compatibility
# with bash and Term::ReadLine::Perl
rl_add_defun('operate-and-get-next',	 \&operate_and_get_next, ord "\co");
rl_add_defun('display-readline-version', \&display_readline_version);
rl_add_defun('change-ornaments',	 \&change_ornaments);

# for ornaments()

# Prompt-start, prompt-end, command-line-start, command-line-end
#     -- zero-width beautifies to emit around prompt and the command line.
# string encoded:
my $rl_term_set = ',,,';

# These variables are used by completion functions.  Don't use for
# other purpose.
my $_i;
my @_matches;
my @_tstrs;
my $_tstrs_init = 0;

1;

# Uncomment the following line to enable AutoSplit.  If you are using
# AutoLoader.pm distributed with Perl 5.004 or earlier, you must
# update AutoLoader.pm due to its bug.

#__END__


#
#	Readline Library function wrappers
#

# Convert keymap name to Keymap if the argument is not reference to Keymap
sub _str2map ($) {
    return ref $_[0] ? $_[0]
	: (rl_get_keymap_by_name($_[0]) || carp "unknown keymap name \`$_[0]\'\n");
}

# Convert function name to Function if the argument is not reference
# to Function
sub _str2fn ($) {
    return ref $_[0] ? $_[0]
	: (rl_named_function($_[0]) || carp "unknown function name \`$_[0]\'\n");
}

sub rl_copy_keymap ($)    { return _rl_copy_keymap(_str2map($_[0])); }
sub rl_discard_keymap ($) { return _rl_discard_keymap(_str2map($_[0])); }
sub rl_set_keymap ($)     { return _rl_set_keymap(_str2map($_[0])); }

# rl_bind_key
sub rl_bind_key ($$;$) {
    if (defined $_[2]) {
	return _rl_bind_key($_[0], _str2fn($_[1]), _str2map($_[2]));
    } else {
	return _rl_bind_key($_[0], _str2fn($_[1]));
    }
}

# rl_bind_key_if_unbound
sub rl_bind_key_if_unbound ($$;$) {
    my ($version) = $Attribs{library_version}
	=~ /(\d+\.\d+)/;
    if ($version < 5.0) {
	carp "rl_bind_key_if_unbound() is not supported.  Ignored\n";
	return;
    }
    if (defined $_[2]) {
	return _rl_bind_key_if_unbound($_[0], _str2fn($_[1]), _str2map($_[2]));
    } else {
	return _rl_bind_key_if_unbound($_[0], _str2fn($_[1]));
    }
}

# rl_unbind_key
sub unbind_key ($;$) {
    if (defined $_[1]) {
	return _rl_unbind_key($_[0], _str2map($_[1]));
    } else {
	return _rl_unbind_key($_[0]);
    }
}

# rl_unbind_function
sub unbind_function ($;$) {
    # libreadline.* in Debian GNU/Linux 2.0 tells wrong value as '2.1-bash'
    my ($version) = $Attribs{library_version}
	=~ /(\d+\.\d+)/;
    if ($version < 2.2) {
	carp "rl_unbind_function() is not supported.  Ignored\n";
	return;
    }
    if (defined $_[1]) {
	return _rl_unbind_function($_[0], _str2map($_[1]));
    } else {
	return _rl_unbind_function($_[0]);
    }
}

# rl_unbind_command
sub unbind_command ($;$) {
    my ($version) = $Attribs{library_version}
	=~ /(\d+\.\d+)/;
    if ($version < 2.2) {
	carp "rl_unbind_command() is not supported.  Ignored\n";
	return;
    }
    if (defined $_[1]) {
	return _rl_unbind_command($_[0], _str2map($_[1]));
    } else {
	return _rl_unbind_command($_[0]);
    }
}

# rl_bind_keyseq
sub rl_bind_keyseq ($$;$) {
    my ($version) = $Attribs{library_version}
	=~ /(\d+\.\d+)/;
    if ($version < 5.0) {
	carp "rl_bind_keyseq() is not supported.  Ignored\n";
	return;
    }
    if (defined $_[2]) {
	return _rl_bind_keyseq($_[0], _str2fn($_[1]), _str2map($_[2]));
    } else {
	return _rl_bind_keyseq($_[0], _str2fn($_[1]));
    }
}

sub rl_set_key ($$;$) {
    my ($version) = $Attribs{library_version}
	=~ /(\d+\.\d+)/;
    if ($version < 4.2) {
	carp "rl_set_key() is not supported.  Ignored\n";
	return;
    }
    if (defined $_[2]) {
	return _rl_set_key($_[0], _str2fn($_[1]), _str2map($_[2]));
    } else {
	return _rl_set_key($_[0], _str2fn($_[1]));
    }
}

# rl_bind_keyseq_if_unbound
sub rl_bind_keyseq_if_unbound ($$;$) {
    my ($version) = $Attribs{library_version}
	=~ /(\d+\.\d+)/;
    if ($version < 5.0) {
	carp "rl_bind_keyseq_if_unbound() is not supported.  Ignored\n";
	return;
    }
    if (defined $_[2]) {
	return _rl_bind_keyseq_if_unbound($_[0], _str2fn($_[1]), _str2map($_[2]));
    } else {
	return _rl_bind_keyseq_if_unbound($_[0], _str2fn($_[1]));
    }
}

sub rl_macro_bind ($$;$) {
    my ($version) = $Attribs{library_version}
	=~ /(\d+\.\d+)/;
    if (defined $_[2]) {
	return _rl_macro_bind($_[0], $_[1], _str2map($_[2]));
    } else {
	return _rl_macro_bind($_[0], $_[1]);
    }
}

sub rl_generic_bind ($$$;$) {
    if      ($_[0] == Term::ReadLine::Gnu::ISFUNC) {
	if (defined $_[3]) {
	    _rl_generic_bind_function($_[1], _str2fn($_[2]), _str2map($_[3]));
	} else {
	    _rl_generic_bind_function($_[1], _str2fn($_[2]));
	}
    } elsif ($_[0] == Term::ReadLine::Gnu::ISKMAP) {
	if (defined $_[3]) {
	    _rl_generic_bind_keymap($_[1], _str2map($_[2]), _str2map($_[3]));
	} else {
	    _rl_generic_bind_keymap($_[1], _str2map($_[2]));
	}
    } elsif ($_[0] == Term::ReadLine::Gnu::ISMACR) {
	if (defined $_[3]) {
	    _rl_generic_bind_macro($_[1], $_[2], _str2map($_[3]));
	} else {
	    _rl_generic_bind_macro($_[1], $_[2]);
	}
    } else {
	carp("Term::ReadLine::Gnu::rl_generic_bind: invalid \`type\'\n");
    }
}

sub rl_call_function ($;$$) {
    if (defined $_[2]) {
	return _rl_call_function(_str2fn($_[0]), $_[1], $_[2]);
    } elsif (defined $_[1]) {
	return _rl_call_function(_str2fn($_[0]), $_[1]);
    } else {
	return _rl_call_function(_str2fn($_[0]));
    }
}

sub rl_invoking_keyseqs ($;$) {
    if (defined $_[1]) {
	return _rl_invoking_keyseqs(_str2fn($_[0]), _str2map($_[1]));
    } else {
	return _rl_invoking_keyseqs(_str2fn($_[0]));
    }
}

sub rl_add_funmap_entry ($$) {
    my ($version) = $Attribs{library_version}
	=~ /(\d+\.\d+)/;
    if ($version < 4.2) {
	carp "rl_add_funmap_entry() is not supported.  Ignored\n";
	return;
    }
    return _rl_add_funmap_entry($_[0], _str2fn($_[1]));
}

sub rl_tty_set_default_bindings (;$) {
    my ($version) = $Attribs{library_version}
	=~ /(\d+\.\d+)/;
    if ($version < 4.2) {
	carp "rl_tty_set_default_bindings() is not supported.  Ignored\n";
	return;
    }
    if (defined $_[0]) {
	return _rl_tty_set_defaut_bindings(_str2map($_[1]));
    } else {
	return _rl_tty_set_defaut_bindings();
    }
}

sub rl_tty_unset_default_bindings (;$) {
    my ($version) = $Attribs{library_version}
	=~ /(\d+\.\d+)/;
    if ($version < 5.0) {
	carp "rl_tty_unset_default_bindings() is not supported.  Ignored\n";
	return;
    }
    if (defined $_[0]) {
	return _rl_tty_unset_defaut_bindings(_str2map($_[1]));
    } else {
	return _rl_tty_unset_defaut_bindings();
    }
}

sub rl_message {
    my $fmt = shift;
    my $line = sprintf($fmt, @_);
    _rl_message($line);
}

sub rl_completion_mode {
    # libreadline.* in Debian GNU/Linux 2.0 tells wrong value as '2.1-bash'
    my ($version) = $Attribs{library_version}
	=~ /(\d+\.\d+)/;
    if ($version < 4.3) {
	carp "rl_completion_mode() is not supported.  Ignored\n";
	return;
    }
    return _rl_completion_mode(_str2fn($_[0]));
}

#
#	for compatibility with Term::ReadLine::Perl
#
sub rl_filename_list {
    my ($text) = @_;

    # lcd : lowest common denominator
    my ($lcd, @matches) = rl_completion_matches($text,
						\&rl_filename_completion_function);
    return @matches ? @matches : $lcd;
}

#
#	History Library function wrappers
#
# history_list
sub hist_list () {
    my ($i, $history_base, $history_length, @d);
    $history_base   = $Attribs{history_base};
    $history_length = $Attribs{history_length};
    for ($i = $history_base; $i < $history_base + $history_length; $i++) {
	push(@d, history_get($i));
    }
    @d;
}

# history_arg_extract
sub hist_arg_extract ( ;$$$ ) {
    my ($line, $first, $last) = @_;
    $line  = $_      unless defined $line;
    $first = 0       unless defined $first;
    $last  = ord '$' unless defined $last; # '
    $first = ord '$' if defined $first and $first eq '$'; # '
    $last  = ord '$' if defined $last  and $last  eq '$'; # '
    &_history_arg_extract($line, $first, $last);
}

sub get_history_event ( $$;$ ) {
    _get_history_event($_[0], $_[1], defined $_[2] ? ord $_[2] : 0);
}

#
#	Ornaments
#

# This routine originates in Term::ReadLine.pm.

# Debian GNU/Linux discourages users from using /etc/termcap.  A
# subroutine ornaments() defined in Term::ReadLine.pm uses
# Term::Caps.pm which requires /etc/termcap.

# This module calls termcap (or its compatible) library, which the GNU
# Readline Library already uses, instead of Term::Caps.pm.

# Some terminals do not support 'ue' (underline end).
use vars qw(%term_no_ue);
%term_no_ue = ( kterm => 1 );

sub ornaments {
    return $rl_term_set unless @_;
    $rl_term_set = shift;
    $rl_term_set ||= ',,,';
    $rl_term_set = $term_no_ue{$ENV{TERM}} ? 'us,me,,' : 'us,ue,,'
	if $rl_term_set eq '1';
    my @ts = split /,/, $rl_term_set, 4;
    my @rl_term_set
	= map {
	    # non-printing characters must be informed to readline
	    my $t;
	    ($_ and $t = tgetstr($_))
		? (Term::ReadLine::Gnu::RL_PROMPT_START_IGNORE
		   . $t
		   . Term::ReadLine::Gnu::RL_PROMPT_END_IGNORE)
		    : '';
	} @ts;
    $Attribs{term_set} = \@rl_term_set;
    return $rl_term_set;
}

#
#	a sample custom function
#

# The equivalent of the Bash shell M-^ history-expand-line editing
# command.

# This routine was borrowed from bash.
sub history_expand_line {
    my ($count, $key) = @_;
    my ($expanded, $new_line) = history_expand($Attribs{line_buffer});
    if ($expanded > 0) {
  	rl_modifying(0, $Attribs{end}); # save undo information
  	$Attribs{line_buffer} = $new_line;
    } elsif ($expanded < 0) {
  	my $OUT = $Attribs{outstream};
  	print $OUT "\n$new_line\n";
  	rl_on_new_line();
    }				# $expanded == 0 : no change
}

# The equivalent of the Korn shell C-o operate-and-get-next-history-line
# editing command. 

# This routine was borrowed from bash.
sub operate_and_get_next {
    my ($count, $key) = @_;

    my $saved_history_line_to_use = -1;
    my $old_rl_startup_hook;

    # Accept the current line.
    rl_call_function('accept-line', 1, $key);

    # Find the current line, and find the next line to use. */
    my $where = where_history();
    if ((history_is_stifled()
	 && ($Attribs{history_length} >= $Attribs{max_input_history}))
	|| ($where >= $Attribs{history_length} - 1)) {
	$saved_history_line_to_use = $where;
    } else {
	$saved_history_line_to_use = $where + 1;
    }
    $old_rl_startup_hook = $Attribs{startup_hook};
    $Attribs{startup_hook} = sub {
	if ($saved_history_line_to_use >= 0) {
	    rl_call_function('previous-history',
			     $Attribs{history_length}
			     - $saved_history_line_to_use,
			     0);
	    $Attribs{startup_hook} = $old_rl_startup_hook;
	    $saved_history_line_to_use = -1;
	}
    };
}

sub display_readline_version {	# show version
    my($count, $key) = @_;	# ignored in this function
    my $OUT = $Attribs{outstream};
    print $OUT
	("\nTerm::ReadLine::Gnu version: $Term::ReadLine::Gnu::VERSION");
    print $OUT
	("\nGNU Readline Library version: $Attribs{library_version}\n");
    rl_on_new_line();
}

# sample function of rl_message()
sub change_ornaments {
    my($count, $key) = @_;	# ignored in this function
    rl_save_prompt;
    rl_message("[S]tandout, [U]nderlining, [B]old, [R]everse, [V]isible bell: ");
    my $c = chr rl_read_key;
    if ($c =~ /s/i) {
	ornaments('so,me,,');
    } elsif ($c =~ /u/i) {
	ornaments('us,me,,');
    } elsif ($c =~ /b/i) {
	ornaments('md,me,,');
    } elsif ($c =~ /r/i) {
	ornaments('mr,me,,');
    } elsif ($c =~ /v/i) {
	ornaments('vb,,,');
    } else {
	rl_ding;
    }
    rl_restore_prompt;
    rl_clear_message;
}

#
#	for tkRunning
#
sub Tk_getc {
    &Term::ReadLine::Tk::Tk_loop
	if $Term::ReadLine::toloop && defined &Tk::DoOneEvent;
    my $FILE = $Attribs{instream};
    return rl_getc($FILE);
}

# redisplay function for secret input like password
# usage:
#	$a->{redisplay_function} = $a->{shadow_redisplay};
#	$line = $t->readline("password> ");
sub shadow_redisplay {
    @_tstrs = _tgetstrs() unless $_tstrs_init;
    # remove prompt start/end mark from prompt string
    my $prompt = $Attribs{prompt}; my $s;
    $s = Term::ReadLine::Gnu::RL_PROMPT_START_IGNORE; $prompt =~ s/$s//g;
    $s = Term::ReadLine::Gnu::RL_PROMPT_END_IGNORE;   $prompt =~ s/$s//g;
    my $OUT = $Attribs{outstream};
    my $oldfh = select($OUT); $| = 1; select($oldfh);
    print $OUT ($_tstrs[0],	# carriage return
		$_tstrs[1],	# clear to EOL
		$prompt, '*' x length($Attribs{line_buffer}));
    print $OUT ($_tstrs[2]	# cursor left
		x (length($Attribs{line_buffer}) - $Attribs{point}));
    $oldfh = select($OUT); $| = 0; select($oldfh);
}

sub _tgetstrs {
    my @s = (tgetstr('cr'),	# carriage return
	     tgetstr('ce'),	# clear to EOL
	     tgetstr('le'));	# cursor left
    warn <<"EOM" unless (defined($s[0]) && defined($s[1]) && defined($s[2]));
Your terminal 'TERM=$ENV{TERM}' does not support enough function.
Check if your environment variable 'TERM' is set correctly.
EOM
    # suppress warning "Use of uninitialized value in print at ..."
    $s[0] = $s[0] || ''; $s[1] = $s[1] || ''; $s[2] = $s[2] || '';
    $_tstrs_init = 1;
    return @s;
}

# callback handler wrapper function for CallbackHandlerInstall method
sub _ch_wrapper {
    my $line = shift;

    if (defined $line) {
	if ($Attribs{do_expand}) {
	    my $result;
	    ($result, $line) = history_expand($line);
	    my $outstream = $Attribs{outstream};
	    print $outstream "$line\n" if ($result);

	    # return without adding line into history
	    if ($result < 0 || $result == 2) {
		return '';	# don't return `undef' which means EOF.
	    }
	}

	# add to history buffer
	add_history($line) 
	    if ($Attribs{MinLength} > 0
		&& length($line) >= $Attribs{MinLength});
    }
    &{$Attribs{_callback_handler}}($line);
}

#
#	List Completion Function
#
sub list_completion_function ( $$ ) {
    my($text, $state) = @_;

    $_i = $state ? $_i + 1 : 0;	# clear counter at the first call
    my $cw = $Attribs{completion_word};
    for (; $_i <= $#{$cw}; $_i++) {
	return $cw->[$_i] if ($cw->[$_i] =~ /^\Q$text/);
    }
    return undef;
}

#
#	wrapper completion function of 'completion_function'
#	for compatibility with Term::ReadLine::Perl
#
sub _trp_completion_function ( $$ ) {
    my($text, $state) = @_;

    my $cf;
    return undef unless defined ($cf = $Attribs{completion_function});

    if ($state) {
	$_i++;
    } else {
	# the first call
	$_i = 0;		# clear index
	@_matches = &$cf($text,
			 $Attribs{line_buffer},
			 $Attribs{point} - length($text));
	# return here since $#_matches is 0 instead of -1 when
	# @_matches = undef
	return undef unless defined $_matches[0];
    }

    for (; $_i <= $#_matches; $_i++) {
	return $_matches[$_i] if ($_matches[$_i] =~ /^\Q$text/);
    }
    return undef;
}

1;

__END__
