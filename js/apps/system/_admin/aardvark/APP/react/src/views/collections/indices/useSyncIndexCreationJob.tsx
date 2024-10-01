import { useSWRConfig } from "swr";
import { useJobSync } from "../../../components/hooks/useJobSync";
import { encodeHelper } from "../../../utils/encodeHelper";
import { useCollectionIndicesContext } from "./CollectionIndicesContext";

export const useSyncIndexCreationJob = () => {
  const { collectionId, collectionName } = useCollectionIndicesContext();
  const { encoded: encodedCollectionName } = encodeHelper(collectionName);
  const { mutate } = useSWRConfig();
  useJobSync({
    onQueue: () => {
      window.arangoHelper.arangoMessage(
        "Index",
        "There is at least one new index in the queue or in the process of being created."
      );
    },
    onSuccess: () => {
      mutate(`/collection/${encodedCollectionName}/indices`);
      mutate(`/collection/${encodedCollectionName}/figures`);
    },
    onError: (error: any) => {
      const errorMessage = error?.response?.parsedBody?.errorMessage;
      window.arangoHelper.arangoError("Index creation failed", errorMessage);
    },
    jobCollectionName: collectionId
  });
};
