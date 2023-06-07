import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Heading, Stack, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { AddAnalyzerModal } from "./AddAnalyzerModal";
import { AnalyzersTable } from "./AnalyzersTable";
import { AnalyzersToolbar } from "./AnalyzersToolbar";

export const AnalyzersView = () => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  return (
    <Box padding="4" width="100%">
      <Stack direction="row" marginBottom="4" alignItems="center">
        <Heading size="lg">Analyzers</Heading>
        <Button
          size="sm"
          leftIcon={<AddIcon />}
          colorScheme="blue"
          onClick={() => {
            onOpen();
          }}
        >
          Add analyzer
        </Button>
      </Stack>
      <AnalyzersToolbar />
      <AnalyzersTable />
      <AddAnalyzerModal isOpen={isOpen} onClose={onClose} />
    </Box>
  );
};
