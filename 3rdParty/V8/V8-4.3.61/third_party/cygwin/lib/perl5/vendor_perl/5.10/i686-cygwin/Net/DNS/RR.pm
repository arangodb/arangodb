package Net::DNS::RR;
#
# $Id: RR.pm 705 2008-02-06 21:59:18Z olaf $
#
use strict;

BEGIN { 
    eval { require bytes; }
} 


use vars qw($VERSION $AUTOLOAD     %rrsortfunct );
use Carp;
use Net::DNS;
use Net::DNS::RR::Unknown;



$VERSION = (qw$LastChangedRevision: 705 $)[1];

=head1 NAME

Net::DNS::RR - DNS Resource Record class

=head1 SYNOPSIS

C<use Net::DNS::RR>

=head1 DESCRIPTION

C<Net::DNS::RR> is the base class for DNS Resource Record (RR) objects.
See also the manual pages for each RR type.

=head1 METHODS

B<WARNING!!!>  Don't assume the RR objects you receive from a query
are of a particular type -- always check an object's type before calling
any of its methods.  If you call an unknown method, you'll get a nasty
warning message and C<Net::DNS::RR> will return C<undef> to the caller.

=cut
#' Stupid Emacs (I Don't even USE emacs!) '

# %RR needs to be available within the scope of the BEGIN block.
# $RR_REGEX is a global just to be on the safe side.  
# %_LOADED is used internally for autoloading the RR subclasses.
use vars qw(%RR %_LOADED $RR_REGEX);

BEGIN {

	%RR = map { $_ => 1 } qw(
		A
		AAAA
		AFSDB
		CNAME
		CERT
		DNAME
		EID
		HINFO
		ISDN
		LOC
		MB
		MG
		MINFO
		MR
		MX
		NAPTR
		NIMLOC
		NS
		NSAP
		NULL
		PTR
		PX
		RP
		RT
		SOA
		SRV
		TKEY
		TSIG
		TXT
		X25
		OPT
		SSHFP
		SPF
		IPSECKEY
	);

	#  Only load DNSSEC if available

	eval { 	    
	    local $SIG{'__DIE__'} = 'DEFAULT';
	    require Net::DNS::RR::SIG; 
	};

	unless ($@) {
		$RR{'SIG'} = 1;
		eval { 	    
		    local $SIG{'__DIE__'} = 'DEFAULT';
		    require Net::DNS::RR::NXT; 
		};
		
		unless ($@) {
		    $RR{'NXT'}	= 1;
		} else {
		    die $@;
		}
		
		eval { 
		    local $SIG{'__DIE__'} = 'DEFAULT';
		    require Net::DNS::RR::KEY; 
		};
		
		unless ($@) {
		    $RR{'KEY'} = 1;
		} else {
		    die $@;
		}

	 	eval { 
		    local $SIG{'__DIE__'} = 'DEFAULT';
		    require Net::DNS::RR::DS; 
		};

	 	unless ($@) {
		    $RR{'DS'} = 1;

		} else {
		    die $@;
		}

	 	eval { 
		    local $SIG{'__DIE__'} = 'DEFAULT';
		    require Net::DNS::RR::RRSIG; 
		};

	 	unless ($@) {
		    $RR{'RRSIG'} = 1;
		    # If RRSIG is available so should the other DNSSEC types
		    eval {		    
			local $SIG{'__DIE__'} = 'DEFAULT';
			require Net::DNS::RR::NSEC; 
		    };
		    unless ($@) {
		      $RR{'NSEC'} = 1;
		    } else {
		    die $@;
		  }
		    eval { 
			local $SIG{'__DIE__'} = 'DEFAULT';
			require Net::DNS::RR::DNSKEY; 
		    };

		    unless ($@) {
		      $RR{'DNSKEY'} = 1;
		    } else {
		      die $@;
		    }
		} 

	 	eval { 
		  local $SIG{'__DIE__'} = 'DEFAULT';
		  require Net::DNS::RR::DLV; 
		};

		unless ($@) {
		  $RR{'DLV'} =1;
		} else {
		  # Die only if we are dealing with a version for which DLV is 
		  # available 
		  die $@ if defined ($Net::DNS::SEC::HAS_DLV) ;

		}

	 	eval { 
		  local $SIG{'__DIE__'} = 'DEFAULT';
		  require Net::DNS::RR::NSEC3; 
		};

		unless ($@) {
		  $RR{'NSEC3'} =1;
		} else {
		  # Die only if we are dealing with a version for which NSEC3 is		  # available 
		  die $@ if defined ($Net::DNS::SEC::HAS_NSEC3);
		}
		
		
	 	eval { 
		  local $SIG{'__DIE__'} = 'DEFAULT';
		  require Net::DNS::RR::NSEC3PARAM; 
		};

		unless ($@) {
		  $RR{'NSEC3PARAM'} =1;
		} else {
		  # Die only if we are dealing with a version for which NSEC3 is 
		  # available 

		  die $@ if defined($Net::DNS::SEC::SVNVERSION) &&  $Net::DNS::SEC::SVNVERSION > 619;   # In the code since. (for users of the SVN trunk)
		}



    }
}

