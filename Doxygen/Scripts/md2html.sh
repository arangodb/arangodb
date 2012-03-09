#!/bin/sh

MARKDOWN="`dirname $0`/Markdown.pl"
INPUT="$1"

if test -z "$INPUT";  then
  echo "usage: $0 <file.md>"
  exit 1
fi

OUTPUT="`dirname $INPUT`/`basename $INPUT .md`.html"

perl "$MARKDOWN" "$INPUT" \
    | sed -r -e "s/href=\"([^\"#]+)([\"#])/href=\"\1\.html\2/g" \
    | sed -e "s/href=\"wiki\//href=\"/g" \
    | sed -e "s/#wiki-/#/g" > $OUTPUT
