#!/bin/bash

cd Documentation/Examples || exit 1

CLEANUPSH=/tmp/removeOld.sh

rm -f "${CLEANUPSH}"
for i in *generated; do
    SRCH=$(echo $i |sed "s;.generated;;" )
    if grep -iqR $SRCH ../DocuBlocks/ ../Books/; then
        echo "NEED $SRCH";
    else
        echo "git rm $i" >> "${CLEANUPSH}"
    fi
done

if test -f ${CLEANUPSH}; then
    echo "Removing $(cat "${CLEANUPSH}" | wc -l) files"
    . "${CLEANUPSH}"
else
    echo "nothing to do."
fi