sub build_regex {
	my $classes = join('|', keys %Net::DNS::classesbyname, 'CLASS\\d+');

	# Longest ones go first, so the regex engine will match AAAA before A.
	my $types   = join('|', sort { length $b <=> length $a } keys %Net::DNS::typesbyname);

	$types .= '|TYPE\\d+';
				
	$RR_REGEX   = " ^ 
					\\s*
    	            (\\S+) # name anything non-space will do 
    	            \\s*                
    	            (\\d+)?           
    	            \\s*
    	            ($classes)?
    	            \\s*
    	            ($types)?
    	            \\s*
    	            (.*)
    	            \$";

#	print STDERR "Regex: $RR_REGEX\n";
}


=head2 new (from string)

 $a     = Net::DNS::RR->new("foo.example.com. 86400 A 10.1.2.3");
 $mx    = Net::DNS::RR->new("example.com. 7200 MX 10 mailhost.example.com.");
 $cname = Net::DNS::RR->new("www.example.com 300 IN CNAME www1.example.com");
 $txt   = Net::DNS::RR->new('baz.example.com 3600 HS TXT "text record"');

Returns a C<Net::DNS::RR> object of the appropriate type and
initialized from the string passed by the user.  The format of the
string is that used in zone files, and is compatible with the string
returned by C<< Net::DNS::RR->string >>.

The name and RR type are required; all other information is optional.
If omitted, the TTL defaults to 0 and the RR class defaults to IN.
Omitting the optional fields is useful for creating the empty RDATA
sections required for certain dynamic update operations.  See the
C<Net::DNS::Update> manual page for additional examples.

All names must be fully qualified.  The trailing dot (.) is optional.

=head2 new (from hash)

 $rr = Net::DNS::RR->new(
	 name    => "foo.example.com",
	 ttl     => 86400,
	 class   => "IN",
	 type    => "A",
	 address => "10.1.2.3",
 );
 
 $rr = Net::DNS::RR->new(
	 name => "foo.example.com",
	 type => "A",
 );

Returns an RR object of the appropriate type, or a C<Net::DNS::RR>
object if the type isn't implemented.  See the manual pages for
each RR type to see what fields the type requires.

The C<Name> and C<Type> fields are required; all others are optional.
If omitted, C<TTL> defaults to 0 and C<Class> defaults to IN.  Omitting
the optional fields is useful for creating the empty RDATA sections
required for certain dynamic update operations.

The fields are case-insensitive, but starting each with uppercase
is recommended.

=cut

#' Stupid Emacs


sub new {
	return new_from_string(@_) if @_ == 2;
	return new_from_string(@_) if @_ == 3;
	
	return new_from_hash(@_);
}


sub new_from_data {
	my $class = shift;
	my ($name, $rrtype, $rrclass, $ttl, $rdlength, $data, $offset) = @_;

	my $self = {	name		=> $name,
			type		=> $rrtype,
			class		=> $rrclass,
			ttl		=> $ttl,
			rdlength	=> $rdlength,
			rdata		=> substr($$data, $offset, $rdlength)
			};

	if ($RR{$rrtype}) {
		my $subclass = $class->_get_subclass($rrtype);
		return $subclass->new($self, $data, $offset);
	} else {
		return Net::DNS::RR::Unknown->new($self, $data, $offset);
	}

}

