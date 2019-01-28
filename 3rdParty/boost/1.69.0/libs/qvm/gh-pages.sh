set -e
asciidoctor README.adoc -o index.html
git add -u README.adoc
git add -u index.html
git commit -m "Documentation update"
git push
cp index.html index1.html
git checkout gh-pages
rm index.html
mv index1.html index.html
git add -u index.html
git commit -m "Documentation update"
git push
git checkout master
