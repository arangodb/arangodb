import { AddIcon } from "@chakra-ui/icons";
import { Box, Button, Heading, Stack } from "@chakra-ui/react";
import React from "react";
import { AnalyzersTable } from "./AnalyzersTable";
import { AnalyzersToolbar } from "./AnalyzersToolbar";

export const AnalyzersView = () => {
  return (
    <Box padding="4">
      <Stack direction="row" marginBottom="4" alignItems="center">
        <Heading size="lg">Analyzers</Heading>
        <Button size="sm" leftIcon={<AddIcon />} colorScheme="blue">
          Add analyzer
        </Button>
      </Stack>
      <AnalyzersToolbar />
      <AnalyzersTable />
    </Box>
  );
};
