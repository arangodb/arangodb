import { ArrowBackIcon } from "@chakra-ui/icons";
import { Box, Button, Stack } from "@chakra-ui/react";
import { AnalyzerDescription } from "arangojs/analyzer";
import { useFormikContext } from "formik";
import React from "react";
import SingleSelect from "../../../components/select/SingleSelect";
import { AnalyzerState } from "../Analyzer.types";
import { useAnalyzersContext } from "../AnalyzersContext";

export const CopyAnalyzerDropdown = () => {
  const { analyzers } = useAnalyzersContext();
  const analyzerOptions =
    analyzers?.map(analyzer => {
      return { value: analyzer.name, label: analyzer.name };
    }) || [];
  const [selectedAnalyzer, setSelectedAnalyzer] = React.useState<
    AnalyzerDescription | undefined
  >(analyzers?.[0]);
  const { setValues, values } = useFormikContext<AnalyzerState>();
  return (
    <Stack direction="row" alignItems="center" flexWrap="wrap">
      <Stack direction="row" alignItems="center">
        <Box width="44">
          <SingleSelect
            options={analyzerOptions}
            onChange={value => {
              const selectedAnalyzer = analyzers?.find(
                analyzer => analyzer.name === value?.value
              );
              setSelectedAnalyzer(selectedAnalyzer);
            }}
            defaultValue={analyzerOptions[0]}
          />
        </Box>
        <Button
          size="xs"
          leftIcon={<ArrowBackIcon />}
          onClick={() => {
            if (!selectedAnalyzer) {
              return;
            }
            setValues({
              ...selectedAnalyzer,
              name: values.name
            } as AnalyzerState);
          }}
        >
          Copy from
        </Button>
      </Stack>
    </Stack>
  );
};
