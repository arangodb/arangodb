import React, { ReactFragment, useState } from 'react';
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../components/modal/Modal";
import { cloneDeep, set } from 'lodash';
import JsonForm from "./forms/JsonForm";
import FeatureForm from "./forms/FeatureForm";
import DelimiterForm from "./forms/DelimiterForm";
import StemForm from "./forms/StemForm";
import NormForm from "./forms/NormForm";
import NGramForm from "./forms/NGramForm";
import BaseForm from "./forms/BaseForm";
import TextForm from "./forms/TextForm";


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

  const forms: { [key: string]: ReactFragment | null } = {
    delimiter: <DelimiterForm formState={formState} updateFormField={updateFormField}/>,
    stem: <StemForm formState={formState} updateFormField={updateFormField}/>,
    norm: <NormForm formState={formState} updateFormField={updateFormField}/>,
    ngram: <NGramForm formState={formState} updateFormField={updateFormField}/>,
    text: <TextForm formState={formState} updateFormField={updateFormField}/>,
    identity: null
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
          {
            showJsonForm
              ? <JsonForm formState={formState} setFormState={setFormState}/>
              : <>
                <BaseForm formState={formState} updateFormField={updateFormField}/>
                <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
                  <fieldset>
                    <legend style={{ fontSize: '12pt' }}>Features</legend>
                    <FeatureForm formState={formState} updateFormField={updateFormField}/>
                  </fieldset>
                </div>

                {
                  formState.type === 'identity' ? null
                    : <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
                      <fieldset>
                        <legend style={{ fontSize: '12pt' }}>Configuration</legend>
                        {forms[formState.type]}
                      </fieldset>
                    </div>
                }
              </>
          }
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
