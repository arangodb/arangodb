ALLBOOKS=HTTP AQL Manual

all: build-books

.PHONY:

newVersionNumber = $(shell cat ../../VERSION)

################################################################################
# per book targets
check-summary:
	@echo "##### checking summary for $(NAME)"
	@find ppbooks/$(NAME) -name \*.md |sed -e "s;ppbooks/$(NAME)/;;" |grep -vf SummaryBlacklist.txt |grep -v gitbook-plugin |sort > /tmp/is_md.txt
	@cat $(NAME)/SUMMARY.md |grep -v '^ *# '|grep '(' |sed -e "s;.*(;;" -e "s;).*;;" |sort  > /tmp/is_summary.txt
	@if test "`comm -3 /tmp/is_md.txt /tmp/is_summary.txt|wc -l`" -ne 0; then \
		echo "not all files are mapped to the summary!"; \
		echo " files found       |    files in summary"; \
		comm -3 /tmp/is_md.txt /tmp/is_summary.txt; \
		exit 1; \
	fi

book-check-leftover-docublocks:
	@echo "##### checking for left over docublocks in $(NAME)"
	@if test "`grep -r \"@startDocuBlock\" --include \"*.md\" ppbooks/$(NAME) | wc -l`" -ne 0; then \
	  echo ; \
	  echo "@startDocuBlock markers still found in generated output files:"; \
	  grep -rl "@startDocuBlock" --include "*.md" ppbooks/$(NAME) | sed -e "s/^/- /g"; \
	  exit 1; \
	fi

book-check-restheader-leftovers:
	@echo "##### checking for restheader leftovers in $(NAME)"
	@if test "`find ppbooks/$(NAME) -name '*.md' -exec grep -- '^@[A-Z]*' {} \; -print | wc -l`" -gt 0; then \
		echo "found these unconverted Swagger Restapi tags: "; \
		find ppbooks/$(NAME) -name '*.md' -exec grep '^@[A-Z]*' {} \; -print; \
		exit 1; \
	fi

# Check for all lines starting with exclamation marks, except image links which are exclamation mar + bracket
book-check-mdpp-leftovers:
	@echo "##### checking for mdpp leftovers for $(NAME)"
	@if test "`find ppbooks/$(NAME) -name '*.md' -exec grep -- '^![^\[!]' {} \; -print | wc -l`" -gt 0; then \
		echo "found these unconverted Markdown-PP tags: "; \
		find ppbooks/$(NAME) -name '*.md' -exec grep '^![^\[!]' {} \; -print; \
		exit 1; \
	fi

ppbook-precheck-bad-code-sections:
	@echo "##### checking for bad code sections in $(NAME)"
	@if grep -qR  '^``` *.* ' $(NAME); then \
		echo "tripple tics with blanks afterwards found: "; \
		grep -R  '^``` *.* ' $(NAME); \
		exit 1; \
	fi

ppbook-check-html-link:
	@echo "##### checking for invalid HTML links in $(NAME)"
	@echo "$(ALLBOOKS)" |sed -e 's; ;\n;g' |sed -e 's;^;/;' -e 's;$$;/;' > /tmp/books.regex

	@egrep -r '\[.*\]\(.*\)' ppbooks/$(NAME)|grep '\.md:'| grep 'html'| grep -v 'http://' | grep -v 'https://' | grep -v 'header.css' | grep -v -f /tmp/books.regex > /tmp/relative_html_links.txt ||true
	@if test "`cat /tmp/relative_html_links.txt |wc -l`" -gt 0; then \
		echo "Found links to .html files inside of the document! use <foo>.md instead!"; \
		echo; \
		cat  /tmp/relative_html_links.txt; \
		exit 1; \
	fi

ppbook-check-directory-link:
	@echo "##### checking for invalid md links in $(NAME)"
	@if test "`egrep -r '\[.*\]\(.*\)' ppbooks/$(NAME) | grep '\.md:' |grep -v html |grep -v http://|grep -v https:// |grep -v header.css |grep -v node_modules |grep -v node_modules | grep -v '\.md' | wc -l`" -gt 0; then \
		echo "Found director links! use ../<directory>/README.md instead!"; \
		echo; \
		egrep -r '\[.*\]\(.*\)' ppbooks/$(NAME) |grep '\.md:' |grep -v html |grep -v http://|grep -v https:// |grep -v header.css |grep -v node_modules |grep -v node_modules | grep -v '\.md' ; \
		exit 1; \
	fi

book-check-markdown-leftovers:
	@echo "##### checking for remaining markdown snippets in the HTML output of $(NAME)"
	@if test "`find books/$(NAME) -name '*.html' -exec grep -- '##' {} \; -print | wc -l`" -gt 0; then \
		echo "found these unconverted markdown titles: "; \
		find books/$(NAME) -name '*.html' -exec grep '##' {} \; -print; \
		exit 1; \
	fi
	@if test "`find books/$(NAME) -name '*.html' -exec grep '\.md\"' {} \; | grep -v data-filepath | wc -l`" -gt 0; then \
		find books/$(NAME) -name '*.html' -exec grep '\.md"' {} \; -print | grep -v data-filepath; \
		echo "found dangling markdown links; see the list above "; \
		exit 1; \
	fi
	@if test "`find books/$(NAME) -name '*.html' -exec grep '\.md#' {} \; -print | wc -l`" -gt 0; then \
		echo "found dangling markdown links: "; \
		find books/$(NAME) -name '*.html' -exec grep '\.md#' {} \; -print; \
		exit 1; \
	fi

