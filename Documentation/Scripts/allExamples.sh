#!/bin/sh
echo "generating /tmp/allExamples.html"
(printf "<html><head></head><body><pre>\n";
 for thisExample in Documentation/Examples/*.generated; do 
     printf "<hr>\n<h3>$thisExample</h3>\n";
     cat $thisExample;
 done;
 printf "</pre></body></html>") > /tmp/allExamples.html
