if test x = x$BOOST_ROOT 
then
    echo BOOST_ROOT not set
    exit 1
fi
#use -r switch on fop for relaxed validation
xsltproc --xinclude --nonet bb2db.xsl safe_numerics.xml \
| fop -r -dpi 300 -xsl db2fo.xsl -xml - -pdf ../safe_numerics.pdf

# equivalent alternative?
# xsltproc --xinclude --nonet bb2db.xsl safe_numerics.xml \
# | xsltproc --xinclude --nonet db2fo.xsl - \
# | fop -r -dpi 300 -fo - -pdf safe_numerics.pdf