sub new_from_string {
	my ($class, $rrstring, $update_type) = @_;

	build_regex() unless $RR_REGEX;

	# strip out comments
	# Test for non escaped ";" by means of the look-behind assertion
	# (the backslash is escaped)
	$rrstring   =~ s/(?<!\\);.*//og;
	
	($rrstring =~ m/$RR_REGEX/xso) || 
		confess qq|qInternal Error: "$rrstring" did not match RR pat.\nPlease report this to the author!\n|;

	my $name    = $1;
	my $ttl     = $2 || 0;
	my $rrclass = $3 || '';


	my $rrtype  = $4 || '';
	my $rdata   = $5 || '';

	$rdata =~ s/\s+$//o if $rdata;
	$name  =~ s/\.$//o  if $name;



	# RFC3597 tweaks
	# This converts to known class and type if specified as TYPE###
	$rrtype  = Net::DNS::typesbyval(Net::DNS::typesbyname($rrtype))      if $rrtype  =~ m/^TYPE\d+/o;
	$rrclass = Net::DNS::classesbyval(Net::DNS::classesbyname($rrclass)) if $rrclass =~ m/^CLASS\d+/o;


	if (!$rrtype && $rrclass && $rrclass eq 'ANY') {
		$rrtype  = 'ANY';
		$rrclass = 'IN';
	} elsif (!$rrclass) {
		$rrclass = 'IN';
	}

	$rrtype ||= 'ANY';
	

	if ($update_type) {
		$update_type = lc $update_type;
		
		if ($update_type eq 'yxrrset') {
			$ttl     = 0;
			$rrclass = 'ANY' unless $rdata;
		} elsif ($update_type eq 'nxrrset') {
			$ttl     = 0;
			$rrclass = 'NONE';
			$rdata   = '';
		} elsif ($update_type eq 'yxdomain') {
			$ttl     = 0;
			$rrclass = 'ANY';
			$rrtype  = 'ANY';
			$rdata   = '';
		} elsif ($update_type eq 'nxdomain') {
			$ttl     = 0;
			$rrclass = 'NONE';
			$rrtype  = 'ANY';
			$rdata   = '';
		} elsif ($update_type =~ /^(rr_)?add$/o) {
			$ttl = 86400 unless $ttl;
		} elsif ($update_type =~ /^(rr_)?del(ete)?$/o) {
			$ttl     = 0;
			$rrclass = $rdata ? 'NONE' : 'ANY';
		}
	}

	# We used to check if $rrtype was defined at this point.  However,
	# we just defaulted it to ANY earlier....

	my $self = {
		'name'     => $name,
		'type'     => $rrtype,
		'class'    => $rrclass,
		'ttl'      => $ttl,
		'rdlength' => 0,
		'rdata'    => '',
	};

	if ($RR{$rrtype} && $rdata !~ m/^\s*\\#/o ) {
		my $subclass = $class->_get_subclass($rrtype);
		return $subclass->new_from_string($self, $rdata);
	} elsif ($RR{$rrtype}) {   # A RR type known to Net::DNS starting with \#
		$rdata =~ m/\\\#\s+(\d+)\s+(.*)$/o;

		my $rdlength = $1;
		my $hexdump  = $2;		
		$hexdump =~ s/\s*//og;

		die "$rdata is inconsistent; length does not match content" 
			if length($hexdump) != $rdlength*2;

		$rdata = pack('H*', $hexdump);
	
		return Net::DNS::RR->new_from_data(
			$name, 
			$rrtype, 
			$rrclass, 
			$ttl, 
			$rdlength, 
			\$rdata, 
			length($rdata) - $rdlength
		);
	} elsif ($rdata=~/\s*\\\#\s+\d+\s+/o) {   
		#We are now dealing with the truly unknown.
		die 'Expected RFC3597 representation of RDATA' 
			unless $rdata =~ m/\\\#\s+(\d+)\s+(.*)$/o;

		my $rdlength = $1;
		my $hexdump  = $2;		
		$hexdump =~ s/\s*//og;

		die "$rdata is inconsistent; length does not match content" 
			if length($hexdump) != $rdlength*2;

		$rdata = pack('H*', $hexdump);

		return Net::DNS::RR->new_from_data(
			$name,
			$rrtype,
			$rrclass,
			$ttl,
			$rdlength,
			\$rdata,
			length($rdata) - $rdlength
		);
	} else {  
		#God knows how to handle these... bless them in the RR class.
		bless $self, $class;
		return $self
	}
	
}

