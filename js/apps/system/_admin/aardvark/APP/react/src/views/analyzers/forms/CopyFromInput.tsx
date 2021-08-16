import { FormProps, FormState, validateAndFix } from "../constants";
import React, { useState } from "react";
import { cloneDeep, findIndex, sortBy } from "lodash";

type CopyFromInputProps = {
  analyzers: FormState[];
} & Pick<FormProps, 'dispatch'>;

const CopyFromInput = ({ analyzers, dispatch }: CopyFromInputProps) => {
  const [selectedAnalyzer, setSelectedAnalyzer] = useState(0);

  const copyFormState = () => {
    const newFormState = cloneDeep(analyzers[selectedAnalyzer]);
    newFormState.name = '';
    validateAndFix(newFormState);

    dispatch({
      type: 'setFormState',
      formState: newFormState
    });
    dispatch({ type: 'regenRenderKey' });
  };

  return <div className={'pure-g'}>
    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <button className={'button-warning'} onClick={copyFormState}>
        Copy to form
      </button>
    </div>
    <div className={'pure-u-16-24 pure-u-md-16-24 pure-u-lg-16-24 pure-u-xl-16-24'}>
      <select value={analyzers[selectedAnalyzer].name} style={{ width: 'auto' }}
              onChange={(event) => {
                const selectedAnalyzerIdx = findIndex(analyzers, { name: event.target.value });

                setSelectedAnalyzer(selectedAnalyzerIdx);
              }}>
        {
          sortBy(analyzers, 'name').map((analyzer, idx) =>
            <option key={idx} value={analyzer.name}>{analyzer.name} ({analyzer.type})</option>)
        }
      </select>
    </div>
  </div>;
};

export default CopyFromInput;
