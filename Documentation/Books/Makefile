all: build-books

.PHONY: md-files

MDPP_FILES = $(wildcard Users/*.mdpp Users/*/*.mdpp Users/*/*/*.mdpp)
MD_FILES = $(wildcard Users/*/*.md Users/*/*/*.md)
newVersionNumber = $(shell cat ../../VERSION)

md-files:
	@for file in $(basename $(MDPP_FILES)); do echo "converting $${file}.mdpp"; markdown-pp.py $${file}.mdpp $${file}.md; done

clean-md-files:
	@for file in $(basename $(MD_FILES)); do rm -v $${file}.md; done
	rm -f  `find -name \*.mdpp |sed "s;.mdpp;.md;"`

clean-books:
	@rm -rv books

check-docublocks:
	grep -R '^@startDocuBlock' . |grep -v allComments.txt > /tmp/rawindoc.txt
	cat /tmp/rawindoc.txt | sed "s;.*ck ;;" |sort -u > /tmp/indoc.txt
	grep -R '^/// @startDocuBlock' ../../lib ../../arangod ../../arangosh ../../js |grep -v aardvark > /tmp/rawinprog.txt
	cat /tmp/rawinprog.txt |sed "s;.*ck ;;" |sort > /tmp/inprog_raw.txt
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
			echo "Programm code:"; \
			for grepit in `comm -1 -3 /tmp/indoc.txt /tmp/inprog.txt`; do \
				grep "$$grepit" /tmp/rawinprog.txt | sed "s;/// @startDocuBlock;\t\t;"; \
			done; \
		fi; \
		exit 1; \
	fi

check-summary:
	@find . -name \*.md |grep -v books/ |sed -e "s;./Users/;;" |grep -vf SummaryBlacklist.txt |sort > /tmp/is_md.txt
	@cat Users/SUMMARY.md |sed -e "s;.*(;;" -e "s;).*;;" |sort |grep -v '# Summary' > /tmp/is_summary.txt
	@if test "`comm -3 /tmp/is_md.txt /tmp/is_summary.txt|wc -l`" -ne 0; then \
		echo "not all files are mapped to the summary!"; \
		echo " files found       |    files in summary"; \
		comm -3 /tmp/is_md.txt /tmp/is_summary.txt; \
		exit 1; \
	fi

book-check-leftover-docublocks:
	@if test "`grep -r \"@startDocuBlock\" --include \"*.html\" books/$(NAME) | wc -l`" -ne 0; then \
	  echo ; \
	  echo "@startDocuBlock markers still found in generated output files:"; \
	  grep -rl "@startDocuBlock" --include "*.html" books/$(NAME) | sed -e "s/^/- /g"; \
	  exit 1; \
	fi

book-check-markdown-leftovers:
	@if test "`find books/Users/ -name '*.html' -exec grep -- '##' {} \; -print | wc -l`" -gt 0; then \
		echo "found these unconverted markdown titles: "; \
		find books/Users/ -name '*.html' -exec grep '##' {} \; -print; \
		exit 1; \
	fi

clean: clean-md-files clean-books
	rm -f allComments.txt

.PHONY: build-books

build-books: build-books-keep-md # not now: check-docublocks
	make clean-md-files
	echo "generate backwards compatibility symlinks:"
	cd books/Users; pwd; \
	find . -name "README.mdpp" |\
	sed -e 's:README\.mdpp$$::' |\
	awk '{print "ln -s index.html " $$1 "README.html"}' |\
	bash

build-books-keep-md: md-files check-summary
	@test -d books || mkdir books
	make build-book NAME=Users
	make book-check-leftover-docublocks NAME=Users
	make book-check-markdown-leftovers NAME=Users

build-book:
	@test -d books/$(NAME) || mkdir books/$(NAME)
	python codeBlockReader.py
	python bot.py
	cd $(NAME) && gitbook install
	cd $(NAME) && gitbook build ./ ./../books/$(NAME)
	python deprecated.py
	cd books/$(NAME) && sed -ie 's/VERSION_NUMBER/$(newVersionNumber)/g' styles/header.js
