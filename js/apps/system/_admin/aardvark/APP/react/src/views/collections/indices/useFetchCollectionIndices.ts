import { HiddenIndex } from "arangojs/indexes";
import { useState } from "react";
import useSWR from "swr";
import { getCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";

export const useFetchCollectionIndices = (collectionName: string) => {
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const db = getCurrentDB();
  const [hasInProgressIndex, setHasInProgressIndex] = useState(false);

  const { data } = useSWR(
    `/collection/${encodedCollectionName}/indices`,
    async () => {
      const indices = await db
        .collection(encodedCollectionName)
        .indexes<HiddenIndex>({ withStats: true, withHidden: true });
      // Check if any index is in progress and update polling state
      const inProgress = indices.some(
        (idx: any) => typeof idx.progress === "number" && idx.progress < 100
      );
      setHasInProgressIndex(inProgress);
      return indices;
    },
    {
      // Poll every 2 seconds while there's an in-progress index
      refreshInterval: hasInProgressIndex ? 2000 : 0
    }
  );
  return { collectionIndices: data };
};
