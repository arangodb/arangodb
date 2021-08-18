import { FormProps, FormState, validateAndFix } from "../constants";
import React, { ChangeEvent, useEffect, useState } from "react";
import { cloneDeep, find, sortBy } from "lodash";
import { Cell, Grid } from "../../../components/pure-css/grid";

type CopyFromInputProps = {
  analyzers: FormState[];
} & Pick<FormProps, 'dispatch'>;

const CopyFromInput = ({ analyzers, dispatch }: CopyFromInputProps) => {
  const [sortedAnalyzers, setSortedAnalyzers] = useState(sortBy(analyzers, 'name'));
  const [selectedAnalyzer, setSelectedAnalyzer] = useState(sortedAnalyzers[0]);

  useEffect(() => {
    setSortedAnalyzers(sortBy(analyzers, 'name'));
  }, [analyzers]);

  useEffect(() => {
    let tempSelectedAnalyzer = find(sortedAnalyzers, { name: selectedAnalyzer.name });
    if (!tempSelectedAnalyzer) {
      setSelectedAnalyzer(sortedAnalyzers[0]);
    }
  }, [selectedAnalyzer.name, sortedAnalyzers]);

  const copyFormState = () => {
    const newFormState = cloneDeep(selectedAnalyzer);
    newFormState.name = '';
    validateAndFix(newFormState);

    dispatch({
      type: 'setFormState',
      formState: newFormState
    });
    dispatch({ type: 'regenRenderKey' });
  };

  const updateSelectedAnalyzer = (event: ChangeEvent<HTMLSelectElement>) => {
    let tempSelectedAnalyzer = find(sortedAnalyzers, { name: event.target.value });
    if (!tempSelectedAnalyzer) {
      tempSelectedAnalyzer = sortedAnalyzers[0];
    }

    setSelectedAnalyzer(tempSelectedAnalyzer);
  };

  return <Grid>
    <Cell size={'1-3'}>
      <button className={'button-warning'} onClick={copyFormState}>
        Copy to form
      </button>
    </Cell>
    <Cell size={'2-3'}>
      <select value={selectedAnalyzer.name} style={{ width: 'auto' }} onChange={updateSelectedAnalyzer}>
        {
          sortedAnalyzers.map((analyzer, idx) =>
            <option key={idx} value={analyzer.name}>{analyzer.name} ({analyzer.type})</option>)
        }
      </select>
    </Cell>
  </Grid>;
};

export default CopyFromInput;
