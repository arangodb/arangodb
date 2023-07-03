import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Heading, Stack, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { AddGraphModal } from "./addGraph/AddGraphModal";
import { GraphsProvider } from "./GraphsContext";
import { GraphsModeProvider } from "./GraphsModeContext";
import { GraphsTable } from "./listGraphs/GraphsTable";

export const GraphsView = () => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  return (
    <Box padding="4" width="100%">
      <GraphViewHeader onOpen={onOpen} />
      <GraphsModeProvider mode="add">
        <AddGraphModal isOpen={isOpen} onClose={onClose} />
      </GraphsModeProvider>
      <GraphsProvider>
        <GraphsTable />
      </GraphsProvider>
    </Box>
  );
};

const GraphViewHeader = ({ onOpen }: { onOpen: () => void }) => {
  return (
    <Stack direction="row" marginBottom="4" alignItems="center">
      <Heading size="lg">Graphs</Heading>
      <Button
        size="sm"
        leftIcon={<AddIcon />}
        colorScheme="blue"
        onClick={() => {
          onOpen();
        }}
      >
        Add graph
      </Button>
    </Stack>
  );
};
