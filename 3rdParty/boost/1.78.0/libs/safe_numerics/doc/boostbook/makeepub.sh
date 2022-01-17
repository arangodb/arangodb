if test x = x$BOOST_ROOT 
then
    echo BOOST_ROOT not set
    exit 1
fi
xsltproc --nonet --xinclude bb2db.xsl safe_numerics.xml | xsltproc --xinclude --nonet --stringparam base.dir ebooktmp db2epub.xsl -
cp *.png ebooktmp/OEBPS/
xsltproc --xinclude --nonet opf.xsl ebooktmp/OEBPS/package.opf >ebooktmp/OEBPS/t.opf
# mv ebooktmp/OEBPS/t.opf ebooktmp/OEBPS/package.opf
cd ebooktmp
zip -r -X ../book.epub mimetype META-INF OEBPS 
cd -
