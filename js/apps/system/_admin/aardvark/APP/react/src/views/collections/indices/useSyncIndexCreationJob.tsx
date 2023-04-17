import { useSWRConfig } from "swr";
import { useJobSync } from "../../../components/hooks/useJobSync";
import { encodeHelper } from "../../../utils/encodeHelper";
import { useCollectionIndicesContext } from "./CollectionIndicesContext";

export const useSyncIndexCreationJob = () => {
  const { collectionId, collectionName } = useCollectionIndicesContext();
  const { mutate } = useSWRConfig();
  useJobSync({
    onQueue: () => {
      window.arangoHelper.arangoMessage(
        "Index",
        "There is at least one new index in the queue or in the process of being created."
      );
    },
    onSuccess: () => {
      const { encoded: encodedCollectionName } = encodeHelper(collectionName);
      mutate(`/index/?collection=${encodedCollectionName}`);
    },
    onError: (error: any) => {
      const errorMessage = error?.response?.body?.errorMessage;
      window.arangoHelper.arangoError("Index creation failed", errorMessage);
    },
    jobCollectionName: collectionId
  });
};
