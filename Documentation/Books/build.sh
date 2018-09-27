#!/bin/bash
ALLBOOKS="HTTP AQL Manual Cookbook Drivers"
OTHER_MIME="pdf epub mobi"

# shellcheck disable=SC2016
TRIPPLETICS='```'
declare -A COLORS
declare -A C
COLORS[RESET]='\033[0m'
# Regular Colors
#COLORS[Black]='\033[0;30m'        # Black
# shellcheck disable=SC2154
COLORS[Red]='\033[0;31m'          # Red
# shellcheck disable=SC2154
COLORS[Green]='\033[0;32m'        # Green
# shellcheck disable=SC2154
COLORS[Yellow]='\033[0;33m'       # Yellow
#COLORS[Blue]='\033[0;34m'         # Blue
#COLORS[Purple]='\033[0;35m'       # Purple
#COLORS[Cyan]='\033[0;36m'         # Cyan
# shellcheck disable=SC2154
COLORS[White]='\033[0;37m'        # White

for i in "${!COLORS[@]}"; do
    # shellcheck disable=SC2086
    C[${i}]=$(echo -e ${COLORS[$i]})
done

# shellcheck disable=SC2034
WRN_COLOR="${C[Yellow]}"
ERR_COLOR="${C[Red]}"
STD_COLOR="${C[White]}"
OK_COLOR="${C[Green]}"
RESET="${C[RESET]}"

newVersionNumber=$( tr -d '\r\n' < ../../VERSION)

isCygwin=0
if test "$(uname -o||true)" == "Cygwin"; then
    isCygwin=1
fi

declare -A ALL_GSEARCH_ID
for book in ${ALLBOOKS}; do
    ALL_GSEARCH_ID[$book]=$(  grep "GSEARCH_ID_${book}" ../../VERSIONS |sed 's;.*"\([0-9a-zA-Z:_-]*\)".*;\1;')
done


GCHANGE_FREQ=$(grep "GCHANGE_FREQ" ../../VERSIONS |sed 's;.*"\([0-9a-zA-Z:]*\)".*;\1;')
GPRIORITY=$(grep "GPRIORITY" ../../VERSIONS |sed 's;.*"\([0-9a-zA-Z.]*\)".*;\1;')
BROWSEABLE_VERSIONS=$(grep "BROWSEABLE_VERSIONS" ../../VERSIONS |sed -e 's;" *$;;' -e 's;.*";;')

function start_X11_display()
{
    PIDFILE="$1"
    if test -f "${PIDFILE}"; then
        stop_X11_display "${PIDFILE}"
    fi
    /usr/bin/daemon "--pidfile=${PIDFILE}" --name=xvfb --inherit --output=/tmp/xvfb.log --  Xvfb "${DISPLAY}" -screen 0 800x600x16  -ac -pn -noreset
}

function stop_X11_display()
{
    PIDFILE=$1
    kill "$(cat "${PIDFILE}")"
    rm -f "${PIDFILE}"
}

################################################################################
# per book targets
function check-summary()
{
    NAME="$1"
    echo "${STD_COLOR}##### checking summary for ${NAME}${RESET}"
    find "ppbooks/${NAME}" -name \*.md | \
        sed -e "s;ppbooks/${NAME}/;;" | \
        grep -vf SummaryBlacklist.txt | \
        grep -v gitbook-plugin | \
        grep -v node_modules/ | \
        sort > /tmp/is_md.txt

    grep -v '^ *# '< "${NAME}/SUMMARY.md" | \
        grep '(' |sed -e "s;.*(;;" -e "s;).*;;" | \
        sort  > /tmp/is_summary.txt

    if test "$(comm -3 /tmp/is_md.txt /tmp/is_summary.txt|wc -l)" -ne 0; then
        echo "${ERR_COLOR}"
        echo "not all files of ${NAME} are mapped to the summary!"
        echo " files found       |    files in summary"
        comm -3 /tmp/is_md.txt /tmp/is_summary.txt
        echo "${RESET}"
        exit 1
    fi
}

