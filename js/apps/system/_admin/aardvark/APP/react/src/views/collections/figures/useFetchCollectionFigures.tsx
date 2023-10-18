import useSWR from "swr";
import { getCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";

export const useFetchCollectionFigures = (collectionName: string) => {
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const { data } = useSWR(`/collection/${encodedCollectionName}/figures`, async () => {
    const db = getCurrentDB();
    const data = await db.collection(collectionName).figures();
    return data;
  });
  return { collectionFigures: data };
};
