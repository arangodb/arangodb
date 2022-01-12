+++
title = "Mapping the HTMLTidy library into the Application"
weight = 45
+++

Once again, we create a custom STL exception type to represent failure
from the HTMLTidy library. We also create an `app` namespace wrapper
for the C `tidy_html()` function which is more C++ friendly.

{{% snippet "finale.cpp" "app_map_tidylib" %}}