function book-check-leftover-docublocks()
{
    NAME="$1"
    echo "${STD_COLOR}##### checking for left over docublocks in ${NAME}${RESET}"
    ERRORS=$(grep -rl "startDocuBlock" --include "*.md" "ppbooks/${NAME}" | sed -e "s/^/- /g")
    if test "$(echo -n "${ERRORS}" | wc -l)" -gt 0; then
        echo "${ERR_COLOR}"
        echo "startDocuBlock markers still found in generated output files:"
        echo "${ERRORS}"
        echo "${RESET}"
        exit 1
    fi
}

function book-check-restheader-leftovers()
{
    NAME="$1"
    echo "${STD_COLOR}##### checking for restheader leftovers in ${NAME}${RESET}"
    ERRORS=$(find "ppbooks/${NAME}" -not \( -path "ppbooks/Drivers/SpringData/*" -prune \) -name "*.md" -exec grep -- '^@[A-Z]*' {} \; -print)
    if test "$(echo -n "${ERRORS}" | wc -l)" -gt 0; then
        echo "${ERR_COLOR}"
        echo "found these unconverted Swagger Restapi tags: "
        echo "${ERRORS}"
        echo "${RESET}"
        exit 1
    fi
}

function ppbook-precheck-bad-code-sections()
{
    NAME="$1"
    echo "${STD_COLOR}##### checking for bad code sections in ${NAME}${RESET}"
    if grep -qR  "^${TRIPPLETICS} *.* " "${NAME}"; then
        echo "${ERR_COLOR}"
        echo "tripple tics with blanks afterwards found: "
        grep -R  "^${TRIPPLETICS} *.* " "${NAME}"
        echo "${RESET}"
        exit 1
    fi
}

function ppbook-precheck-bad-headings()
{
    NAME="$1"
    echo "${STD_COLOR}##### checking for headers that won't proper display on github in ${NAME}${RESET}"
    if grep -qRI  '^##*[a-zA-Z]' "${NAME}"; then
        echo "${ERR_COLOR}"
        echo "Headlines broken on github found: "
        grep -RI  '^##*[a-zA-Z]' "${NAME}"
        echo "${RESET}"
        exit 1
    fi
}

function ppbook-check-html-link()
{
    NAME="$1"
    MSG="$2"
    echo "${STD_COLOR}##### checking for invalid HTML links in ${NAME}${RESET}"
    echo "${ALLBOOKS}" | tr " " "\n" | sed -e 's;^;/;' -e 's;$;/;' > /tmp/books.regex

    set +e
    grep -r -E '\[.*\]\(.*\)' "ppbooks/${NAME}"| \
        grep '\.md:'| grep 'html'| \
        grep -v 'http://' | \
        grep -v 'https://' | \
        grep -v 'header.css' | \
        grep -v -f /tmp/books.regex > /tmp/relative_html_links.txt
    set -e

    if test "$(wc -l < /tmp/relative_html_links.txt)" -gt 0; then
        echo "${ERR_COLOR}"
        echo "Found links to .html files inside of the document! use <foo>.md instead!"
        echo "${MSG}"
        cat  /tmp/relative_html_links.txt
        echo "${RESET}"
        exit 1
    fi
}

function ppbook-check-directory-link()
{
    NAME="$1"
    echo "${STD_COLOR}##### checking for invalid md links in ${NAME}${RESET}"
    set +e
    ERRORS=$(grep -r -E '\[.*\]\(.*\)' "ppbooks/${NAME}" | \
                    grep '\.md:' | \
                    grep -v html | \
                    grep -v http://| \
                    grep -v https:// | \
                    grep -v header.css | \
                    grep -v node_modules | \
                    grep -v node_modules | \
                    grep -v '\.md')
    set -e
    nERRORS=$(echo -n "${ERRORS}" | wc -l)
    if test "$nERRORS" -gt 0; then
        echo "${ERR_COLOR}"
        echo "Found director links! use ../<directory>/README.md instead!"
        echo "${ERRORS}"
        echo "${RESET}"
        exit 1
    fi
}

