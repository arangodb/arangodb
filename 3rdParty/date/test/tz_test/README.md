To test: * Install tz.cpp by downloading the IANA timezone database
at: http://www.iana.org/time-zones You only need the data, not the
code.

* Change the string `install` in tz.cpp to point to your downloaded
IANA database.

* Compile validate.cpp along with tz.cpp.

* Run the binary and direct the terminal output to a temporary file.

* Unzip the tzdata file that has the version corresponding to the IANA
database you downloaded (e.g. tzdata2015f.txt.zip).

* Compare the unzipped txt file with the output of your validate test
program. If they are identical, the test passes, else it fails.

Miscellaneous:

You can also compare one version of the tzdatabase with another using
these uncompressed text files.  The text files contain for each
timezone its initial state, and each {offset, abbreviation} change
contained in the database up through the year 2035.  As the database
versions change, minor updates to the set of these transitions are
typically made, typically due to changes in government policies.

The tests in this section will run much faster with optimizations
cranked up.