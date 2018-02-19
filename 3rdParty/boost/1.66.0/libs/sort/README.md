sort
====

Boost Sort library that includes spreadsort, a hybrid radix sort that is faster than O(n*log(n))

Replace "/" with "\" in paths below if you're using Windows.
To install, download boost, run bootstrap, and copy this library into <your boost root>/libs/sort.  

Run the unit tests from your boost root:
./b2 libs/sort/test

Then go to <your boost root>/libs/sort and run tune.pl:
Then from the same directory verify correctness and speed on small data sets:
perl tune.pl -small [-windows]
(it needs the windows option to build for windows)
This tests sorting on many different distributions and data types, making sure the results are identical to std::sort and showing a speed comparison that is a weighted average across multiple data distributions.
If you're interested in more accurate speed comparisons, run the same command either without the -small option, or with the -large option instead.  This will take substantially longer.

Documentation is available from the index.html in this same directory, including a description of the algorithm, how to use it, and why it's faster.

Finally, if you have an unusual computing system, you may want to use the -tune option to tune.pl, to tune the constants used by the library for your specific system.  BEWARE that this will overwrite the default boost/sort/spreadsort/detail/constants.hpp provided by the library.  Making a copy first is a good idea.  Also note that it doesn't tune MAX_SPLITS, the most important parameter, because that should only be tuned with the -large option and it can overfit to the specific amount of data passed in.

Feel free to contact spreadsort@gmail.com with any questions about this library.

Copyright 2014-2015 Steven Ross
Distributed under the Boost Software License, Version 1.0. (See http://www.boost.org/LICENSE_1_0.txt)