sub new_from_hash {
	my $class    = shift;
	my %keyval   = @_;
	my $self     = {};

	while ( my ($key, $val) = each %keyval ) {
		( $self->{lc $key} = $val ) =~ s/\.+$// if defined $val;
	}

	croak('RR name not specified') unless defined $self->{name};
	croak('RR type not specified') unless defined $self->{type};

	$self->{'ttl'}   ||= 0;
	$self->{'class'} ||= 'IN';

	$self->{'rdlength'} = length $self->{'rdata'}
		if $self->{'rdata'};

	if ($RR{$self->{'type'}}) {
		my $subclass = $class->_get_subclass($self->{'type'});
	   
	    if (uc $self->{'type'} ne 'OPT') {
			bless $self, $subclass;
			
			return $self;
	    } else {  
			# Special processing of OPT. Since TTL and CLASS are
			# set by other variables. See Net::DNS::RR::OPT 
			# documentation
			return $subclass->new_from_hash($self);
	    }
	} elsif ($self->{'type'} =~ /TYPE\d+/o) {
		bless $self, 'Net::DNS::RR::Unknown';
		return $self;
	} else {
	 	bless $self, $class;
	 	return $self;
	}
}


=head2 parse

    ($rrobj, $offset) = Net::DNS::RR->parse(\$data, $offset);

Parses a DNS resource record at the specified location within a DNS packet.
The first argument is a reference to the packet data.
The second argument is the offset within the packet where the resource record begins.

Returns a Net::DNS::RR object and the offset of the next location in the packet.

Parsing is aborted if the object could not be created (e.g., corrupt or insufficient data).

=cut

use constant PACKED_LENGTH => length pack 'n2 N n', (0)x4;

sub parse {
	my ($objclass, $data, $offset) = @_;

	my ($name, $index) = Net::DNS::Packet::dn_expand($data, $offset);
	die 'Exception: corrupt or incomplete data' unless $index;

	my $rdindex = $index + PACKED_LENGTH;
	die 'Exception: incomplete data' if length $$data < $rdindex;
	my ($type, $class, $ttl, $rdlength) = unpack("\@$index n2 N n", $$data);

	my $next = $rdindex + $rdlength;
	die 'Exception: incomplete data' if length $$data < $next;

	$type = Net::DNS::typesbyval($type) || $type;

	# Special case for OPT RR where CLASS should be
	# interpreted as 16 bit unsigned (RFC2671, 4.3)
	if ($type ne 'OPT') {
		$class = Net::DNS::classesbyval($class) || $class;
	}
	# else just retain numerical value

	my $self = $objclass->new_from_data($name, $type, $class, $ttl, $rdlength, $data, $rdindex);
	die 'Exception: corrupt or incomplete RR subtype data' unless defined $self;

	return wantarray ? ($self, $next) : $self;
}


#
# Some people have reported that Net::DNS dies because AUTOLOAD picks up
# calls to DESTROY.
#
sub DESTROY {}

=head2 print

    $rr->print;

Prints the record to the standard output.  Calls the
B<string> method to get the RR's string representation.

=cut
#' someone said that emacs gets screwy here.  Who am I to claim otherwise...

sub print {	print &string, "\n"; }

=head2 string

    print $rr->string, "\n";

Returns a string representation of the RR.  Calls the
B<rdatastr> method to get the RR-specific data.

=cut

