This repository contains hashes of build tools used by Chromium and related
projects. The actual binaries are pulled from Google Storage, normally as part
of a gclient hook.

The repository is separate so that the shared build tools can be shared between
the various Chromium-related projects without each one needing to maintain
their own versionining of each binary.

One uploads new versions of the tools using the 'gsutil' binary from the
Google Storage SDK:

  https://developers.google.com/storage/docs/gsutil

There is a checked-in version of gsutil as part of depot_tools.

To initialize gsutil's credentials:

  python ~/depot_tools/third_party/gsutil/gsutil config

  That will give a URL which you should log into with your web browser. The
  username should be the one that is on the ACL for the "chromium-gn" bucket
  (probably your @google.com address). Contact the build team for help getting
  access if necessary.

  Copy the code back to the command line util. Ignore the project ID (it's OK
  to just leave blank when prompted).

Instructions for how to upload the .sha1 digests of the GN binaries
can be found at https://code.google.com/p/chromium/wiki/UpdateGNBinaries .
