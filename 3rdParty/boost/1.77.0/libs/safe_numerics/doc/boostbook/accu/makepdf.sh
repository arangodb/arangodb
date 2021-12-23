#use -r switch on fop for relaxed validation
xsltproc --xinclude --nonet bb2db.xsl accu.xml > accudocbook4.xml
fop -r -dpi 300 -xsl db2fo.xsl -xml accudocbook4.xml -pdf accu.pdf