function book-check-markdown-leftovers()
{
    NAME="$1"
    echo "${STD_COLOR}##### checking for remaining markdown snippets in the HTML output of ${NAME}${RESET}"
    ERRORS=$(find "books/${NAME}" -name '*.html' -exec grep -- '^##' {} \; -print)
    if test "$(echo -n "${ERRORS}" | wc -l)" -gt 0; then
        echo "${ERR_COLOR}";
        echo "found these unconverted markdown titles: "
        echo "${ERRORS}"
        echo "${RESET}";
        exit 1;
    fi

    set +e
    ERRORS=$(find "books/${NAME}" -name '*.html' -exec grep -- '&amp;gt;' {} \; -print)
    set -e
    if test "$(echo -n "${ERRORS}" | wc -l)" -gt 0; then
        echo "${ERR_COLOR}"
        echo "found these double converted > signs: "
        echo "${ERRORS}"
        echo "${RESET}"
        exit 1;
    fi

    set +e
    ERRORS=$(find "books/${NAME}" -name '*.html' -exec grep '"[a-zA-Z/\.]*\.md\"[ />]' {} \; -print | grep -v data-filepath)
    set -e
    if test "$(echo -n "${ERRORS}" | wc -l)" -gt 0; then
        echo "${ERR_COLOR}"
        echo "${ERRORS}"
        echo "found dangling markdown links; see the list above "
        echo "${RESET}"
        exit 1
    fi

    set +e
    ERRORS=$(find "books/${NAME}" -name '*.html' -exec grep '"[a-zA-Z/\.]*\.md#' {} \; -print)
    set -e
    if test "$(echo -n "${ERRORS}" | wc -l)" -gt 0; then
        echo "${ERR_COLOR}"
        echo "found dangling markdown links: "
        echo "${ERRORS}"
        echo "${RESET}"
        exit 1
    fi

    set +e
    ERRORS=$(find "books/${NAME}" -name '*.html' -exec grep "${TRIPPLETICS}" {} \; -print)
    set -e
    if test "$(echo -n "${ERRORS}" | wc -l)" -gt 0; then
        echo "${ERR_COLOR}"
        echo "found dangling markdown code sections: "
        echo "${ERRORS}"
        echo "${RESET}"
        exit 1
    fi

    set +e
    ERRORS=$(find "books/${NAME}" -name '*.html' -exec grep '\]<a href' {} \; -print)
    set -e
    if test "$(echo -n "${ERRORS}" | wc -l)" -gt 0; then
        echo "${ERR_COLOR}"
        echo "found unconverted markdown links: "
        echo "${ERRORS}"
        echo "${RESET}"
        exit 1
    fi

    set +e
    ERRORS=$(find "books/${NAME}" -name '*.html' -exec grep '\[.*\](.*[\.html|\.md|http|#.*])' {} \; -print)
    set -e
    if test "$(echo -n "${ERRORS}" | wc -l)" -gt 0; then
        echo "${ERR_COLOR}"
        echo "found unconverted markdown links: "
        echo "${ERRORS}"
        echo "${RESET}"
        exit 1
    fi
}

function ppbook-check-two-links()
{
    NAME="$1"
    echo "${STD_COLOR}##### checking for two links in a single line in ${NAME}${RESET}"
    ERRORS=$(find "ppbooks/${NAME}" \
                  -path "ppbooks/${NAME}/node_modules" \
                  -prune -o \
                  -name '*.md' \
                  -print | while IFS= read -r ppfile; do
                 ERR=$(grep -e '](.*](' "${ppfile}" | grep -v '|'||true)
                 if test -n "${ERR}"; then
                     printf "\n${ppfile}: \n ${ERR}"
                 fi
             done
          )
    if test "$(printf "${ERRORS}" | wc -l)" -gt 0; then
        echo "${ERR_COLOR}";
        echo "found these files with two links in one line: "
        echo "${ERRORS}"
        echo "${RESET}";
        exit 1;
    fi
}

