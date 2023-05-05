import { ArrowBackIcon } from "@chakra-ui/icons";
import { Box, Button, Stack } from "@chakra-ui/react";
import React from "react";
import SingleSelect from "../../components/select/SingleSelect";
import { useAnalyzersContext } from "./AnalyzersContext";

export const CopyAnalyzerDropdown = () => {
  const { analyzers } = useAnalyzersContext();
  const analyzerOptions =
    analyzers?.map(analyzer => {
      return { value: analyzer.name, label: analyzer.name };
    }) || [];
  return (
    <Stack direction="row" alignItems="center" flexWrap="wrap">
      <Stack direction="row" alignItems="center">
        <Box width="44">
          <SingleSelect
            options={analyzerOptions}
            onChange={value => {
              console.log(value);
            }}
            defaultValue={analyzerOptions[0]}
          />
        </Box>
        <Button
          size="xs"
          leftIcon={<ArrowBackIcon />}
          onClick={() => {
            console.log("copy from");
          }}
        >
          Copy from
        </Button>
      </Stack>
    </Stack>
  );
};
