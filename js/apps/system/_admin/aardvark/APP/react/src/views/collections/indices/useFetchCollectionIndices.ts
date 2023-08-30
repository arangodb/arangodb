import useSWR from "swr";
import { getCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";

export const useFetchCollectionIndices = (collectionName: string) => {
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const { data } = useSWR(`/collection/${encodedCollectionName}/indices`, async () => {
    const db = getCurrentDB();
    const data = await db.collection(collectionName).indexes();
    return data;
  });
  return { collectionIndices: data };
};
