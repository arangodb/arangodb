import React, { useState } from 'react';
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../components/modal/Modal";
import { typeNameMap } from "./constants";
import { map, omit } from 'underscore';
import { getChangeHandler } from "../../utils/helpers";

const restrictedTypeNameMap = omit(typeNameMap, 'identity');

const AddAnalyzer = () => {
  const [show, setShow] = useState(false);
  const [analyzerName, setAnalyzerName] = useState('');
  const [analyzerType, setAnalyzerType] = useState('delimiter');

  const handleAdd = () => {
    setShow(false);
  };

  return <>
    <button className={'pure-button'} onClick={() => setShow(true)} style={{
      background: 'transparent',
      color: 'white',
      paddingLeft: 0,
      paddingTop: 0
    }}>
      <i className="fa fa-plus-circle"/> Add Analyzer
    </button>
    <Modal show={show} setShow={setShow}>
      <ModalHeader title={'Create Analyzer'}/>
      <ModalBody>
        <div className={'pure-g'} style={{ minWidth: '50vw' }}>
          <div className={'pure-u-2-5 pure-u-md-2-5 pure-u-lg-2-5 pure-u-xl-2-5'}>
            <label htmlFor={'analyzer-name'}>Analyzer Name:</label>
            <input id="analyzer-name" type="text" placeholder="Analyzer Name" style={{ width: 'auto' }}
                   value={analyzerName} onChange={getChangeHandler(setAnalyzerName)} key={'analyzer-name'}/>
          </div>

          {/* Spacer Div*/}
          <div className={'pure-u-1-5 pure-u-md-1-5 pure-u-lg-1-5 pure-u-xl-1-5'}/>

          <div className={'pure-u-2-5 pure-u-md-2-5 pure-u-lg-2-5 pure-u-xl-2-5'}>
            <label htmlFor={'analyzer-type'}>Analyzer Type:</label>
            <select id="analyzer-type" style={{ width: 'auto' }} value={analyzerType} key={'analyzer-type'}
                    onChange={getChangeHandler(setAnalyzerType)}>
              {
                map(restrictedTypeNameMap, (value, key) => <option key={key} value={key}>{value}</option>)
              }
            </select>
          </div>
        </div>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Close</button>
        <button className="button-success" style={{ float: 'right' }} onClick={handleAdd}>Create</button>
      </ModalFooter>
    </Modal>
  </>;
};

export default AddAnalyzer;
