import { CollectionType } from "arangojs";
import useSWR from "swr";
import { getCurrentDB } from "../../../utils/arangoClient";

const useFetchCollections = () => {
  const currentDB = getCurrentDB();
  const fetchCollections = async () => {
    const collections = await currentDB.listCollections();
    const sortedCollection = collections.sort((a, b) => {
      const nameA = a.name.toUpperCase();
      const nameB = b.name.toUpperCase();
      if (nameA < nameB) {
        return -1;
      }
      if (nameA > nameB) {
        return 1;
      }
      return 0;
    });
    return sortedCollection;
  };

  const { data } = useSWR("/collections", fetchCollections);
  return { collections: data };
};
export const useCollectionOptions = () => {
  const { collections } = useFetchCollections();
  const edgeCollectionOptions = collections
    ?.filter(collection => {
      return collection.type === CollectionType.EDGE_COLLECTION;
    })
    .map(collection => {
      return {
        value: collection.name,
        label: collection.name
      };
    });
  const documentCollectionOptions = collections
    ?.filter(collection => {
      return collection.type === CollectionType.DOCUMENT_COLLECTION;
    })
    .map(collection => {
      return {
        value: collection.name,
        label: collection.name
      };
    });
  return { edgeCollectionOptions, documentCollectionOptions };
};
