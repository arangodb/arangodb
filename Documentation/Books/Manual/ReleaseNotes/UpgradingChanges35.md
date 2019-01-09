Incompatible changes in ArangoDB 3.5
====================================

It is recommended to check the following list of incompatible changes **before**
upgrading to ArangoDB 3.5, and adjust any client programs if necessary.

The following incompatible changes have been made in ArangoDB 3.5:

AQL
---

3.5 enforces the invalidation of variables in AQL queries after usage of a AQL 
COLLECT statements as documented. The documentation for variable invalidation claims
that

    The COLLECT statement will eliminate all local variables in the current scope. 
    After COLLECT only the variables introduced by COLLECT itself are available.

However, the described behavior was not enforced when a COLLECT was preceded by a
FOR loop that was itself preceded by a COLLECT. In the following query the final
RETURN statement accesses variable `key1` though the variable should have been 
invalidated by the COLLECT directly before it:

    FOR x1 IN 1..2 
      COLLECT key1 = x1 
      FOR x2 IN 1..2 
        COLLECT key2 = x2 
        RETURN [key2, key1] 
  
In previous releases, this query was
parsed ok, but the contents of variable `key1` in the final RETURN statement were
undefined. 
  
This change is about making queries as the above fail with a parse error, as an 
unknown variable `key1` is accessed here, avoiding the undefined behavior. This is 
also in line with what the documentation states about variable invalidation.

