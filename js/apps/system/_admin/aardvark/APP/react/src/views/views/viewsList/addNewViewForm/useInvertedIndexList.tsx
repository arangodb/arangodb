import { ArangojsResponse } from "arangojs/lib/request";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import { encodeHelper } from "../../../../utils/encodeHelper";

export type IndexType = {
  id: string;
  name: string;
  type: "inverted";
};
interface CollectionsListResponse extends ArangojsResponse {
  body: { indexes: Array<IndexType> };
}

export const useInvertedIndexList = (collection: string) => {
  const { encoded: encodedCollectionName } = encodeHelper(collection);
  const { data, ...rest } = useSWR<CollectionsListResponse>(
    ["/index", `collection=${encodedCollectionName}`],
    (args: string[]) => {
      const [path, qs] = args;
      if (collection) {
        return getApiRouteForCurrentDB().get(path, qs) as any;
      }
    }
  );
  const invertedIndexes = data?.body.indexes.filter(indexValue => {
    return indexValue.type === "inverted";
  });
  return { indexesList: invertedIndexes, ...rest };
};
