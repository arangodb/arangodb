import { useDisclosure } from "@chakra-ui/react";
import React, { createContext, ReactNode, useContext } from "react";
import { useDeleteIndex } from "./useDeleteIndex";

type CollectionIndicesContextType = {
  collectionName: string;
  onDeleteIndex: (data: { id: string; onSuccess: () => void }) => void;
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
  const { onDeleteIndex } = useDeleteIndex();
  return (
    <CollectionIndicesContext.Provider
      value={{
        onDeleteIndex,
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
