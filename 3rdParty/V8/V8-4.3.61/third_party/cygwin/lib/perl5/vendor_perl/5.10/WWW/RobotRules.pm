package WWW::RobotRules;

$VERSION = "5.810";
sub Version { $VERSION; }

use strict;
use URI ();



sub new {
    my($class, $ua) = @_;

    # This ugly hack is needed to ensure backwards compatibility.
    # The "WWW::RobotRules" class is now really abstract.
    $class = "WWW::RobotRules::InCore" if $class eq "WWW::RobotRules";

    my $self = bless { }, $class;
    $self->agent($ua);
    $self;
}


sub parse {
    my($self, $robot_txt_uri, $txt, $fresh_until) = @_;
    $robot_txt_uri = URI->new("$robot_txt_uri");
    my $netloc = $robot_txt_uri->host . ":" . $robot_txt_uri->port;

    $self->clear_rules($netloc);
    $self->fresh_until($netloc, $fresh_until || (time + 365*24*3600));

    my $ua;
    my $is_me = 0;		# 1 iff this record is for me
    my $is_anon = 0;		# 1 iff this record is for *
    my $seen_disallow = 0;      # watch for missing record separators
    my @me_disallowed = ();	# rules disallowed for me
    my @anon_disallowed = ();	# rules disallowed for *

    # blank lines are significant, so turn CRLF into LF to avoid generating
    # false ones
    $txt =~ s/\015\012/\012/g;

    # split at \012 (LF) or \015 (CR) (Mac text files have just CR for EOL)
    for(split(/[\012\015]/, $txt)) {

	# Lines containing only a comment are discarded completely, and
        # therefore do not indicate a record boundary.
	next if /^\s*\#/;

	s/\s*\#.*//;        # remove comments at end-of-line

	if (/^\s*$/) {	    # blank line
	    last if $is_me; # That was our record. No need to read the rest.
	    $is_anon = 0;
	    $seen_disallow = 0;
	}
        elsif (/^\s*User-Agent\s*:\s*(.*)/i) {
	    $ua = $1;
	    $ua =~ s/\s+$//;

	    if ($seen_disallow) {
		# treat as start of a new record
		$seen_disallow = 0;
		last if $is_me; # That was our record. No need to read the rest.
		$is_anon = 0;
	    }

	    if ($is_me) {
		# This record already had a User-agent that
		# we matched, so just continue.
	    }
	    elsif ($ua eq '*') {
		$is_anon = 1;
	    }
	    elsif($self->is_me($ua)) {
		$is_me = 1;
	    }
	}
	elsif (/^\s*Disallow\s*:\s*(.*)/i) {
	    unless (defined $ua) {
		warn "RobotRules <$robot_txt_uri>: Disallow without preceding User-agent\n" if $^W;
		$is_anon = 1;  # assume that User-agent: * was intended
	    }
	    my $disallow = $1;
	    $disallow =~ s/\s+$//;
	    $seen_disallow = 1;
	    if (length $disallow) {
		my $ignore;
		eval {
		    my $u = URI->new_abs($disallow, $robot_txt_uri);
		    $ignore++ if $u->scheme ne $robot_txt_uri->scheme;
		    $ignore++ if lc($u->host) ne lc($robot_txt_uri->host);
		    $ignore++ if $u->port ne $robot_txt_uri->port;
		    $disallow = $u->path_query;
		    $disallow = "/" unless length $disallow;
		};
		next if $@;
		next if $ignore;
	    }

	    if ($is_me) {
		push(@me_disallowed, $disallow);
	    }
	    elsif ($is_anon) {
		push(@anon_disallowed, $disallow);
	    }
	}
	else {
	    warn "RobotRules <$robot_txt_uri>: Unexpected line: $_\n" if $^W;
	}
    }

    if ($is_me) {
	$self->push_rules($netloc, @me_disallowed);
    }
    else {
	$self->push_rules($netloc, @anon_disallowed);
    }
}


