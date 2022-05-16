all: spirit-reference.pdf

%.dvi: %.tex
	tex $<

%.pdf: %.dvi
	dvips -Ppdf -t landscape -q -f $< \
	| gs -q -P- -dSAFER -dNOPAUSE -dBATCH -sDEVICE=pdfwrite \
	-sstdout=%stderr -sOutputFile=$@ -c .setpdfwrite -
