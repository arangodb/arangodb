import { Box, Heading, Input } from "@chakra-ui/react";
import React from "react";
import useSWR from "swr";
import { getCurrentDB } from "../../utils/arangoClient";
import { AnalyzersTable } from "./AnalyzersTable";

export const AnalyzersView = () => {
  const { analyzers } = useFetchAnalyzers();
  return (
    <Box padding="4">
      <Heading size="lg">Analyzers</Heading>
      {/* toolbar with search and filter */}
      <Box marginTop="4">
        <Input background="white" placeholder="Search" />
      </Box>

      {/* list of analyzers */}
      <AnalyzersTable analyzers={analyzers} />
    </Box>
  );
};
const useFetchAnalyzers = () => {
  const { data } = useSWR("/analyzers", async () => {
    const db = getCurrentDB();
    const data = await db.listAnalyzers();
    return data;
  });
  return { analyzers: data };
};