function check-dangling-anchors()
{
    rm -rf /tmp/tags/
    echo "${STD_COLOR}##### checking for dangling anchors${RESET}"
    find books/ -name '*.html' | while IFS= read -r htmlf; do
        fn=$(basename "${htmlf}")
        dir=$(sed "s;/$fn;;" <<< "$htmlf")
        mkdir -p "/tmp/tags/${dir}"
        grep '<h. ' < "${htmlf}" | \
            sed -e 's;.*id=";;' -e 's;".*;;' > "/tmp/tags/${dir}/${fn}"
    done

    fail=0
    rm -f /tmp/failduplicatetags.txt
    find /tmp/tags -type f | while IFS= read -r htmlf; do
        (sort "${htmlf}" |grep -v ^$ > /tmp/sorted.txt) || true
        (sort -u "${htmlf}" |grep -v ^$ > /tmp/sortedunique.txt) || true
        if test "$(comm -3 /tmp/sorted.txt /tmp/sortedunique.txt|wc -l)" -ne 0; then
            echo "${ERR_COLOR}"
            echo "in ${htmlf}: "
            comm -3 /tmp/sorted.txt /tmp/sortedunique.txt
            echo "${RESET}"
            touch /tmp/failduplicatetags.txt
        fi       
    done
    
    rm -f /tmp/sorted.txt /tmp/sortedunique.txt
    if test -f /tmp/failduplicatetags.txt; then
        echo "${ERR_COLOR}"
        echo "duplicate anchors detected - see above"
        echo "${RESET}"
        rm -f /tmp/failduplicatetags.txt
        exit 1
    fi

    rm -f /tmp/anchorlist.txt

    echo "${STD_COLOR}##### fetching anchors from generated http files${RESET}"
    for file in $(find books -name \*.html); do 
        # - strip of the menu
        # - then the page tail.
        # - remove links to external pages
        cat $file | \
            sed -r -n -e '/normal markdown-section/,${p}'| \
            sed -r -n -e '/.*id="page-footer".*/q;p' | \
            grep '<a href="' | \
            grep -v 'target="_blank"' | \
            sed -e 's;.*href=";;' -e 's;".*;;' > /tmp/thisdoc.txt
        # Links with anchors:
        cat /tmp/thisdoc.txt |grep '#' | sed "s;\(.*\)#\(.*\);${file},\1,\2;" >> /tmp/anchorlist.txt
        # links without anchors:
        cat /tmp/thisdoc.txt |grep -v '#' | sed "s;\(.*\);${file},\1,;" >> /tmp/anchorlist.txt
        
    done

    echo "${STD_COLOR}##### cross checking anchors${RESET}"
    NO=0
    echo "${NO}" > /tmp/anchorlistcount.txt
    # shellcheck disable=SC2002
    cat /tmp/anchorlist.txt | while IFS= read -r i; do
        ANCHOR=$(echo "$i" | cut '-d,' -f 3)
        FN=$(echo "$i" | cut '-d,' -f 2)
        SFN=$(echo "$i" | cut '-d,' -f 1)

        if test -z "$FN"; then
            FN="$SFN"
        else
            SFNP=$(sed 's;/[a-zA-Z0-9.-]*.html;;' <<< "$SFN")
            FN="${SFNP}/${FN}"
        fi
        if test -d "$FN"; then
            FN="${FN}index.html"
        fi
        if test ! -f "/tmp/tags/${FN}"; then
            echo "${ERR_COLOR}"
            echo "File referenced by ${i} doesn't exist."
            NO=$((NO + 1))
            echo "${RESET}"
        else
            if test -n "$ANCHOR"; then
                if grep -q "^$ANCHOR$" "/tmp/tags/${FN}"; then
                    true
                else
                    echo "${ERR_COLOR}"
                    echo "Anchor not found in $i"
                    NO=$((NO + 1))
                    echo "${RESET}${WRN_COLOR}available anchors in that file:${RESET}${STD_COLOR}"
                    cat "/tmp/tags/${FN}" |sort
                    echo "${RESET}"
                fi
            fi
        fi
        echo "${NO}" > /tmp/anchorlistcount.txt
    done
    NO="$(cat /tmp/anchorlistcount.txt)"
    if test "${NO}" -gt 0; then
        echo "${ERR_COLOR}"
        echo "${NO} Dangling anchors found!"
        echo "${WRN_COLOR}"
        echo "${1}"
        echo "${RESET}"
        exit 1
    fi
    rm -rf /tmp/anchorlist.txt /tmp/tags
}

