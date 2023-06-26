import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Heading, Stack, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { AddGraphModal } from "./addGraph/AddGraphModal";
import { GraphsTable } from "./listGraphs/GraphsTable";

export const GraphsView = () => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  return (
    <Box padding="4" width="100%">
      <GraphViewHeader onOpen={onOpen} />
      <AddGraphModal isOpen={isOpen} onClose={onClose} />
      <GraphsTable />
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
