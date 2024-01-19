import { Box, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { ListHeader } from "../../components/table/ListHeader";
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
  return (
    <ListHeader
      onButtonClick={onOpen}
      heading="Analyzers"
      buttonText="Add analyzer"
    />
  );
};
