Pre-processing of MPL-containers
--------------------------------

Pre-processing of MPL-containers can be accomplished using the script
"boost_mpl_preprocess.py". In the simple case call it with a single
argument which is the path to the source-directory of Boost.

  python boost_mpl_preprocess.py <path-to-boost-sourcedir>

If the Boost source-directory is the one this script resides in, you
can just call it without any arguments.

  python boost_mpl_preprocess.py

Either way, this will pre-process all four MPL-container types (vector,
list, set, map) and makes them able to hold up to 100 elements. They can
be used either in their 'numbered' or their 'variadic' form.

Additionally, the script also allows more fine-grained pre-processing.
The maximal number of elements an MPL-container type is able to hold can
be different from the one of other MPL-container types and it can also
differ between its 'numbered' and 'variadic' form.
To see all options, call the script like this:

  python boost_mpl_preprocess.py --help


Fixing pre-processing of MPL-containers
---------------------------------------

Sadly, pre-processing of MPL-containers might fail, if the source-files
used as input are missing some header-comments required during the pre-
processing step.
However, the script "boost_mpl_preprocess.py" makes sure to patch these
input source-files prior to pre-processing (by implicitly calling script
"fix_boost_mpl_preprocess.py" with the chosen settings). It only patches
the source-files needed for pre-processing the selected MPL-container
types and their selected form ('numbered' or 'variadic').
If calling it with a single (or no) argument (as in the former section)
all input source-files will be patched automatically.

Instead of fixing the input-files implicitly during pre-processing one
can also fix them explicitly by calling "fix_boost_mpl_preprocess.py"
directly.
If you just want to test if any fixing is needed call it like this:

  python fix_boost_mpl_preprocess.py --check-only <path-to-boost-sourcedir>

This will tell you if any fixing is needed. In such a case call the script
"fix_boost_mpl_preprocess.py" like this:

  python fix_boost_mpl_preprocess.py <path-to-boost-sourcedir>

This will fix the header-comments of all the source-files needed during
pre-processing. Calling "boost_mpl_preprocess.py" afterwards should then
successfully pre-process the MPL-containers (without the need of implicitly
fixing any files again).

Note:
Failure of pre-processing can be checked by examining at least one of the
following directories in which automatically generated files will be put
during pre-processing. If at least one file in these directories (or sub-
directories therein) has a size of zero bytes, fixing is needed.

 <path-to-boost-sourcedir>/boost/mpl/vector/aux_/preprocessed/
 <path-to-boost-sourcedir>/boost/mpl/list/aux_/preprocessed/
 <path-to-boost-sourcedir>/boost/mpl/set/aux_/preprocessed/
 <path-to-boost-sourcedir>/boost/mpl/map/aux_/preprocessed/
 <path-to-boost-sourcedir>/boost/mpl/aux_/preprocessed/

