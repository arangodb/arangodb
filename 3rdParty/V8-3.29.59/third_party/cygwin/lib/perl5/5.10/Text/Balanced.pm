# EXTRACT VARIOUSLY DELIMITED TEXT SEQUENCES FROM STRINGS.
# FOR FULL DOCUMENTATION SEE Balanced.pod

use 5.005;
use strict;

package Text::Balanced;

use Exporter;
use SelfLoader;
use vars qw { $VERSION @ISA %EXPORT_TAGS };

use version; $VERSION = qv('2.0.0');
@ISA		= qw ( Exporter );
		     
%EXPORT_TAGS	= ( ALL => [ qw(
				&extract_delimited
				&extract_bracketed
				&extract_quotelike
				&extract_codeblock
				&extract_variable
				&extract_tagged
				&extract_multiple

				&gen_delimited_pat
				&gen_extract_tagged

				&delimited_pat
			       ) ] );

Exporter::export_ok_tags('ALL');

# PROTOTYPES

sub _match_bracketed($$$$$$);
sub _match_variable($$);
sub _match_codeblock($$$$$$$);
sub _match_quotelike($$$$);

# HANDLE RETURN VALUES IN VARIOUS CONTEXTS

sub _failmsg {
	my ($message, $pos) = @_;
	$@ = bless { error=>$message, pos=>$pos }, "Text::Balanced::ErrorMsg";
}

sub _fail
{
	my ($wantarray, $textref, $message, $pos) = @_;
	_failmsg $message, $pos if $message;
	return (undef,$$textref,undef) if $wantarray;
	return undef;
}

sub _succeed
{
	$@ = undef;
	my ($wantarray,$textref) = splice @_, 0, 2;
	my ($extrapos, $extralen) = @_>18 ? splice(@_, -2, 2) : (0,0);
	my ($startlen, $oppos) = @_[5,6];
	my $remainderpos = $_[2];
	if ($wantarray)
	{
		my @res;
		while (my ($from, $len) = splice @_, 0, 2)
		{
			push @res, substr($$textref,$from,$len);
		}
		if ($extralen) {	# CORRECT FILLET
			my $extra = substr($res[0], $extrapos-$oppos, $extralen, "\n");
			$res[1] = "$extra$res[1]";
			eval { substr($$textref,$remainderpos,0) = $extra;
			       substr($$textref,$extrapos,$extralen,"\n")} ;
				#REARRANGE HERE DOC AND FILLET IF POSSIBLE
			pos($$textref) = $remainderpos-$extralen+1; # RESET \G
		}
		else {
			pos($$textref) = $remainderpos;		    # RESET \G
		}
		return @res;
	}
	else
	{
		my $match = substr($$textref,$_[0],$_[1]);
		substr($match,$extrapos-$_[0]-$startlen,$extralen,"") if $extralen;
		my $extra = $extralen
			? substr($$textref, $extrapos, $extralen)."\n" : "";
		eval {substr($$textref,$_[4],$_[1]+$_[5])=$extra} ;	#CHOP OUT PREFIX & MATCH, IF POSSIBLE
		pos($$textref) = $_[4];				# RESET \G
		return $match;
	}
}

# BUILD A PATTERN MATCHING A SIMPLE DELIMITED STRING

sub gen_delimited_pat($;$)  # ($delimiters;$escapes)
{
	my ($dels, $escs) = @_;
	return "" unless $dels =~ /\S/;
	$escs = '\\' unless $escs;
	$escs .= substr($escs,-1) x (length($dels)-length($escs));
	my @pat = ();
	my $i;
	for ($i=0; $i<length $dels; $i++)
	{
		my $del = quotemeta substr($dels,$i,1);
		my $esc = quotemeta substr($escs,$i,1);
		if ($del eq $esc)
		{
			push @pat, "$del(?:[^$del]*(?:(?:$del$del)[^$del]*)*)$del";
		}
		else
		{
			push @pat, "$del(?:[^$esc$del]*(?:$esc.[^$esc$del]*)*)$del";
		}
	}
	my $pat = join '|', @pat;
	return "(?:$pat)";
}

*delimited_pat = \&gen_delimited_pat;


# THE EXTRACTION FUNCTIONS

