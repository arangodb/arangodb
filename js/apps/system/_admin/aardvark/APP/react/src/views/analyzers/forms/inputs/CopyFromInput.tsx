import { FormState } from "../../constants";
import { FormProps } from "../../../../utils/constants";
import React, { ChangeEvent, useEffect, useState } from "react";
import { cloneDeep, find, sortBy } from "lodash";
import { Cell, Grid } from "../../../../components/pure-css/grid";
import { validateAndFix } from "../../helpers";
import { IconButton } from "../../../../components/pure-css/buttons";

type CopyFromInputProps = {
  analyzers: FormState[];
} & Pick<FormProps<FormState>, 'dispatch'>;

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
    <Cell size={'2-3'}>
      <select value={selectedAnalyzer.name} style={{
        width: 'auto',
        float: 'right'
      }} onChange={updateSelectedAnalyzer}>
        {
          sortedAnalyzers.map((analyzer, idx) =>
            <option key={idx} value={analyzer.name}>{analyzer.name} ({analyzer.type})</option>)
        }
      </select>
    </Cell>
    <Cell size={'1-3'}>
      <IconButton icon={'hand-o-left'} type={'warning'} onClick={copyFormState}>Copy from</IconButton>
    </Cell>
  </Grid>;
};

export default CopyFromInput;
