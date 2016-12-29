all: build-books

.PHONY:

newVersionNumber = $(shell cat ../../VERSION)

################################################################################
# per book targets
check-summary:
	@find ppbooks/$(NAME) -name \*.md |sed -e "s;ppbooks/$(NAME)/;;" |grep -vf SummaryBlacklist.txt |sort > /tmp/is_md.txt
	@cat $(NAME)/SUMMARY.md |grep '(' |sed -e "s;.*(;;" -e "s;).*;;" |sort |grep -v '# Summary' > /tmp/is_summary.txt
	@if test "`comm -3 /tmp/is_md.txt /tmp/is_summary.txt|wc -l`" -ne 0; then \
		echo "not all files are mapped to the summary!"; \
		echo " files found       |    files in summary"; \
		comm -3 /tmp/is_md.txt /tmp/is_summary.txt; \
		exit 1; \
	fi

book-check-leftover-docublocks:
	@if test "`grep -r \"@startDocuBlock\" --include \"*.md\" ppbooks/$(NAME) | wc -l`" -ne 0; then \
	  echo ; \
	  echo "@startDocuBlock markers still found in generated output files:"; \
	  grep -rl "@startDocuBlock" --include "*.md" ppbooks/$(NAME) | sed -e "s/^/- /g"; \
	  exit 1; \
	fi

book-check-restheader-leftovers:
	@if test "`find ppbooks/$(NAME) -name '*.md' -exec grep -- '^@[A-Z]*' {} \; -print | wc -l`" -gt 0; then \
		echo "found these unconverted Swagger Restapi tags: "; \
		find ppbooks/$(NAME) -name '*.md' -exec grep '^@[A-Z]*' {} \; -print; \
		exit 1; \
	fi

# Check for all lines starting with exclamation marks, except image links which are exclamation mar + bracket
book-check-mdpp-leftovers:
	@if test "`find ppbooks/$(NAME) -name '*.md' -exec grep -- '^![^\[]' {} \; -print | wc -l`" -gt 0; then \
		echo "found these unconverted Markdown-PP tags: "; \
		find ppbooks/$(NAME) -name '*.md' -exec grep '^![^\[]' {} \; -print; \
		exit 1; \
	fi

ppbook-precheck-bad-code-sections:
	@if grep -qR  '^``` *.* ' $(NAME); then \
		echo "tripple tics with blanks afterwards found: "; \
		grep -R  '^``` *.* ' $(NAME); \
		exit 1; \
	fi

ppbook-check-html-link:
	@if test "`egrep -r '\[.*\]\(.*\)' ppbooks/$(NAME) |grep '\.md:' |grep html |grep -v http://|grep -v https:// |grep -v header.css |wc -l`" -gt 0; then \
		echo "Found links to .html files inside of the document! use <foo>.md instead!"; \
		echo; \
		egrep -r '\[.*\]\(.*\)' ppbooks/$(NAME)  | grep '\.md:' | grep html |grep -v http://|grep -v https:// |grep -v header.css ; \
		exit 1; \
	fi

ppbook-check-directory-link:
	@if test "`egrep -r '\[.*\]\(.*\)' ppbooks/$(NAME) | grep '\.md:' |grep -v html |grep -v http://|grep -v https:// |grep -v header.css |grep -v node_modules |grep -v node_modules | grep -v '\.md' | wc -l`" -gt 0; then \
		echo "Found director links! use ../<directory>/README.md instead!"; \
		echo; \
		egrep -r '\[.*\]\(.*\)' ppbooks/$(NAME) |grep '\.md:' |grep -v html |grep -v http://|grep -v https:// |grep -v header.css |grep -v node_modules |grep -v node_modules | grep -v '\.md' ; \
		exit 1; \
	fi

book-check-markdown-leftovers:
	@if test "`find books/$(NAME) -name '*.html' -exec grep -- '##' {} \; -print | wc -l`" -gt 0; then \
		echo "found these unconverted markdown titles: "; \
		find books/$(NAME) -name '*.html' -exec grep '##' {} \; -print; \
		exit 1; \
	fi
	@if test "`find books/$(NAME) -name '*.html' -exec grep '\.md\"[ />]' {} \; | grep -v data-filepath | wc -l`" -gt 0; then \
		find books/$(NAME) -name '*.html' -exec grep '\.md"[ />]' {} \; -print | grep -v data-filepath; \
		echo "found dangling markdown links; see the list above "; \
		exit 1; \
	fi
	@if test "`find books/$(NAME) -name '*.html' -exec grep '\.md#' {} \; -print | wc -l`" -gt 0; then \
		echo "found dangling markdown links: "; \
		find books/$(NAME) -name '*.html' -exec grep '\.md#' {} \; -print; \
		exit 1; \
	fi

