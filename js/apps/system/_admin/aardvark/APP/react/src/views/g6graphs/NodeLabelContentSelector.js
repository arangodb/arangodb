import React, { useState } from 'react';
import { Select } from 'antd';

const NodeLabelContentSelector = ({ onNodeLabelContentChange} ) => {
  const labelsInput = React.createRef();

  const handleChange = () => {
    console.log('NodeLabelContentSelector (handleChange) clicked');
    const typeModelMerged = {
      label: (labelsInput.current.value ? labelsInput.current.value : '_key'),
    };
    console.log('NodeLabelContentSelector (typeModelMerged): ', typeModelMerged);
    onNodeLabelContentChange(typeModelMerged);
  };

  return <>
          <h4>Node label</h4>
          <label for="labels">Labels</label>
          <input name="labels" type="input" style={{ width: '200px' }} placeholder="Attribute key" ref={labelsInput} />
          <button onClick={handleChange}>Update labels</button>
          <hr />
  </>;
}

export default NodeLabelContentSelector;
