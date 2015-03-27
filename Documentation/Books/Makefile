all: build-books

.PHONY: md-files

MDPP_FILES = $(wildcard Users/*.mdpp Users/*/*.mdpp Users/*/*/*.mdpp)
MD_FILES = $(wildcard Users/*/*.md Users/*/*/*.md)

md-files:
	@for file in $(basename $(MDPP_FILES)); do echo "converting $${file}.mdpp"; markdown-pp.py $${file}.mdpp $${file}.md; done

clean-md-files:
	@for file in $(basename $(MD_FILES)); do rm -v $${file}.md; done

clean-books:
	@rm -rv books

clean: clean-md-files clean-books

.PHONY: build-books

build-books: build-books-keep-md
	make clean-md-files

build-books-keep-md: md-files
	@test -d books || mkdir books
	make build-book NAME=Users

build-book:
	@test -d books/$(NAME) || mkdir books/$(NAME)
	python codeBlockReader.py
	python bot.py
	cd $(NAME) && gitbook build ./ ./../books/$(NAME)
	cp Users/arangodbLogo.png books/Users/
	cp Users/cookbookLogo.png books/Users/
	cp Users/googlegroupsIcon.png books/Users/
	python deprecated.py