function book-check-images-referenced()
{
    NAME="$1"
    echo "${STD_COLOR}##### checking for unused image files ${NAME}${RESET}"
    ERRORS=$(find "${NAME}" -name '*.png' | while IFS= read -r image; do
            baseimage=$(basename "$image")
            if ! grep -Rq "${baseimage}" "${NAME}"; then
                printf "\n${image}"
            fi
        done
    )
    if test "$(printf "${ERRORS}" | wc -l)" -gt 0; then
        echo "${ERR_COLOR}";
        echo "the following images are not referenced by any page: "
        echo "${ERRORS}"
        echo "${RESET}";
        exit 1;
    fi
}

function build-book-symlinks()
{
    echo "${STD_COLOR}##### generate backwards compatibility symlinks for ${NAME}${RESET}"
    cd "books/${NAME}"
    pwd
    find . -name "README.md" |\
        sed -e 's:README\.md$::' |\
        awk '{print "ln -s index.html " "$1" "README.html"}' |\
        bash
}

function build-book()
{
    python ../Scripts/codeBlockReader.py || exit 1
    export NAME="$1"
    echo "${STD_COLOR}##### Generating book ${NAME}${RESET}"
    ppbook-precheck-bad-code-sections "${NAME}"
    ppbook-precheck-bad-headings "${NAME}"

    if test ! -d "ppbooks/${NAME}"; then
        mkdir -p "ppbooks/${NAME}"
        WD=$(pwd)
        find "${NAME}" -type d | while IFS= read -r dir; do 
            cd "${WD}/ppbooks"
            test -d "${dir}" || mkdir -p "${dir}"
        done
    fi
    if ditaa --help > /dev/null; then
        echo "${STD_COLOR} - generating ditaa images${RESET}"
        find "${NAME}" -name "*.ditaa" | while IFS= read -r image; do
            mkdir -p $(dirname "ppbooks/${image//ditaa/png}")
            ditaa "${image}" "ppbooks/${image//ditaa/png}"
        done
    else
        echo "${ERR_COLOR} - generating FAKE ditaa images - no ditaa installed${RESET}"
        find "${NAME}" -name "*.ditaa" | while IFS= read -r image; do
            mkdir -p $(dirname "ppbooks/${image//ditaa/png}")
            cp "../../js/node/node_modules/mocha/images/error.png" \
               "ppbooks/${image//ditaa/png}"
        done
    fi
    echo "${STD_COLOR} - preparing environment${RESET}"
    (
        cd "ppbooks/${NAME}"
        if ! test -L SUMMARY.md; then
            ln -s "../../${NAME}/SUMMARY.md" .
        fi
        if ! test -f FOOTER.html ; then
            cp "../../${NAME}/FOOTER.html" .
        fi
    )


    (
        cd "ppbooks/${NAME}"
        mkdir -p styles
        cp -a "../../${NAME}/styles/"* styles/
    )
    WD=$(pwd)

    echo "${STD_COLOR} - generating MD-Files${RESET}"
    python ../Scripts/generateMdFiles.py \
           "${NAME}" \
           ppbooks/ \
           ../../js/apps/system/_admin/aardvark/APP/api-docs.json \
           "${FILTER}" || exit 1

    test -d "books/${NAME}" || mkdir -p "books/${NAME}"

    echo "${STD_COLOR} - Checking integrity ${VERSION}${RESET}"
    check-summary "${NAME}"
    book-check-leftover-docublocks "${NAME}"
    book-check-restheader-leftovers "${NAME}"
    ppbook-check-two-links "${NAME}"
    ppbook-check-directory-link "${NAME}"
    book-check-images-referenced "${NAME}"

    if echo "${newVersionNumber}" | grep -q devel; then
        VERSION="${newVersionNumber} $(date +' %d. %b %Y ')"
        RELEASE_DIRECTORY=devel
    else
        VERSION="${newVersionNumber}"
        RELEASE_DIRECTORY=$(sed "s;\\.[0-9]*$;;" <<< "${newVersionNumber}")
    fi
    export VERSION

    if ! test -f "ppbooks/${NAME}/book.json" ; then
        cp "${NAME}/book.json" "ppbooks/${NAME}"
    fi

    for facilityfile in book.json styles/header.js README.md; do
        export facilityfile
        export RELEASE_DIRECTORY
        (
            cd "ppbooks/${NAME}"
            sed -e "s/VERSION_NUMBER/v${VERSION}/g" \
                -e "s;/devel;/${RELEASE_DIRECTORY};" \
                -e "s;@GSEARCH_ID@;${ALL_GSEARCH_ID[${NAME}]};" \
                -e "s;@GCHANGE_FREQ@;${GCHANGE_FREQ};" \
                -e "s;@GPRIORITY@;${GPRIORITY};" \
                -e "s;@BROWSEABLE_VERSIONS@;${BROWSEABLE_VERSIONS};" \
                \
                -i "${facilityfile}"
        )
    done

    echo "${STD_COLOR} - Building Version ${VERSION}${RESET}"

    if test -d "${NODE_MODULES_DIR}"; then
        echo "${STD_COLOR}#### Installing plugins from ${NODE_MODULES_DIR}${RESET}"
        cp -a "${NODE_MODULES_DIR}" "ppbooks/${NAME}"
    else
        echo "${STD_COLOR}#### Downloading plugins from ${NODE_MODULES_DIR}${RESET}"
        (cd "ppbooks/${NAME}"; gitbook install -g)
    fi
    echo "${STD_COLOR} - Building Book ${NAME} ${RESET}"
    (cd "ppbooks/${NAME}" && gitbook "${GITBOOK_ARGS[@]}" build "./" "./../../books/${NAME}")
    rm -f "./books/${NAME}/FOOTER.html"
    echo "${STD_COLOR} - deleting markdown files in output (gitbook 3.x bug)"
    find "./books/${NAME}/" -type f -name "*.md" -delete

    book-check-markdown-leftovers "${NAME}"
}

