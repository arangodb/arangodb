import { Box, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { AddCollectionModal } from "./addCollection/AddCollectionModal";
import { CollectionsTable } from "./listCollections/CollectionsTable";

export const CollectionsView = () => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  return (
    <Box padding="4" width="100%">
      <CollectionsTable onAddCollectionClick={onOpen} />
      <AddCollectionModal isOpen={isOpen} onClose={onClose} />
    </Box>
  );
};
