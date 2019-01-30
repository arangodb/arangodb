#!/bin/sh
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

treeroot="$(dirname "$0")/.."
cd "${treeroot}/source/data"

# Excludes region data. On Android Java API is used to get the data.
# Due to a bug in ICU, an empty region list always uses 70kB pool.res bundle.
# As a work around, include the minimal version of en.txt
echo Overwriting region/reslocal.mk...
cat >region/reslocal.mk <<END
REGION_CLDR_VERSION = %version%
REGION_SYNTHETIC_ALIAS =
REGION_ALIAS_SOURCE =
REGION_SOURCE = en.txt
END

echo Overwriting region/en.txt...
cat >region/en.txt <<END
en{
    Countries{
        US{"United States"}
    }
}
END

# On Android Java API is used to get lang data, except for the language and
# script names for zh_Hans and zh_Hant which are not supported by Java API.
# Here remove all lang data except those names.
# See the comments in GetDisplayNameForLocale() (in Chromium's
# src/ui/base/l10n/l10n_util.cc) about why we need the scripts.
for i in lang/*.txt; do
  echo Overwriting $i...
  sed '/^    Keys{$/,/^    }$/d
       /^    Languages{$/,/^    }$/{
         /^    Languages{$/p
         /^        zh{/p
         /^    }$/p
         d
       }
       /^    LanguagesShort{$/,/^    }$/d
       /^    Scripts{$/,/^    }$/{
         /^    Scripts{$/p
         /^        Hans{/p
         /^        Hant{/p
         /^    }$/p
         d
       }
       /^    Types{$/,/^    }$/d
       /^    Variants{$/,/^    }$/d
       /^    calendar{$/,/^    }$/d
       /^    codePatterns{$/,/^    }$/d
       /^    localeDisplayPattern{$/,/^    }$/d' -i $i
done

echo DONE.