function build-book-dist()
{
    NAME="$1"
    export DISPLAY="$2"
    cd "ppbooks/${NAME}"
    for ext in ${OTHER_MIME}; do
        OUTPUT="${OUTPUT_DIR}/ArangoDB_${NAME}_${newVersionNumber}.${ext}"
        if gitbook "${GITBOOK_ARGS[@]}" "${ext}" ./ "${OUTPUT}"; then
            echo "success building ${OUTPUT}"
        else
            exit 1
        fi
    done
}

function clean-book()
{
    NAME="$1"
    rm -rf "books/${NAME}"
    if test -z "${FILTER}"; then
        rm -rf "ppbooks/${NAME}"
    fi
}

function clean-book-intermediate()
{
    NAME="$1"
    if test -z "${FILTER}"; then
        rm -rf "ppbooks/${NAME}"
    fi

}

################################################################################
# Global targets


#************************************************************
# Check docublocks - checks whether docublock are
#  - files in intermediate output directories and temporary
#    files are excludes (with # in their names)
#  - unique in the source
#  - all docublocks are used somewhere in the documentation
#
function check-docublocks()
{
    grep -R '@startDocuBlock' --include "*.h" --include "*.cpp" --include "*.js" --include "*.md" . |\
        grep -v '@startDocuBlockInline' |\
        grep -v stash |\
        grep -v ppbook |\
        grep -v allComments.txt |\
        grep -v Makefile |\
        grep -v '.*~:.*' |\
        grep -v '.*#.*:.*' \
             > /tmp/rawindoc.txt

    grep -R '@startDocuBlockInline' --include "*.h" --include "*.cpp" --include "*.js" --include "*.md" . |\
        grep -v ppbook |\
        grep -v stash |\
        grep -v allComments.txt |\
        grep -v Makefile |\
        grep -v '.*~:.*' |\
        grep -v '.*#.*:.*' \
             >> /tmp/rawindoc.txt

    sed  -e "s;\r$;;" -e "s;.*ck ;;" -e "s;.*ne ;;" < /tmp/rawindoc.txt |sort -u > /tmp/indoc.txt

    set +e
    grep -R '^@startDocuBlock' ../DocuBlocks --include "*.md" |grep -v aardvark > /tmp/rawinprog.txt
    # searching the Inline docublocks needs some more blacklisting:
    grep -R '@startDocuBlockInline' --include "*.h" --include "*.cpp" --include "*.js" --include "*.md" . |\
        grep -v ppbook |\
        grep -v stash |\
        grep -v allComments.txt |\
        grep -v build.sh |\
        grep -v '.*~:.*' |\
        grep -v '.*#.*:.*' \
             >> /tmp/rawinprog.txt

    # These files are converted to docublocks on the fly and only live in memory. 
    for file in ../Examples/*.json ; do
        echo "$file" |sed -e "s;.*/;Generated: @startDocuBlock program_options_;" -e "s;.json;;" >> /tmp/rawinprog.txt
    done
    set -e
    echo "Generated: startDocuBlockInline errorCodes">> /tmp/rawinprog.txt

    sed -e "s;\r$;;" -e "s;.*ck ;;" -e "s;.*ne ;;" < /tmp/rawinprog.txt  |sort > /tmp/inprog_raw.txt
    sort -u < /tmp/inprog_raw.txt > /tmp/inprog.txt

    if test "$(wc -l < /tmp/inprog.txt)" -ne "$(wc -l < /tmp/inprog_raw.txt)"; then 
        echo "${ERR_COLOR}"
        echo "Duplicate entry found in the source trees:"
        comm -3 /tmp/inprog_raw.txt /tmp/inprog.txt
        echo "${WRN_COLOR}"
        echo "${1}"
        echo "${RESET}"
        exit 1
    fi

    if test "$(comm -3 /tmp/indoc.txt /tmp/inprog.txt |wc -l)" -ne 0; then
        echo "${ERR_COLOR}"
        echo "Not all blocks were found on both sides:"
        echo "Documentation      |     Programcode:"
        comm -3 /tmp/indoc.txt /tmp/inprog.txt
        if test "$(comm -2 -3 /tmp/indoc.txt /tmp/inprog.txt |wc -l)" -gt 0; then
            echo "Documentation: "
            for grepit in $(comm -2 -3 /tmp/indoc.txt /tmp/inprog.txt); do
                grep "$grepit" /tmp/rawindoc.txt
            done
        fi
        if test "$(comm -1 -3 /tmp/indoc.txt /tmp/inprog.txt |wc -l)" -gt 0; then
            echo "Program code:"
            for grepit in $(comm -1 -3 /tmp/indoc.txt /tmp/inprog.txt); do
                grep "$grepit" /tmp/rawinprog.txt | sed "s;/// @startDocuBlock;\t\t;"
            done
        fi
        echo "${WRN_COLOR}"
        echo "${1}"
        echo "${RESET}"
        exit 1
    fi
}

