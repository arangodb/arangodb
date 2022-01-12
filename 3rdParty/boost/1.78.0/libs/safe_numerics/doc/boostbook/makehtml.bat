if x == x%BOOST_ROOT% goto abort

xsltproc --nonet --xinclude bb2db.xsl safe_numerics.xml | xsltproc --nonet db2html.xsl -
xcopy /i /q /y *.jpg ..\html
if not exist ..\html md ..\html
xcopy /i /q /y %BOOST_ROOT%\doc\src\boostbook.css ..\html
if not exist ..\html md ..\html\images
xcopy /i /q /y /s %BOOST_ROOT%\doc\html\images ..\html\images
exit

:abort
echo BOOST_ROOT not set
exit
