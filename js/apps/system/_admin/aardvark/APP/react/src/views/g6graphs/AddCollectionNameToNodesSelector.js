import React, { useState } from 'react';
import { Select } from 'antd';

const AddCollectionNameToNodesSelector = ({ onAddCollectionNameToNodesChange } ) => {
  const labelsInput = React.createRef();
  const SelectOption = Select.Option;

  const handleChange = () => {
    console.log('NodeLabelContentSelector (handleChange) clicked');
    const typeModelMerged = {
      label: (labelsInput.current.value ? labelsInput.current.value : '_key'),
    };
    console.log('NodeLabelContentSelector (typeModelMerged): ', typeModelMerged);
    onAddCollectionNameToNodesChange(typeModelMerged);
  };

  const addCollectionNameToNodes = () => {
    onAddCollectionNameToNodesChange(true);
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
          <h4>Add collection name (nodes)</h4>
          <button onClick={addCollectionNameToNodes}>Add collection name</button>
          <hr />
  </>;
}

export default AddCollectionNameToNodesSelector;