function clean-intermediate()
{
    NAME=$1
    FILTER=$2
    clean-book-intermediate "${NAME}" "${FILTER}"
}

function clean()
{
    NAME=$1
    clean-intermediate "${NAME}"
    rm -f allComments.txt
}

function build-book-keep-md()
{
    NAME="$1"
    test -d books || mkdir books
    build-book "${NAME}"
}

function build-books()
{
    rm -rf /tmp/tags
    set -e
    for book in ${ALLBOOKS}; do
        clean-intermediate "${book}"
    done

    for book in ${ALLBOOKS}; do
        build-book-keep-md "${book}"
    done

    for book in ${ALLBOOKS}; do
        ppbook-check-html-link "${book}" ""
    done

    check-docublocks ""
    echo "${STD_COLOR}##### Generating redirect index.html${RESET}"; \
    echo '<html><head><meta http-equiv="refresh" content="0; url=Manual/"></head><body></body></html>' > books/index.html
    check-dangling-anchors ""
}

function build-dist-books()
{
    set -e
    if test -z "${OUTPUT_DIR}"; then
        echo "please specify --outputDir"
        exit 1
    fi
    rm -rf books ppbooks
    PIDFILE=/tmp/xvfb_20_0.pid
    if test "${isCygwin}" -eq 0 -a -z "${DISPLAY}"; then
        DISPLAY=:20.0
        start_X11_display "${PIDFILE}" "${DISP}"
        trap 'stop_X11_display "${PIDFILE}"' 0
    fi
    export DISPLAY

    WD=$(pwd)
    build-books
    mkdir -p "${OUTPUT_DIR}"
    (
        mv books "ArangoDB-${newVersionNumber}"
        pwd
        tar -czf "${OUTPUT_DIR}/ArangoDB-${newVersionNumber}.tar.gz" "ArangoDB-${newVersionNumber}"
        mv "ArangoDB-${newVersionNumber}" books
    )
    for book in $ALLBOOKS; do
        cd "$WD"; build-book-dist "${book}"
    done
}

