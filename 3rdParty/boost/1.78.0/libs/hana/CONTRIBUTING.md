# How to contribute

Contributions are always very much appreciated. However, to make sure the
process of accepting patches goes smoothly for everyone (especially for
the maintainer), you should try to follow these few simple guidelines when
you contribute:

1. Fork the repository.
2. Create a new branch based on the `develop` branch (`git checkout -b your_branch develop`).
   If your contribution is a bug fix, you should name your branch `bugfix/xxx`;
   for a feature, it should be `feature/xxx`. Otherwise, just use your good
   judgment. Consistent naming of branches is appreciated since it makes the
   output of `git branch` easier to understand with a single glance.
3. Do your modifications on that branch. Except for special cases, your
   contribution should include proper unit tests and documentation. Also,
   please try to follow the style guide below.
4. Make sure your modifications did not break anything by building and
   running the tests:

   ```shell
   mkdir build
   cd build
   cmake ..
   cmake --build . --target check
   ```
5. Commit your changes. Your commit message should start with a one line
   short description of the modifications, with the details and explanations
   of your modifications following in subsequent paragraphs or bullet points.
   Please limit your lines to about 78 characters in commit messages, since
   it makes the output easier to read in `git log`. Also, starting your commit
   message with a tag describing the nature of the commit is nice, since it
   makes the commit history easier to skim through. For commits that do not
   change any functionality (e.g. refactoring or fixing typos), the `[NFC]`
   tag (No Functionality Change) can be used. Here's an example of an
   acceptable commit message:
   ```
   [Searchable] Refactor the interface

   - Rename elem to contains
   - Rename subset to is_subset, and make is_subset applicable in infix notation
   - Add the at_key method
   - operator[] is now bound to at_key instead of find
   ```
   When applicable, please squash adjacent _wip_ commits into a single
   _logical_ commit. If your contribution has several logical commits,
   it's fine.
6. Push the changes to your fork (`git push origin your_branch`).
7. Open a pull request against Hana's `develop` branch (not against `master`).
   I will do my best to respond in a timely manner. I might discuss your patch
   and suggest some modifications, or I might amend your patch myself and ask
   you for feedback. You will always be given proper credit.


## Style guide

I'm not going to write an exhaustive style guide, but here are a couple of
points you should watch out for:
- Indent using 4 spaces.
- Do not leave trailing white spaces at the end of lines, and no more than a
  single newline at the end of a source file.
- Hana's `#include`s go first, then a blank line and system headers.
  `#include`s within each block should be sorted in alphabetical order.
- Use your own judgment and stick to the style of the surrounding code.
