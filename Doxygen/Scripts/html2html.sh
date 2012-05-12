#!/bin/sh

INPUT="$1"
OUTPUT="$2"

if test -z "$INPUT" -o -z "$OUTPUT";  then
  echo "usage: $0 <intput> <output>"
  exit 1
fi

HEADER='<html><head><title>ArangoDB Manual</title> <style media="screen" type="text/css" style="display:none">body{background-color:white;font:13px Helvetica,arial,freesans,clean,sans-serif;line-height:1.4;color:#333;}#access{font-size:16px;margin-left:12px;display:block;margin-left:10px;margin-right:10px;background-color:#F3F1EE!important;}#access a{border-right:1px solid #DBDEDF;color:#A49F96;display:block;line-height:38px;padding:0 10px;text-decoration:none;}#navigation ul{text-transform:uppercase;list-style:none;margin:0;}#navigation li{float:left;position:relative;}#container{width:920px;margin:0 auto;}a{color:#4183C4;text-decoration:none;}.contents h2{font-size:24px;border-bottom:1px solid #CCC;color:black;}.contents h1{font-size:33px;border-bottom:1px solid #CCC;color:black;}.clearfix:after{content:".";display:block;clear:both;font-size:0;height:0;visibility:hidden;}/**/ *:first-child+html .clearfix{min-height:0;}/**/ * html .clearfix{height:1%;}</style></head><body><div id="container"><img src="images/logo_arangodb.jpg" width="397" height="67" alt="ArangoDB"><div id="access" role="navigation"><div id="navigation"><ul id="menu-ahome" class="menu"><li><a href="Home.html">Table of contents</a></li> <li><a href="http://www.arangodb.org">ArangoDB homepage</a></li></ul></div><div class="clearfix"></div></div><div>'
FOOTER='</div></body></html>'

sed -e "s~<?php include \"include/header.php\" ?> ~${HEADER}~" $INPUT \
    | sed -e "s~<?php include \"include/footer.php\" ?> ~${FOOTER}~" \
    | sed -e "s~<div class=\"title\">\\([^<]*\\)</div>~<h1>\\1</h1>~" > $OUTPUT
