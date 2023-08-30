import { Index } from "arangojs/indexes";
import React, { createContext, ReactNode, useContext, useState } from "react";
import { useFetchCollectionIndices } from "./useFetchCollectionIndices";
import { useSupportedIndexTypes } from "./useSupportedIndexTypes";
import { usePermissionsCheck } from "./usePermissionsCheck";
import { useSetupBreadcrumbs } from "./useSetupBreadcrumbs";
import { useSetupNav } from "./useSetupNav";
import { useDisclosure } from "@chakra-ui/react";
import { useFetchCollectionFigures } from "../figures/useFetchCollectionFigures";

type CollectionIndicesContextType = {
  collectionIndices: Index[] | undefined;
  collectionName: string;
  collectionId: string;
  indexTypeOptions: { value: Index["type"]; label: string }[];
  onOpenForm: () => void;
  onCloseForm: () => void;
  isFormOpen: boolean;
  readOnly: boolean;
  stats: { count: number; size: number } | undefined;
};
const CollectionIndicesContext = createContext<CollectionIndicesContextType>(
  {} as CollectionIndicesContextType
);

export const CollectionIndicesProvider = ({
  children,
  collectionName,
  collectionId
}: {
  children: ReactNode;
  collectionName: string;
  collectionId: string;
}) => {
  const {
    onOpen: onOpenForm,
    onClose: onCloseForm,
    isOpen: isFormOpen
  } = useDisclosure();
  const [readOnly, setReadOnly] = useState(false);
  usePermissionsCheck({ setReadOnly, collectionName });
  useSetupBreadcrumbs({ readOnly, collectionName });
  useSetupNav({ collectionName });
  const { indexTypeOptions } = useSupportedIndexTypes();
  const { collectionIndices } = useFetchCollectionIndices(collectionName);
  const { collectionFigures } = useFetchCollectionFigures(collectionName);
  return (
    <CollectionIndicesContext.Provider
      value={{
        collectionIndices,
        collectionName,
        collectionId,
        indexTypeOptions,
        onOpenForm,
        onCloseForm,
        isFormOpen,
        readOnly,
        stats: collectionFigures?.figures.indexes
      }}
    >
      {children}
    </CollectionIndicesContext.Provider>
  );
};

export const useCollectionIndicesContext = () => {
  return useContext(CollectionIndicesContext);
};
