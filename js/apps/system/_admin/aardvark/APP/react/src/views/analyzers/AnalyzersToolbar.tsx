import { Box, Checkbox } from "@chakra-ui/react";
import React from "react";
import { useAnalyzersContext } from "./AnalyzersContext";

export const AnalyzersToolbar = () => {
  const { showSystemAnalyzers, setShowSystemAnalyzers } = useAnalyzersContext();
  return (
    <Box marginTop="4">
      <Checkbox
        onChange={() => {
          setShowSystemAnalyzers(!showSystemAnalyzers);
        }}
        isChecked={showSystemAnalyzers}
      >
        Show system analyzers
      </Checkbox>
    </Box>
  );
};
