#!/bin/bash
# this script picks all json-header files, and dumps them into a windows RC file style file
echo > jsonresource.h
(
i=1
for JSON in *.json ; do
    RCNAME="#define IDS_$(echo "${JSON}" |sed -e "s;.json;;" -e 's/\(.*\)/\U\1/') ${i}"
    echo "${RCNAME}" >> jsonresource.h
    i=$(($i + 1))
done
printf '#include "jsonresource.h"\r\n'
for JSON in *.json ; do
    RCNAME="IDS_$(echo "${JSON}"|sed -e "s;.json;;" -e 's/\(.*\)/\U\1/')"

    printf "${RCNAME} RCDATA\r\n{\r\n"
    # first cut off the .h related syntax...
    cat "${JSON}" |sed -e 's;^R"=($;;' -e 's;^)="$;;' | \
        sed -e 's;";"";g' -e 's;^;  ";' -e 's;$;",;'
    # then escape quotes (by doubling them) and pre & post pend quotes.
    printf '"\\0"\r\n}\r\n'
done
printf "\r\n}\r\n"
) > json.rc
