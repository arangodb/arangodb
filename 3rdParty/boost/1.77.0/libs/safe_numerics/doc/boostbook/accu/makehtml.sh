if test x = x$BOOST_ROOT 
then
    echo BOOST_ROOT not set
fi
mkdir html
xsltproc --xinclude --nonet bb2db.xsl accu.xml > accudocbook4.xml
xsltproc --nonet db2html.xsl accudocbook4.xml
cp accu_logo.png html
cp $BOOST_ROOT/doc/src/boostbook.css html
cp -R $BOOST_ROOT/doc/html/images html
