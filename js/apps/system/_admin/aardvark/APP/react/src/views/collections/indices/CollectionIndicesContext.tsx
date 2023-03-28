import { useDisclosure } from "@chakra-ui/react";
import React, { createContext, ReactNode, useContext, useState } from "react";
import { useDeleteIndex } from "./useDeleteIndex";
import { useSupportedIndexTypes } from "./useSupportedIndexTypes";
import { usePermissionsCheck } from "./usePermissionsCheck";
import { useSetupBreadcrumbs } from "./useSetupBreadcrumbs";
import { IndexType } from "./useFetchIndices";

type CollectionIndicesContextType = {
  collectionName: string;
  collectionId: string;
  indexTypeOptions: { value: IndexType; label: string }[];
  onDeleteIndex: (data: { id: string; onSuccess: () => void }) => void;
  onOpenForm: () => void;
  onCloseForm: () => void;
  isFormOpen: boolean;
  readOnly: boolean;
};
const CollectionIndicesContext = createContext<CollectionIndicesContextType>(
  {} as CollectionIndicesContextType
);

export const CollectionIndicesProvider = ({
  collectionName,
  collection,
  children
}: {
  collectionName: string;
  collection: { id: string; attributes: { id: string } };
  children: ReactNode;
}) => {
  const {
    onOpen: onOpenForm,
    onClose: onCloseForm,
    isOpen: isFormOpen
  } = useDisclosure();
  const collectionId = collection.attributes.id;
  const { onDeleteIndex } = useDeleteIndex({ collectionId });
  const { indexTypeOptions } = useSupportedIndexTypes();
  const [readOnly, setReadOnly] = useState(false);
  usePermissionsCheck({ setReadOnly, collectionName });
  useSetupBreadcrumbs({ readOnly, collectionName });
  return (
    <CollectionIndicesContext.Provider
      value={{
        onDeleteIndex,
        collectionId,
        collectionName,
        onOpenForm,
        onCloseForm,
        isFormOpen,
        readOnly,
        indexTypeOptions
      }}
    >
      {children}
    </CollectionIndicesContext.Provider>
  );
};

export const useCollectionIndicesContext = () => {
  return useContext(CollectionIndicesContext);
};
