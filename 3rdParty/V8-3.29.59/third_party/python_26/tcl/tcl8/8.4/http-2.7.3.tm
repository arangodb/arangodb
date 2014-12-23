# http.tcl --
#
#	Client-side HTTP for GET, POST, and HEAD commands. These routines can
#	be used in untrusted code that uses the Safesock security policy.
#	These procedures use a callback interface to avoid using vwait, which
#	is not defined in the safe base.
#
# See the file "license.terms" for information on usage and redistribution of
# this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: http.tcl,v 1.67.2.6 2009/04/09 17:05:39 dgp Exp $

package require Tcl 8.4
# Keep this in sync with pkgIndex.tcl and with the install directories in
# Makefiles
package provide http 2.7.3

namespace eval http {
    # Allow resourcing to not clobber existing data

    variable http
    if {![info exists http]} {
	array set http {
	    -accept */*
	    -proxyhost {}
	    -proxyport {}
	    -proxyfilter http::ProxyRequired
	    -urlencoding utf-8
	}
	set http(-useragent) "Tcl http client package [package provide http]"
    }

    proc init {} {
	# Set up the map for quoting chars. RFC3986 Section 2.3 say percent
	# encode all except: "... percent-encoded octets in the ranges of
	# ALPHA (%41-%5A and %61-%7A), DIGIT (%30-%39), hyphen (%2D), period
	# (%2E), underscore (%5F), or tilde (%7E) should not be created by URI
	# producers ..."
	for {set i 0} {$i <= 256} {incr i} {
	    set c [format %c $i]
	    if {![string match {[-._~a-zA-Z0-9]} $c]} {
		set map($c) %[format %.2x $i]
	    }
	}
	# These are handled specially
	set map(\n) %0d%0a
	variable formMap [array get map]

	# Create a map for HTTP/1.1 open sockets
	variable socketmap
	if {[info exists socketmap]} {
	    # Close but don't remove open sockets on re-init
	    foreach {url sock} [array get socketmap] {
		catch {close $sock}
	    }
	}
	array set socketmap {}
    }
    init

    variable urlTypes
    if {![info exists urlTypes]} {
	set urlTypes(http) [list 80 ::socket]
    }

    variable encodings [string tolower [encoding names]]
    # This can be changed, but iso8859-1 is the RFC standard.
    variable defaultCharset
    if {![info exists defaultCharset]} {
	set defaultCharset "iso8859-1"
    }

    # Force RFC 3986 strictness in geturl url verification?
    variable strict
    if {![info exists strict]} {
	set strict 1
    }

    # Let user control default keepalive for compatibility
    variable defaultKeepalive
    if {![info exists defaultKeepalive]} {
	set defaultKeepalive 0
    }

    namespace export geturl config reset wait formatQuery register unregister
    # Useful, but not exported: data size status code
}

# http::Log --
#
#	Debugging output -- define this to observe HTTP/1.1 socket usage.
#	Should echo any args received.
#
# Arguments:
#     msg	Message to output
#
proc http::Log {args} {}

# http::register --
#
#     See documentation for details.
#
# Arguments:
#     proto	URL protocol prefix, e.g. https
#     port	Default port for protocol
#     command	Command to use to create socket
# Results:
#     list of port and command that was registered.

proc http::register {proto port command} {
    variable urlTypes
    set urlTypes($proto) [list $port $command]
}

# http::unregister --
#
#     Unregisters URL protocol handler
#
# Arguments:
#     proto	URL protocol prefix, e.g. https
# Results:
#     list of port and command that was unregistered.

proc http::unregister {proto} {
    variable urlTypes
    if {![info exists urlTypes($proto)]} {
	return -code error "unsupported url type \"$proto\""
    }
    set old $urlTypes($proto)
    unset urlTypes($proto)
    return $old
}

# http::config --
#
#	See documentation for details.
#
# Arguments:
#	args		Options parsed by the procedure.
# Results:
#        TODO

proc http::config {args} {
    variable http
    set options [lsort [array names http -*]]
    set usage [join $options ", "]
    if {[llength $args] == 0} {
	set result {}
	foreach name $options {
	    lappend result $name $http($name)
	}
	return $result
    }
    set options [string map {- ""} $options]
    set pat ^-(?:[join $options |])$
    if {[llength $args] == 1} {
	set flag [lindex $args 0]
	if {![regexp -- $pat $flag]} {
	    return -code error "Unknown option $flag, must be: $usage"
	}
	return $http($flag)
    } else {
	foreach {flag value} $args {
	    if {![regexp -- $pat $flag]} {
		return -code error "Unknown option $flag, must be: $usage"
	    }
	    set http($flag) $value
	}
    }
}

# http::Finish --
#
#	Clean up the socket and eval close time callbacks
#
# Arguments:
#	token	    Connection token.
#	errormsg    (optional) If set, forces status to error.
#       skipCB      (optional) If set, don't call the -command callback. This
#		    is useful when geturl wants to throw an exception instead
#		    of calling the callback. That way, the same error isn't
#		    reported to two places.
#
# Side Effects:
#        Closes the socket

proc http::Finish {token {errormsg ""} {skipCB 0}} {
    variable $token
    upvar 0 $token state
    global errorInfo errorCode
    if {$errormsg ne ""} {
	set state(error) [list $errormsg $errorInfo $errorCode]
	set state(status) "error"
    }
    if {
	($state(status) eq "timeout") || ($state(status) eq "error") ||
	([info exists state(connection)] && ($state(connection) eq "close"))
    } then {
        CloseSocket $state(sock) $token
    }
    if {[info exists state(after)]} {
	after cancel $state(after)
    }
    if {[info exists state(-command)] && !$skipCB} {
	if {[catch {eval $state(-command) {$token}} err]} {
	    if {$errormsg eq ""} {
		set state(error) [list $err $errorInfo $errorCode]
		set state(status) error
	    }
	}
	# Command callback may already have unset our state
	unset -nocomplain state(-command)
    }
}

# http::CloseSocket -
#
#	Close a socket and remove it from the persistent sockets table.  If
#	possible an http token is included here but when we are called from a
#	fileevent on remote closure we need to find the correct entry - hence
#	the second section.

proc ::http::CloseSocket {s {token {}}} {
    variable socketmap
    catch {fileevent $s readable {}}
    set conn_id {}
    if {$token ne ""} {
        variable $token
        upvar 0 $token state
        if {[info exists state(socketinfo)]} {
	    set conn_id $state(socketinfo)
        }
    } else {
        set map [array get socketmap]
        set ndx [lsearch -exact $map $s]
        if {$ndx != -1} {
	    incr ndx -1
	    set conn_id [lindex $map $ndx]
        }
    }
    if {$conn_id eq {} || ![info exists socketmap($conn_id)]} {
        Log "Closing socket $s (no connection info)"
        if {[catch {close $s} err]} {
	    Log "Error: $err"
	}
    } else {
	if {[info exists socketmap($conn_id)]} {
	    Log "Closing connection $conn_id (sock $socketmap($conn_id))"
	    if {[catch {close $socketmap($conn_id)} err]} {
		Log "Error: $err"
	    }
	    unset socketmap($conn_id)
	} else {
	    Log "Cannot close connection $conn_id - no socket in socket map"
	}
    }
}

# http::reset --
#
#	See documentation for details.
#
# Arguments:
#	token	Connection token.
#	why	Status info.
#
# Side Effects:
#       See Finish

proc http::reset {token {why reset}} {
    variable $token
    upvar 0 $token state
    set state(status) $why
    catch {fileevent $state(sock) readable {}}
    catch {fileevent $state(sock) writable {}}
    Finish $token
    if {[info exists state(error)]} {
	set errorlist $state(error)
	unset state
	eval ::error $errorlist
    }
}

# http::geturl --
#
#	Establishes a connection to a remote url via http.
#
# Arguments:
#       url		The http URL to goget.
#       args		Option value pairs. Valid options include:
#				-blocksize, -validate, -headers, -timeout
# Results:
#	Returns a token for this connection. This token is the name of an
#	array that the caller should unset to garbage collect the state.

proc http::geturl {url args} {
    variable http
    variable urlTypes
    variable defaultCharset
    variable defaultKeepalive
    variable strict

    # Initialize the state variable, an array. We'll return the name of this
    # array as the token for the transaction.

    if {![info exists http(uid)]} {
	set http(uid) 0
    }
    set token [namespace current]::[incr http(uid)]
    variable $token
    upvar 0 $token state
    reset $token

    # Process command options.

    array set state {
	-binary		false
	-blocksize	8192
	-queryblocksize 8192
	-validate	0
	-headers	{}
	-timeout	0
	-type		application/x-www-form-urlencoded
	-queryprogress	{}
	-protocol	1.1
	binary		0
	state		connecting
	meta		{}
	coding		{}
	currentsize	0
	totalsize	0
	querylength	0
	queryoffset	0
	type		text/html
	body		{}
	status		""
	http		""
	connection	close
    }
    set state(-keepalive) $defaultKeepalive
    set state(-strict) $strict
    # These flags have their types verified [Bug 811170]
    array set type {
	-binary		boolean
	-blocksize	integer
	-queryblocksize integer
	-strict		boolean
	-timeout	integer
	-validate	boolean
    }
    set state(charset)	$defaultCharset
    set options {
	-binary -blocksize -channel -command -handler -headers -keepalive
	-method -myaddr -progress -protocol -query -queryblocksize
	-querychannel -queryprogress -strict -timeout -type -validate
    }
    set usage [join [lsort $options] ", "]
    set options [string map {- ""} $options]
    set pat ^-(?:[join $options |])$
    foreach {flag value} $args {
	if {[regexp -- $pat $flag]} {
	    # Validate numbers
	    if {
		[info exists type($flag)] &&
		![string is $type($flag) -strict $value]
	    } then {
		unset $token
		return -code error \
		    "Bad value for $flag ($value), must be $type($flag)"
	    }
	    set state($flag) $value
	} else {
	    unset $token
	    return -code error "Unknown option $flag, can be: $usage"
	}
    }

    # Make sure -query and -querychannel aren't both specified

    set isQueryChannel [info exists state(-querychannel)]
    set isQuery [info exists state(-query)]
    if {$isQuery && $isQueryChannel} {
	unset $token
	return -code error "Can't combine -query and -querychannel options!"
    }

    # Validate URL, determine the server host and port, and check proxy case
    # Recognize user:pass@host URLs also, although we do not do anything with
    # that info yet.

    # URLs have basically four parts.
    # First, before the colon, is the protocol scheme (e.g. http)
    # Second, for HTTP-like protocols, is the authority
    #	The authority is preceded by // and lasts up to (but not including)
    #	the following / and it identifies up to four parts, of which only one,
    #	the host, is required (if an authority is present at all). All other
    #	parts of the authority (user name, password, port number) are optional.
    # Third is the resource name, which is split into two parts at a ?
    #	The first part (from the single "/" up to "?") is the path, and the
    #	second part (from that "?" up to "#") is the query. *HOWEVER*, we do
    #	not need to separate them; we send the whole lot to the server.
    # Fourth is the fragment identifier, which is everything after the first
    #	"#" in the URL. The fragment identifier MUST NOT be sent to the server
    #	and indeed, we don't bother to validate it (it could be an error to
    #	pass it in here, but it's cheap to strip).
    #
    # An example of a URL that has all the parts:
    #
    #     http://jschmoe:xyzzy@www.bogus.net:8000/foo/bar.tml?q=foo#changes
    #
    # The "http" is the protocol, the user is "jschmoe", the password is
    # "xyzzy", the host is "www.bogus.net", the port is "8000", the path is
    # "/foo/bar.tml", the query is "q=foo", and the fragment is "changes".
    #
    # Note that the RE actually combines the user and password parts, as
    # recommended in RFC 3986. Indeed, that RFC states that putting passwords
    # in URLs is a Really Bad Idea, something with which I would agree utterly.
    # Also note that we do not currently support IPv6 addresses.
    #
    # From a validation perspective, we need to ensure that the parts of the
    # URL that are going to the server are correctly encoded.  This is only
    # done if $state(-strict) is true (inherited from $::http::strict).

    set URLmatcher {(?x)		# this is _expanded_ syntax
	^
	(?: (\w+) : ) ?			# <protocol scheme>
	(?: //
	    (?:
		(
		    [^@/\#?]+		# <userinfo part of authority>
		) @
	    )?
	    ( [^/:\#?]+ )		# <host part of authority>
	    (?: : (\d+) )?		# <port part of authority>
	)?
	( / [^\#?]* (?: \? [^\#?]* )?)?	# <path> (including query)
	(?: \# (.*) )?			# <fragment>
	$
    }

    # Phase one: parse
    if {![regexp -- $URLmatcher $url -> proto user host port srvurl]} {
	unset $token
	return -code error "Unsupported URL: $url"
    }
    # Phase two: validate
    if {$host eq ""} {
	# Caller has to provide a host name; we do not have a "default host"
	# that would enable us to handle relative URLs.
	unset $token
	return -code error "Missing host part: $url"
	# Note that we don't check the hostname for validity here; if it's
	# invalid, we'll simply fail to resolve it later on.
    }
    if {$port ne "" && $port > 65535} {
	unset $token
	return -code error "Invalid port number: $port"
    }
    # The user identification and resource identification parts of the URL can
    # have encoded characters in them; take care!
    if {$user ne ""} {
	# Check for validity according to RFC 3986, Appendix A
	set validityRE {(?xi)
	    ^
	    (?: [-\w.~!$&'()*+,;=:] | %[0-9a-f][0-9a-f] )+
	    $
	}
	if {$state(-strict) && ![regexp -- $validityRE $user]} {
	    unset $token
	    # Provide a better error message in this error case
	    if {[regexp {(?i)%(?![0-9a-f][0-9a-f]).?.?} $user bad]} {
		return -code error \
			"Illegal encoding character usage \"$bad\" in URL user"
	    }
	    return -code error "Illegal characters in URL user"
	}
    }
    if {$srvurl ne ""} {
	# Check for validity according to RFC 3986, Appendix A
	set validityRE {(?xi)
	    ^
	    # Path part (already must start with / character)
	    (?:	      [-\w.~!$&'()*+,;=:@/]  | %[0-9a-f][0-9a-f] )*
	    # Query part (optional, permits ? characters)
	    (?: \? (?: [-\w.~!$&'()*+,;=:@/?] | %[0-9a-f][0-9a-f] )* )?
	    $
	}
	if {$state(-strict) && ![regexp -- $validityRE $srvurl]} {
	    unset $token
	    # Provide a better error message in this error case
	    if {[regexp {(?i)%(?![0-9a-f][0-9a-f])..} $srvurl bad]} {
		return -code error \
		    "Illegal encoding character usage \"$bad\" in URL path"
	    }
	    return -code error "Illegal characters in URL path"
	}
    } else {
	set srvurl /
    }
    if {$proto eq ""} {
	set proto http
    }
    if {![info exists urlTypes($proto)]} {
	unset $token
	return -code error "Unsupported URL type \"$proto\""
    }
    set defport [lindex $urlTypes($proto) 0]
    set defcmd [lindex $urlTypes($proto) 1]

    if {$port eq ""} {
	set port $defport
    }
    if {![catch {$http(-proxyfilter) $host} proxy]} {
	set phost [lindex $proxy 0]
	set pport [lindex $proxy 1]
    }

    # OK, now reassemble into a full URL
    set url ${proto}://
    if {$user ne ""} {
	append url $user
	append url @
    }
    append url $host
    if {$port != $defport} {
	append url : $port
    }
    append url $srvurl
    # Don't append the fragment!
    set state(url) $url

    # If a timeout is specified we set up the after event and arrange for an
    # asynchronous socket connection.

    set sockopts [list]
    if {$state(-timeout) > 0} {
	set state(after) [after $state(-timeout) \
		[list http::reset $token timeout]]
	lappend sockopts -async
    }

    # If we are using the proxy, we must pass in the full URL that includes
    # the server name.

    if {[info exists phost] && ($phost ne "")} {
	set srvurl $url
	set targetAddr [list $phost $pport]
    } else {
	set targetAddr [list $host $port]
    }
    # Proxy connections aren't shared among different hosts.
    set state(socketinfo) $host:$port

    # See if we are supposed to use a previously opened channel.
    if {$state(-keepalive)} {
	variable socketmap
	if {[info exists socketmap($state(socketinfo))]} {
	    if {[catch {fconfigure $socketmap($state(socketinfo))}]} {
		Log "WARNING: socket for $state(socketinfo) was closed"
		unset socketmap($state(socketinfo))
	    } else {
		set sock $socketmap($state(socketinfo))
		Log "reusing socket $sock for $state(socketinfo)"
		catch {fileevent $sock writable {}}
		catch {fileevent $sock readable {}}
	    }
	}
	# don't automatically close this connection socket
	set state(connection) {}
    }
    if {![info exists sock]} {
	# Pass -myaddr directly to the socket command
	if {[info exists state(-myaddr)]} {
	    lappend sockopts -myaddr $state(-myaddr)
	}
        if {[catch {eval $defcmd $sockopts $targetAddr} sock]} {
	    # something went wrong while trying to establish the connection.
	    # Clean up after events and such, but DON'T call the command
	    # callback (if available) because we're going to throw an
	    # exception from here instead.

	    set state(sock) $sock
	    Finish $token "" 1
	    cleanup $token
	    return -code error $sock
        }
    }
    set state(sock) $sock
    Log "Using $sock for $state(socketinfo)" \
        [expr {$state(-keepalive)?"keepalive":""}]
    if {$state(-keepalive)} {
        set socketmap($state(socketinfo)) $sock
    }

    # Wait for the connection to complete.

    if {$state(-timeout) > 0} {
	fileevent $sock writable [list http::Connect $token]
	http::wait $token

	if {![info exists state]} {
	    # If we timed out then Finish has been called and the users
	    # command callback may have cleaned up the token. If so we end up
	    # here with nothing left to do.
	    return $token
	} elseif {$state(status) eq "error"} {
	    # Something went wrong while trying to establish the connection.
	    # Clean up after events and such, but DON'T call the command
	    # callback (if available) because we're going to throw an
	    # exception from here instead.
	    set err [lindex $state(error) 0]
	    cleanup $token
	    return -code error $err
	} elseif {$state(status) ne "connect"} {
	    # Likely to be connection timeout
	    return $token
	}
	set state(status) ""
    }

    # Send data in cr-lf format, but accept any line terminators

    fconfigure $sock -translation {auto crlf} -buffersize $state(-blocksize)

    # The following is disallowed in safe interpreters, but the socket is
    # already in non-blocking mode in that case.

    catch {fconfigure $sock -blocking off}
    set how GET
    if {$isQuery} {
	set state(querylength) [string length $state(-query)]
	if {$state(querylength) > 0} {
	    set how POST
	    set contDone 0
	} else {
	    # There's no query data.
	    unset state(-query)
	    set isQuery 0
	}
    } elseif {$state(-validate)} {
	set how HEAD
    } elseif {$isQueryChannel} {
	set how POST
	# The query channel must be blocking for the async Write to
	# work properly.
	fconfigure $state(-querychannel) -blocking 1 -translation binary
	set contDone 0
    }
    if {[info exists state(-method)] && $state(-method) ne ""} {
	set how $state(-method)
    }

    if {[catch {
	puts $sock "$how $srvurl HTTP/$state(-protocol)"
	puts $sock "Accept: $http(-accept)"
	array set hdrs $state(-headers)
	if {[info exists hdrs(Host)]} {
	    # Allow Host spoofing. [Bug 928154]
	    puts $sock "Host: $hdrs(Host)"
	} elseif {$port == $defport} {
	    # Don't add port in this case, to handle broken servers. [Bug
	    # #504508]
	    puts $sock "Host: $host"
	} else {
	    puts $sock "Host: $host:$port"
	}
	unset hdrs
	puts $sock "User-Agent: $http(-useragent)"
        if {$state(-protocol) == 1.0 && $state(-keepalive)} {
	    puts $sock "Connection: keep-alive"
        }
        if {$state(-protocol) > 1.0 && !$state(-keepalive)} {
	    puts $sock "Connection: close" ;# RFC2616 sec 8.1.2.1
        }
        if {[info exists phost] && ($phost ne "") && $state(-keepalive)} {
	    puts $sock "Proxy-Connection: Keep-Alive"
        }
        set accept_encoding_seen 0
	foreach {key value} $state(-headers) {
	    if {[string equal -nocase $key "host"]} {
		continue
	    }
	    if {[string equal -nocase $key "accept-encoding"]} {
		set accept_encoding_seen 1
	    }
	    set value [string map [list \n "" \r ""] $value]
	    set key [string trim $key]
	    if {[string equal -nocase $key "content-length"]} {
		set contDone 1
		set state(querylength) $value
	    }
	    if {[string length $key]} {
		puts $sock "$key: $value"
	    }
	}
	# Soft zlib dependency check - no package require
        if {
	    !$accept_encoding_seen &&
	    ([package vsatisfies [package provide Tcl] 8.6]
		|| [llength [package provide zlib]]) &&
	    !([info exists state(-channel)] || [info exists state(-handler)])
        } then {
	    puts $sock "Accept-Encoding: gzip, identity, *;q=0.1"
        }
	if {$isQueryChannel && $state(querylength) == 0} {
	    # Try to determine size of data in channel. If we cannot seek, the
	    # surrounding catch will trap us

	    set start [tell $state(-querychannel)]
	    seek $state(-querychannel) 0 end
	    set state(querylength) \
		    [expr {[tell $state(-querychannel)] - $start}]
	    seek $state(-querychannel) $start
	}

	# Flush the request header and set up the fileevent that will either
	# push the POST data or read the response.
	#
	# fileevent note:
	#
	# It is possible to have both the read and write fileevents active at
	# this point. The only scenario it seems to affect is a server that
	# closes the connection without reading the POST data. (e.g., early
	# versions TclHttpd in various error cases). Depending on the
	# platform, the client may or may not be able to get the response from
	# the server because of the error it will get trying to write the post
	# data.  Having both fileevents active changes the timing and the
	# behavior, but no two platforms (among Solaris, Linux, and NT) behave
	# the same, and none behave all that well in any case. Servers should
	# always read their POST data if they expect the client to read their
	# response.

	if {$isQuery || $isQueryChannel} {
	    puts $sock "Content-Type: $state(-type)"
	    if {!$contDone} {
		puts $sock "Content-Length: $state(querylength)"
	    }
	    puts $sock ""
	    fconfigure $sock -translation {auto binary}
	    fileevent $sock writable [list http::Write $token]
	} else {
	    puts $sock ""
	    flush $sock
	    fileevent $sock readable [list http::Event $sock $token]
	}

	if {![info exists state(-command)]} {
	    # geturl does EVERYTHING asynchronously, so if the user calls it
	    # synchronously, we just do a wait here.

	    wait $token
	    if {$state(status) eq "error"} {
		# Something went wrong, so throw the exception, and the
		# enclosing catch will do cleanup.
		return -code error [lindex $state(error) 0]
	    }
	}
    } err]} then {
	# The socket probably was never connected, or the connection dropped
	# later.

	# Clean up after events and such, but DON'T call the command callback
	# (if available) because we're going to throw an exception from here
	# instead.

	# if state(status) is error, it means someone's already called Finish
	# to do the above-described clean up.
	if {$state(status) ne "error"} {
	    Finish $token $err 1
	}
	cleanup $token
	return -code error $err
    }

    return $token
}

# Data access functions:
# Data - the URL data
# Status - the transaction status: ok, reset, eof, timeout
# Code - the HTTP transaction code, e.g., 200
# Size - the size of the URL data

proc http::data {token} {
    variable $token
    upvar 0 $token state
    return $state(body)
}
proc http::status {token} {
    if {![info exists $token]} {
	return "error"
    }
    variable $token
    upvar 0 $token state
    return $state(status)
}
proc http::code {token} {
    variable $token
    upvar 0 $token state
    return $state(http)
}
proc http::ncode {token} {
    variable $token
    upvar 0 $token state
    if {[regexp {[0-9]{3}} $state(http) numeric_code]} {
	return $numeric_code
    } else {
	return $state(http)
    }
}
proc http::size {token} {
    variable $token
    upvar 0 $token state
    return $state(currentsize)
}
proc http::meta {token} {
    variable $token
    upvar 0 $token state
    return $state(meta)
}
proc http::error {token} {
    variable $token
    upvar 0 $token state
    if {[info exists state(error)]} {
	return $state(error)
    }
    return ""
}

# http::cleanup
#
#	Garbage collect the state associated with a transaction
#
# Arguments
#	token	The token returned from http::geturl
#
# Side Effects
#	unsets the state array

proc http::cleanup {token} {
    variable $token
    upvar 0 $token state
    if {[info exists state]} {
	unset state
    }
}

# http::Connect
#
#	This callback is made when an asyncronous connection completes.
#
# Arguments
#	token	The token returned from http::geturl
#
# Side Effects
#	Sets the status of the connection, which unblocks
# 	the waiting geturl call

proc http::Connect {token} {
    variable $token
    upvar 0 $token state
    global errorInfo errorCode
    if {
	[eof $state(sock)] ||
	[string length [fconfigure $state(sock) -error]]
    } then {
	Finish $token "connect failed [fconfigure $state(sock) -error]" 1
    } else {
	set state(status) connect
	fileevent $state(sock) writable {}
    }
    return
}

# http::Write
#
#	Write POST query data to the socket
#
# Arguments
#	token	The token for the connection
#
# Side Effects
#	Write the socket and handle callbacks.

proc http::Write {token} {
    variable $token
    upvar 0 $token state
    set sock $state(sock)

    # Output a block.  Tcl will buffer this if the socket blocks
    set done 0
    if {[catch {
	# Catch I/O errors on dead sockets

	if {[info exists state(-query)]} {
	    # Chop up large query strings so queryprogress callback can give
	    # smooth feedback.

	    puts -nonewline $sock \
		[string range $state(-query) $state(queryoffset) \
		     [expr {$state(queryoffset) + $state(-queryblocksize) - 1}]]
	    incr state(queryoffset) $state(-queryblocksize)
	    if {$state(queryoffset) >= $state(querylength)} {
		set state(queryoffset) $state(querylength)
		puts $sock ""
		set done 1
	    }
	} else {
	    # Copy blocks from the query channel

	    set outStr [read $state(-querychannel) $state(-queryblocksize)]
	    puts -nonewline $sock $outStr
	    incr state(queryoffset) [string length $outStr]
	    if {[eof $state(-querychannel)]} {
		set done 1
	    }
	}
    } err]} then {
	# Do not call Finish here, but instead let the read half of the socket
	# process whatever server reply there is to get.

	set state(posterror) $err
	set done 1
    }
    if {$done} {
	catch {flush $sock}
	fileevent $sock writable {}
	fileevent $sock readable [list http::Event $sock $token]
    }

    # Callback to the client after we've completely handled everything.

    if {[string length $state(-queryprogress)]} {
	eval $state(-queryprogress) \
	    [list $token $state(querylength) $state(queryoffset)]
    }
}

# http::Event
#
#	Handle input on the socket
#
# Arguments
#	sock	The socket receiving input.
#	token	The token returned from http::geturl
#
# Side Effects
#	Read the socket and handle callbacks.

proc http::Event {sock token} {
    variable $token
    upvar 0 $token state

    if {![info exists state]} {
	Log "Event $sock with invalid token '$token' - remote close?"
	if {![eof $sock]} {
	    if {[set d [read $sock]] ne ""} {
		Log "WARNING: additional data left on closed socket"
	    }
	}
	CloseSocket $sock
	return
    }
    if {$state(state) eq "connecting"} {
	if {[catch {gets $sock state(http)} n]} {
	    return [Finish $token $n]
	} elseif {$n >= 0} {
	    set state(state) "header"
	}
    } elseif {$state(state) eq "header"} {
	if {[catch {gets $sock line} n]} {
	    return [Finish $token $n]
	} elseif {$n == 0} {
	    # We have now read all headers
	    # We ignore HTTP/1.1 100 Continue returns. RFC2616 sec 8.2.3
	    if {$state(http) == "" || [lindex $state(http) 1] == 100} {
		return
	    }

	    set state(state) body

	    # If doing a HEAD, then we won't get any body
	    if {$state(-validate)} {
		Eof $token
		return
	    }

	    # For non-chunked transfer we may have no body - in this case we
	    # may get no further file event if the connection doesn't close
	    # and no more data is sent. We can tell and must finish up now -
	    # not later.
	    if {
		!(([info exists state(connection)]
			&& ($state(connection) eq "close"))
		    || [info exists state(transfer)])
		&& ($state(totalsize) == 0)
	    } then {
		Log "body size is 0 and no events likely - complete."
		Eof $token
		return
	    }

	    # We have to use binary translation to count bytes properly.
	    fconfigure $sock -translation binary

	    if {
		$state(-binary) || ![string match -nocase text* $state(type)]
	    } then {
		# Turn off conversions for non-text data
		set state(binary) 1
	    }
	    if {
		$state(binary) || [string match *gzip* $state(coding)] ||
		[string match *compress* $state(coding)]
	    } then {
		if {[info exists state(-channel)]} {
		    fconfigure $state(-channel) -translation binary
		}
	    }
	    if {
		[info exists state(-channel)] &&
		![info exists state(-handler)]
	    } then {
		# Initiate a sequence of background fcopies
		fileevent $sock readable {}
		CopyStart $sock $token
		return
	    }
	} elseif {$n > 0} {
	    # Process header lines
	    if {[regexp -nocase {^([^:]+):(.+)$} $line x key value]} {
		switch -- [string tolower $key] {
		    content-type {
			set state(type) [string trim [string tolower $value]]
			# grab the optional charset information
			regexp -nocase {charset\s*=\s*(\S+?);?} \
			    $state(type) -> state(charset)
		    }
		    content-length {
			set state(totalsize) [string trim $value]
		    }
		    content-encoding {
			set state(coding) [string trim $value]
		    }
		    transfer-encoding {
			set state(transfer) \
			    [string trim [string tolower $value]]
		    }
		    proxy-connection -
		    connection {
			set state(connection) \
			    [string trim [string tolower $value]]
		    }
		}
		lappend state(meta) $key [string trim $value]
	    }
	}
    } else {
	# Now reading body
	if {[catch {
	    if {[info exists state(-handler)]} {
		set n [eval $state(-handler) [list $sock $token]]
	    } elseif {[info exists state(transfer_final)]} {
		set line [getTextLine $sock]
		set n [string length $line]
		if {$n > 0} {
		    Log "found $n bytes following final chunk"
		    append state(transfer_final) $line
		} else {
		    Log "final chunk part"
		    Eof $token
		}
	    } elseif {
		[info exists state(transfer)]
		&& $state(transfer) eq "chunked"
	    } then {
		set size 0
		set chunk [getTextLine $sock]
		set n [string length $chunk]
		if {[string trim $chunk] ne ""} {
		    scan $chunk %x size
		    if {$size != 0} {
			set bl [fconfigure $sock -blocking]
			fconfigure $sock -blocking 1
			set chunk [read $sock $size]
			fconfigure $sock -blocking $bl
			set n [string length $chunk]
			if {$n >= 0} {
			    append state(body) $chunk
			}
			if {$size != [string length $chunk]} {
			    Log "WARNING: mis-sized chunk:\
				was [string length $chunk], should be $size"
			}
			getTextLine $sock
		    } else {
			set state(transfer_final) {}
		    }
		}
	    } else {
		#Log "read non-chunk $state(currentsize) of $state(totalsize)"
		set block [read $sock $state(-blocksize)]
		set n [string length $block]
		if {$n >= 0} {
		    append state(body) $block
		}
	    }
	    if {[info exists state]} {
		if {$n >= 0} {
		    incr state(currentsize) $n
		}
		# If Content-Length - check for end of data.
		if {
		    ($state(totalsize) > 0)
		    && ($state(currentsize) >= $state(totalsize))
		} then {
		    Eof $token
		}
	    }
	} err]} then {
	    return [Finish $token $err]
	} else {
	    if {[info exists state(-progress)]} {
		eval $state(-progress) \
		    [list $token $state(totalsize) $state(currentsize)]
	    }
	}
    }

    # catch as an Eof above may have closed the socket already
    if {![catch {eof $sock} eof] && $eof} {
	if {[info exists $token]} {
	    set state(connection) close
	    Eof $token
	} else {
	    # open connection closed on a token that has been cleaned up.
	    CloseSocket $sock
	}
	return
    }
}

# http::getTextLine --
#
#	Get one line with the stream in blocking crlf mode
#
# Arguments
#	sock	The socket receiving input.
#
# Results:
#	The line of text, without trailing newline

proc http::getTextLine {sock} {
    set tr [fconfigure $sock -translation]
    set bl [fconfigure $sock -blocking]
    fconfigure $sock -translation crlf -blocking 1
    set r [gets $sock]
    fconfigure $sock -translation $tr -blocking $bl
    return $r
}

# http::CopyStart
#
#	Error handling wrapper around fcopy
#
# Arguments
#	sock	The socket to copy from
#	token	The token returned from http::geturl
#
# Side Effects
#	This closes the connection upon error

proc http::CopyStart {sock token} {
    variable $token
    upvar 0 $token state
    if {[catch {
	fcopy $sock $state(-channel) -size $state(-blocksize) -command \
	    [list http::CopyDone $token]
    } err]} then {
	Finish $token $err
    }
}

# http::CopyDone
#
#	fcopy completion callback
#
# Arguments
#	token	The token returned from http::geturl
#	count	The amount transfered
#
# Side Effects
#	Invokes callbacks

proc http::CopyDone {token count {error {}}} {
    variable $token
    upvar 0 $token state
    set sock $state(sock)
    incr state(currentsize) $count
    if {[info exists state(-progress)]} {
	eval $state(-progress) \
	    [list $token $state(totalsize) $state(currentsize)]
    }
    # At this point the token may have been reset
    if {[string length $error]} {
	Finish $token $error
    } elseif {[catch {eof $sock} iseof] || $iseof} {
	Eof $token
    } else {
	CopyStart $sock $token
    }
}

# http::Eof
#
#	Handle eof on the socket
#
# Arguments
#	token	The token returned from http::geturl
#
# Side Effects
#	Clean up the socket

proc http::Eof {token {force 0}} {
    variable $token
    upvar 0 $token state
    if {$state(state) eq "header"} {
	# Premature eof
	set state(status) eof
    } else {
	set state(status) ok
    }

    if {($state(coding) eq "gzip") && [string length $state(body)] > 0} {
        if {[catch {
	    if {[package vsatisfies [package present Tcl] 8.6]} {
		# The zlib integration into 8.6 includes proper gzip support
		set state(body) [zlib gunzip $state(body)]
	    } else {
		set state(body) [Gunzip $state(body)]
	    }
        } err]} then {
	    return [Finish $token $err]
        }
    }

    if {!$state(binary)} {
        # If we are getting text, set the incoming channel's encoding
        # correctly.  iso8859-1 is the RFC default, but this could be any IANA
        # charset.  However, we only know how to convert what we have
        # encodings for.

        set enc [CharsetToEncoding $state(charset)]
        if {$enc ne "binary"} {
	    set state(body) [encoding convertfrom $enc $state(body)]
        }

        # Translate text line endings.
        set state(body) [string map {\r\n \n \r \n} $state(body)]
    }

    Finish $token
}

# http::wait --
#
#	See documentation for details.
#
# Arguments:
#	token	Connection token.
#
# Results:
#        The status after the wait.

proc http::wait {token} {
    variable $token
    upvar 0 $token state

    if {![info exists state(status)] || $state(status) eq ""} {
	# We must wait on the original variable name, not the upvar alias
	vwait ${token}(status)
    }

    return [status $token]
}

# http::formatQuery --
#
#	See documentation for details.  Call http::formatQuery with an even
#	number of arguments, where the first is a name, the second is a value,
#	the third is another name, and so on.
#
# Arguments:
#	args	A list of name-value pairs.
#
# Results:
#	TODO

proc http::formatQuery {args} {
    set result ""
    set sep ""
    foreach i $args {
	append result $sep [mapReply $i]
	if {$sep eq "="} {
	    set sep &
	} else {
	    set sep =
	}
    }
    return $result
}

# http::mapReply --
#
#	Do x-www-urlencoded character mapping
#
# Arguments:
#	string	The string the needs to be encoded
#
# Results:
#       The encoded string

proc http::mapReply {string} {
    variable http
    variable formMap

    # The spec says: "non-alphanumeric characters are replaced by '%HH'". Use
    # a pre-computed map and [string map] to do the conversion (much faster
    # than [regsub]/[subst]). [Bug 1020491]

    if {$http(-urlencoding) ne ""} {
	set string [encoding convertto $http(-urlencoding) $string]
	return [string map $formMap $string]
    }
    set converted [string map $formMap $string]
    if {[string match "*\[\u0100-\uffff\]*" $converted]} {
	regexp {[\u0100-\uffff]} $converted badChar
	# Return this error message for maximum compatability... :^/
	return -code error \
	    "can't read \"formMap($badChar)\": no such element in array"
    }
    return $converted
}

# http::ProxyRequired --
#	Default proxy filter.
#
# Arguments:
#	host	The destination host
#
# Results:
#       The current proxy settings

proc http::ProxyRequired {host} {
    variable http
    if {[info exists http(-proxyhost)] && [string length $http(-proxyhost)]} {
	if {
	    ![info exists http(-proxyport)] ||
	    ![string length $http(-proxyport)]
	} then {
	    set http(-proxyport) 8080
	}
	return [list $http(-proxyhost) $http(-proxyport)]
    }
}

# http::CharsetToEncoding --
#
#	Tries to map a given IANA charset to a tcl encoding.  If no encoding
#	can be found, returns binary.
#

proc http::CharsetToEncoding {charset} {
    variable encodings

    set charset [string tolower $charset]
    if {[regexp {iso-?8859-([0-9]+)} $charset -> num]} {
	set encoding "iso8859-$num"
    } elseif {[regexp {iso-?2022-(jp|kr)} $charset -> ext]} {
	set encoding "iso2022-$ext"
    } elseif {[regexp {shift[-_]?js} $charset]} {
	set encoding "shiftjis"
    } elseif {[regexp {(?:windows|cp)-?([0-9]+)} $charset -> num]} {
	set encoding "cp$num"
    } elseif {$charset eq "us-ascii"} {
	set encoding "ascii"
    } elseif {[regexp {(?:iso-?)?lat(?:in)?-?([0-9]+)} $charset -> num]} {
	switch -- $num {
	    5 {set encoding "iso8859-9"}
	    1 - 2 - 3 {
		set encoding "iso8859-$num"
	    }
	}
    } else {
	# other charset, like euc-xx, utf-8,...  may directly map to encoding
	set encoding $charset
    }
    set idx [lsearch -exact $encodings $encoding]
    if {$idx >= 0} {
	return $encoding
    } else {
	return "binary"
    }
}

# http::Gunzip --
#
#	Decompress data transmitted using the gzip transfer coding.
#

# FIX ME: redo using zlib sinflate
proc http::Gunzip {data} {
    binary scan $data Scb5icc magic method flags time xfl os
    set pos 10
    if {$magic != 0x1f8b} {
        return -code error "invalid data: supplied data is not in gzip format"
    }
    if {$method != 8} {
        return -code error "invalid compression method"
    }

    # lassign [split $flags ""] f_text f_crc f_extra f_name f_comment
    foreach {f_text f_crc f_extra f_name f_comment} [split $flags ""] break
    set extra ""
    if {$f_extra} {
	binary scan $data @${pos}S xlen
        incr pos 2
        set extra [string range $data $pos $xlen]
        set pos [incr xlen]
    }

    set name ""
    if {$f_name} {
        set ndx [string first \0 $data $pos]
        set name [string range $data $pos $ndx]
        set pos [incr ndx]
    }

    set comment ""
    if {$f_comment} {
        set ndx [string first \0 $data $pos]
        set comment [string range $data $pos $ndx]
        set pos [incr ndx]
    }

    set fcrc ""
    if {$f_crc} {
	set fcrc [string range $data $pos [incr pos]]
        incr pos
    }

    binary scan [string range $data end-7 end] ii crc size
    set inflated [zlib inflate [string range $data $pos end-8]]
    set chk [zlib crc32 $inflated]
    if {($crc & 0xffffffff) != ($chk & 0xffffffff)} {
	return -code error "invalid data: checksum mismatch $crc != $chk"
    }
    return $inflated
}

# Local variables:
# indent-tabs-mode: t
# End:
