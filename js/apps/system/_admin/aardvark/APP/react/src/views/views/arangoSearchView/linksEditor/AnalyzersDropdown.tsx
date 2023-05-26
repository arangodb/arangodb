import { FormLabel } from "@chakra-ui/react";
import { AnalyzerDescription } from "arangojs/analyzer";
import React, { useEffect, useState } from "react";
import MultiSelect from "../../../../components/select/MultiSelect";
import { getCurrentDB } from "../../../../utils/arangoClient";
import { useLinksSetter } from "./useLinksSetter";

export const AnalyzersDropdown = () => {
  const analyzersOptions = useFetchAnalyzers();
  const options = analyzersOptions.map(analyzer => ({
    label: analyzer.name,
    value: analyzer.name
  }));
  const { analyzers, addAnalyzer, removeAnalyzer } = useLinksSetter();
  return (
    <>
      <FormLabel htmlFor="analyzers">Analyzers</FormLabel>
      <MultiSelect
        options={options}
        value={analyzers}
        inputId="analyzers"
        onChange={(_, action) => {
          if (action.action === "remove-value") {
            removeAnalyzer(action.removedValue.value);
            return;
          }
          if (action.action === "select-option" && action.option?.value) {
            addAnalyzer(action.option.value);
          }
        }}
      />
    </>
  );
};

const useFetchAnalyzers = () => {
  const [analyzers, setAnalyzers] = useState<AnalyzerDescription[]>([]);
  useEffect(() => {
    const fetchAnalyzers = async () => {
      const db = getCurrentDB();
      const analyzers = await db.listAnalyzers();
      setAnalyzers(analyzers);
    };
    fetchAnalyzers();
  }, []);
  return analyzers;
};