sub string {
	my $self = shift;
	my $data = $self->rdatastr || '; no data';
  
	join "\t", "$self->{name}.", $self->{ttl}, $self->{class}, $self->{type}, $data;
}

=head2 rdatastr

    $s = $rr->rdatastr;

Returns a string containing RR-specific data.  Subclasses will need
to implement this method.

=cut

sub rdatastr {
	my $self = shift;
	return exists $self->{'rdlength'}
	       ? "; rdlength = $self->{'rdlength'}"
	       : '';
}

=head2 name

    $name = $rr->name;

Returns the record's domain name.

=head2 type

    $type = $rr->type;

Returns the record's type.

=head2 class

    $class = $rr->class;

Returns the record's class.

=cut

# Used to AUTOLOAD this, but apparently some versions of Perl (specifically
# 5.003_07, included with some Linux distributions) would return the
# class the object was blessed into, instead of the RR's class.

sub class {
	my $self = shift;

	if (@_) {
		$self->{'class'} = shift;
	} elsif (!exists $self->{'class'}) {
		Carp::carp('class: no such method');
		return undef;
	}
	return $self->{'class'};
}
	

=head2 ttl

    $ttl = $rr->ttl;

Returns the record's time-to-live (TTL).

=head2 rdlength

    $rdlength = $rr->rdlength;

Returns the length of the record's data section.

=head2 rdata

    $rdata = $rr->rdata

Returns the record's data section as binary data.

=cut
#'
sub rdata {
	my $self = shift;
	my $retval = undef;

	if (@_ == 2) {
		my ($packet, $offset) = @_;
		$retval = $self->rr_rdata($packet, $offset);
	}
	elsif (exists $self->{'rdata'}) {
		$retval = $self->{'rdata'};
	}

	return $retval;
}

sub rr_rdata {
	my $self = shift;
	return exists $self->{'rdata'} ? $self->{'rdata'} : '';
}

#------------------------------------------------------------------------------
# sub data
#
# This method is called by Net::DNS::Packet->data to get the binary
# representation of an RR.
#------------------------------------------------------------------------------

sub data {
	my ($self, $packet, $offset) = @_;
	my $data;


	# Don't compress TSIG or TKEY names and don't mess with EDNS0 packets
	if (uc($self->{'type'}) eq 'TSIG' || uc($self->{'type'}) eq 'TKEY') {
		my $tmp_packet = Net::DNS::Packet->new();
		$data = $tmp_packet->dn_comp($self->{'name'}, 0);
		return undef unless defined $data;
	} elsif (uc($self->{'type'}) eq 'OPT') {
		my $tmp_packet = Net::DNS::Packet->new();
		$data = $tmp_packet->dn_comp('', 0);
	} else {
		$data  = $packet->dn_comp($self->{'name'}, $offset);
		return undef unless defined $data;	
	}

	my $qtype     = uc($self->{'type'});
	my $qtype_val = ($qtype =~ m/^\d+$/o) ? $qtype : Net::DNS::typesbyname($qtype);
	$qtype_val    = 0 if !defined($qtype_val);

	my $qclass     = uc($self->{'class'});
	my $qclass_val = ($qclass =~ m/^\d+$/o) ? $qclass : Net::DNS::classesbyname($qclass);
	$qclass_val    = 0 if !defined($qclass_val);
	$data .= pack('n', $qtype_val);
	
	# If the type is OPT then class will need to contain a decimal number
	# containing the UDP payload size. (RFC2671 section 4.3)
	if (uc($self->{'type'}) ne 'OPT') {
	    $data .= pack('n', $qclass_val);
	} else {
	    $data .= pack('n', $self->{'class'});
	}
	
	$data .= pack('N', $self->{'ttl'});

	$offset += length($data) + &Net::DNS::INT16SZ;	# allow for rdlength

	my $rdata = $self->rdata($packet, $offset);

	$data .= pack('n', length $rdata);
	$data.=$rdata;

	return $data;
}





#------------------------------------------------------------------------------
#  This method is called by SIG objects verify method. 
#  It is almost the same as data but needed to get an representation of the
#  packets in wire format withoud domain name compression.
#  It is essential to DNSSEC RFC 2535 section 8
#------------------------------------------------------------------------------