book-check-dangling-anchors:
	@echo "##### checking for dangling anchors"
	for htmlf in `find books/ -name \*.html `; do \
	    fn=`basename $${htmlf}` ; \
	    dir=`echo $$htmlf |sed "s;/$$fn;;"`; \
	    mkdir -p /tmp/tags/$${dir}; \
	    cat $${htmlf} |grep '<h. ' |sed -e 's;.*id=";;' -e 's;".*;;' > /tmp/tags/$${dir}/$${fn}; \
	done

	@echo "fetching anchors from generated http files"
	@grep -R "a href.*#" books/ | \
		egrep -v "(styles/header\.js|/app\.js|class=\"navigation|https*://|href=\"#\")" | \
		sed 's;\(.*\.html\):.*a href="\(.*\)#\(.*\)">.*</a>.*;\1,\2,\3;' | grep -v " " > /tmp/anchorlist.txt

	@echo "cross checking anchors"
	@NO=0; \
	for i in `cat /tmp/anchorlist.txt`; do \
	    ANCHOR=`echo $$i | cut '-d,' -f 3`; \
	    FN=`echo $$i | cut '-d,' -f 2`; \
	    SFN=`echo $$i | cut '-d,' -f 1`; \
	    if test -z "$$FN"; then \
		FN=$$SFN; \
	    else \
		SFNP=`echo $$SFN | sed 's;/[a-zA-Z0-9]*.html;;'`; \
		FN="$${SFNP}/$${FN}"; \
	    fi; \
	    if test -d "$$FN"; then \
		FN="$${FN}index.html"; \
	    fi; \
	    if test -n "$$ANCHOR"; then \
		if test ! -f /tmp/tags/$${FN}; then \
			echo "File referenced by $${i} doesn't exist."; \
			NO=$$((NO + 1)); \
		else if grep -q "^$$ANCHOR$$" /tmp/tags/$$FN; then \
			    /bin/true; \
			else \
			    echo "Anchor not found in $$i"; \
			    NO=$$((NO + 1)); \
			fi; \
		    fi; \
	    fi; \
	done; \
	if test "$${NO}" -gt 0; then \
	    echo "$${NO} Dangling anchors found!"; \
	    exit 1; \
	fi
	rm -rf /tmp/anchorlist.txt /tmp/tags

build-book-symlinks:
	echo "##### generate backwards compatibility symlinks for $(NAME)"
	cd books/$(NAME); pwd; \
	find . -name "README.mdpp" |\
	sed -e 's:README\.mdpp$$::' |\
	awk '{print "ln -s index.html " $$1 "README.html"}' |\
	bash

build-book:
	@echo "##### Generating book $(NAME)"
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
	cd ppbooks/$(NAME); if ! test -f HEADER.html ; then cp ../../$(NAME)/HEADER.html . ; fi
	if test -z "${RELEASE_DIRECTORY}"; then \
		if ! test -f ppbooks/$(NAME)/book.json ; then cp $(NAME)/book.json ppbooks/$(NAME) ; fi; \
	else \
		if ! test -f ppbooks/$(NAME)/book.json ; then cat $(NAME)/book.json |sed "s;/devel;/${RELEASE_DIRECTORY};" > ppbooks/$(NAME)/book.json ; fi; \
	fi
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
	cd ppbooks/$(NAME) && sed -i -e 's/VERSION_NUMBER/v$(newVersionNumber)/g' HEADER.html
	cd ppbooks/$(NAME) && sed -i -e 's/VERSION_NUMBER/v$(newVersionNumber)/g' README.md
	cd ppbooks/$(NAME) && sed -i -e 's/VERSION_NUMBER/v$(newVersionNumber)/g' book.json

	test -d books/$(NAME) || mkdir -p books/$(NAME)

	make check-summary
	make book-check-leftover-docublocks
	make book-check-restheader-leftovers
	make book-check-mdpp-leftovers
	make ppbook-check-directory-link

	cd ppbooks/$(NAME) && gitbook install
	cd ppbooks/$(NAME) && gitbook build ./ ./../../books/$(NAME)
	rm -f ./books/$(NAME)/HEADER.html
	python ../Scripts/deprecated.py

	make book-check-markdown-leftovers

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
	grep -R '^@startDocuBlock' ../DocuBlocks --include "*.md" --include "*.mdpp" |grep -v aardvark > /tmp/rawinprog.txt
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
	make clean-book-intermediate NAME=$(NAME) FILTER=$(FILTER)


clean: clean-intermediate
	rm -f allComments.txt


.PHONY: build-books

build-books-keep-md:
	@test -d books || mkdir books
	python ../Scripts/codeBlockReader.py
	make build-book NAME=$(NAME)

build-books:
	rm -rf /tmp/tags
	for book in $(ALLBOOKS); do \
		make clean-intermediate NAME=$${book}; \
	done

	for book in $(ALLBOOKS); do \
		make build-books-keep-md NAME=$${book}; \
	done

	for book in $(ALLBOOKS); do \
		make ppbook-check-html-link NAME=$${book}; \
	done

	make check-docublocks
	make book-check-dangling-anchors
	echo '<html><head><meta http-equiv="refresh" content="0; url=Manual/"></head><body></body></html>' > books/index.html