book-check-dangling-anchors:
	@grep -R "a href.*#" books/$(NAME) | \
		egrep -v "(styles/header\.js|/app\.js|class=\"navigation|https*://)" | \
		sed 's;\(.*\.html\):.*a href="\(.*\)#\(.*\)">.*</a>.*;\1,\2,\3;' > /tmp/anchorlist.txt

	@NO=0; \
	for i in `cat /tmp/anchorlist.txt`; do \
	    ANCHOR=`echo $$i | cut '-d,' -f 3`; \
	    FN=`echo $$i | cut '-d,' -f 2`; \
	    SFN=`echo $$i | cut '-d,' -f 1`; \
	    if test -z "$$FN"; then \
	        FN=$$SFN; \
	    else \
	        SFNP=`echo $$SFN | sed 's;/[a-zA-Z]*.html;;'`; \
	        FN="$${SFNP}/$${FN}"; \
	    fi; \
	    if test -n "$$ANCHOR"; then \
	        if grep -q "id=\"$$ANCHOR\">" $$FN; then \
	            /bin/true; \
	        else \
	            echo "$$i"; \
	            NO=$$((NO + 1)); \
	        fi; \
	    fi; \
	done; \
	if test "$${NO}" -gt 0; then \
	    echo "$${NO} Dangling anchors found!"; \
	    exit 1; \
	fi

build-book-symlinks:
	echo "generate backwards compatibility symlinks:"
	cd books/$(NAME); pwd; \
	find . -name "README.mdpp" |\
	sed -e 's:README\.mdpp$$::' |\
	awk '{print "ln -s index.html " $$1 "README.html"}' |\
	bash