sub _canonicaldata {
	my $self = shift;
	my $data='';
	{   
	    my $name=$self->{'name'};
	    my @dname=Net::DNS::name2labels($name);
	    for (my $i=0;$i<@dname;$i++){
		$data .= pack ('C',length $dname[$i] );
		$data .= lc($dname[$i] );
	    }
	    $data .= pack ('C','0');
	}
	$data .= pack('n', Net::DNS::typesbyname(uc($self->{'type'})));
	$data .= pack('n', Net::DNS::classesbyname(uc($self->{'class'})));
	$data .= pack('N', $self->{'ttl'});
	
	
	my $rdata = $self->_canonicalRdata;
	
	$data .= pack('n', length $rdata);
	$data .= $rdata;
	return $data;


}

# These are methods that are used in the DNSSEC context...  Some RR
# have domain names in them. Verification works only on RRs with
# uncompressed domain names. (Canonical format as in sect 8 of
# RFC2535) _canonicalRdata is overwritten in those RR objects that
# have domain names in the RDATA and _name2wire is used to convert a
# domain name to "wire format"


sub _canonicalRdata {
    my $self=shift;
    my $packet=Net::DNS::Packet->new();
    my $rdata = $self->rr_rdata($packet,0);
    return $rdata;
}





sub _name2wire   {   
    my ($self, $name) = @_;

    my $rdata="";
    my $compname = "";
    my @dname = Net::DNS::name2labels($name);


    for (@dname) {
		$rdata .= pack('C', length $_);
		$rdata .= $_ ;
    }
    
    $rdata .= pack('C', '0');
    return $rdata;
}





sub AUTOLOAD {
	my ($self) = @_;  # If we do shift here, it will mess up the goto below.
	my ($name) = $AUTOLOAD =~ m/^.*::(.*)$/o;
	if ($name =~ /set_rrsort_func/){
	    return Net::DNS::RR::set_rrsort_func(@_);
	}
	if ($name =~ /get_rrsort_func/){
	    return Net::DNS::RR::get_rrsort_func(@_);
	}
	# XXX -- We should test that we do in fact carp on unknown methods.	
	unless (exists $self->{$name}) {
	    my $rr_string = $self->string;
	    Carp::carp(<<"AMEN");
	    
***
***  WARNING!!!  The program has attempted to call the method
***  "$name" for the following RR object:
***
***  $rr_string
***
***  This object does not have a method "$name".  THIS IS A BUG
***  IN THE CALLING SOFTWARE, which has incorrectly assumed that
***  the object would be of a particular type.  The calling
***  software should check the type of each RR object before
***  calling any of its methods.
***
***  Net::DNS has returned undef to the caller.
*** 

AMEN
return;
	}
	
	no strict q/refs/;
	
	# Build a method in the class.
	*{$AUTOLOAD} = sub {
	    my ($self, $new_val) = @_;
	    
	    if (defined $new_val) {
		$self->{$name} = $new_val;
	    }
	    
	    return $self->{$name};
	};
	
	# And jump over to it.
	goto &{$AUTOLOAD};
}



#
#  Net::DNS::RR->_get_subclass($type)
#
# Return a subclass, after loading a subclass (if needed)
#
sub _get_subclass {
	my ($class, $type) = @_;
	
	return unless $type and $RR{$type};
	
	my $subclass = join('::', $class, $type);
	
	unless ($_LOADED{$subclass}) {
		eval "require $subclass";
		die $@ if $@;
		$_LOADED{$subclass}++;
	}
	
	return $subclass;
}	




=head1 Sorting of RR arrays

As of version 0.55 there is functionality to help you sort RR
arrays. The sorting is done by Net::DNS::rrsort(), see the
L<Net::DNS> documentation. This package provides class methods to set
the sorting functions used for a particular RR based on a particular
attribute.


=head2 set_rrsort_func

