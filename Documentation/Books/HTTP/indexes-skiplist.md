---
layout: default
description: Working with Skiplist Indexes
---
Working with Skiplist Indexes
=============================

If a suitable skip-list index exists, then `/_api/simple/range` and other operations
will use this index to execute queries.

<!-- js/actions/api-index.js -->
{% docublock post_api_index_skiplist %}
