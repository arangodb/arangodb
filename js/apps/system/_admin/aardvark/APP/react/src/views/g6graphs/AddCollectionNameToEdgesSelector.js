import React, { useState } from 'react';
import { Select } from 'antd';

const AddCollectionNameToEdgesSelector = ({ onAddCollectionNameToEdgesChange } ) => {
  const labelsInput = React.createRef();
  const SelectOption = Select.Option;

  const handleChange = () => {
    console.log('EdgeLabelContentSelector (handleChange) clicked');
    const typeModelMerged = {
      label: (labelsInput.current.value ? labelsInput.current.value : '_key'),
    };
    console.log('EdgeLabelContentSelector (typeModelMerged): ', typeModelMerged);
    onAddCollectionNameToEdgesChange(typeModelMerged);
  };

  const addCollectionNameToEdges = () => {
    onAddCollectionNameToEdgesChange(true);
  }

  /*
  <Select name="addCollectionName" style={{ width: '200px' }} value={'no'} onChange={handleChange}>
            <SelectOption key={'no'} value={'no'}>
              {'no'}
            </SelectOption>
            <SelectOption key={'yes'} value={'yes'}>
              {'yes'}
            </SelectOption>
          </Select>
  */

  return <>
          <h4>Add collection name (edges)</h4>
          <button onClick={addCollectionNameToEdges}>Add collection name to edges</button>
          <hr />
  </>;
}

export default AddCollectionNameToEdgesSelector;
