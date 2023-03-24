"""
This example shows how to add spatial data to an information retrieval
system.  Such systems work by converting documents into a collection of
"index terms" (e.g., representing words or phrases), and then building an
"inverted index" that maps each term to a list of documents (and document
positions) where that term occurs.

This example shows how to convert spatial data into index terms, which can
then be indexed along with the other document information.

This is a port of the C++ term_index.cc example for the Python API.
"""
import argparse
from collections import defaultdict

import pywraps2 as s2


def main():
    parser = argparse.ArgumentParser(
        description=(
            "This example shows how to convert spatial data into index terms, "
            "which can then be indexed along with the other document "
            "information."
        )
    )
    parser.add_argument(
        '--num_documents', type=int, default=10000, help="Number of documents"
    )
    parser.add_argument(
        '--num_queries', type=int, default=10000, help="Number of queries"
    )
    parser.add_argument(
        '--query_radius_km', type=float, default=100,
        help="Query radius in kilometers"
    )

    args = parser.parse_args()

    # A prefix added to spatial terms to distinguish them from other index terms
    # (e.g. representing words or phrases).
    PREFIX = "s2:"

    # Create a set of "documents" to be indexed.  Each document consists of a
    # single point.  (You can easily substitute any S2Region type here, or even
    # index a mixture of region types using S2Region.  Other
    # region types include polygons, polylines, rectangles, discs, buffered
    # geometry, etc.)
    documents = []
    for i in range(args.num_documents):
        documents.append(s2.S2Testing.RandomPoint())

    # We use a dict as our inverted index.  The key is an index term, and
    # the value is the set of "document ids" where this index term is present.
    index = defaultdict(set)

    # Create an indexer suitable for an index that contains points only.
    # (You may also want to adjust min_level() or max_level() if you plan
    # on querying very large or very small regions.)
    indexer = s2.S2RegionTermIndexer()
    indexer.set_index_contains_points_only(True)

    # Add the documents to the index.
    for docid, index_region in enumerate(documents):
        for term in indexer.GetIndexTerms(index_region, PREFIX):
            index[term].add(docid)

    # Convert the query radius to an angle representation.
    radius = s2.S1Angle.Radians(s2.S2Earth.KmToRadians(args.query_radius_km))

    # Count the number of documents (points) found in all queries.
    num_found = 0
    for i in range(args.num_queries):
        # Choose a random center for query.
        query_region = s2.S2Cap(s2.S2Testing.RandomPoint(), radius)

        # Convert the query region to a set of terms, and compute the union of
        # the document ids associated with those terms.  (An actual information
        # retrieval system would do something more sophisticated.)
        candidates = set()
        for term in indexer.GetQueryTerms(query_region, PREFIX):
            candidates |= index[term]

        # "candidates" now contains all documents that intersect the query
        # region, along with some documents that nearly intersect it.  We can
        # prune the results by retrieving the original "document" and checking
        # the distance more precisely.
        result = []
        for docid in candidates:
            if query_region.Contains(documents[docid]):
                result.append(docid)

        # Now do something with the results (in this example we just count
        # them).
        num_found += len(result)

    print("Found %d points in %d queries" % (num_found, args.num_queries))


if __name__ == "__main__":
    main()
