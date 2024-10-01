import useSWR from "swr";
import { getCurrentDB } from "../../../../utils/arangoClient";

export const useCollectionsList = () => {
  const { data: collectionsList, ...rest } = useSWR(
    ["/collection", "excludeSystem=true"],
    () => getCurrentDB().listCollections(true)
  );
  return {
    collectionsList,
    ...rest
  };
};
