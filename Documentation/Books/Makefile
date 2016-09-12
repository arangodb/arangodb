ALLBOOKS=HTTP AQL Manual

all: build-books

.PHONY:

RESET          = \033[0m
make_std_color = \033[3$1m      # defined for 1 through 7
make_color     = \033[38;5;$1m  # defined for 1 through 255
WRN_COLOR = $(strip $(call make_std_color,3))
ERR_COLOR = $(strip $(call make_std_color,1))
STD_COLOR = $(strip $(call make_color,8))

newVersionNumber = $(shell cat ../../VERSION)

################################################################################
# per book targets
check-summary:
	@echo "${STD_COLOR}##### checking summary for $(NAME)${RESET}"
	@find ppbooks/$(NAME) -name \*.md |sed -e "s;ppbooks/$(NAME)/;;" |grep -vf SummaryBlacklist.txt |grep -v gitbook-plugin |sort > /tmp/is_md.txt
	@cat $(NAME)/SUMMARY.md |grep -v '^ *# '|grep '(' |sed -e "s;.*(;;" -e "s;).*;;" |sort  > /tmp/is_summary.txt
	@if test "`comm -3 /tmp/is_md.txt /tmp/is_summary.txt|wc -l`" -ne 0; then \
		echo "${ERR_COLOR}"; \
		echo "not all files are mapped to the summary!"; \
		echo " files found       |    files in summary"; \
		comm -3 /tmp/is_md.txt /tmp/is_summary.txt; \
		echo "${RESET}"; \
		exit 1; \
	fi

book-check-leftover-docublocks:
	@echo "${STD_COLOR}##### checking for left over docublocks in $(NAME)${RESET}"
	@if test "`grep -r \"@startDocuBlock\" --include \"*.md\" ppbooks/$(NAME) | wc -l`" -ne 0; then \
	  echo "${ERR_COLOR}"; \
	  echo "@startDocuBlock markers still found in generated output files:"; \
	  grep -rl "@startDocuBlock" --include "*.md" ppbooks/$(NAME) | sed -e "s/^/- /g"; \
	  echo "${RESET}"; \
	  exit 1; \
	fi

book-check-restheader-leftovers:
	@echo "${STD_COLOR}##### checking for restheader leftovers in $(NAME)${RESET}"
	@if test "`find ppbooks/$(NAME) -name '*.md' -exec grep -- '^@[A-Z]*' {} \; -print | wc -l`" -gt 0; then \
		echo "${ERR_COLOR}"; \
		echo "found these unconverted Swagger Restapi tags: "; \
		find ppbooks/$(NAME) -name '*.md' -exec grep '^@[A-Z]*' {} \; -print; \
		echo "${RESET}"; \
		exit 1; \
	fi

# Check for all lines starting with exclamation marks, except image links which are exclamation mar + bracket
book-check-mdpp-leftovers:
	@echo "${STD_COLOR}##### checking for mdpp leftovers for $(NAME)${RESET}"
	@if test "`find ppbooks/$(NAME) -name '*.md' -exec grep -- '^![^\[!]' {} \; -print | wc -l`" -gt 0; then \
		echo "${ERR_COLOR}"; \
		echo "found these unconverted Markdown-PP tags: "; \
		find ppbooks/$(NAME) -name '*.md' -exec grep '^![^\[!]' {} \; -print; \
		echo "${RESET}"; \
		exit 1; \
	fi

ppbook-precheck-bad-code-sections:
	@echo "${STD_COLOR}##### checking for bad code sections in $(NAME)${RESET}"
	@if grep -qR  '^``` *.* ' $(NAME); then \
		echo "${ERR_COLOR}"; \
		echo "tripple tics with blanks afterwards found: "; \
		grep -R  '^``` *.* ' $(NAME); \
		echo "${RESET}"; \
		exit 1; \
	fi

ppbook-check-html-link:
	@echo "${STD_COLOR}##### checking for invalid HTML links in $(NAME)${RESET}"
	@echo "$(ALLBOOKS)" | tr " " "\n" | sed -e 's;^;/;' -e 's;$$;/;' > /tmp/books.regex

	@egrep -r '\[.*\]\(.*\)' ppbooks/$(NAME)|grep '\.md:'| grep 'html'| grep -v 'http://' | grep -v 'https://' | grep -v 'header.css' | grep -v -f /tmp/books.regex > /tmp/relative_html_links.txt || true
	@if test "`cat /tmp/relative_html_links.txt |wc -l`" -gt 0; then \
		echo "${ERR_COLOR}"; \
		echo "Found links to .html files inside of the document! use <foo>.md instead!"; \
		echo; \
		cat  /tmp/relative_html_links.txt; \
		echo "${RESET}"; \
		exit 1; \
	fi

ppbook-check-directory-link:
	@echo "${STD_COLOR}##### checking for invalid md links in $(NAME)${RESET}"
	@if test "`egrep -r '\[.*\]\(.*\)' ppbooks/$(NAME) | grep '\.md:' |grep -v html |grep -v http://|grep -v https:// |grep -v header.css |grep -v node_modules |grep -v node_modules | grep -v '\.md' | wc -l`" -gt 0; then \
		echo "${ERR_COLOR}"; \
		echo "Found director links! use ../<directory>/README.md instead!"; \
		echo; \
		egrep -r '\[.*\]\(.*\)' ppbooks/$(NAME) |grep '\.md:' |grep -v html |grep -v http://|grep -v https:// |grep -v header.css |grep -v node_modules |grep -v node_modules | grep -v '\.md' ; \
		echo "${RESET}"; \
		exit 1; \
	fi

book-check-markdown-leftovers:
	@echo "${STD_COLOR}##### checking for remaining markdown snippets in the HTML output of $(NAME)${RESET}"
	@if test "`find books/$(NAME) -name '*.html' -exec grep -- '##' {} \; -print | wc -l`" -gt 0; then \
		echo "${ERR_COLOR}"; \
		echo "found these unconverted markdown titles: "; \
		find books/$(NAME) -name '*.html' -exec grep '##' {} \; -print; \
		echo "${RESET}"; \
		exit 1; \
	fi
	@if test "`find books/$(NAME) -name '*.html' -exec grep -- '&amp;gt;' {} \; -print | wc -l`" -gt 0; then \
		echo "${ERR_COLOR}"; \
		echo "found these double converted > signs: "; \
		find books/$(NAME) -name '*.html' -exec grep '&amp;gt;' {} \; -print; \
		echo "${RESET}"; \
		exit 1; \
	fi
	@if test "`find books/$(NAME) -name '*.html' -exec grep '\.md\"' {} \; | grep -v data-filepath | wc -l`" -gt 0; then \
		echo "${ERR_COLOR}"; \
		find books/$(NAME) -name '*.html' -exec grep '\.md"' {} \; -print | grep -v data-filepath; \
		echo "found dangling markdown links; see the list above "; \
		echo "${RESET}"; \
		exit 1; \
	fi
	@if test "`find books/$(NAME) -name '*.html' -exec grep '\.md#' {} \; -print | wc -l`" -gt 0; then \
		echo "${ERR_COLOR}"; \
		echo "found dangling markdown links: "; \
		find books/$(NAME) -name '*.html' -exec grep '\.md#' {} \; -print; \
		echo "${RESET}"; \
		exit 1; \
	fi
	@if test "`find books/$(NAME) -name '*.html' -exec grep '\`\`\`' {} \; -print | wc -l`" -gt 0; then \
		echo "${ERR_COLOR}"; \
		echo "found dangling markdown code sections: "; \
		find books/$(NAME) -name '*.html' -exec grep '```' {} \; -print| sed "s;^books/$(NAME)/\(.*\).html;\n   In file: $(NAME)/\1.mdpp\n\n;"; \
		echo "${RESET}"; \
		exit 1; \
	fi
	@if test "`find books/$(NAME) -name '*.html' -exec grep '\]<a href' {} \; -print | wc -l`" -gt 0; then \
		echo "${ERR_COLOR}"; \
		echo "found unconverted markdown links: "; \
		find books/$(NAME) -name '*.html' -exec grep '\]<a href' {} \; -print| sed "s;^books/$(NAME)/\(.*\).html;\n   In file: $(NAME)/\1.mdpp\n\n;"; \
		echo "${RESET}"; \
		exit 1; \
	fi
	@if test "`find books/$(NAME) -name '*.html' -exec grep '\[.*\](.*[\.html|\.md|http|#.*])' {} \; -print | wc -l`" -gt 0; then \
		echo "${ERR_COLOR}"; \
		echo "found unconverted markdown links: "; \
		find books/$(NAME) -name '*.html' -exec grep '\[.*\](.*[\.html|\.md|http.*|#.*])' {} \; -print| sed "s;^books/$(NAME)/\(.*\).html;\n   In file: $(NAME)/\1.mdpp\n\n;"; \
		echo "${RESET}"; \
		exit 1; \
	fi

book-check-dangling-anchors:
	@echo "${STD_COLOR}##### checking for dangling anchors${RESET}"
	for htmlf in `find books/ -name \*.html `; do \
	    fn=`basename $${htmlf}` ; \
	    dir=`echo $$htmlf |sed "s;/$$fn;;"`; \
	    mkdir -p /tmp/tags/$${dir}; \
	    cat $${htmlf} |grep '<h. ' |sed -e 's;.*id=";;' -e 's;".*;;' > /tmp/tags/$${dir}/$${fn}; \
	done

	@echo "${STD_COLOR}fetching anchors from generated http files${RESET}"
	@grep -R "a href.*#" books/ | \
		egrep -v "(styles/header\.js|/app\.js|class=\"navigation|https*://|href=\"#\")" | \
		sed 's;\(.*\.html\):.*a href="\(.*\)#\(.*\)">.*</a>.*;\1,\2,\3;' | grep -v " " > /tmp/anchorlist.txt

	@echo "${STD_COLOR}cross checking anchors${RESET}"
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
			echo "${ERR_COLOR}"; \
			echo "File referenced by $${i} doesn't exist."; \
			NO=$$((NO + 1)); \
			echo "${RESET}"; \
		else if grep -q "^$$ANCHOR$$" /tmp/tags/$$FN; then \
			    true; \
			else \
			    echo "${ERR_COLOR}"; \
			    echo "Anchor not found in $$i"; \
			    NO=$$((NO + 1)); \
			    echo "${RESET}"; \
			fi; \
		    fi; \
	    fi; \
	done; \
	if test "$${NO}" -gt 0; then \
		echo "${ERR_COLOR}"; \
		echo "$${NO} Dangling anchors found!"; \
		echo "${RESET}"; \
	    exit 1; \
	fi
	rm -rf /tmp/anchorlist.txt /tmp/tags

book-check-images-referenced:
	exitcode=0; \
	for image in `find $(NAME) -name \*.png `; do \
		baseimage=`basename $$image`; \
		if ! grep -Rq $$baseimage $(NAME); then \
			echo "${ERR_COLOR}"; \
			echo "$$image is not used!";\
			echo "${RESET}"; \
			exitcode=1;\
		fi; \
	done; \
	exit $${exitcode}

build-book-symlinks:
	echo "${STD_COLOR}##### generate backwards compatibility symlinks for $(NAME)${RESET}"
	cd books/$(NAME); pwd; \
	find . -name "README.mdpp" |\
	sed -e 's:README\.mdpp$$::' |\
	awk '{print "ln -s index.html " $$1 "README.html"}' |\
	bash

build-book:
	@echo "${STD_COLOR}##### Generating book $(NAME)${RESET}"
	make ppbook-precheck-bad-code-sections $(NAME)

	@if test ! -d ppbooks/$(NAME); then \
	  mkdir -p ppbooks/$(NAME); \
	  WD=`pwd`; \
	  for dir in `find $(NAME) -type d `; do \
		cd $${WD}/ppbooks; \
		test -d $${dir} || mkdir -p $${dir}; \
	  done; \
	fi
	@if ditaa --help > /dev/null; then \
	   echo "${STD_COLOR}generating ditaa images${RESET}"; \
	   for image in `find $(NAME) -name \*.ditaa`; do \
		ditaa $${image} ppbooks/`echo $${image}|sed "s;ditaa;png;"` ; \
	   done; \
	else \
	   echo "${ERR_COLOR}generating FAKE ditaa images - no ditaa installed${RESET}"; \
	   for image in `find $(NAME) -name \*.ditaa`; do \
		cp ../../js/node/node_modules/mocha/images/error.png ppbooks/`echo $${image}|sed "s;ditaa;png;"` ; \
	   done; \
	fi

	cd ppbooks/$(NAME); if ! test -L SUMMARY.md; then ln -s ../../$(NAME)/SUMMARY.md . ; fi
	cd ppbooks/$(NAME); if ! test -f HEADER.html ; then cp ../../$(NAME)/HEADER.html . ; fi
	@if test -z "${RELEASE_DIRECTORY}"; then \
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
	make book-check-images-referenced

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
#  - unique in the source
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
		echo "${ERR_COLOR}"; \
		echo "Duplicate entry found in the source trees:"; \
		comm -3 /tmp/inprog_raw.txt /tmp/inprog.txt; \
		echo "${RESET}"; \
		exit 1; \
	fi
	if test "`comm -3 /tmp/indoc.txt /tmp/inprog.txt |wc -l `" -ne 0; then \
		echo "${ERR_COLOR}"; \
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
		echo "${RESET}"; \
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
	set -e; for book in $(ALLBOOKS); do \
		make clean-intermediate NAME=$${book}; \
	done

	set -e; for book in $(ALLBOOKS); do \
		make build-books-keep-md NAME=$${book}; \
	done

	set -e; for book in $(ALLBOOKS); do \
		make ppbook-check-html-link NAME=$${book}; \
	done

	make check-docublocks
	make book-check-dangling-anchors
	echo "${STD_COLOR}Generating redirect index.html${RESET}"; \
	echo '<html><head><meta http-equiv="refresh" content="0; url=Manual/"></head><body></body></html>' > books/index.html
