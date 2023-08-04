import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Heading, Stack, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { AddCollectionModal } from "./addCollection/AddCollectionModal";
import { CollectionsTable } from "./listCollections/CollectionsTable";

export const CollectionsView = () => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  return (
    <Box padding="4" width="100%">
      <CollectionViewHeader onOpen={onOpen} />
      <CollectionsTable />
      <AddCollectionModal isOpen={isOpen} onClose={onClose} />
    </Box>
  );
};

const CollectionViewHeader = ({ onOpen }: { onOpen: () => void }) => {
  return (
    <Stack direction="row" marginBottom="4" alignItems="center">
      <Heading size="lg">Collections</Heading>
      <Button
        size="sm"
        leftIcon={<AddIcon />}
        colorScheme="blue"
        onClick={onOpen}
      >
        Add collection
      </Button>
    </Stack>
  );
};
