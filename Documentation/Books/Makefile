all: build-books

.PHONY: md-files

MDPP_FILES = $(wildcard *.mdpp */*.mdpp */*/*.mdpp)

md-files:
  @for file in $(basename $(MDPP_FILES)); do echo "converting $${file}.mdpp"; markdown-pp.py $${file}.mdpp $${file}.md; done


.PHONY: build-books

build-books: md-files
  @test -d books || mkdir books
  make build-book NAME=Users

build-book:
  @test -d books/$(NAME) || mkdir books/$(NAME)
  cd $(NAME) && gitbook build -o ../books/$(NAME)
  cp Users/arangodb_logo2.png books/Users/