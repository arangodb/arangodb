import { ArangojsResponse } from "arangojs/lib/request";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";

export type CollectionType = {
  globallyUniqueId: string;
  id: string;
  name: string;
  type: "search-alias" | "arangosearch";
};
interface CollectionsListResponse extends ArangojsResponse {
  body: { result: Array<CollectionType> };
}

export const useCollectionsList = () => {
  const { data, ...rest } = useSWR<CollectionsListResponse>(
    ["/collection", "excludeSystem=true"],
    (args: string[]) => {
      const [path, qs] = args;
      return getApiRouteForCurrentDB().get(path, qs) as any;
    }
  );
  return { collectionsList: data && data.body && data.body.result, ...rest };
};
