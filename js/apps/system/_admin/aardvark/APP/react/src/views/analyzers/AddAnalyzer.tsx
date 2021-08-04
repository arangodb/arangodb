import React, { ChangeEvent, useState } from 'react';
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../components/modal/Modal";
import { typeNameMap } from "./constants";
import { cloneDeep, map, omit, set } from 'lodash';
import JsonForm from "./forms/JsonForm";
import FeatureForm from "./forms/FeatureForm";
import DelimiterForm from "./forms/DelimiterForm";


const restrictedTypeNameMap = omit(typeNameMap, 'identity');
const initialFormState = {
  name: '',
  type: 'delimiter',
  features: [],
  properties: {}
};

const AddAnalyzer = () => {
  const [show, setShow] = useState(false);
  const [showJsonForm, setShowJsonForm] = useState(false);
  const [formState, setFormState] = useState(initialFormState);

  const handleAdd = () => {
    setShow(false);
  };

  const toggleJsonForm = () => {
    setShowJsonForm(!showJsonForm);
  };

  const updateFormField = (field: string, value: any) => {
    const newFormState = cloneDeep(formState);

    set(newFormState, field, value);
    setFormState(newFormState);
  };

  const updateName = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('name', event.target.value);
  };

  const updateType = (event: ChangeEvent<HTMLSelectElement>) => {
    updateFormField('type', event.target.value);
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
      <ModalHeader title={'Create Analyzer'}>
        <span style={{ float: 'right' }}>
          <button className={'pure-button'} onClick={toggleJsonForm} style={{
            fontSize: '70%'
          }}>
          {showJsonForm ? 'Switch to form view' : 'Switch to code view'}
        </button>
        </span>
      </ModalHeader>
      <ModalBody>
        <div className={'pure-g'} style={{ minWidth: '50vw' }}>
          <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
            {
              showJsonForm
                ? <JsonForm formState={formState} setFormState={setFormState}/>
                : <div className={'pure-g'}>
                  <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
                    <label htmlFor={'analyzer-name'}>Analyzer Name:</label>
                    <input id="analyzer-name" type="text" placeholder="Analyzer Name" value={formState.name}
                           onChange={updateName} style={{
                      height: 'auto',
                      width: 'auto'
                    }}/>
                  </div>

                  <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
                    <label htmlFor={'analyzer-type'}>Analyzer Type:</label>
                    <select id="analyzer-type" value={formState.type} style={{ width: 'auto' }}
                            onChange={updateType}>
                      {
                        map(restrictedTypeNameMap, (value, key) => <option key={key}
                                                                           value={key}>{value}</option>)
                      }
                    </select>
                  </div>

                  <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
                    <fieldset>
                      <legend>Features</legend>
                      <FeatureForm formState={formState} updateFormField={updateFormField}/>
                    </fieldset>
                  </div>

                  <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
                    <fieldset>
                      <legend>Configuration</legend>
                      <DelimiterForm formState={formState} updateFormField={updateFormField}/>
                    </fieldset>
                  </div>
                </div>
            }
          </div>
        </div>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Close</button>
        <button className="button-success" style={{ float: 'right' }} onClick={handleAdd}>Create</button>
      </ModalFooter>
    </Modal>
  </>
    ;
};

export default AddAnalyzer;
