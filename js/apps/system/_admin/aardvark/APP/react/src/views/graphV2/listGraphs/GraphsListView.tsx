import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Heading, Stack, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { useDatabaseReadOnly } from "../../../utils/useDatabaseReadOnly";
import { AddGraphModal } from "../addGraph/AddGraphModal";
import { GraphsListProvider } from "./GraphsListContext";
import { GraphsModeProvider } from "./GraphsModeContext";
import { GraphsTable } from "./GraphsTable";

export const GraphsListView = () => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  return (
    <Box padding="4" width="100%">
      <GraphsListProvider>
        <GraphListViewHeader onOpen={onOpen} />
        <GraphsModeProvider mode="add">
          <AddGraphModal isOpen={isOpen} onClose={onClose} />
        </GraphsModeProvider>
        <GraphsTable />
      </GraphsListProvider>
    </Box>
  );
};

const GraphListViewHeader = ({ onOpen }: { onOpen: () => void }) => {
  const { readOnly, isLoading } = useDatabaseReadOnly();
  return (
    <Stack direction="row" marginBottom="4" alignItems="center">
      <Heading size="lg">Graphs</Heading>
      <Button
        size="sm"
        isLoading={isLoading}
        isDisabled={readOnly}
        leftIcon={<AddIcon />}
        colorScheme="green"
        onClick={onOpen}
      >
        Add graph
      </Button>
    </Stack>
  );
};