#
# Returns TRUE if the given name matches the
# name of this robot
#
sub is_me {
    my($self, $ua_line) = @_;
    my $me = $self->agent;

    # See whether my short-name is a substring of the
    #  "User-Agent: ..." line that we were passed:
    
    if(index(lc($me), lc($ua_line)) >= 0) {
      LWP::Debug::debug("\"$ua_line\" applies to \"$me\"")
       if defined &LWP::Debug::debug;
      return 1;
    }
    else {
      LWP::Debug::debug("\"$ua_line\" does not apply to \"$me\"")
       if defined &LWP::Debug::debug;
      return '';
    }
}


sub allowed {
    my($self, $uri) = @_;
    $uri = URI->new("$uri");
    
    return 1 unless $uri->scheme eq 'http' or $uri->scheme eq 'https';
     # Robots.txt applies to only those schemes.
    
    my $netloc = $uri->host . ":" . $uri->port;

    my $fresh_until = $self->fresh_until($netloc);
    return -1 if !defined($fresh_until) || $fresh_until < time;

    my $str = $uri->path_query;
    my $rule;
    for $rule ($self->rules($netloc)) {
	return 1 unless length $rule;
	return 0 if index($str, $rule) == 0;
    }
    return 1;
}


# The following methods must be provided by the subclass.
sub agent;
sub visit;
sub no_visits;
sub last_visits;
sub fresh_until;
sub push_rules;
sub clear_rules;
sub rules;
sub dump;



package WWW::RobotRules::InCore;

use vars qw(@ISA);
@ISA = qw(WWW::RobotRules);



sub agent {
    my ($self, $name) = @_;
    my $old = $self->{'ua'};
    if ($name) {
        # Strip it so that it's just the short name.
        # I.e., "FooBot"                                      => "FooBot"
        #       "FooBot/1.2"                                  => "FooBot"
        #       "FooBot/1.2 [http://foobot.int; foo@bot.int]" => "FooBot"

	$name = $1 if $name =~ m/(\S+)/; # get first word
	$name =~ s!/.*!!;  # get rid of version
	unless ($old && $old eq $name) {
	    delete $self->{'loc'}; # all old info is now stale
	    $self->{'ua'} = $name;
	}
    }
    $old;
}


sub visit {
    my($self, $netloc, $time) = @_;
    return unless $netloc;
    $time ||= time;
    $self->{'loc'}{$netloc}{'last'} = $time;
    my $count = \$self->{'loc'}{$netloc}{'count'};
    if (!defined $$count) {
	$$count = 1;
    }
    else {
	$$count++;
    }
}


sub no_visits {
    my ($self, $netloc) = @_;
    $self->{'loc'}{$netloc}{'count'};
}


sub last_visit {
    my ($self, $netloc) = @_;
    $self->{'loc'}{$netloc}{'last'};
}


sub fresh_until {
    my ($self, $netloc, $fresh_until) = @_;
    my $old = $self->{'loc'}{$netloc}{'fresh'};
    if (defined $fresh_until) {
	$self->{'loc'}{$netloc}{'fresh'} = $fresh_until;
    }
    $old;
}


sub push_rules {
    my($self, $netloc, @rules) = @_;
    push (@{$self->{'loc'}{$netloc}{'rules'}}, @rules);
}


sub clear_rules {
    my($self, $netloc) = @_;
    delete $self->{'loc'}{$netloc}{'rules'};
}


sub rules {
    my($self, $netloc) = @_;
    if (defined $self->{'loc'}{$netloc}{'rules'}) {
	return @{$self->{'loc'}{$netloc}{'rules'}};
    }
    else {
	return ();
    }
}


sub dump
{
    my $self = shift;
    for (keys %$self) {
	next if $_ eq 'loc';
	print "$_ = $self->{$_}\n";
    }
    for (keys %{$self->{'loc'}}) {
	my @rules = $self->rules($_);
	print "$_: ", join("; ", @rules), "\n";
    }
}


1;

__END__


# Bender: "Well, I don't have anything else
#          planned for today.  Let's get drunk!"

=head1 NAME

WWW::RobotRules - database of robots.txt-derived permissions

