import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Heading, Stack, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { useDatabaseReadOnly } from "../../utils/useDatabaseReadOnly";
import { AddAnalyzerModal } from "./addAnalyzer/AddAnalyzerModal";
import { AnalyzersTable } from "./listAnalyzers/AnalyzersTable";

export const AnalyzersView = () => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  return (
    <Box padding="4" width="100%">
      <AnalyzerViewHeader onOpen={onOpen} />
      <AnalyzersTable />
      <AddAnalyzerModal isOpen={isOpen} onClose={onClose} />
    </Box>
  );
};

const AnalyzerViewHeader = ({ onOpen }: { onOpen: () => void }) => {
  const { readOnly, isLoading } = useDatabaseReadOnly();

  return (
    <Stack direction="row" marginBottom="4" alignItems="center">
      <Heading size="lg">Analyzers</Heading>
      <Button
        size="sm"
        isLoading={isLoading}
        isDisabled={readOnly}
        leftIcon={<AddIcon />}
        colorScheme="green"
        onClick={() => {
          onOpen();
        }}
      >
        Add analyzer
      </Button>
    </Stack>
  );
};
