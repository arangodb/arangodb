import { CollectionMetadata } from "arangojs/collection";
import useSWR from "swr";
import { getCurrentDB } from "../../utils/arangoClient";

// Note: Index creation progress is now tracked via native progress field
// from /_api/index (see useFetchCollectionIndices.ts), not via Aardvark jobs.

export const useFetchCollections = () => {
  const { data: collections, ...rest } = useSWR<CollectionMetadata[]>("/collections", () => {
    return getCurrentDB().listCollections(false);
  });

  return { collections, ...rest };
};
