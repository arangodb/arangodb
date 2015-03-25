This is a simple plain-markdown template.

It can be used to convert your docco output even further,
for example to output to PDF via `LaTeX` with [`pandoc`][pandoc].

The resulting markdown uses code fences (<code>```</code>), 
which is compatible with Github- and pandoc-flavoured markdown.

Example PDF output:

    # fetch some annotated source code
    cd /tmp
    wget https://raw.github.com/documentcloud/underscore/master/underscore.js 
    # make plain with docco
    docco -l plain-markdown underscore.js
    cd ./docs
    # make PDF from plaintext with pandoc
    pandoc -f markdown docs/underscore.html -o underscore.pdf


`pandoc` has a ton of options and output formats, you could also export to ePub, iBook- and Kindle-eBooks, Mediawiki markup, etc. [RTFM][pandoc-man]!

An full-fledged scientific-looking PDF could use these options:

    pandoc -f markdown \
      --table-of-contents \
      --highlight-style=pygments \
      -V title="Underscore.js" \
      -V author="Prof. Dr. J. Ashkenas" \
      -V date="$(date +%d-%m-%Y)" \
      -V documentclass="book" \
      --chapters \
      -o docs/underscore.pdf \
      docs/underscore.html


This is as quick-and-dirty as the original docco,
so there are some **bugs**:

- The code blocks don't always fit on paper (if the lines are too long). This is a serious issue, but could be fixed "easily" in [pandoc's LaTeX template][] (where "easy" is relative, as always when dealing with LaTeX)

- No syntax higlighting. Could maybe built into the template -- it would be just a matter of printing something like <code>```javascript</code> as the start of each code fence.

- The plaintext output file is markdown, but the filename has .html (not really important since it is just an intermediary file)



[pandoc]: http://johnmacfarlane.net/pandoc/index.html
[pandoc-man]: http://johnmacfarlane.net/pandoc/README.html
[pandoc's LaTeX template]: https://github.com/jgm/pandoc-templates/blob/master/default.latex