sub extract_delimited (;$$$$)
{
	my $textref = defined $_[0] ? \$_[0] : \$_;
	my $wantarray = wantarray;
	my $del  = defined $_[1] ? $_[1] : qq{\'\"\`};
	my $pre  = defined $_[2] ? $_[2] : '\s*';
	my $esc  = defined $_[3] ? $_[3] : qq{\\};
	my $pat = gen_delimited_pat($del, $esc);
	my $startpos = pos $$textref || 0;
	return _fail($wantarray, $textref, "Not a delimited pattern", 0)
		unless $$textref =~ m/\G($pre)($pat)/gc;
	my $prelen = length($1);
	my $matchpos = $startpos+$prelen;
	my $endpos = pos $$textref;
	return _succeed $wantarray, $textref,
			$matchpos, $endpos-$matchpos,		# MATCH
			$endpos,   length($$textref)-$endpos,	# REMAINDER
			$startpos, $prelen;			# PREFIX
}

sub extract_bracketed (;$$$)
{
	my $textref = defined $_[0] ? \$_[0] : \$_;
	my $ldel = defined $_[1] ? $_[1] : '{([<';
	my $pre  = defined $_[2] ? $_[2] : '\s*';
	my $wantarray = wantarray;
	my $qdel = "";
	my $quotelike;
	$ldel =~ s/'//g and $qdel .= q{'};
	$ldel =~ s/"//g and $qdel .= q{"};
	$ldel =~ s/`//g and $qdel .= q{`};
	$ldel =~ s/q//g and $quotelike = 1;
	$ldel =~ tr/[](){}<>\0-\377/[[(({{<</ds;
	my $rdel = $ldel;
	unless ($rdel =~ tr/[({</])}>/)
        {
		return _fail $wantarray, $textref,
			     "Did not find a suitable bracket in delimiter: \"$_[1]\"",
			     0;
	}
	my $posbug = pos;
	$ldel = join('|', map { quotemeta $_ } split('', $ldel));
	$rdel = join('|', map { quotemeta $_ } split('', $rdel));
	pos = $posbug;

	my $startpos = pos $$textref || 0;
	my @match = _match_bracketed($textref,$pre, $ldel, $qdel, $quotelike, $rdel);

	return _fail ($wantarray, $textref) unless @match;

	return _succeed ( $wantarray, $textref,
			  $match[2], $match[5]+2,	# MATCH
			  @match[8,9],			# REMAINDER
			  @match[0,1],			# PREFIX
			);
}

sub _match_bracketed($$$$$$)	# $textref, $pre, $ldel, $qdel, $quotelike, $rdel
{
	my ($textref, $pre, $ldel, $qdel, $quotelike, $rdel) = @_;
	my ($startpos, $ldelpos, $endpos) = (pos $$textref = pos $$textref||0);
	unless ($$textref =~ m/\G$pre/gc)
	{
		_failmsg "Did not find prefix: /$pre/", $startpos;
		return;
	}

	$ldelpos = pos $$textref;

	unless ($$textref =~ m/\G($ldel)/gc)
	{
		_failmsg "Did not find opening bracket after prefix: \"$pre\"",
		         pos $$textref;
		pos $$textref = $startpos;
		return;
	}

	my @nesting = ( $1 );
	my $textlen = length $$textref;
	while (pos $$textref < $textlen)
	{
		next if $$textref =~ m/\G\\./gcs;

		if ($$textref =~ m/\G($ldel)/gc)
		{
			push @nesting, $1;
		}
		elsif ($$textref =~ m/\G($rdel)/gc)
		{
			my ($found, $brackettype) = ($1, $1);
			if ($#nesting < 0)
			{
				_failmsg "Unmatched closing bracket: \"$found\"",
					 pos $$textref;
				pos $$textref = $startpos;
			        return;
			}
			my $expected = pop(@nesting);
			$expected =~ tr/({[</)}]>/;
			if ($expected ne $brackettype)
			{
				_failmsg qq{Mismatched closing bracket: expected "$expected" but found "$found"},
					 pos $$textref;
				pos $$textref = $startpos;
			        return;
			}
			last if $#nesting < 0;
		}
		elsif ($qdel && $$textref =~ m/\G([$qdel])/gc)
		{
			$$textref =~ m/\G[^\\$1]*(?:\\.[^\\$1]*)*(\Q$1\E)/gsc and next;
			_failmsg "Unmatched embedded quote ($1)",
				 pos $$textref;
			pos $$textref = $startpos;
			return;
		}
		elsif ($quotelike && _match_quotelike($textref,"",1,0))
		{
			next;
		}

		else { $$textref =~ m/\G(?:[a-zA-Z0-9]+|.)/gcs }
	}
	if ($#nesting>=0)
	{
		_failmsg "Unmatched opening bracket(s): "
				. join("..",@nesting)."..",
		         pos $$textref;
		pos $$textref = $startpos;
		return;
	}

	$endpos = pos $$textref;
	
	return (
		$startpos,  $ldelpos-$startpos,		# PREFIX
		$ldelpos,   1,				# OPENING BRACKET
		$ldelpos+1, $endpos-$ldelpos-2,		# CONTENTS
		$endpos-1,  1,				# CLOSING BRACKET
		$endpos,    length($$textref)-$endpos,	# REMAINDER
	       );
}

sub _revbracket($)
{
	my $brack = reverse $_[0];
	$brack =~ tr/[({</])}>/;
	return $brack;
}

my $XMLNAME = q{[a-zA-Z_:][a-zA-Z0-9_:.-]*};

sub extract_tagged (;$$$$$) # ($text, $opentag, $closetag, $pre, \%options)
{
	my $textref = defined $_[0] ? \$_[0] : \$_;
	my $ldel    = $_[1];
	my $rdel    = $_[2];
	my $pre     = defined $_[3] ? $_[3] : '\s*';
	my %options = defined $_[4] ? %{$_[4]} : ();
	my $omode   = defined $options{fail} ? $options{fail} : '';
	my $bad     = ref($options{reject}) eq 'ARRAY' ? join('|', @{$options{reject}})
		    : defined($options{reject})	       ? $options{reject}
		    :					 ''
		    ;
	my $ignore  = ref($options{ignore}) eq 'ARRAY' ? join('|', @{$options{ignore}})
		    : defined($options{ignore})	       ? $options{ignore}
		    :					 ''
		    ;

	if (!defined $ldel) { $ldel = '<\w+(?:' . gen_delimited_pat(q{'"}) . '|[^>])*>'; }
	$@ = undef;

	my @match = _match_tagged($textref, $pre, $ldel, $rdel, $omode, $bad, $ignore);

	return _fail(wantarray, $textref) unless @match;
	return _succeed wantarray, $textref,
			$match[2], $match[3]+$match[5]+$match[7],	# MATCH
			@match[8..9,0..1,2..7];				# REM, PRE, BITS
}

sub _match_tagged	# ($$$$$$$)
{
	my ($textref, $pre, $ldel, $rdel, $omode, $bad, $ignore) = @_;
	my $rdelspec;

	my ($startpos, $opentagpos, $textpos, $parapos, $closetagpos, $endpos) = ( pos($$textref) = pos($$textref)||0 );

	unless ($$textref =~ m/\G($pre)/gc)
	{
		_failmsg "Did not find prefix: /$pre/", pos $$textref;
		goto failed;
	}

	$opentagpos = pos($$textref);

	unless ($$textref =~ m/\G$ldel/gc)
	{
		_failmsg "Did not find opening tag: /$ldel/", pos $$textref;
		goto failed;
	}

	$textpos = pos($$textref);

	if (!defined $rdel)
	{
		$rdelspec = substr($$textref, $-[0], $+[0] - $-[0]);
		unless ($rdelspec =~ s/\A([[(<{]+)($XMLNAME).*/ quotemeta "$1\/$2". _revbracket($1) /oes)
		{
			_failmsg "Unable to construct closing tag to match: $rdel",
				 pos $$textref;
			goto failed;
		}
	}
	else
	{
		$rdelspec = eval "qq{$rdel}" || do {
			my $del;
			for (qw,~ ! ^ & * ) _ + - = } ] : " ; ' > . ? / | ',)
				{ next if $rdel =~ /\Q$_/; $del = $_; last }
			unless ($del) {
				use Carp;
				croak "Can't interpolate right delimiter $rdel"
			}
			eval "qq$del$rdel$del";
		};
	}

	while (pos($$textref) < length($$textref))
	{
		next if $$textref =~ m/\G\\./gc;

		if ($$textref =~ m/\G(\n[ \t]*\n)/gc )
		{
			$parapos = pos($$textref) - length($1)
				unless defined $parapos;
		}
		elsif ($$textref =~ m/\G($rdelspec)/gc )
		{
			$closetagpos = pos($$textref)-length($1);
			goto matched;
		}
		elsif ($ignore && $$textref =~ m/\G(?:$ignore)/gc)
		{
			next;
		}
		elsif ($bad && $$textref =~ m/\G($bad)/gcs)
		{
			pos($$textref) -= length($1);	# CUT OFF WHATEVER CAUSED THE SHORTNESS
			goto short if ($omode eq 'PARA' || $omode eq 'MAX');
			_failmsg "Found invalid nested tag: $1", pos $$textref;
			goto failed;
		}
		elsif ($$textref =~ m/\G($ldel)/gc)
		{
			my $tag = $1;
			pos($$textref) -= length($tag);	# REWIND TO NESTED TAG
			unless (_match_tagged(@_))	# MATCH NESTED TAG
			{
				goto short if $omode eq 'PARA' || $omode eq 'MAX';
				_failmsg "Found unbalanced nested tag: $tag",
					 pos $$textref;
				goto failed;
			}
		}
		else { $$textref =~ m/./gcs }
	}

short:
	$closetagpos = pos($$textref);
	goto matched if $omode eq 'MAX';
	goto failed unless $omode eq 'PARA';

	if (defined $parapos) { pos($$textref) = $parapos }
	else		      { $parapos = pos($$textref) }

	return (
		$startpos,    $opentagpos-$startpos,		# PREFIX
		$opentagpos,  $textpos-$opentagpos,		# OPENING TAG
		$textpos,     $parapos-$textpos,		# TEXT
		$parapos,     0,				# NO CLOSING TAG
		$parapos,     length($$textref)-$parapos,	# REMAINDER
	       );
	
matched:
	$endpos = pos($$textref);
	return (
		$startpos,    $opentagpos-$startpos,		# PREFIX
		$opentagpos,  $textpos-$opentagpos,		# OPENING TAG
		$textpos,     $closetagpos-$textpos,		# TEXT
		$closetagpos, $endpos-$closetagpos,		# CLOSING TAG
		$endpos,      length($$textref)-$endpos,	# REMAINDER
	       );

failed:
	_failmsg "Did not find closing tag", pos $$textref unless $@;
	pos($$textref) = $startpos;
	return;
}

sub extract_variable (;$$)
{
	my $textref = defined $_[0] ? \$_[0] : \$_;
	return ("","","") unless defined $$textref;
	my $pre  = defined $_[1] ? $_[1] : '\s*';

	my @match = _match_variable($textref,$pre);

	return _fail wantarray, $textref unless @match;

	return _succeed wantarray, $textref,
			@match[2..3,4..5,0..1];		# MATCH, REMAINDER, PREFIX
}

sub _match_variable($$)
{
#  $#
#  $^
#  $$
	my ($textref, $pre) = @_;
	my $startpos = pos($$textref) = pos($$textref)||0;
	unless ($$textref =~ m/\G($pre)/gc)
	{
		_failmsg "Did not find prefix: /$pre/", pos $$textref;
		return;
	}
	my $varpos = pos($$textref);
        unless ($$textref =~ m{\G\$\s*(?!::)(\d+|[][&`'+*./|,";%=~:?!\@<>()-]|\^[a-z]?)}gci)
	{
	    unless ($$textref =~ m/\G((\$#?|[*\@\%]|\\&)+)/gc)
	    {
		_failmsg "Did not find leading dereferencer", pos $$textref;
		pos $$textref = $startpos;
		return;
	    }
	    my $deref = $1;

	    unless ($$textref =~ m/\G\s*(?:::|')?(?:[_a-z]\w*(?:::|'))*[_a-z]\w*/gci
	    	or _match_codeblock($textref, "", '\{', '\}', '\{', '\}', 0)
		or $deref eq '$#' or $deref eq '$$' )
	    {
		_failmsg "Bad identifier after dereferencer", pos $$textref;
		pos $$textref = $startpos;
		return;
	    }
	}

	while (1)
	{
		next if $$textref =~ m/\G\s*(?:->)?\s*[{]\w+[}]/gc;
		next if _match_codeblock($textref,
					 qr/\s*->\s*(?:[_a-zA-Z]\w+\s*)?/,
					 qr/[({[]/, qr/[)}\]]/,
					 qr/[({[]/, qr/[)}\]]/, 0);
		next if _match_codeblock($textref,
					 qr/\s*/, qr/[{[]/, qr/[}\]]/,
					 qr/[{[]/, qr/[}\]]/, 0);
		next if _match_variable($textref,'\s*->\s*');
		next if $$textref =~ m/\G\s*->\s*\w+(?![{([])/gc;
		last;
	}
	
	my $endpos = pos($$textref);
	return ($startpos, $varpos-$startpos,
		$varpos,   $endpos-$varpos,
		$endpos,   length($$textref)-$endpos
		);
}

sub extract_codeblock (;$$$$$)
{
	my $textref = defined $_[0] ? \$_[0] : \$_;
	my $wantarray = wantarray;
	my $ldel_inner = defined $_[1] ? $_[1] : '{';
	my $pre        = defined $_[2] ? $_[2] : '\s*';
	my $ldel_outer = defined $_[3] ? $_[3] : $ldel_inner;
	my $rd         = $_[4];
	my $rdel_inner = $ldel_inner;
	my $rdel_outer = $ldel_outer;
	my $posbug = pos;
	for ($ldel_inner, $ldel_outer) { tr/[]()<>{}\0-\377/[[((<<{{/ds }
	for ($rdel_inner, $rdel_outer) { tr/[]()<>{}\0-\377/]]))>>}}/ds }
	for ($ldel_inner, $ldel_outer, $rdel_inner, $rdel_outer)
	{
		$_ = '('.join('|',map { quotemeta $_ } split('',$_)).')'
	}
	pos = $posbug;

	my @match = _match_codeblock($textref, $pre,
				     $ldel_outer, $rdel_outer,
				     $ldel_inner, $rdel_inner,
				     $rd);
	return _fail($wantarray, $textref) unless @match;
	return _succeed($wantarray, $textref,
			@match[2..3,4..5,0..1]	# MATCH, REMAINDER, PREFIX
		       );

}

sub _match_codeblock($$$$$$$)
{
	my ($textref, $pre, $ldel_outer, $rdel_outer, $ldel_inner, $rdel_inner, $rd) = @_;
	my $startpos = pos($$textref) = pos($$textref) || 0;
	unless ($$textref =~ m/\G($pre)/gc)
	{
		_failmsg qq{Did not match prefix /$pre/ at"} .
			    substr($$textref,pos($$textref),20) .
			    q{..."},
		         pos $$textref;
		return; 
	}
	my $codepos = pos($$textref);
	unless ($$textref =~ m/\G($ldel_outer)/gc)	# OUTERMOST DELIMITER
	{
		_failmsg qq{Did not find expected opening bracket at "} .
			     substr($$textref,pos($$textref),20) .
			     q{..."},
		         pos $$textref;
		pos $$textref = $startpos;
		return;
	}
	my $closing = $1;
	   $closing =~ tr/([<{/)]>}/;
	my $matched;
	my $patvalid = 1;
	while (pos($$textref) < length($$textref))
	{
		$matched = '';
		if ($rd && $$textref =~ m#\G(\Q(?)\E|\Q(s?)\E|\Q(s)\E)#gc)
		{
			$patvalid = 0;
			next;
		}

		if ($$textref =~ m/\G\s*#.*/gc)
		{
			next;
		}

		if ($$textref =~ m/\G\s*($rdel_outer)/gc)
		{
			unless ($matched = ($closing && $1 eq $closing) )
			{
				next if $1 eq '>';	# MIGHT BE A "LESS THAN"
				_failmsg q{Mismatched closing bracket at "} .
					     substr($$textref,pos($$textref),20) .
					     qq{...". Expected '$closing'},
					 pos $$textref;
			}
			last;
		}

		if (_match_variable($textref,'\s*') ||
		    _match_quotelike($textref,'\s*',$patvalid,$patvalid) )
		{
			$patvalid = 0;
			next;
		}


		# NEED TO COVER MANY MORE CASES HERE!!!
		if ($$textref =~ m#\G\s*(?!$ldel_inner)
					( [-+*x/%^&|.]=?
					| [!=]~
					| =(?!>)
					| (\*\*|&&|\|\||<<|>>)=?
					| split|grep|map|return
					| [([]
					)#gcx)
		{
			$patvalid = 1;
			next;
		}

		if ( _match_codeblock($textref, '\s*', $ldel_inner, $rdel_inner, $ldel_inner, $rdel_inner, $rd) )
		{
			$patvalid = 1;
			next;
		}

		if ($$textref =~ m/\G\s*$ldel_outer/gc)
		{
			_failmsg q{Improperly nested codeblock at "} .
				     substr($$textref,pos($$textref),20) .
				     q{..."},
				 pos $$textref;
			last;
		}

		$patvalid = 0;
		$$textref =~ m/\G\s*(\w+|[-=>]>|.|\Z)/gc;
	}
	continue { $@ = undef }

	unless ($matched)
	{
		_failmsg 'No match found for opening bracket', pos $$textref
			unless $@;
		return;
	}

	my $endpos = pos($$textref);
	return ( $startpos, $codepos-$startpos,
		 $codepos, $endpos-$codepos,
		 $endpos,  length($$textref)-$endpos,
	       );
}


my %mods   = (
		'none'	=> '[cgimsox]*',
		'm'	=> '[cgimsox]*',
		's'	=> '[cegimsox]*',
		'tr'	=> '[cds]*',
		'y'	=> '[cds]*',
		'qq'	=> '',
		'qx'	=> '',
		'qw'	=> '',
		'qr'	=> '[imsx]*',
		'q'	=> '',
	     );

sub extract_quotelike (;$$)
{
	my $textref = $_[0] ? \$_[0] : \$_;
	my $wantarray = wantarray;
	my $pre  = defined $_[1] ? $_[1] : '\s*';

	my @match = _match_quotelike($textref,$pre,1,0);
	return _fail($wantarray, $textref) unless @match;
	return _succeed($wantarray, $textref,
			$match[2], $match[18]-$match[2],	# MATCH
			@match[18,19],				# REMAINDER
			@match[0,1],				# PREFIX
			@match[2..17],				# THE BITS
			@match[20,21],				# ANY FILLET?
		       );
};

sub _match_quotelike($$$$)	# ($textref, $prepat, $allow_raw_match)
{
	my ($textref, $pre, $rawmatch, $qmark) = @_;

	my ($textlen,$startpos,
	    $oppos,
	    $preld1pos,$ld1pos,$str1pos,$rd1pos,
	    $preld2pos,$ld2pos,$str2pos,$rd2pos,
	    $modpos) = ( length($$textref), pos($$textref) = pos($$textref) || 0 );

	unless ($$textref =~ m/\G($pre)/gc)
	{
		_failmsg qq{Did not find prefix /$pre/ at "} .
			     substr($$textref, pos($$textref), 20) .
			     q{..."},
		         pos $$textref;
		return; 
	}
	$oppos = pos($$textref);

	my $initial = substr($$textref,$oppos,1);

	if ($initial && $initial =~ m|^[\"\'\`]|
		     || $rawmatch && $initial =~ m|^/|
		     || $qmark && $initial =~ m|^\?|)
	{
		unless ($$textref =~ m/ \Q$initial\E [^\\$initial]* (\\.[^\\$initial]*)* \Q$initial\E /gcsx)
		{
			_failmsg qq{Did not find closing delimiter to match '$initial' at "} .
				     substr($$textref, $oppos, 20) .
				     q{..."},
				 pos $$textref;
			pos $$textref = $startpos;
			return;
		}
		$modpos= pos($$textref);
		$rd1pos = $modpos-1;

		if ($initial eq '/' || $initial eq '?') 
		{
			$$textref =~ m/\G$mods{none}/gc
		}

		my $endpos = pos($$textref);
		return (
			$startpos,	$oppos-$startpos,	# PREFIX
			$oppos,		0,			# NO OPERATOR
			$oppos,		1,			# LEFT DEL
			$oppos+1,	$rd1pos-$oppos-1,	# STR/PAT
			$rd1pos,	1,			# RIGHT DEL
			$modpos,	0,			# NO 2ND LDEL
			$modpos,	0,			# NO 2ND STR
			$modpos,	0,			# NO 2ND RDEL
			$modpos,	$endpos-$modpos,	# MODIFIERS
			$endpos, 	$textlen-$endpos,	# REMAINDER
		       );
	}

	unless ($$textref =~ m{\G(\b(?:m|s|qq|qx|qw|q|qr|tr|y)\b(?=\s*\S)|<<)}gc)
	{
		_failmsg q{No quotelike operator found after prefix at "} .
			     substr($$textref, pos($$textref), 20) .
			     q{..."},
		         pos $$textref;
		pos $$textref = $startpos;
		return;
	}

	my $op = $1;
	$preld1pos = pos($$textref);
	if ($op eq '<<') {
		$ld1pos = pos($$textref);
		my $label;
		if ($$textref =~ m{\G([A-Za-z_]\w*)}gc) {
			$label = $1;
		}
		elsif ($$textref =~ m{ \G ' ([^'\\]* (?:\\.[^'\\]*)*) '
				     | \G " ([^"\\]* (?:\\.[^"\\]*)*) "
				     | \G ` ([^`\\]* (?:\\.[^`\\]*)*) `
				     }gcsx) {
			$label = $+;
		}
		else {
			$label = "";
		}
		my $extrapos = pos($$textref);
		$$textref =~ m{.*\n}gc;
		$str1pos = pos($$textref)--;
		unless ($$textref =~ m{.*?\n(?=\Q$label\E\n)}gc) {
			_failmsg qq{Missing here doc terminator ('$label') after "} .
				     substr($$textref, $startpos, 20) .
				     q{..."},
				 pos $$textref;
			pos $$textref = $startpos;
			return;
		}
		$rd1pos = pos($$textref);
        $$textref =~ m{\Q$label\E\n}gc;
		$ld2pos = pos($$textref);
		return (
			$startpos,	$oppos-$startpos,	# PREFIX
			$oppos,		length($op),		# OPERATOR
			$ld1pos,	$extrapos-$ld1pos,	# LEFT DEL
			$str1pos,	$rd1pos-$str1pos,	# STR/PAT
			$rd1pos,	$ld2pos-$rd1pos,	# RIGHT DEL
			$ld2pos,	0,			# NO 2ND LDEL
			$ld2pos,	0,                	# NO 2ND STR
			$ld2pos,	0,	                # NO 2ND RDEL
			$ld2pos,	0,                      # NO MODIFIERS
			$ld2pos,	$textlen-$ld2pos,	# REMAINDER
			$extrapos,      $str1pos-$extrapos,	# FILLETED BIT
		       );
	}

	$$textref =~ m/\G\s*/gc;
	$ld1pos = pos($$textref);
	$str1pos = $ld1pos+1;

	unless ($$textref =~ m/\G(\S)/gc)	# SHOULD USE LOOKAHEAD
	{
		_failmsg "No block delimiter found after quotelike $op",
		         pos $$textref;
		pos $$textref = $startpos;
		return;
	}
	pos($$textref) = $ld1pos;	# HAVE TO DO THIS BECAUSE LOOKAHEAD BROKEN
	my ($ldel1, $rdel1) = ("\Q$1","\Q$1");
	if ($ldel1 =~ /[[(<{]/)
	{
		$rdel1 =~ tr/[({</])}>/;
		defined(_match_bracketed($textref,"",$ldel1,"","",$rdel1))
		|| do { pos $$textref = $startpos; return };
        $ld2pos = pos($$textref);
        $rd1pos = $ld2pos-1;
	}
	else
	{
		$$textref =~ /\G$ldel1[^\\$ldel1]*(\\.[^\\$ldel1]*)*$ldel1/gcs
		|| do { pos $$textref = $startpos; return };
        $ld2pos = $rd1pos = pos($$textref)-1;
	}

	my $second_arg = $op =~ /s|tr|y/ ? 1 : 0;
	if ($second_arg)
	{
		my ($ldel2, $rdel2);
		if ($ldel1 =~ /[[(<{]/)
		{
			unless ($$textref =~ /\G\s*(\S)/gc)	# SHOULD USE LOOKAHEAD
			{
				_failmsg "Missing second block for quotelike $op",
					 pos $$textref;
				pos $$textref = $startpos;
				return;
			}
			$ldel2 = $rdel2 = "\Q$1";
			$rdel2 =~ tr/[({</])}>/;
		}
		else
		{
			$ldel2 = $rdel2 = $ldel1;
		}
		$str2pos = $ld2pos+1;

		if ($ldel2 =~ /[[(<{]/)
		{
			pos($$textref)--;	# OVERCOME BROKEN LOOKAHEAD 
			defined(_match_bracketed($textref,"",$ldel2,"","",$rdel2))
			|| do { pos $$textref = $startpos; return };
		}
		else
		{
			$$textref =~ /[^\\$ldel2]*(\\.[^\\$ldel2]*)*$ldel2/gcs
			|| do { pos $$textref = $startpos; return };
		}
		$rd2pos = pos($$textref)-1;
	}
	else
	{
		$ld2pos = $str2pos = $rd2pos = $rd1pos;
	}

	$modpos = pos $$textref;

	$$textref =~ m/\G($mods{$op})/gc;
	my $endpos = pos $$textref;

	return (
		$startpos,	$oppos-$startpos,	# PREFIX
		$oppos,		length($op),		# OPERATOR
		$ld1pos,	1,			# LEFT DEL
		$str1pos,	$rd1pos-$str1pos,	# STR/PAT
		$rd1pos,	1,			# RIGHT DEL
		$ld2pos,	$second_arg,		# 2ND LDEL (MAYBE)
		$str2pos,	$rd2pos-$str2pos,	# 2ND STR (MAYBE)
		$rd2pos,	$second_arg,		# 2ND RDEL (MAYBE)
		$modpos,	$endpos-$modpos,	# MODIFIERS
		$endpos,	$textlen-$endpos,	# REMAINDER
	       );
}

my $def_func = 
[
	sub { extract_variable($_[0], '') },
	sub { extract_quotelike($_[0],'') },
	sub { extract_codeblock($_[0],'{}','') },
];

sub extract_multiple (;$$$$)	# ($text, $functions_ref, $max_fields, $ignoreunknown)
{
	my $textref = defined($_[0]) ? \$_[0] : \$_;
	my $posbug = pos;
	my ($lastpos, $firstpos);
	my @fields = ();

	#for ($$textref)
	{
		my @func = defined $_[1] ? @{$_[1]} : @{$def_func};
		my $max  = defined $_[2] && $_[2]>0 ? $_[2] : 1_000_000_000;
		my $igunk = $_[3];

		pos $$textref ||= 0;

		unless (wantarray)
		{
			use Carp;
			carp "extract_multiple reset maximal count to 1 in scalar context"
				if $^W && defined($_[2]) && $max > 1;
			$max = 1
		}

		my $unkpos;
		my $func;
		my $class;

		my @class;
		foreach $func ( @func )
		{
			if (ref($func) eq 'HASH')
			{
				push @class, (keys %$func)[0];
				$func = (values %$func)[0];
			}
			else
			{
				push @class, undef;
			}
		}

		FIELD: while (pos($$textref) < length($$textref))
		{
			my ($field, $rem);
			my @bits;
			foreach my $i ( 0..$#func )
			{
				my $pref;
				$func = $func[$i];
				$class = $class[$i];
				$lastpos = pos $$textref;
				if (ref($func) eq 'CODE')
					{ ($field,$rem,$pref) = @bits = $func->($$textref) }
				elsif (ref($func) eq 'Text::Balanced::Extractor')
					{ @bits = $field = $func->extract($$textref) }
				elsif( $$textref =~ m/\G$func/gc )
					{ @bits = $field = defined($1)
                                ? $1
                                : substr($$textref, $-[0], $+[0] - $-[0])
                    }
				$pref ||= "";
				if (defined($field) && length($field))
				{
					if (!$igunk) {
						$unkpos = $lastpos
							if length($pref) && !defined($unkpos);
						if (defined $unkpos)
						{
							push @fields, substr($$textref, $unkpos, $lastpos-$unkpos).$pref;
							$firstpos = $unkpos unless defined $firstpos;
							undef $unkpos;
							last FIELD if @fields == $max;
						}
					}
					push @fields, $class
						? bless (\$field, $class)
						: $field;
					$firstpos = $lastpos unless defined $firstpos;
					$lastpos = pos $$textref;
					last FIELD if @fields == $max;
					next FIELD;
				}
			}
			if ($$textref =~ /\G(.)/gcs)
			{
				$unkpos = pos($$textref)-1
					unless $igunk || defined $unkpos;
			}
		}
		
		if (defined $unkpos)
		{
			push @fields, substr($$textref, $unkpos);
			$firstpos = $unkpos unless defined $firstpos;
			$lastpos = length $$textref;
		}
		last;
	}

	pos $$textref = $lastpos;
	return @fields if wantarray;

	$firstpos ||= 0;
	eval { substr($$textref,$firstpos,$lastpos-$firstpos)="";
	       pos $$textref = $firstpos };
	return $fields[0];
}


sub gen_extract_tagged # ($opentag, $closetag, $pre, \%options)
{
	my $ldel    = $_[0];
	my $rdel    = $_[1];
	my $pre     = defined $_[2] ? $_[2] : '\s*';
	my %options = defined $_[3] ? %{$_[3]} : ();
	my $omode   = defined $options{fail} ? $options{fail} : '';
	my $bad     = ref($options{reject}) eq 'ARRAY' ? join('|', @{$options{reject}})
		    : defined($options{reject})	       ? $options{reject}
		    :					 ''
		    ;
	my $ignore  = ref($options{ignore}) eq 'ARRAY' ? join('|', @{$options{ignore}})
		    : defined($options{ignore})	       ? $options{ignore}
		    :					 ''
		    ;

	if (!defined $ldel) { $ldel = '<\w+(?:' . gen_delimited_pat(q{'"}) . '|[^>])*>'; }

	my $posbug = pos;
	for ($ldel, $pre, $bad, $ignore) { $_ = qr/$_/ if $_ }
	pos = $posbug;

	my $closure = sub
	{
		my $textref = defined $_[0] ? \$_[0] : \$_;
		my @match = Text::Balanced::_match_tagged($textref, $pre, $ldel, $rdel, $omode, $bad, $ignore);

		return _fail(wantarray, $textref) unless @match;
		return _succeed wantarray, $textref,
				$match[2], $match[3]+$match[5]+$match[7],	# MATCH
				@match[8..9,0..1,2..7];				# REM, PRE, BITS
	};

	bless $closure, 'Text::Balanced::Extractor';
}

package Text::Balanced::Extractor;

sub extract($$)	# ($self, $text)
{
	&{$_[0]}($_[1]);
}

package Text::Balanced::ErrorMsg;

use overload '""' => sub { "$_[0]->{error}, detected at offset $_[0]->{pos}" };

1;

__END__

=head1 NAME

Text::Balanced - Extract delimited text sequences from strings.


=head1 SYNOPSIS

 use Text::Balanced qw (
			extract_delimited
			extract_bracketed
			extract_quotelike
			extract_codeblock
			extract_variable
			extract_tagged
			extract_multiple

			gen_delimited_pat
			gen_extract_tagged
		       );

 # Extract the initial substring of $text that is delimited by
 # two (unescaped) instances of the first character in $delim.

	($extracted, $remainder) = extract_delimited($text,$delim);


 # Extract the initial substring of $text that is bracketed
 # with a delimiter(s) specified by $delim (where the string
 # in $delim contains one or more of '(){}[]<>').

	($extracted, $remainder) = extract_bracketed($text,$delim);


 # Extract the initial substring of $text that is bounded by
 # an XML tag.

	($extracted, $remainder) = extract_tagged($text);


 # Extract the initial substring of $text that is bounded by
 # a C<BEGIN>...C<END> pair. Don't allow nested C<BEGIN> tags

	($extracted, $remainder) =
		extract_tagged($text,"BEGIN","END",undef,{bad=>["BEGIN"]});


 # Extract the initial substring of $text that represents a
 # Perl "quote or quote-like operation"

	($extracted, $remainder) = extract_quotelike($text);


 # Extract the initial substring of $text that represents a block
 # of Perl code, bracketed by any of character(s) specified by $delim
 # (where the string $delim contains one or more of '(){}[]<>').

	($extracted, $remainder) = extract_codeblock($text,$delim);


 # Extract the initial substrings of $text that would be extracted by
 # one or more sequential applications of the specified functions
 # or regular expressions

	@extracted = extract_multiple($text,
				      [ \&extract_bracketed,
					\&extract_quotelike,
					\&some_other_extractor_sub,
					qr/[xyz]*/,
					'literal',
				      ]);

# Create a string representing an optimized pattern (a la Friedl)
# that matches a substring delimited by any of the specified characters
# (in this case: any type of quote or a slash)

	$patstring = gen_delimited_pat(q{'"`/});


# Generate a reference to an anonymous sub that is just like extract_tagged
# but pre-compiled and optimized for a specific pair of tags, and consequently
# much faster (i.e. 3 times faster). It uses qr// for better performance on
# repeated calls, so it only works under Perl 5.005 or later.

	$extract_head = gen_extract_tagged('<HEAD>','</HEAD>');

	($extracted, $remainder) = $extract_head->($text);


=head1 DESCRIPTION

The various C<extract_...> subroutines may be used to
extract a delimited substring, possibly after skipping a
specified prefix string. By default, that prefix is
optional whitespace (C</\s*/>), but you can change it to whatever
you wish (see below).

The substring to be extracted must appear at the
current C<pos> location of the string's variable
(or at index zero, if no C<pos> position is defined).
In other words, the C<extract_...> subroutines I<don't>
extract the first occurrence of a substring anywhere
in a string (like an unanchored regex would). Rather,
they extract an occurrence of the substring appearing
immediately at the current matching position in the
string (like a C<\G>-anchored regex would).



=head2 General behaviour in list contexts

In a list context, all the subroutines return a list, the first three
elements of which are always:

=over 4

=item [0]

The extracted string, including the specified delimiters.
If the extraction fails C<undef> is returned.

=item [1]

The remainder of the input string (i.e. the characters after the
extracted string). On failure, the entire string is returned.

=item [2]

The skipped prefix (i.e. the characters before the extracted string).
On failure, C<undef> is returned.

=back 

Note that in a list context, the contents of the original input text (the first
argument) are not modified in any way. 

However, if the input text was passed in a variable, that variable's
C<pos> value is updated to point at the first character after the
extracted text. That means that in a list context the various
subroutines can be used much like regular expressions. For example:

	while ( $next = (extract_quotelike($text))[0] )
	{
		# process next quote-like (in $next)
	}


=head2 General behaviour in scalar and void contexts

In a scalar context, the extracted string is returned, having first been
removed from the input text. Thus, the following code also processes
each quote-like operation, but actually removes them from $text:

	while ( $next = extract_quotelike($text) )
	{
		# process next quote-like (in $next)
	}

Note that if the input text is a read-only string (i.e. a literal),
no attempt is made to remove the extracted text.

In a void context the behaviour of the extraction subroutines is
exactly the same as in a scalar context, except (of course) that the
extracted substring is not returned.

=head2 A note about prefixes

Prefix patterns are matched without any trailing modifiers (C</gimsox> etc.)
This can bite you if you're expecting a prefix specification like
'.*?(?=<H1>)' to skip everything up to the first <H1> tag. Such a prefix
pattern will only succeed if the <H1> tag is on the current line, since
. normally doesn't match newlines.

To overcome this limitation, you need to turn on /s matching within
the prefix pattern, using the C<(?s)> directive: '(?s).*?(?=<H1>)'


=head2 C<extract_delimited>

The C<extract_delimited> function formalizes the common idiom
of extracting a single-character-delimited substring from the start of
a string. For example, to extract a single-quote delimited string, the
following code is typically used:

	($remainder = $text) =~ s/\A('(\\.|[^'])*')//s;
	$extracted = $1;

but with C<extract_delimited> it can be simplified to:

	($extracted,$remainder) = extract_delimited($text, "'");

C<extract_delimited> takes up to four scalars (the input text, the
delimiters, a prefix pattern to be skipped, and any escape characters)
and extracts the initial substring of the text that
is appropriately delimited. If the delimiter string has multiple
characters, the first one encountered in the text is taken to delimit
the substring.
The third argument specifies a prefix pattern that is to be skipped
(but must be present!) before the substring is extracted.
The final argument specifies the escape character to be used for each
delimiter.

All arguments are optional. If the escape characters are not specified,
every delimiter is escaped with a backslash (C<\>).
If the prefix is not specified, the
pattern C<'\s*'> - optional whitespace - is used. If the delimiter set
is also not specified, the set C</["'`]/> is used. If the text to be processed
is not specified either, C<$_> is used.

In list context, C<extract_delimited> returns a array of three
elements, the extracted substring (I<including the surrounding
delimiters>), the remainder of the text, and the skipped prefix (if
any). If a suitable delimited substring is not found, the first
element of the array is the empty string, the second is the complete
original text, and the prefix returned in the third element is an
empty string.

In a scalar context, just the extracted substring is returned. In
a void context, the extracted substring (and any prefix) are simply
removed from the beginning of the first argument.

Examples:

	# Remove a single-quoted substring from the very beginning of $text:

		$substring = extract_delimited($text, "'", '');

	# Remove a single-quoted Pascalish substring (i.e. one in which
	# doubling the quote character escapes it) from the very
	# beginning of $text:

		$substring = extract_delimited($text, "'", '', "'");

	# Extract a single- or double- quoted substring from the
	# beginning of $text, optionally after some whitespace
	# (note the list context to protect $text from modification):

		($substring) = extract_delimited $text, q{"'};


	# Delete the substring delimited by the first '/' in $text:

		$text = join '', (extract_delimited($text,'/','[^/]*')[2,1];

Note that this last example is I<not> the same as deleting the first
quote-like pattern. For instance, if C<$text> contained the string:

	"if ('./cmd' =~ m/$UNIXCMD/s) { $cmd = $1; }"
	
then after the deletion it would contain:

	"if ('.$UNIXCMD/s) { $cmd = $1; }"

not:

	"if ('./cmd' =~ ms) { $cmd = $1; }"
	

See L<"extract_quotelike"> for a (partial) solution to this problem.


=head2 C<extract_bracketed>

Like C<"extract_delimited">, the C<extract_bracketed> function takes
up to three optional scalar arguments: a string to extract from, a delimiter
specifier, and a prefix pattern. As before, a missing prefix defaults to
optional whitespace and a missing text defaults to C<$_>. However, a missing
delimiter specifier defaults to C<'{}()[]E<lt>E<gt>'> (see below).

C<extract_bracketed> extracts a balanced-bracket-delimited
substring (using any one (or more) of the user-specified delimiter
brackets: '(..)', '{..}', '[..]', or '<..>'). Optionally it will also
respect quoted unbalanced brackets (see below).

A "delimiter bracket" is a bracket in list of delimiters passed as
C<extract_bracketed>'s second argument. Delimiter brackets are
specified by giving either the left or right (or both!) versions
of the required bracket(s). Note that the order in which
two or more delimiter brackets are specified is not significant.

A "balanced-bracket-delimited substring" is a substring bounded by
matched brackets, such that any other (left or right) delimiter
bracket I<within> the substring is also matched by an opposite
(right or left) delimiter bracket I<at the same level of nesting>. Any
type of bracket not in the delimiter list is treated as an ordinary
character.

In other words, each type of bracket specified as a delimiter must be
balanced and correctly nested within the substring, and any other kind of
("non-delimiter") bracket in the substring is ignored.

For example, given the string:

	$text = "{ an '[irregularly :-(] {} parenthesized >:-)' string }";

then a call to C<extract_bracketed> in a list context:

	@result = extract_bracketed( $text, '{}' );

would return:

	( "{ an '[irregularly :-(] {} parenthesized >:-)' string }" , "" , "" )

since both sets of C<'{..}'> brackets are properly nested and evenly balanced.
(In a scalar context just the first element of the array would be returned. In
a void context, C<$text> would be replaced by an empty string.)

Likewise the call in:

	@result = extract_bracketed( $text, '{[' );

would return the same result, since all sets of both types of specified
delimiter brackets are correctly nested and balanced.

However, the call in:

	@result = extract_bracketed( $text, '{([<' );

would fail, returning:

	( undef , "{ an '[irregularly :-(] {} parenthesized >:-)' string }"  );

because the embedded pairs of C<'(..)'>s and C<'[..]'>s are "cross-nested" and
the embedded C<'E<gt>'> is unbalanced. (In a scalar context, this call would
return an empty string. In a void context, C<$text> would be unchanged.)

Note that the embedded single-quotes in the string don't help in this
case, since they have not been specified as acceptable delimiters and are
therefore treated as non-delimiter characters (and ignored).

However, if a particular species of quote character is included in the
delimiter specification, then that type of quote will be correctly handled.
for example, if C<$text> is:

	$text = '<A HREF=">>>>">link</A>';

then

	@result = extract_bracketed( $text, '<">' );

returns:

	( '<A HREF=">>>>">', 'link</A>', "" )

as expected. Without the specification of C<"> as an embedded quoter:

	@result = extract_bracketed( $text, '<>' );

the result would be:

	( '<A HREF=">', '>>>">link</A>', "" )

In addition to the quote delimiters C<'>, C<">, and C<`>, full Perl quote-like
quoting (i.e. q{string}, qq{string}, etc) can be specified by including the
letter 'q' as a delimiter. Hence:

	@result = extract_bracketed( $text, '<q>' );

would correctly match something like this:

	$text = '<leftop: conj /and/ conj>';

See also: C<"extract_quotelike"> and C<"extract_codeblock">.


=head2 C<extract_variable>

C<extract_variable> extracts any valid Perl variable or
variable-involved expression, including scalars, arrays, hashes, array
accesses, hash look-ups, method calls through objects, subroutine calls
through subroutine references, etc.

The subroutine takes up to two optional arguments:

=over 4

=item 1.

A string to be processed (C<$_> if the string is omitted or C<undef>)

=item 2.

A string specifying a pattern to be matched as a prefix (which is to be
skipped). If omitted, optional whitespace is skipped.

=back

On success in a list context, an array of 3 elements is returned. The
elements are:

=over 4

=item [0]

the extracted variable, or variablish expression

=item [1]

the remainder of the input text,

=item [2]

the prefix substring (if any),

=back

On failure, all of these values (except the remaining text) are C<undef>.

In a scalar context, C<extract_variable> returns just the complete
substring that matched a variablish expression. C<undef> is returned on
failure. In addition, the original input text has the returned substring
(and any prefix) removed from it.

In a void context, the input text just has the matched substring (and
any specified prefix) removed.


=head2 C<extract_tagged>

C<extract_tagged> extracts and segments text between (balanced)
specified tags. 

The subroutine takes up to five optional arguments:

=over 4

=item 1.

A string to be processed (C<$_> if the string is omitted or C<undef>)

=item 2.

A string specifying a pattern to be matched as the opening tag.
If the pattern string is omitted (or C<undef>) then a pattern
that matches any standard XML tag is used.

=item 3.

A string specifying a pattern to be matched at the closing tag. 
If the pattern string is omitted (or C<undef>) then the closing
tag is constructed by inserting a C</> after any leading bracket
characters in the actual opening tag that was matched (I<not> the pattern
that matched the tag). For example, if the opening tag pattern
is specified as C<'{{\w+}}'> and actually matched the opening tag 
C<"{{DATA}}">, then the constructed closing tag would be C<"{{/DATA}}">.

=item 4.

A string specifying a pattern to be matched as a prefix (which is to be
skipped). If omitted, optional whitespace is skipped.

=item 5.

A hash reference containing various parsing options (see below)

=back

The various options that can be specified are:

=over 4

=item C<reject =E<gt> $listref>

The list reference contains one or more strings specifying patterns
that must I<not> appear within the tagged text.

For example, to extract
an HTML link (which should not contain nested links) use:

        extract_tagged($text, '<A>', '</A>', undef, {reject => ['<A>']} );

=item C<ignore =E<gt> $listref>

The list reference contains one or more strings specifying patterns
that are I<not> be be treated as nested tags within the tagged text
(even if they would match the start tag pattern).

For example, to extract an arbitrary XML tag, but ignore "empty" elements:

        extract_tagged($text, undef, undef, undef, {ignore => ['<[^>]*/>']} );

(also see L<"gen_delimited_pat"> below).


=item C<fail =E<gt> $str>

The C<fail> option indicates the action to be taken if a matching end
tag is not encountered (i.e. before the end of the string or some
C<reject> pattern matches). By default, a failure to match a closing
tag causes C<extract_tagged> to immediately fail.

However, if the string value associated with <reject> is "MAX", then
C<extract_tagged> returns the complete text up to the point of failure.
If the string is "PARA", C<extract_tagged> returns only the first paragraph
after the tag (up to the first line that is either empty or contains
only whitespace characters).
If the string is "", the the default behaviour (i.e. failure) is reinstated.

For example, suppose the start tag "/para" introduces a paragraph, which then
continues until the next "/endpara" tag or until another "/para" tag is
encountered:

        $text = "/para line 1\n\nline 3\n/para line 4";

        extract_tagged($text, '/para', '/endpara', undef,
                                {reject => '/para', fail => MAX );

        # EXTRACTED: "/para line 1\n\nline 3\n"

Suppose instead, that if no matching "/endpara" tag is found, the "/para"
tag refers only to the immediately following paragraph:

        $text = "/para line 1\n\nline 3\n/para line 4";

        extract_tagged($text, '/para', '/endpara', undef,
                        {reject => '/para', fail => MAX );

        # EXTRACTED: "/para line 1\n"

Note that the specified C<fail> behaviour applies to nested tags as well.

=back

On success in a list context, an array of 6 elements is returned. The elements are:

=over 4

=item [0]

the extracted tagged substring (including the outermost tags),

=item [1]

the remainder of the input text,

=item [2]

the prefix substring (if any),

=item [3]

the opening tag

=item [4]

the text between the opening and closing tags

=item [5]

the closing tag (or "" if no closing tag was found)

=back

On failure, all of these values (except the remaining text) are C<undef>.

In a scalar context, C<extract_tagged> returns just the complete
substring that matched a tagged text (including the start and end
tags). C<undef> is returned on failure. In addition, the original input
text has the returned substring (and any prefix) removed from it.

In a void context, the input text just has the matched substring (and
any specified prefix) removed.


=head2 C<gen_extract_tagged>

(Note: This subroutine is only available under Perl5.005)

C<gen_extract_tagged> generates a new anonymous subroutine which
extracts text between (balanced) specified tags. In other words,
it generates a function identical in function to C<extract_tagged>.

The difference between C<extract_tagged> and the anonymous
subroutines generated by
C<gen_extract_tagged>, is that those generated subroutines:

=over 4

=item * 

do not have to reparse tag specification or parsing options every time
they are called (whereas C<extract_tagged> has to effectively rebuild
its tag parser on every call);

=item *

make use of the new qr// construct to pre-compile the regexes they use
(whereas C<extract_tagged> uses standard string variable interpolation 
to create tag-matching patterns).

=back

The subroutine takes up to four optional arguments (the same set as
C<extract_tagged> except for the string to be processed). It returns
a reference to a subroutine which in turn takes a single argument (the text to
be extracted from).

In other words, the implementation of C<extract_tagged> is exactly
equivalent to:

        sub extract_tagged
        {
                my $text = shift;
                $extractor = gen_extract_tagged(@_);
                return $extractor->($text);
        }

(although C<extract_tagged> is not currently implemented that way, in order
to preserve pre-5.005 compatibility).

Using C<gen_extract_tagged> to create extraction functions for specific tags 
is a good idea if those functions are going to be called more than once, since
their performance is typically twice as good as the more general-purpose
C<extract_tagged>.


=head2 C<extract_quotelike>

C<extract_quotelike> attempts to recognize, extract, and segment any
one of the various Perl quotes and quotelike operators (see
L<perlop(3)>) Nested backslashed delimiters, embedded balanced bracket
delimiters (for the quotelike operators), and trailing modifiers are
all caught. For example, in:

        extract_quotelike 'q # an octothorpe: \# (not the end of the q!) #'
        
        extract_quotelike '  "You said, \"Use sed\"."  '

        extract_quotelike ' s{([A-Z]{1,8}\.[A-Z]{3})} /\L$1\E/; '

        extract_quotelike ' tr/\\\/\\\\/\\\//ds; '

the full Perl quotelike operations are all extracted correctly.

Note too that, when using the /x modifier on a regex, any comment
containing the current pattern delimiter will cause the regex to be
immediately terminated. In other words:

        'm /
                (?i)            # CASE INSENSITIVE
                [a-z_]          # LEADING ALPHABETIC/UNDERSCORE
                [a-z0-9]*       # FOLLOWED BY ANY NUMBER OF ALPHANUMERICS
           /x'

will be extracted as if it were:

        'm /
                (?i)            # CASE INSENSITIVE
                [a-z_]          # LEADING ALPHABETIC/'

This behaviour is identical to that of the actual compiler.

C<extract_quotelike> takes two arguments: the text to be processed and
a prefix to be matched at the very beginning of the text. If no prefix 
is specified, optional whitespace is the default. If no text is given,
C<$_> is used.

In a list context, an array of 11 elements is returned. The elements are:

=over 4

=item [0]

the extracted quotelike substring (including trailing modifiers),

=item [1]

the remainder of the input text,

=item [2]

the prefix substring (if any),

=item [3]

the name of the quotelike operator (if any),

=item [4]

the left delimiter of the first block of the operation,

=item [5]

the text of the first block of the operation
(that is, the contents of
a quote, the regex of a match or substitution or the target list of a
translation),

=item [6]

the right delimiter of the first block of the operation,

=item [7]

the left delimiter of the second block of the operation
(that is, if it is a C<s>, C<tr>, or C<y>),

=item [8]

the text of the second block of the operation 
(that is, the replacement of a substitution or the translation list
of a translation),

=item [9]

the right delimiter of the second block of the operation (if any),

=item [10]

the trailing modifiers on the operation (if any).

=back

For each of the fields marked "(if any)" the default value on success is
an empty string.
On failure, all of these values (except the remaining text) are C<undef>.


In a scalar context, C<extract_quotelike> returns just the complete substring
that matched a quotelike operation (or C<undef> on failure). In a scalar or
void context, the input text has the same substring (and any specified
prefix) removed.

Examples:

        # Remove the first quotelike literal that appears in text

                $quotelike = extract_quotelike($text,'.*?');

        # Replace one or more leading whitespace-separated quotelike
        # literals in $_ with "<QLL>"

                do { $_ = join '<QLL>', (extract_quotelike)[2,1] } until $@;


        # Isolate the search pattern in a quotelike operation from $text

                ($op,$pat) = (extract_quotelike $text)[3,5];
                if ($op =~ /[ms]/)
                {
                        print "search pattern: $pat\n";
                }
                else
                {
                        print "$op is not a pattern matching operation\n";
                }


=head2 C<extract_quotelike> and "here documents"

C<extract_quotelike> can successfully extract "here documents" from an input
string, but with an important caveat in list contexts.

Unlike other types of quote-like literals, a here document is rarely
a contiguous substring. For example, a typical piece of code using
here document might look like this:

        <<'EOMSG' || die;
        This is the message.
        EOMSG
        exit;

Given this as an input string in a scalar context, C<extract_quotelike>
would correctly return the string "<<'EOMSG'\nThis is the message.\nEOMSG",
leaving the string " || die;\nexit;" in the original variable. In other words,
the two separate pieces of the here document are successfully extracted and
concatenated.

In a list context, C<extract_quotelike> would return the list

=over 4

=item [0]

"<<'EOMSG'\nThis is the message.\nEOMSG\n" (i.e. the full extracted here document,
including fore and aft delimiters),

=item [1]

" || die;\nexit;" (i.e. the remainder of the input text, concatenated),

=item [2]

"" (i.e. the prefix substring -- trivial in this case),

=item [3]

"<<" (i.e. the "name" of the quotelike operator)

=item [4]

"'EOMSG'" (i.e. the left delimiter of the here document, including any quotes),

=item [5]

"This is the message.\n" (i.e. the text of the here document),

=item [6]

"EOMSG" (i.e. the right delimiter of the here document),

=item [7..10]

"" (a here document has no second left delimiter, second text, second right
delimiter, or trailing modifiers).

=back

However, the matching position of the input variable would be set to
"exit;" (i.e. I<after> the closing delimiter of the here document),
which would cause the earlier " || die;\nexit;" to be skipped in any
sequence of code fragment extractions.

To avoid this problem, when it encounters a here document whilst
extracting from a modifiable string, C<extract_quotelike> silently
rearranges the string to an equivalent piece of Perl:

        <<'EOMSG'
        This is the message.
        EOMSG
        || die;
        exit;

in which the here document I<is> contiguous. It still leaves the
matching position after the here document, but now the rest of the line
on which the here document starts is not skipped.

To prevent <extract_quotelike> from mucking about with the input in this way
(this is the only case where a list-context C<extract_quotelike> does so),
you can pass the input variable as an interpolated literal:

        $quotelike = extract_quotelike("$var");


=head2 C<extract_codeblock>

C<extract_codeblock> attempts to recognize and extract a balanced
bracket delimited substring that may contain unbalanced brackets
inside Perl quotes or quotelike operations. That is, C<extract_codeblock>
is like a combination of C<"extract_bracketed"> and
C<"extract_quotelike">.

C<extract_codeblock> takes the same initial three parameters as C<extract_bracketed>:
a text to process, a set of delimiter brackets to look for, and a prefix to
match first. It also takes an optional fourth parameter, which allows the
outermost delimiter brackets to be specified separately (see below).

Omitting the first argument (input text) means process C<$_> instead.
Omitting the second argument (delimiter brackets) indicates that only C<'{'> is to be used.
Omitting the third argument (prefix argument) implies optional whitespace at the start.
Omitting the fourth argument (outermost delimiter brackets) indicates that the
value of the second argument is to be used for the outermost delimiters.

Once the prefix an dthe outermost opening delimiter bracket have been
recognized, code blocks are extracted by stepping through the input text and
trying the following alternatives in sequence:

=over 4

=item 1.

Try and match a closing delimiter bracket. If the bracket was the same
species as the last opening bracket, return the substring to that
point. If the bracket was mismatched, return an error.

=item 2.

Try to match a quote or quotelike operator. If found, call
C<extract_quotelike> to eat it. If C<extract_quotelike> fails, return
the error it returned. Otherwise go back to step 1.

=item 3.

Try to match an opening delimiter bracket. If found, call
C<extract_codeblock> recursively to eat the embedded block. If the
recursive call fails, return an error. Otherwise, go back to step 1.

=item 4.

Unconditionally match a bareword or any other single character, and
then go back to step 1.

=back


Examples:

        # Find a while loop in the text

                if ($text =~ s/.*?while\s*\{/{/)
                {
                        $loop = "while " . extract_codeblock($text);
                }

        # Remove the first round-bracketed list (which may include
        # round- or curly-bracketed code blocks or quotelike operators)

                extract_codeblock $text, "(){}", '[^(]*';


The ability to specify a different outermost delimiter bracket is useful
in some circumstances. For example, in the Parse::RecDescent module,
parser actions which are to be performed only on a successful parse
are specified using a C<E<lt>defer:...E<gt>> directive. For example:

        sentence: subject verb object
                        <defer: {$::theVerb = $item{verb}} >

Parse::RecDescent uses C<extract_codeblock($text, '{}E<lt>E<gt>')> to extract the code
within the C<E<lt>defer:...E<gt>> directive, but there's a problem.

A deferred action like this:

                        <defer: {if ($count>10) {$count--}} >

will be incorrectly parsed as:

                        <defer: {if ($count>

because the "less than" operator is interpreted as a closing delimiter.

But, by extracting the directive using
S<C<extract_codeblock($text, '{}', undef, 'E<lt>E<gt>')>>
the '>' character is only treated as a delimited at the outermost
level of the code block, so the directive is parsed correctly.

=head2 C<extract_multiple>

The C<extract_multiple> subroutine takes a string to be processed and a 
list of extractors (subroutines or regular expressions) to apply to that string.

In an array context C<extract_multiple> returns an array of substrings
of the original string, as extracted by the specified extractors.
In a scalar context, C<extract_multiple> returns the first
substring successfully extracted from the original string. In both
scalar and void contexts the original string has the first successfully
extracted substring removed from it. In all contexts
C<extract_multiple> starts at the current C<pos> of the string, and
sets that C<pos> appropriately after it matches.

Hence, the aim of of a call to C<extract_multiple> in a list context
is to split the processed string into as many non-overlapping fields as
possible, by repeatedly applying each of the specified extractors
to the remainder of the string. Thus C<extract_multiple> is
a generalized form of Perl's C<split> subroutine.

The subroutine takes up to four optional arguments:

=over 4

=item 1.

A string to be processed (C<$_> if the string is omitted or C<undef>)

=item 2.

A reference to a list of subroutine references and/or qr// objects and/or
literal strings and/or hash references, specifying the extractors
to be used to split the string. If this argument is omitted (or
C<undef>) the list:

        [
                sub { extract_variable($_[0], '') },
                sub { extract_quotelike($_[0],'') },
                sub { extract_codeblock($_[0],'{}','') },
        ]

is used.


=item 3.

An number specifying the maximum number of fields to return. If this
argument is omitted (or C<undef>), split continues as long as possible.

If the third argument is I<N>, then extraction continues until I<N> fields
have been successfully extracted, or until the string has been completely 
processed.

Note that in scalar and void contexts the value of this argument is 
automatically reset to 1 (under C<-w>, a warning is issued if the argument 
has to be reset).

=item 4.

A value indicating whether unmatched substrings (see below) within the
text should be skipped or returned as fields. If the value is true,
such substrings are skipped. Otherwise, they are returned.

=back

The extraction process works by applying each extractor in
sequence to the text string.

If the extractor is a subroutine it is called in a list context and is
expected to return a list of a single element, namely the extracted
text. It may optionally also return two further arguments: a string
representing the text left after extraction (like $' for a pattern
match), and a string representing any prefix skipped before the
extraction (like $` in a pattern match). Note that this is designed
to facilitate the use of other Text::Balanced subroutines with
C<extract_multiple>. Note too that the value returned by an extractor
subroutine need not bear any relationship to the corresponding substring
of the original text (see examples below).

If the extractor is a precompiled regular expression or a string,
it is matched against the text in a scalar context with a leading
'\G' and the gc modifiers enabled. The extracted value is either
$1 if that variable is defined after the match, or else the
complete match (i.e. $&).

If the extractor is a hash reference, it must contain exactly one element.
The value of that element is one of the
above extractor types (subroutine reference, regular expression, or string).
The key of that element is the name of a class into which the successful
return value of the extractor will be blessed.

If an extractor returns a defined value, that value is immediately
treated as the next extracted field and pushed onto the list of fields.
If the extractor was specified in a hash reference, the field is also
blessed into the appropriate class, 

If the extractor fails to match (in the case of a regex extractor), or returns an empty list or an undefined value (in the case of a subroutine extractor), it is
assumed to have failed to extract.
If none of the extractor subroutines succeeds, then one
character is extracted from the start of the text and the extraction
subroutines reapplied. Characters which are thus removed are accumulated and
eventually become the next field (unless the fourth argument is true, in which
case they are discarded).

For example, the following extracts substrings that are valid Perl variables:

        @fields = extract_multiple($text,
                                   [ sub { extract_variable($_[0]) } ],
                                   undef, 1);

This example separates a text into fields which are quote delimited,
curly bracketed, and anything else. The delimited and bracketed
parts are also blessed to identify them (the "anything else" is unblessed):

        @fields = extract_multiple($text,
                   [
                        { Delim => sub { extract_delimited($_[0],q{'"}) } },
                        { Brack => sub { extract_bracketed($_[0],'{}') } },
                   ]);

This call extracts the next single substring that is a valid Perl quotelike
operator (and removes it from $text):

        $quotelike = extract_multiple($text,
                                      [
                                        sub { extract_quotelike($_[0]) },
                                      ], undef, 1);

Finally, here is yet another way to do comma-separated value parsing:

        @fields = extract_multiple($csv_text,
                                  [
                                        sub { extract_delimited($_[0],q{'"}) },
                                        qr/([^,]+)(.*)/,
                                  ],
                                  undef,1);

The list in the second argument means:
I<"Try and extract a ' or " delimited string, otherwise extract anything up to a comma...">.
The undef third argument means:
I<"...as many times as possible...">,
and the true value in the fourth argument means
I<"...discarding anything else that appears (i.e. the commas)">.

If you wanted the commas preserved as separate fields (i.e. like split
does if your split pattern has capturing parentheses), you would
just make the last parameter undefined (or remove it).


=head2 C<gen_delimited_pat>

The C<gen_delimited_pat> subroutine takes a single (string) argument and
   > builds a Friedl-style optimized regex that matches a string delimited
by any one of the characters in the single argument. For example:

        gen_delimited_pat(q{'"})

returns the regex:

        (?:\"(?:\\\"|(?!\").)*\"|\'(?:\\\'|(?!\').)*\')

Note that the specified delimiters are automatically quotemeta'd.

A typical use of C<gen_delimited_pat> would be to build special purpose tags
for C<extract_tagged>. For example, to properly ignore "empty" XML elements
(which might contain quoted strings):

        my $empty_tag = '<(' . gen_delimited_pat(q{'"}) . '|.)+/>';

        extract_tagged($text, undef, undef, undef, {ignore => [$empty_tag]} );


C<gen_delimited_pat> may also be called with an optional second argument,
which specifies the "escape" character(s) to be used for each delimiter.
For example to match a Pascal-style string (where ' is the delimiter
and '' is a literal ' within the string):

        gen_delimited_pat(q{'},q{'});

Different escape characters can be specified for different delimiters.
For example, to specify that '/' is the escape for single quotes
and '%' is the escape for double quotes:

        gen_delimited_pat(q{'"},q{/%});

If more delimiters than escape chars are specified, the last escape char
is used for the remaining delimiters.
If no escape char is specified for a given specified delimiter, '\' is used.

=head2 C<delimited_pat>

Note that C<gen_delimited_pat> was previously called C<delimited_pat>.
That name may still be used, but is now deprecated.
        

=head1 DIAGNOSTICS

In a list context, all the functions return C<(undef,$original_text)>
on failure. In a scalar context, failure is indicated by returning C<undef>
(in this case the input text is not modified in any way).

In addition, on failure in I<any> context, the C<$@> variable is set.
Accessing C<$@-E<gt>{error}> returns one of the error diagnostics listed
below.
Accessing C<$@-E<gt>{pos}> returns the offset into the original string at
which the error was detected (although not necessarily where it occurred!)
Printing C<$@> directly produces the error message, with the offset appended.
On success, the C<$@> variable is guaranteed to be C<undef>.

The available diagnostics are:

=over 4

=item  C<Did not find a suitable bracket: "%s">

The delimiter provided to C<extract_bracketed> was not one of
C<'()[]E<lt>E<gt>{}'>.

=item  C<Did not find prefix: /%s/>

A non-optional prefix was specified but wasn't found at the start of the text.

=item  C<Did not find opening bracket after prefix: "%s">

C<extract_bracketed> or C<extract_codeblock> was expecting a
particular kind of bracket at the start of the text, and didn't find it.

=item  C<No quotelike operator found after prefix: "%s">

C<extract_quotelike> didn't find one of the quotelike operators C<q>,
C<qq>, C<qw>, C<qx>, C<s>, C<tr> or C<y> at the start of the substring
it was extracting.

=item  C<Unmatched closing bracket: "%c">

C<extract_bracketed>, C<extract_quotelike> or C<extract_codeblock> encountered
a closing bracket where none was expected.

=item  C<Unmatched opening bracket(s): "%s">

C<extract_bracketed>, C<extract_quotelike> or C<extract_codeblock> ran 
out of characters in the text before closing one or more levels of nested
brackets.

=item C<Unmatched embedded quote (%s)>

C<extract_bracketed> attempted to match an embedded quoted substring, but
failed to find a closing quote to match it.

=item C<Did not find closing delimiter to match '%s'>

C<extract_quotelike> was unable to find a closing delimiter to match the
one that opened the quote-like operation.

=item  C<Mismatched closing bracket: expected "%c" but found "%s">

C<extract_bracketed>, C<extract_quotelike> or C<extract_codeblock> found
a valid bracket delimiter, but it was the wrong species. This usually
indicates a nesting error, but may indicate incorrect quoting or escaping.

=item  C<No block delimiter found after quotelike "%s">

C<extract_quotelike> or C<extract_codeblock> found one of the
quotelike operators C<q>, C<qq>, C<qw>, C<qx>, C<s>, C<tr> or C<y>
without a suitable block after it.

=item C<Did not find leading dereferencer>

C<extract_variable> was expecting one of '$', '@', or '%' at the start of
a variable, but didn't find any of them.

=item C<Bad identifier after dereferencer>

C<extract_variable> found a '$', '@', or '%' indicating a variable, but that
character was not followed by a legal Perl identifier.

=item C<Did not find expected opening bracket at %s>

C<extract_codeblock> failed to find any of the outermost opening brackets
that were specified.

=item C<Improperly nested codeblock at %s>

A nested code block was found that started with a delimiter that was specified
as being only to be used as an outermost bracket.

=item  C<Missing second block for quotelike "%s">

C<extract_codeblock> or C<extract_quotelike> found one of the
quotelike operators C<s>, C<tr> or C<y> followed by only one block.

=item C<No match found for opening bracket>

C<extract_codeblock> failed to find a closing bracket to match the outermost
opening bracket.

=item C<Did not find opening tag: /%s/>

C<extract_tagged> did not find a suitable opening tag (after any specified
prefix was removed).

=item C<Unable to construct closing tag to match: /%s/>

C<extract_tagged> matched the specified opening tag and tried to
modify the matched text to produce a matching closing tag (because
none was specified). It failed to generate the closing tag, almost
certainly because the opening tag did not start with a
bracket of some kind.

=item C<Found invalid nested tag: %s>

C<extract_tagged> found a nested tag that appeared in the "reject" list
(and the failure mode was not "MAX" or "PARA").

=item C<Found unbalanced nested tag: %s>

C<extract_tagged> found a nested opening tag that was not matched by a
corresponding nested closing tag (and the failure mode was not "MAX" or "PARA").

=item C<Did not find closing tag>

C<extract_tagged> reached the end of the text without finding a closing tag
to match the original opening tag (and the failure mode was not
"MAX" or "PARA").




=back


=head1 AUTHOR

Damian Conway (damian@conway.org)


=head1 BUGS AND IRRITATIONS

There are undoubtedly serious bugs lurking somewhere in this code, if
only because parts of it give the impression of understanding a great deal
more about Perl than they really do. 

Bug reports and other feedback are most welcome.


=head1 COPYRIGHT

 Copyright (c) 1997-2001, Damian Conway. All Rights Reserved.
 This module is free software. It may be used, redistributed
     and/or modified under the same terms as Perl itself.
