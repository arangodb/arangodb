import { FormState } from "../constants";
import React, { useState } from "react";
import { chain, findIndex, sortBy, uniqueId } from "lodash";

interface CopyFromInputProps {
  analyzers: FormState[];
  setFormState: (state: FormState) => void;
  setRenderKey: (key: string) => void;
}

const CopyFromInput = ({ analyzers, setFormState, setRenderKey }: CopyFromInputProps) => {
  const [selectedAnalyzer, setSelectedAnalyzer] = useState(0);

  const copyFormState = () => {
    const newFormState = chain(analyzers[selectedAnalyzer])
      .cloneDeep()
      .pick('name', 'type', 'features', 'properties')
      .value();
    newFormState.name = '';

    setFormState(newFormState);
    setRenderKey(uniqueId('force_re-render_'));
  };

  return <div className={'pure-g'}>
    <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
      <button className={'pure-button'} onClick={copyFormState}>
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
