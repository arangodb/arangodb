+++
title = "Acknowledgements"
description = "Giving thanks to those who made Outcome happen"
+++

## github contributors

{{%ghcontributors "https://api.github.com/repos/ned14/outcome/contributors?per_page=100" %}}

## This pretty, modern C++ documentation

* [Hugo](https://gohugo.io) - static website generator of this documentation.
* [hugo-theme-docdock](https://github.com/vjeantet/hugo-theme-docdock) - the Hugo theme used by this documentation.
* [Standardese](https://github.com/foonathan/standardese) - the API reference generator (up until Outcome v2.0)

# Special thanks for Outcome v2.1

Once again Andrzej Krzemienski stands out for a never ceasing flow of excellent questions,
"what if?"'s, eagle eyed spotting of corner case logic bugs, and design contradictions.
Thank you Andrzej!

My thanks to the Microsoft Visual C++ compiler team for incorporating Outcome into the
MSVC test suite, and thus finding many interesting corner case quirks in how best to
interpret the C++ standard. In some cases, Outcome was refactored to be less ambiguous;
in others, defects had to be submitted to WG21 because the standard wording was not clear.
The Visual C++ compiler team were particularly generous with their time in helping track
down the cause of these issues, complete with submitting pull requests with bug fixes.
I am very grateful to them.

# Special thanks for Outcome v2.0

For a second time, Charley Bay stepped up as review manager. Given how much work it was
for the v1 review, I can only say **thank you**.

Andrzej Krzemienski went far beyond the call of duty in the development of Outcome v2.
He wrote the front page, and the start of the tutorial. He thus set the tone, pacing,
style and form of the tutorial which I merely continued for the rest of the tutorial.
He also volunteered considerable amounts of his time as as primary peer reviewer for
the v2 design and implementation, asking many very valuable "stupid questions" at least
one of which caused a major rethink and refactor. If Outcome v2 passes its second peer
review, it's because of Andrzej. Thank you.

Jonathan Müller invested countless hours in his doxygen replacement tool Standardese
which Outcome uses to generate the reference API docs, and a not insignificant number
of those went on fixing issues for Outcome. Thank you.

# Special thanks for Outcome v1

To Paul Bristow who <a href="https://lists.boost.org/Archives/boost/2015/05/222687.php">
proposed the name "Outcome"</a> for the library after a very extended
period of name bikeshedding on boost-dev. I had been minded to call the library "Boost.Donkey"
just to shut everyone up because the name bike shedding was getting ridiculous. But
Outcome is a lot nicer, so thank you Paul.

My heartfelt thanks to Charley Bay for acting as review manager for Outcome in May 2017.
It is becoming ever harder to find a Boost review manager, so thank you! My thanks also
to the CppCast team Rob Irving and Jason Turner for so quickly getting me on to CppCast
to discuss `expected<T, E>` during the Outcome peer review to help publicise the review.

More general thanks are due to those on boost-dev, Reddit and SG14 for extensive and often very detailed
feedback on the library pretty much from its beginning. You are all too numerous to
remember, but certainly Tongari and Andrzej Krzemienski come to mind as having engaged
in particularly long discussion threads with tons of useful feedback which clarified my
thinking. Andrzej also went through the documentation with a fine toothed comb before the
review, finding many small errata and confusing wording.

Finally, my thanks to Vicente for driving Expected from its beginnings to hopefully
standardisation before 2020. It's many years of work getting something standardised, even
more years than getting a library into Boost which as you can see from the history above
took about three years.
