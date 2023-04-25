import { Box, Heading } from "@chakra-ui/react";
import React from "react";
import { AnalyzersTable } from "./AnalyzersTable";

export const AnalyzersView = () => {
  return (
    <Box padding="4">
      <Heading size="lg">Analyzers</Heading>
      <AnalyzersTable />
    </Box>
  );
};