build-book:
	make ppbook-precheck-bad-code-sections $(NAME)
	if test ! -d ppbooks/$(NAME); then \
	  mkdir -p ppbooks/$(NAME); \
	  WD=`pwd`; \
	  for dir in `find $(NAME) -type d `; do \
	  	cd $${WD}/ppbooks; \
	  	test -d $${dir} || mkdir -p $${dir}; \
	  done; \
	fi
	cd ppbooks/$(NAME); if ! test -L SUMMARY.md; then ln -s ../../$(NAME)/SUMMARY.md . ; fi
	cd ppbooks/$(NAME); if ! test -f book.json ; then cp ../../$(NAME)/book.json . ; fi
	cd ppbooks/$(NAME); cp -a ../../$(NAME)/styles/* styles/
	WD=`pwd`; \
	for pic in `find $(NAME) -name \*.png`; do \
		cd $${WD}/ppbooks; \
		echo cd $${WD}/ppbooks; \
		echo cp $${WD}/$${pic} $${pic}; \
		cp $${WD}/$${pic} $${pic}; \
	done
	python ../Scripts/generateMdFiles.py $(NAME) ppbooks/ ../../js/apps/system/_admin/aardvark/APP/api-docs.json $(FILTER)

	cd ppbooks/$(NAME) && sed -i -e 's/VERSION_NUMBER/v$(newVersionNumber)/g' styles/header.js
	cd ppbooks/$(NAME) && sed -i -e 's/VERSION_NUMBER/v$(newVersionNumber)/g' README.md
	cd ppbooks/$(NAME) && sed -i -e 's/VERSION_NUMBER/v$(newVersionNumber)/g' book.json

	test -d books/$(NAME) || mkdir -p books/$(NAME)

	make check-summary
	make book-check-leftover-docublocks
	make book-check-restheader-leftovers
	make book-check-mdpp-leftovers
	make ppbook-check-html-link
	make ppbook-check-directory-link

	cd ppbooks/$(NAME) && gitbook install
	cd ppbooks/$(NAME) && gitbook build ./ ./../../books/$(NAME)
	python ../Scripts/deprecated.py

	make book-check-markdown-leftovers
	# make book-check-dangling-anchors

clean-book:
	@rm -rvf books/$(NAME)
	@if test -z "$(FILTER)"; then rm -rvf ppbooks/$(NAME); fi

clean-book-intermediate:
	@if test -z "$(FILTER)"; then rm -rvf ppbooks/$(NAME); fi


################################################################################
# Global targets


#************************************************************
# Check docublocks - checks whether docublock are 
#  - files in intermediate output directories and temporary
#    files are excludes (with # in their names)
#  - uniq in the source 
#  - all docublocks are used somewhere in the documentation
#  
check-docublocks:
	grep -R '@startDocuBlock' --include "*.h" --include "*.cpp" --include "*.js" --include "*.mdpp" . |\
		grep -v '@startDocuBlockInline' |\
		grep -v ppbook |\
		grep -v allComments.txt |\
		grep -v Makefile |\
		grep -v '.*~:.*' |\
		grep -v '.*#.*:.*' \
		   > /tmp/rawindoc.txt
	grep -R '@startDocuBlockInline' --include "*.h" --include "*.cpp" --include "*.js" --include "*.mdpp" . |\
		grep -v ppbook |\
		grep -v allComments.txt |\
		grep -v Makefile |\
		grep -v '.*~:.*' |\
		grep -v '.*#.*:.*' \
		   >> /tmp/rawindoc.txt
	cat /tmp/rawindoc.txt | sed  -e "s;.*ck ;;" -e "s;.*ne ;;" |sort -u > /tmp/indoc.txt
	grep -R '^/// @startDocuBlock' ../../lib ../../arangod ../../arangosh ../../js --include "*.h" --include "*.cpp" --include "*.js" --include "*.mdpp" |grep -v aardvark > /tmp/rawinprog.txt
# searching the Inline docublocks needs some more blacklisting:
	grep -R '@startDocuBlockInline' --include "*.h" --include "*.cpp" --include "*.js" --include "*.mdpp" . |\
		grep -v ppbook |\
		grep -v allComments.txt |\
		grep -v Makefile |\
		grep -v '.*~:.*' |\
		grep -v '.*#.*:.*' \
		   >> /tmp/rawinprog.txt
	echo "Generated: @startDocuBlockInline errorCodes">> /tmp/rawinprog.txt
	cat /tmp/rawinprog.txt |sed -e "s;.*ck ;;" -e "s;.*ne ;;" |sort > /tmp/inprog_raw.txt
	@cat /tmp/inprog_raw.txt |sort -u > /tmp/inprog.txt
	@if test "`cat /tmp/inprog.txt |wc -l`" -ne "`cat /tmp/inprog_raw.txt|wc -l`"; then \
		echo "Duplicate entry found in the source trees:"; \
		comm -3 /tmp/inprog_raw.txt /tmp/inprog.txt; \
		exit 1; \
	fi
	if test "`comm -3 /tmp/indoc.txt /tmp/inprog.txt |wc -l `" -ne 0; then \
		echo "Not all blocks were found on both sides:"; \
		echo "Documentation      |     Programcode:"; \
		comm -3 /tmp/indoc.txt /tmp/inprog.txt; \
		if test "`comm -2 -3 /tmp/indoc.txt /tmp/inprog.txt |wc -l`" -gt 0; then \
			echo "Documentation: "; \
			for grepit in `comm -2 -3 /tmp/indoc.txt /tmp/inprog.txt`; do \
				grep "$$grepit" /tmp/rawindoc.txt; \
			done; \
		fi; \
		if test "`comm -1 -3 /tmp/indoc.txt /tmp/inprog.txt |wc -l`" -gt 0; then \
			echo "Program code:"; \
			for grepit in `comm -1 -3 /tmp/indoc.txt /tmp/inprog.txt`; do \
				grep "$$grepit" /tmp/rawinprog.txt | sed "s;/// @startDocuBlock;\t\t;"; \
			done; \
		fi; \
		exit 1; \
	fi

clean-intermediate:
	make clean-book-intermediate NAME=Users FILTER=$(FILTER)


clean: clean-intermediate
	rm -f allComments.txt


.PHONY: build-books

build-books-keep-md:
	@test -d books || mkdir books
	python ../Scripts/codeBlockReader.py
	make build-book NAME=Users

build-books: clean-intermediate build-books-keep-md check-docublocks
