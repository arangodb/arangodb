import { useDisclosure } from "@chakra-ui/react";
import React, { createContext, ReactNode, useContext } from "react";

type CollectionIndicesContextType = {
  collectionName: string;
  onOpenForm: () => void;
  onCloseForm: () => void;
  isFormOpen: boolean;
};
const CollectionIndicesContext = createContext<CollectionIndicesContextType>(
  {} as CollectionIndicesContextType
);

export const CollectionIndicesProvider = ({
  collectionName,
  children
}: {
  collectionName: string;
  children: ReactNode;
}) => {
  const {
    onOpen: onOpenForm,
    onClose: onCloseForm,
    isOpen: isFormOpen
  } = useDisclosure();
  return (
    <CollectionIndicesContext.Provider
      value={{
        collectionName,
        onOpenForm,
        onCloseForm,
        isFormOpen
      }}
    >
      {children}
    </CollectionIndicesContext.Provider>
  );
};

export const useCollectionIndicesContext = () => {
  return useContext(CollectionIndicesContext);
};
