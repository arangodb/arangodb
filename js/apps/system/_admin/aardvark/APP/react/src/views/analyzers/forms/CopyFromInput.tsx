import { FormProps, FormState, validateAndFix } from "../constants";
import React, { ChangeEvent, useEffect, useState } from "react";
import { cloneDeep, find, sortBy } from "lodash";

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

  return <div className={'pure-g'}>
    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <button className={'button-warning'} onClick={copyFormState}>
        Copy to form
      </button>
    </div>
    <div className={'pure-u-16-24 pure-u-md-16-24 pure-u-lg-16-24 pure-u-xl-16-24'}>
      <select value={selectedAnalyzer.name} style={{ width: 'auto' }} onChange={updateSelectedAnalyzer}>
        {
          sortedAnalyzers.map((analyzer, idx) =>
            <option key={idx} value={analyzer.name}>{analyzer.name} ({analyzer.type})</option>)
        }
      </select>
    </div>
  </div>;
};

export default CopyFromInput;