=head1 SYNOPSIS

 use WWW::RobotRules;
 my $rules = WWW::RobotRules->new('MOMspider/1.0');

 use LWP::Simple qw(get);

 {
   my $url = "http://some.place/robots.txt";
   my $robots_txt = get $url;
   $rules->parse($url, $robots_txt) if defined $robots_txt;
 }

 {
   my $url = "http://some.other.place/robots.txt";
   my $robots_txt = get $url;
   $rules->parse($url, $robots_txt) if defined $robots_txt;
 }

 # Now we can check if a URL is valid for those servers
 # whose "robots.txt" files we've gotten and parsed:
 if($rules->allowed($url)) {
     $c = get $url;
     ...
 }

=head1 DESCRIPTION

This module parses F</robots.txt> files as specified in
"A Standard for Robot Exclusion", at
<http://www.robotstxt.org/wc/norobots.html>
Webmasters can use the F</robots.txt> file to forbid conforming
robots from accessing parts of their web site.

The parsed files are kept in a WWW::RobotRules object, and this object
provides methods to check if access to a given URL is prohibited.  The
same WWW::RobotRules object can be used for one or more parsed
F</robots.txt> files on any number of hosts.

The following methods are provided:

=over 4

=item $rules = WWW::RobotRules->new($robot_name)

This is the constructor for WWW::RobotRules objects.  The first
argument given to new() is the name of the robot.

=item $rules->parse($robot_txt_url, $content, $fresh_until)

The parse() method takes as arguments the URL that was used to
retrieve the F</robots.txt> file, and the contents of the file.

=item $rules->allowed($uri)

Returns TRUE if this robot is allowed to retrieve this URL.

=item $rules->agent([$name])

Get/set the agent name. NOTE: Changing the agent name will clear the robots.txt
rules and expire times out of the cache.

=back

=head1 ROBOTS.TXT

The format and semantics of the "/robots.txt" file are as follows
(this is an edited abstract of
<http://www.robotstxt.org/wc/norobots.html> ):

The file consists of one or more records separated by one or more
blank lines. Each record contains lines of the form

  <field-name>: <value>

The field name is case insensitive.  Text after the '#' character on a
line is ignored during parsing.  This is used for comments.  The
following <field-names> can be used:

=over 3

=item User-Agent

The value of this field is the name of the robot the record is
describing access policy for.  If more than one I<User-Agent> field is
present the record describes an identical access policy for more than
one robot. At least one field needs to be present per record.  If the
value is '*', the record describes the default access policy for any
robot that has not not matched any of the other records.

The I<User-Agent> fields must occur before the I<Disallow> fields.  If a
record contains a I<User-Agent> field after a I<Disallow> field, that
constitutes a malformed record.  This parser will assume that a blank
line should have been placed before that I<User-Agent> field, and will
break the record into two.  All the fields before the I<User-Agent> field
will constitute a record, and the I<User-Agent> field will be the first
field in a new record.

=item Disallow

The value of this field specifies a partial URL that is not to be
visited. This can be a full path, or a partial path; any URL that
starts with this value will not be retrieved

=back

=head1 ROBOTS.TXT EXAMPLES

The following example "/robots.txt" file specifies that no robots
should visit any URL starting with "/cyberworld/map/" or "/tmp/":

  User-agent: *
  Disallow: /cyberworld/map/ # This is an infinite virtual URL space
  Disallow: /tmp/ # these will soon disappear

This example "/robots.txt" file specifies that no robots should visit
any URL starting with "/cyberworld/map/", except the robot called
"cybermapper":

  User-agent: *
  Disallow: /cyberworld/map/ # This is an infinite virtual URL space

  # Cybermapper knows where to go.
  User-agent: cybermapper
  Disallow:

This example indicates that no robots should visit this site further:

  # go away
  User-agent: *
  Disallow: /

This is an example of a malformed robots.txt file.

  # robots.txt for ancientcastle.example.com
  # I've locked myself away.
  User-agent: *
  Disallow: /
  # The castle is your home now, so you can go anywhere you like.
  User-agent: Belle
  Disallow: /west-wing/ # except the west wing!
  # It's good to be the Prince...
  User-agent: Beast
  Disallow: 

This file is missing the required blank lines between records.
However, the intention is clear.

=head1 SEE ALSO

L<LWP::RobotUA>, L<WWW::RobotRules::AnyDBM_File>
