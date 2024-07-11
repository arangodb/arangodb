import { Index } from "arangojs/indexes";
import { ArangojsResponse } from "arangojs/lib/request";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";

export type IndexWithFigures = Index & {
  figures: {
    [key: string]: any;
  };
};
interface IndicesResponse extends ArangojsResponse {
  body: {
    indexes: IndexWithFigures[];
  };
}
export const useFetchCollectionIndices = (collectionName: string) => {
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const { data } = useSWR(
    `/collection/${encodedCollectionName}/indices`,
    async () => {
      const data = (await getApiRouteForCurrentDB().get(
        `/index/`,
        `collection=${encodedCollectionName}&withStats=true&withHidden=true`
      )) as IndicesResponse;
      return data.body.indexes;
    }
  );
  return { collectionIndices: data };
};