function printHelp()
{
    cat <<EOF
Usage: VERB arguments
Available Verbs:
    build-dist-books - build all books in all representations (HTML(+tarball)/PDF/...) - takes some time.
    build-books - builds the HTML representation of all books
    build-book - build one book specified with --name, optionally specify --filter to limit the md files to be regenerated.
    build-book-keep-md - don't flush pregenerated files while building a book - shortcut version.
    clean - clean the working directory

If ../../VERSION contains the string "devel" the current date will be added to all pages containing the version.

Available Arguments:
    name - if a single book is to be built its name - one of [ ${ALLBOOKS} ]
    filter - if only one file should be processed (speedup) specify its md filename
    outputDir - [dist target only] where to put all generated files to
    nodeModulesDir - directory pre-loaded with the required gitbook plugins so we don't need to download them

EOF
}

VERB=$1
shift

if test -z "$VERB"; then
    VERB=build-books
fi

while [ $# -gt 0 ];  do
    case "$1" in
        --name)
            shift
            NAME=$1
            shift
            ;;
        --filter)
            shift
            FILTER=$1
            export FILTER
            shift
            ;;
        --outputDir)
            shift
            OUTPUT_DIR=$1
            shift
            ;;
        --cookBook)
            shift
            shift
            ;;
        --nodeModulesDir)
            shift
            NODE_MODULES_DIR=$1
            export NODE_MODULES_DIR
            shift
            ;;
        *)
            printHelp
            exit 1
            ;;

    esac
done

case "$VERB" in
    build-books)
        build-books
        ;;
    build-book)
        if test -z "$NAME"; then
            echo "you need to specify the name!"
            printHelp
            exit 1
        fi
        build-book "$NAME"
        check-docublocks "some of the above errors may be because of referenced books weren't rebuilt."
        check-dangling-anchors "some of the above errors may be because of referenced books weren't rebuilt."
        ppbook-check-html-link "${NAME}" "some of the above errors may be because of referenced books weren't rebuilt."
        ;;
    check-book)
        check-summary "${NAME}"
        book-check-leftover-docublocks "${NAME}"
        book-check-restheader-leftovers "${NAME}"
        ppbook-check-two-links "${NAME}"
        ppbook-check-directory-link "${NAME}"
        book-check-images-referenced "${NAME}"
        book-check-markdown-leftovers "${NAME}"        
        check-dangling-anchors "${NAME}" "some of the above errors may be because of referenced books weren't rebuilt."
        ;;
    build-dist-books)
        build-dist-books
        ;;
    build-book-keep-md)
        if test -z "$NAME"; then
            echo "you need to specify the name!"
            printHelp
            exit 1
        fi
        build-book-keep-md "$NAME"
        ;;
    clean)
        clean "$@"
        ;;
    *)
        if test -d "${VERB}"; then
            guessBookName="${VERB/\/}"
            if [[ $ALLBOOKS = *"${guessBookName}"* ]]; then
                build-book "$guessBookName"
                check-docublocks "some of the above errors may be because of referenced books weren't rebuilt."
                check-dangling-anchors "some of the above errors may be because of referenced books weren't rebuilt."
                exit 0
            fi
        fi
        printHelp
        exit 1
        ;;
esac

echo "${OK_COLOR}Well done!${RESET}"
