all: build-books

.PHONY:

newVersionNumber = $(shell cat ../../VERSION)

################################################################################
# per book targets
check-summary:
	@find . -name \*.md |grep -v books/ |sed -e "s;./$(NAME)/;;" |grep -vf SummaryBlacklist.txt |sort > /tmp/is_md.txt
	@cat $(NAME)/SUMMARY.md |sed -e "s;.*(;;" -e "s;).*;;" |sort |grep -v '# Summary' > /tmp/is_summary.txt
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
	@if test "`find books/$(NAME) -name '*.html' -exec grep -- '##' {} \; -print | wc -l`" -gt 0; then \
		echo "found these unconverted markdown titles: "; \
		find books/$(NAME) -name '*.html' -exec grep '##' {} \; -print; \
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
	test -d ppbooks/$(NAME) || mkdir -p ppbooks/$(NAME)
	cd ppbooks/$(NAME); test -l SUMMARY.md || ln -s ../../$(NAME)/SUMMARY.md .
	cd ppbooks/$(NAME); test -l book.json || ln -s ../../$(NAME)/book.json .
	cd ppbooks/$(NAME); test d styles || cp -a ../../$(NAME)/styles .
	cd ppbooks/$(NAME) && sed -ie 's/VERSION_NUMBER/$(newVersionNumber)/g' styles/header.js
	python generateMdFiles.py $(NAME) ppbooks/

	test -d books/$(NAME) || mkdir -p books/$(NAME)

	cd ppbooks/$(NAME) && gitbook install
	cd ppbooks/$(NAME) && gitbook build ./ ./../../books/$(NAME)
	python deprecated.py

	make book-check-leftover-docublocks
	make book-check-markdown-leftovers

clean-book:
	@rm -rvf books/$(NAME) ppbooks/$(NAME)

clean-book-intermediate:
	@rm -rvf ppbooks/$(NAME)


################################################################################
# Global targets

check-docublocks:
	grep -R '@startDocuBlock' . |grep -v allComments.txt > /tmp/rawindoc.txt
	cat /tmp/rawindoc.txt | sed "s;.*ck ;;" |sort -u > /tmp/indoc.txt
	grep -R '^/// @startDocuBlock' ../../lib ../../arangod ../../arangosh ../../js |grep -v aardvark > /tmp/rawinprog.txt
	grep -R '@startDocuBlockInline' >> /tmp/rawinprog.txt
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
			echo "Programm code:"; \
			for grepit in `comm -1 -3 /tmp/indoc.txt /tmp/inprog.txt`; do \
				grep "$$grepit" /tmp/rawinprog.txt | sed "s;/// @startDocuBlock;\t\t;"; \
			done; \
		fi; \
		exit 1; \
	fi

clean-intermediate:
	make clean-book-intermediate NAME=Users


clean: clean-intermediate
	rm -f allComments.txt


.PHONY: build-books

build-books-keep-md:
	@test -d books || mkdir books
	python codeBlockReader.py
	make build-book NAME=Users

build-books: clean-intermediate build-books-keep-md check-docublocks
