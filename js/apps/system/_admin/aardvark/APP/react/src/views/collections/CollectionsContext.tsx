import React, { createContext, ReactNode, useContext } from "react";
import { LockableCollectionDescription, useFetchCollections } from "./useFetchCollections";

type CollectionsContextType = {
  collections: LockableCollectionDescription[] | undefined;
  isFormDisabled: boolean;
};
const CollectionsContext = createContext<CollectionsContextType>({} as CollectionsContextType);

export const CollectionsProvider = ({
  children,
  isFormDisabled
}: {
  children: ReactNode;
  isFormDisabled?: boolean;
}) => {
  const { collections } = useFetchCollections();
  return (
    <CollectionsContext.Provider
      value={{
        collections,
        isFormDisabled: !!isFormDisabled
      }}
    >
      {children}
    </CollectionsContext.Provider>
  );
};

export const useCollectionsContext = () => {
  return useContext(CollectionsContext);
};