import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Heading, Stack, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { AddDatabaseModal } from "./addDatabase/AddDatabaseModal";
import { DatabasesTable } from "./listDatabases/DatabasesTable";

export const DatabasesView = () => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  return (
    <Box padding="4" width="100%">
      <DatabaseViewHeader onOpen={onOpen} />
      <DatabasesTable />
      <AddDatabaseModal isOpen={isOpen} onClose={onClose} />
    </Box>
  );
};

const DatabaseViewHeader = ({ onOpen }: { onOpen: () => void }) => {
  return (
    <Stack direction="row" marginBottom="4" alignItems="center">
      <Heading size="lg">Databases</Heading>
      <Button
        size="sm"
        leftIcon={<AddIcon />}
        colorScheme="blue"
        onClick={onOpen}
      >
        Add database
      </Button>
    </Stack>
  );
};
