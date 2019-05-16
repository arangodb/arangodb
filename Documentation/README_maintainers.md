# ArangoDB Documentation Maintainers manual

- [Using Docker container](#using-docker-container)
- [Installing on local system](#installing-on-local-system)
- [Add / Synchronize external documentation](#add--synchronize-external-documentation)
- [Generate users documentation](#generate-users-documentation)
- [Using Gitbook](#using-gitbook)
- [Examples](#examples)
  * [Where to add new...](#where-to-add-new)
  * [generate](#generate)
- [write markdown](#write-markdown)
- [Include ditaa diagrams](#include-ditaa-diagrams)
- [Read / use the documentation](#read--use-the-documentation)
- [arangod Example tool](#arangod-example-tool)
  * [OUTPUT, RUN and AQL specifics](#output-run-and-aql-specifics)
- [Swagger integration](#swagger-integration)
  * [RESTQUERYPARAMETERS](#restqueryparameters)
  * [RESTURLPARAMETERS](#resturlparameters)
  * [RESTDESCRIPTION](#restdescription)
  * [RESTRETURNCODES](#restreturncodes)
  * [RESTREPLYBODY](#restreplybody)
  * [RESTHEADER](#restheader)
  * [RESTURLPARAM](#resturlparam)
  * [RESTALLBODYPARAM](#restallbodyparam)
  * [RESTBODYPARAM](#restbodyparam)
  * [RESTSTRUCT](#reststruct)

# Using Docker container

We provide the docker container `arangodb/documentation-builder` which brings
all neccessary dependencies to build the documentation.

The files and a description how to (re-)generate the Docker image are here:<br/>
https://github.com/arangodb-helper/build-docker-containers/tree/master/distros/debian/jessie.docu

You can automagically build it using

    ./scripts/generateDocumentation.sh

which will start the docker container, compile ArangoDB, generate fresh example snippets, generate swagger, and all gitbook
produced output files.

You can also use `proselint` inside of that container to let it proof read your english ;-)

# Installing on local system

Dependencies to build documentation:

- [swagger 2](http://swagger.io/) for the API-Documentation inside aardvark (no installation required)

- [Node.js](https://nodejs.org/)

    Make sure the option to *Add to PATH* is enabled.
    After installation, the following commands should be available everywhere:

    - `node`
    - `npm`

    If not, add the installation path to your environment variable PATH.
    Gitbook requires more recent node versions.

- [Gitbook](https://github.com/GitbookIO/gitbook)

    Can be installed with Node's package manager NPM:

        npm install gitbook-cli -g

- [ditaa (DIagrams Through Ascii Art)](http://ditaa.sourceforge.net/) to build the 
  ascii art diagrams (optional)

- Calibre2 (optional, only required if you want to build the e-book version)

  http://calibre-ebook.com/download

  Run the installer and follow the instructions.

# Add / Synchronize external documentation

We maintain documentation along with their respective git repositories of their component - 
be driver or other utilities which shouldn't be directly in sync to the ArangoDB core documentation.
The maintainer of the respective component can alter the documentation, and once a good point in 
time is reached, it can be sync'ed over via `Documentation/Scripts/fetchRefs.sh`, which spiders 
the `SUMMARY.md` files of all books, creates a clone of the external resource, adds a `don't edit this here` note to the files, and copies them over. 
Use your *github username* as first parameter to clone using HTTP + authentification, or `git` if you want to use ssh+key for authentification

The syntax of the `SUMMARY.md` integration are special comment lines that contain `git` in them in a semicolon separated value list:

 - The git repository - the gitrepository containing the documentation - we will clone this; If authentification is required, prepend an `@` to `github.com`
 - The directory name where to clone it under `Documentation/Books/repos` (so several integration points can share a working copy)
 - Subdirectory - the sub-directory inside of the git repository to integrate, also used in the `VERSIONS` file
 - Source - may be empty if the whole Subdirectory should be mapped into the book the `SUMMARY.md` is in, else specify source files (one per line) or directories
 - Destination - may be empty if the sub-directory on the remote repo should be mapped into the book the `SUMMARY.md` is located in; else specify a file or directory.

If a other than the default branch should be checked out you can specify the branch|tag in the VERSIONS file. The syntax is `EXTERNAL_DOC_{the-directory-name}={remote-branch-name|tag}`

If private repositories with authentification need to be cloned, the integrator can specify a username/password pair to the script. He/She can also create a clone in the `Documentation/Books/repos/$1` directory - where the script would clone it. 

The script will reset & pull the repos. 

For testing the user can checkout other remote branches in that directory.

Below the integration lines regular lines referencing the integrated .md's have to be put to add the .md's to the books summary.

Please note that the SUMMARY.md integration checks will fail if unreferenced .md's are present, or .md's are missing. 

The fetched .md's should be committed along with the changes of the `SUMMARY.md`

An example integrating an authentificated directory structure:

    #   https://@github.com/arangodb/arangosync.git;arangosync;doc-integration/Manual;;/
      * [Datacenter to datacenter replication](Scalability/DC2DC/README.md)
        * [Introduction](Scalability/DC2DC/Introduction.md)
        * [Applicability](Scalability/DC2DC/Applicability.md)
        * [Requirements](Scalability/DC2DC/Requirements.md)

Another example, integrating a single README.md from an unauthentificated repo mapping it into `Drivers/js/`:

    * [Drivers](Drivers/README.md)
    # https://github.com/arangodb/arangojs.git;arangojs;;README.md;Drivers/js/
      * [Javascript](Drivers/js/README.md)

# Generate users documentation

If you've edited examples, see below how to regenerate them with 
[`./utils/generateExamples.sh`](https://github.com/arangodb/arangodb/blob/devel/utils/generateExamples.sh).
If you've edited REST (AKA HTTP) documentation, first invoke 
[`./utils/generateSwagger.sh`](https://github.com/arangodb/arangodb/blob/devel/utils/generateSwagger.sh).
Run `cd Documentation/Books && ./build.sh` to generate it.
The documentation will be generated in subfolders in `arangodb/Documentation/Books/books` -
use your favorite browser to read it.

You may encounter permission problems with gitbook and its npm invocations.
In that case, you need to run the command as root / Administrator.

If you see "device busy" errors on Windows, retry. Make sure you don't have
intermediate files open in the ppbooks / books subfolder (i.e. browser or editor).
It can also temporarily occur during phases of high HDD / SSD load.

The build-scripts contain several sanity checks, i.e. whether all examples are
used, and no dead references are there. (see building examples in that case below)

If the markdown files aren't converted to html, or `index.html` shows a single
chapter only (content of `README.md`), make sure
[Cygwin create native symlinks](https://docs.arangodb.com/devel/Cookbook/Compiling/Windows.html)
It does not, if `SUMMARY.md` in `Books/ppbooks/` looks like this:

    !<symlink>ÿþf o o

If sub-chapters do not show in the navigation, try another browser (Firefox).
Chrome's security policies are pretty strict about localhost and file://
protocol. You may access the docs through a local web server to lift the
restrictions. You can use pythons build in http server for this.

    ~/books$ python -m SimpleHTTPServer 8000

To only regereneate one file (faster) you may specify a filter:

    make build-book NAME=Manual FILTER=Manual/Aql/Invoke.md

(regular expressions allowed)

# Using Gitbook

The `arangodb/Documentation/Books/build.sh` script generates a website
version of the manual.

If you want to generate all media ala PDF, ePUB, run `arangodb/Documentation/books/build.sh  build-dist-books` (takes some time to complete).

If you want to generate only one of them, run below 
build commands in `arangodb/Documentation/Books/books/[Manual|HTTP|AQL]/`. Calibre's
`ebook-convert` will be used for the conversion.

Generate a PDF:

    gitbook pdf ./ppbooks/Manual ./target/path/filename.pdf

Generate an ePub:

    gitbook epub ./ppbooks/Manual ./target/path/filename.epub

# Examples

## Where to add new...

- Documentation/DocuBlocks/* - markdown comments with execution section
 - Documentation/Books/Aql|Cookbook|HTTP|Manual/SUMMARY.md - index of all sub documentations

## generate

 - `./utils/generateExamples.sh --onlyThisOne geoIndexSelect` will only produce one example - *geoIndexSelect*
 - `./utils/generateExamples.sh --onlyThisOne 'MOD.*'` will only produce the examples matching that regex; Note that
   examples with enumerations in their name may base on others in their series - so you should generate the whole group.
 - running `onlyThisOne` in conjunction with a pre-started server cuts down the execution time even more.
   In addition to the `--onlyThisOne ...` specify i.e. `--server.endpoint tcp://127.0.0.1:8529` to utilize your already running arangod.
   Please note that examples may collide with existing collections like 'test' - you need to make sure your server is clean enough.
 - you can use generateExamples like that:
    `./utils/generateExamples.sh \
       --server.endpoint 'tcp://127.0.0.1:8529' \
       --withPython C:/tools/python2/python.exe \
       --onlyThisOne 'MOD.*'`
 - `./Documentation/Scripts/allExamples.sh` generates a file where you can inspect all examples for readability.
 - `./utils/generateSwagger.sh` - on top level to generate the documentation interactively with the server; you may use
    [the swagger editor](https://github.com/swagger-api/swagger-editor) to revalidate whether
    *../../js/apps/system/_admin/aardvark/APP/api-docs.json* is accurate.
 - `cd Documentation/Books; make` - to generate the HTML documentation


# write markdown

*md* files are used for the structure. To join it with parts extracted from the program documentation
you need to place hooks:
  - `@startDocuBlock <tdocuBlockName>` is replaced by a Docublock extracted from source.
  - `@startDocuBlockInline <docuBlockName>` till `@endDocuBlock <docuBlockName>`
     is replaced in with its own evaluated content - so  *@EXAMPLE_AQL | @EXAMPLE_ARANGOSH_[OUTPUT | RUN]* sections are executed
     the same way as inside of source code documentation.

# Include ditaa diagrams

We use the [beautifull ditaa (DIagrams Through Ascii Art)](http://ditaa.sourceforge.net/) to generate diagrams explaining flows etc.
in our documentation.

We have i.e. `Manual/Graphs/graph_user_in_group.ditaa` which is transpiled by ditaa into a png file, thus you simply include
a png file of the same name as image into the markdown: `![User in group example](graph_user_in_group.png)` to reference it.

# Read / use the documentation

 - `file:///Documentation/Books/books/Manual/index.html` contains the generated manual
 - JS-Console - Tools/API - [Interactive swagger documentation](https://arangodb.com/2018/03/using-arangodb-swaggerio-interactive-api-documentation/) which you can play with.

# arangod Example tool

`./utils/generateExamples.sh` picks examples from the code documentation, executes them, and creates a transcript including their results.

Here is how its details work:
  - all *Documentation/DocuBlocks/*.md* and *Documentation/Books/*.md* are searched.
  - all lines inside of source code starting with '///' are matched, all lines in .md files.
  - an example start is marked with *@EXAMPLE_ARANGOSH_OUTPUT* or *@EXAMPLE_ARANGOSH_RUN*
  - the example is named by the string provided in brackets after the above key
  - the output is written to `Documentation/Examples/<name>.generated`
  - examples end with *@END_EXAMPLE_[OUTPUT|RUN|AQL]*
  - all code in between is executed as javascript in the **arangosh** while talking to a valid **arangod**.
  You may inspect the generated js code in `/tmp/arangosh.examples.js`

## OUTPUT, RUN and AQL specifics

By default, Examples should be self contained and thus not depend on each other. They should clean up the collections they create.
Building will fail if resources aren't cleaned.
However, if you intend a set of OUTPUT and RUN to demonstrate interactively and share generated *ids*, you have to use an alphabetical
sortable naming scheme so they're executed in sequence. Using `<modulename>_<sequencenumber>[a|b|c|d]_thisDoesThat` seems to be a good scheme.

  - EXAMPLE_ARANGOSH_OUTPUT is intended to create samples that the users can cut'n'paste into their arangosh. Its used for javascript api documentation.
    * wrapped lines:
      Lines starting with a pipe (`/// |`) are joined together with the next following line.
      You have to use this if you want to execute loops, functions or commands which shouldn't be torn apart by the framework.
    * Lines starting with *var*:
      The command behaves similar to the arangosh: the server reply won't be printed.
      However, the variable will be in the scope of the other lines - else it won't.
    * Lines starting with a Tilde (`/// ~`):
      These lines can be used for setup/teardown purposes. They are completely invisible in the generated example transcript.
    * `~removeIgnoreCollection("test")` - the collection test may live across several tests.
    * `~addIgnoreCollection("test")` - the collection test will again be alarmed if left over.

  - it is executed line by line. If a line is intended to fail (aka throw an exception),
    you have to specify `// xpError(ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED)` so the exception will be caught;
    else the example is marked as broken.
    If you need to wrap that line, you may want to make the next line starting by a tilde to suppress an empty line:

        /// | someLongStatement()
        /// ~ // xpError(ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED)

 - EXAMPLE_ARANGOSH_RUN is intended to be pasted into a unix shell with *cURL* to demonstrate how the REST-HTTP-APIs work.
   The whole chunk of code is executed at once, and is expected **not to throw**.
   You should use **assert(condition)** to ensure the result is what you've expected.
   The *body* can be a string, or a javascript object which is then represented in JSON format.

    * Send the HTTP-request: `var response = logCurlRequest('POST', url, body);`
    * check its response:    `assert(response.code === 200);`
    * output a JSON server Reply: `logJsonResponse(response);` (will fail if its not a valid json)
    * output a JSONL server Reply: `logJsonLResponse(response);` (will fail if its not a valid json;
      use if the server responds with one json document per line; Add a note to the user that this is `*(One JSON document per line)*` above the example)
    * output HTML to the user: `logHtmlResponse(response);` (i.e. redirects have HTML documents)
    * output the plain text to dump to the user: `logRawResponse(response);` (**don't use this if you expect a json reply**)
    * dump the reply to the errorlog for testing (will mark run as failed): `logErrorResponse(response);`

 - EXAMPLE_AQL is intended to contain AQL queries that can be pasted into arangosh or the webinterfaces query editor.
   Usually this query references an example dataset generator in `js/common/modules/@arangodb/examples/examples.js`
   which the users can also invoke to generate the data in their installation. 
   This sort of example consists of three parts: 
    - @DATASET{datasetName} - (optional) the name of the dataset in the above mentioned `examples.js` to be instanciated before executing this query. 
    - @EXPLAIN{TRUE|FALSE} - (optional) print execution plan of the AQL query. The default is `FALSE`.
    - A following AQL query which may either end at the end of the comment block, or at the next section:
    - @BV - (optional) verbatim object containing the bind parameters to be passed into the query. Will also be put into the generated snippet.

# Swagger integration

`./utils/generateSwagger.sh` scans the documentation, and generates swagger output.
It scans for all documentationblocks containing `@RESTHEADER`.
It is a prerequisite for integrating these blocks into the gitbook documentation.

Tokens may have curly brackets with comma separated fields. They may optionally be followed by subsequent text
lines with a long descriptions.

Sections group a set of tokens; they don't have parameters.

**Types**
Swagger has several native types referenced below:
*[integer|long|float|double|string|byte|binary|boolean|date|dateTime|password]* -
[see the swagger documentation](https://github.com/swagger-api/swagger-spec/blob/master/versions/2.0.md#data-types)

It consists of several sections which in term have sub-parameters:

**Supported Sections:**

## RESTQUERYPARAMETERS

Parameters to be appended to the URL in form of ?foo=bar
add *RESTQUERYPARAM*s below

## RESTURLPARAMETERS

Section of parameters placed in the URL in curly brackets, add *RESTURLPARAM*s below.

## RESTDESCRIPTION

Long text describing this route.

## RESTRETURNCODES

should consist of several *RESTRETURNCODE* tokens.

**Supported Tokens:**

## RESTREPLYBODY

Similar  to RESTBODYPARAM - just what the server will reply with.

## RESTHEADER

Up to 3 parameters.
* *[GET|POST|PUT|DELETE] <url>* url should start with a */*, it may contain parameters in curly brackets, that
  have to be documented in subsequent *RESTURLPARAM* tokens in the *RESTURLPARAMETERS* section.
* long description
* operationId - this is a uniq string that identifies the source parameter for this rest route. It defaults to a de-spaced `long description` - if set once it shouldn't be changed anymore.

## RESTURLPARAM

Consists of 3 values separated by ',':
Attributes:
  - *name*: name of the parameter
  - *type*:
  - *[required|optionas]* Optional is not supported anymore. Split the docublock into one with and one without.

Folowed by a long description.

## RESTALLBODYPARAM

This API has a schemaless json body (in doubt just plain ascii)

## RESTBODYPARAM

Attributes:
  - name - the name of the parameter
  - type - the swaggertype of the parameter
  - required/optional - whether the user can omit this parameter
  - subtype / format (can be empty)
    - subtype: if type is object or array, this references the enclosed variables.
               can be either a swaggertype, or a *RESTRUCT*
    - format: if type is a native swagger type, some support a format to specify them

## RESTSTRUCT

Groups a set of attributes under the `structure name` to an object that can be referenced
in other *RESTSTRUCT*s or *RESTBODYPARAM* attributes of type array or object

Attributes:
  - name - the name of the parameter
  - structure name - the **type** under which this structure can be reached (should be uniq!)
  - type - the swaggertype of the parameter (or another *RESTSTRUCT*...)
  - required/optional - whether the user can omit this parameter
  - subtype / format (can be empty)
    - subtype: if type is object or array, this references the enclosed variables.
               can be either a swaggertype, or a *RESTRUCT*
    - format: if type is a native swagger type, some support a format to specify them
