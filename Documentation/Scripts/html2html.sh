#!/bin/sh

INPUT="$1"
OUTPUT="$2"

if test -z "$INPUT" -o -z "$OUTPUT";  then
  echo "usage: $0 <intput> <output>"
  exit 1
fi

HEADER='<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/xhtml;charset=UTF-8"/>
<title>ArangoDB: About ArangoDB</title>
<link rel="stylesheet" type="text/css" href="arangodb.css">
</head>
<body>
<div>'

FOOTER='</div></body></html>'

(echo $HEADER && cat $INPUT && echo $FOOTER) > $OUTPUT
