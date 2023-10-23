#!/bin/sh

FULL_HTML=0
KEEP_TITLE=0

while [ "$#" -gt 0 ];  do
  case "$1" in
    --full-html)
      FULL_HTML=1
      ;;

    --keep-title)
      KEEP_TITLE=1
      ;;

    --*)
      echo "$0: unknown option '$1'"
      exit 1
      ;;

    *)
      INPUT="$1"
      shift
      OUTPUT="$1"
      ;;
  esac

  shift
done

if test -z "$INPUT" -o -z "$OUTPUT";  then
  echo "usage: $0 <intput> <output>"
  exit 1
fi

if test "x$FULL_HTML" = "x0";  then
  HEADER=""
  FOOTER=""
else
  HEADER='<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"><html xmlns="http://www.w3.org/1999/xhtml"><head><meta http-equiv="Content-Type" content="text/xhtml;charset=UTF-8"/><title>ArangoDB: About ArangoDB</title></head><body><div id="content"><div class="offlineview"><div>'
  FOOTER='</div></div></body></html>'
fi

rm -f $OUTPUT.tmp
touch $OUTPUT.tmp || exit 1

if test -n "$HEADER";  then
  echo $HEADER >> $OUTPUT.tmp || exit 1
fi

cat $INPUT \
  | sed -e 's:<\(/*\)h5>:<\1h6>:g' \
  | sed -e 's:<\(/*\)h4>:<\1h5>:g' \
  | sed -e 's:<\(/*\)h3>:<\1h4>:g' \
  | sed -e 's:<\(/*\)h2>:<\1h3>:g' \
  | sed -e 's:<\(/*\)h1>:<\1h2>:g' \
  | sed -e 's:<span class="comment">:<span class="doxy-comment">:g' >> $OUTPUT.tmp || exit 1

if test "x$KEEP_TITLE" = "x0";  then
  mv $OUTPUT.tmp $OUTPUT.tmp1 || exit 1
  sed -e 's:<div class="title">\([^<]*\)</div>:<h1>\1</h1>:g' < $OUTPUT.tmp1 >> $OUTPUT.tmp || exit 1
  rm $OUTPUT.tmp1 || exit 1
fi

if test -n "$FOOTER";  then
  echo $FOOTER >> $OUTPUT.tmp || exit 1
fi

mv $OUTPUT.tmp $OUTPUT || exit 1
