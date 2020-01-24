#!/usr/bin/env bash
# we first count open sockets in total on the system:
echo "open sockets: "
F="/tmp/$$_netstat.log"
# we store it in a temporary file, since gatherng this information may become expensive if
# its more then some thousand connections:
netstat -n |grep ^tcp > $F
C=$(wc -l < "$F")
echo $C
# if this outdoes a sane number, lets go into detail.
if test "$C" -gt 1024; then
    # we use lsof to find all tcp sockets and which processes own them.
    LF="/tmp/$$_lsof.log"
    lsof -n -iTCP > "$LF"
    # we group all sockets by target ip:port
    for sockets in $(grep ^tcp $F | sed -e "s;  *; ;g"  |cut -d ' '  -f 5 |sort -u ) ; do
        # count the connections to one ip:port
	SC=$(grep "$sockets" $F |wc -l)
	if test "$SC" -gt "16"; then
            # if the connection outoes a sane figure, we want to count & output them:
	    echo "$sockets - $SC"
            # and here we want to know which processes are attached to them.
            # this will output one line per active connection, so this may become much:
	    grep "$sockets" "$LF"
	fi
    done
    rm "$LF"
fi
rm "$F"

