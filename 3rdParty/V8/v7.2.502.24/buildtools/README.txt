This repository contains hashes of build tools used by Chromium and related
projects. The actual binaries are pulled from Google Storage, normally as part
of a gclient hook.

The repository is separate so that the shared build tools can be shared between
the various Chromium-related projects without each one needing to maintain
their own versionining of each binary.

________________________________________
UPDATING AND ROLLING BUILDTOOLS MANUALLY

When you update buildtools, you should roll the new version into the Chromium
repository right away. Otherwise, the next person who makes a change will end
up rolling (and testing) your change. If there are any unresolved problems with
your change, the next person will be blocked.

  - From the buildtools directory, make a branch, edit and upload normally.

  - Get your change reviewed and landed. There are no trybots so landing will
    be very fast.

  - Get the hash for the commit that commit-bot made. Make a new branch in
    the Chromium repository and paste the hash into the line in //DEPS
    labeled "buildtools_revision".

  - You can TBR changes to the DEPS file since the git hashes can't be reviewed
    in any practical way. Submit that patch to the commit queue.

  - If this roll identifies a problem with your patch, fix it promptly. If you
    are unable to fix it promptly, it's best to revert your buildtools patch
    to avoid blocking other people that want to make changes.

________________________
ADDING BINARIES MANUALLY

One uploads new versions of the tools using the 'gsutil' binary from the
Google Storage SDK:

  https://developers.google.com/storage/docs/gsutil

There is a checked-in version of gsutil as part of depot_tools.

To initialize gsutil's credentials:

  python ~/depot_tools/third_party/gsutil/gsutil config

  That will give a URL which you should log into with your web browser.

  Copy the code back to the command line util. Ignore the project ID (it's OK
  to just leave blank when prompted).