Net::DNS::RR::SRV->set_rrsort_func("priority",
			       sub {
				   my ($a,$b)=($Net::DNS::a,$Net::DNS::b);
				   $a->priority <=> $b->priority
				   ||
				   $b->weight <=> $a->weight
                     }

Net::DNS::RR::SRV->set_rrsort_func("default_sort",
			       sub {
				   my ($a,$b)=($Net::DNS::a,$Net::DNS::b);
				   $a->priority <=> $b->priority
				   ||
				   $b->weight <=> $a->weight
                     }

set_rrsort_func needs to be called as a class method. The first
argument is the attribute name on which the sorting will need to take
place. If you specify "default_sort" than that is the sort algorithm
that will be used in the case that rrsort() is called without an RR
attribute as argument.

The second argument is a reference to a function that uses the
variables $a and $b global to the C<from Net::DNS>(!!)package for the
sorting. During the sorting $a and $b will contain references to
objects from the class you called the set_prop_sort from. In other
words, you can rest assured that the above sorting function will only
get Net::DNS::RR::SRV objects.

The above example is the sorting function that actually is implemented in 
SRV.

=cut




sub set_rrsort_func{
    my $class=shift;
    my $attribute=shift;
    my $funct=shift;
#    print "Using ".__PACKAGE__."set_rrsort: $class\n";
    my ($type) = $class =~ m/^.*::(.*)$/o;
    $Net::DNS::RR::rrsortfunct{$type}{$attribute}=$funct;
}
		
sub get_rrsort_func {
    my $class=shift;    
    my $attribute=shift;  #can be undefined.
    my $sortsub;
    my ($type) = $class =~ m/^.*::(.*)$/o;


#    print "Using ".__PACKAGE__." get_rrsort: $class ($type,$attribute)\n";
#    use Data::Dumper;
#    print Dumper %Net::DNS::rrsortfunct;

    if (defined($attribute) &&
	exists($Net::DNS::RR::rrsortfunct{$type}) &&
	exists($Net::DNS::RR::rrsortfunct{$type}{$attribute})
	){
	#  The default overwritten by the class variable in Net::DNS
	return $Net::DNS::RR::rrsortfunct{$type}{$attribute};
    }elsif(
	! defined($attribute) &&
	exists($Net::DNS::RR::rrsortfunct{$type}) &&
	exists($Net::DNS::RR::rrsortfunct{$type}{'default_sort'})
	){
	#  The default overwritten by the class variable in Net::DNS
	return $Net::DNS::RR::rrsortfunct{$type}{'default_sort'};
    }
    elsif( defined($attribute) ){
	
	return sub{
	    my ($a,$b)=($Net::DNS::a,$Net::DNS::b);
	    ( exists($a->{$attribute}) &&   
	      $a->{$attribute} <=> $b->{$attribute})
		||
		$a->_canonicaldata() cmp $b->_canonicaldata()
	};
    }else{
	return sub{
	    my ($a,$b)=($Net::DNS::a,$Net::DNS::b);
	    $a->_canonicaldata() cmp $b->_canonicaldata()
	};
    }    

    return $sortsub;
}






	
sub STORABLE_freeze {
	my ($self, $cloning) = @_;

	return if $cloning;
	
	return ('', {%$self});
}

sub STORABLE_thaw {
	my ($self, $cloning, undef, $data) = @_;

	%{$self}  = %{$data};
	
	__PACKAGE__->_get_subclass($self->{'type'});
	
	return $self;
}

=head1 BUGS

This version of C<Net::DNS::RR> does little sanity checking on user-created
RR objects.

=head1 COPYRIGHT

Copyright (c) 1997-2002 Michael Fuhr. 

Portions Copyright (c) 2002-2004 Chris Reinhardt.

Portions Copyright (c) 2005-2007 Olaf Kolkman 

Portions Copyright (c) 2007 Dick Franks 

All rights reserved.  This program is free software; you may redistribute
it and/or modify it under the same terms as Perl itself.

EDNS0 extensions by Olaf Kolkman.

=head1 SEE ALSO

L<perl(1)>, L<Net::DNS>, L<Net::DNS::Resolver>, L<Net::DNS::Packet>,
L<Net::DNS::Update>, L<Net::DNS::Header>, L<Net::DNS::Question>,
RFC 1035 Section 4.1.3

=cut

1;
