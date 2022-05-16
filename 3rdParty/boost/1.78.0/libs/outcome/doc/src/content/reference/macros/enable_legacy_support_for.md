+++
title = "`BOOST_OUTCOME_ENABLE_LEGACY_SUPPORT_FOR`"
description = "Enables backwards features and naming compatibility for earlier versions of Outcome."
+++

As Outcome has evolved, some features and especially naming were retired in newer versions. Define this macro to enable backwards compatibility aliasing from old features and naming to new features and naming.

*Overridable*: Define before inclusion.

*Default*: The current version of Outcome, expressed in hundreds e.g. Outcome v2.10 is `210`.

*Header*: `<boost/outcome/config.hpp>`