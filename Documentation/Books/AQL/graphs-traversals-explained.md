---
layout: default
description: A traversal starts at one specific document (startVertex) and follows alledges connected to this document
---
Traversals explained
====================

General query idea
------------------

A traversal starts at one specific document (*startVertex*) and follows all
edges connected to this document. For all documents (*vertices*) that are
targeted by these edges it will again follow all edges connected to them and
so on. It is possible to define how many of these follow iterations should be
executed at least (*min* depth) and at most (*max* depth).

For all vertices that were visited during this process in the range between
*min* depth and *max* depth iterations you will get a result in form of a
set with three items:

1. The visited vertex.
2. The edge pointing to it.
3. The complete path from startVertex to the visited vertex as object with an
  attribute *edges* and an attribute *vertices*, each a list of the coresponding
  elements. These lists are sorted, which means the first element in *vertices*
  is the *startVertex* and the last is the visited vertex, and the n-th element
  in *edges* connects the n-th element with the (n+1)-th element in *vertices*.

Example execution
-----------------

Let's take a look at a simple example to explain how it works.
This is the graph that we are going to traverse:

![traversal graph](../images/traversal_graph.png)

We use the following parameters for our query:

1. We start at the vertex **A**.
2. We use a *min* depth of 1.
3. We use a *max* depth of 2.
4. We follow only in `OUTBOUND` direction of edges

![traversal graph step 1](../images/traversal_graph1.png)

Now it walks to one of the direct neighbors of **A**, say **B** (note: ordering
is not guaranteed!):

![traversal graph step 2](../images/traversal_graph2.png)

The query will remember the state (red circle) and will emit the first result
**A** → **B** (black box). This will also prevent the traverser to be trapped
in cycles. Now again it will visit one of the direct neighbors of **B**, say **E**:

![traversal graph step 3](../images/traversal_graph3.png)

We have limited the query with a *max* depth of *2*, so it will not pick any
neighbor of **E**, as the path from **A** to **E** already requires *2* steps.
Instead, we will go back one level to **B** and continue with any other direct
neighbor there:

![traversal graph step 4](../images/traversal_graph4.png)

Again after we produced this result we will step back to **B**.
But there is no neighbor of **B** left that we have not yet visited.
Hence we go another step back to **A** and continue with any other neighbor there.

![traversal graph step 5](../images/traversal_graph5.png)

And identical to the iterations before we will visit **H**:

![traversal graph step 6](../images/traversal_graph6.png)

And **J**:

![traversal graph step 7](../images/traversal_graph7.png)

After these steps there is no further result left. So all together this query
has returned the following paths:

1. **A** → **B**
2. **A** → **B** → **E**
3. **A** → **B** → **C**
4. **A** → **G**
5. **A** → **G** → **H**
6. **A** → **G** → **J**
