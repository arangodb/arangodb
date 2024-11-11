import { HiddenIndex } from "arangojs/indexes";
import useSWR from "swr";
import { getCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";

export const useFetchCollectionIndices = (collectionName: string) => {
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const db = getCurrentDB();
  const { data } = useSWR(
    `/collection/${encodedCollectionName}/indices`,
    async () => {
      return db
        .collection(encodedCollectionName)
        .indexes<HiddenIndex>({ withStats: true, withHidden: true });
    }
  );
  return { collectionIndices: data };
};
