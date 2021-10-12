import React, { MouseEvent, useState } from 'react';
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../components/modal/Modal";
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { mutate } from "swr";
import { noop, pick } from 'lodash';
import { FormState } from "./constants";
import { State } from '../../utils/constants';
import { Cell, Grid } from "../../components/pure-css/grid";
import BaseForm from "./forms/BaseForm";
import FeatureForm from "./forms/FeatureForm";
import { getForm } from "./helpers";
import Textarea from "../../components/pure-css/form/Textarea";
import { IconButton } from "../../components/pure-css/buttons";

declare var frontendConfig: { [key: string]: any };
declare var arangoHelper: { [key: string]: any };

type ButtonProps = {
  analyzer: FormState;
  modalCid: string;
}

const DeleteButton = ({ analyzer, modalCid }: ButtonProps) => {
  const [show, setShow] = useState(false);
  const [forceDelete, setForceDelete] = useState(false);

  const toggleForce = () => {
    setForceDelete(!forceDelete);
  };

  const handleDelete = async () => {
    try {
      const result = await getApiRouteForCurrentDB().delete(`/analyzer/${analyzer.name}`, { force: forceDelete });

      if (result.body.error) {
        arangoHelper.arangoError('Failure', `Got unexpected server response: ${result.body.errorMessage}`);
      } else {
        arangoHelper.arangoNotification('Success', `Deleted Analyzer: ${analyzer.name}`);
        await mutate('/analyzer');
      }
    } catch (e) {
      arangoHelper.arangoError('Failure', `Got unexpected server response: ${e.message}`);
    }
  };

  return <>
    <IconButton icon={'trash-o'} style={{ background: 'transparent' }} onClick={() => setShow(true)}/>
    <Modal show={show} setShow={setShow} cid={modalCid}>
      <ModalHeader title={`Delete Analyzer ${analyzer.name}?`}/>
      <ModalBody>
        <p>
          This Analyzer might be in use. Deleting it will impact the Views using it. Clicking on
          the <b>Delete</b> button will only Delete the Analyzer if it is not in use.
        </p>
        <p>
          Select the <b>Force Delete</b> option below if you want to delete the Analyzer even if it is being
          used.
        </p>
        <label htmlFor={'force-delete'} className="pure-checkbox">
          <input id={'force-delete'} type={'checkbox'} checked={forceDelete} onChange={toggleForce}
                 style={{ width: 'auto' }}/> Force Delete
        </label>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Close</button>
        <button className="button-danger" style={{ float: 'right' }} onClick={handleDelete}>Delete</button>
      </ModalFooter>
    </Modal>
  </>;
};

const ViewButton = ({ analyzer, modalCid }: ButtonProps) => {
  const [show, setShow] = useState(false);
  const [showJsonForm, setShowJsonForm] = useState(false);

  const state: State<FormState> = {
    formState: pick(analyzer, 'name', 'type', 'features', 'properties') as FormState,
    formCache: {},
    show: false,
    showJsonForm: false,
    lockJsonForm: false,
    renderKey: ''
  };

  const handleClick = (event: MouseEvent<HTMLElement>) => {
    event.preventDefault();
    setShow(true);
  };

  const toggleJsonForm = () => {
    setShowJsonForm(!showJsonForm);
  };

  const formState = state.formState;
  let jsonFormState = '';
  let jsonRows = 1;
  if (showJsonForm) {
    jsonFormState = JSON.stringify(formState, null, 4);
    jsonRows = jsonFormState.split('\n').length;
  }

  return <>
    <IconButton icon={'eye'} onClick={handleClick} style={{ background: 'transparent' }}/>
    <Modal show={show} setShow={setShow} cid={modalCid}>
      <ModalHeader title={formState.name}>
        <button className={'button-info'} onClick={toggleJsonForm} style={{ float: 'right' }}>
          {showJsonForm ? 'Switch to form view' : 'Switch to code view'}
        </button>
      </ModalHeader>
      <ModalBody maximize={true} show={show}>
        <Grid>
          {
            showJsonForm
              ? <Cell size={'1'}>
                <Textarea label={'JSON Dump'} disabled={true} value={jsonFormState} rows={jsonRows}
                          style={{ cursor: 'text' }}/>
              </Cell>
              : <>
                <Cell size={'11-24'}>
                  <fieldset>
                    <legend style={{ fontSize: '12pt' }}>Basic</legend>
                    <BaseForm formState={formState} dispatch={noop} disabled={true}/>
                  </fieldset>
                </Cell>
                <Cell size={'1-12'}/>
                <Cell size={'11-24'}>
                  <fieldset>
                    <legend style={{ fontSize: '12pt' }}>Features</legend>
                    <FeatureForm formState={formState} dispatch={noop} disabled={true}/>
                  </fieldset>
                </Cell>

                {
                  formState.type === 'identity' ? null
                    : <Cell size={'1'}>
                      <fieldset>
                        <legend style={{ fontSize: '12pt' }}>Configuration</legend>
                        {getForm({
                          formState,
                          dispatch: noop
                        })}
                      </fieldset>
                    </Cell>
                }
              </>
          }
        </Grid>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Close</button>
      </ModalFooter>
    </Modal>
  </>;
};

interface ActionProps {
  analyzer: FormState;
  permission: string;
  modalCidSuffix: string;
}

const Actions = ({ analyzer, permission, modalCidSuffix }: ActionProps) => {
  const isUserDefined = analyzer.name.includes('::');
  const isSameDB = isUserDefined
    ? analyzer.name.split('::')[0] === frontendConfig.db
    : frontendConfig.db === '_system';
  const isAdminUser = permission === 'rw' || !frontendConfig.authenticationEnabled;
  const canDelete = isUserDefined && isSameDB && isAdminUser;

  return <>
    <ViewButton analyzer={analyzer} modalCid={`modal-content-view-${modalCidSuffix}`}/>
    {
      canDelete
        ? <>
          &nbsp;
          <DeleteButton analyzer={analyzer} modalCid={`modal-content-delete-${modalCidSuffix}`}/>
        </>
        : null
    }
  </>;
};

export default Actions;
