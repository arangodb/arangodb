import { InvertedIndex } from "arangojs/indexes";
import useSWR from "swr";
import { getCurrentDB } from "../../../../utils/arangoClient";
import { encodeHelper } from "../../../../utils/encodeHelper";

export const useInvertedIndexList = (collection: string) => {
  const { encoded: encodedCollectionName } = encodeHelper(collection);
  const { data: indexes, ...rest } = useSWR(
    ["/index", `collection=${encodedCollectionName}`],
    () => {
      if (collection) {
        return getCurrentDB().collection(encodedCollectionName).indexes();
      }
    }
  );
  const invertedIndexes = indexes?.filter(indexValue => {
    return indexValue.type === "inverted";
  }) as InvertedIndex[] | undefined;
  return { indexesList: invertedIndexes, ...rest };
};
