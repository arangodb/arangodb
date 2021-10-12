import React, { MouseEvent, useState } from 'react';
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../components/modal/Modal";
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import useSWR, { mutate } from "swr";
import { FormState } from "./constants";
import { State } from '../../utils/constants';
import { Cell, Grid } from "../../components/pure-css/grid";
import Textarea from "../../components/pure-css/form/Textarea";
import { omit } from 'lodash';
import { IconButton } from "../../components/arango/buttons";

declare var frontendConfig: { [key: string]: any };
declare var arangoHelper: { [key: string]: any };

type ButtonProps = {
  view: FormState;
  modalCid: string;
}

const DeleteButton = ({ view, modalCid }: ButtonProps) => {
  const [show, setShow] = useState(false);

  const handleDelete = async () => {
    try {
      const result = await getApiRouteForCurrentDB().delete(`/view/${view.name}`);

      if (result.body.error) {
        arangoHelper.arangoError('Failure', `Got unexpected server response: ${result.body.errorMessage}`);
      } else {
        arangoHelper.arangoNotification('Success', `Deleted View: ${view.name}`);
        await mutate('/view');
      }
    } catch (e) {
      arangoHelper.arangoError('Failure', `Got unexpected server response: ${e.message}`);
    }
  };

  return <>
    <IconButton icon={'trash-o'} style={{ background: 'transparent' }} onClick={() => setShow(true)}/>
    <Modal show={show} setShow={setShow} cid={modalCid}>
      <ModalHeader title={`Delete View ${view.name}?`}/>
      <ModalBody>
        <p>
          Are you sure? Clicking on the <b>Delete</b> button will permanently delete the View.
        </p>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Close</button>
        <button className="button-danger" style={{ float: 'right' }}
                onClick={handleDelete}>Delete
        </button>
      </ModalFooter>
    </Modal>
  </>;
};

const ViewButton = ({ view, modalCid }: ButtonProps) => {
  const [show, setShow] = useState(false);
  const [showJsonForm, setShowJsonForm] = useState(true);
  const { data } = useSWR(show
    ? `/view/${view.name}/properties`
    : null, (path) => getApiRouteForCurrentDB().get(path));

  const fullView = data ? omit(data.body, 'error', 'code') : view;

  const state: State<FormState> = {
    formState: fullView as FormState,
    formCache: {},
    show: false,
    showJsonForm,
    lockJsonForm: true,
    renderKey: ''
  };

  let formState = state.formState;
  let jsonFormState = '';
  let jsonRows = 1;
  if (showJsonForm) {
    jsonFormState = JSON.stringify(formState, null, 4);
    jsonRows = jsonFormState.split('\n').length;
  }

  const handleClick = (event: MouseEvent<HTMLElement>) => {
    event.preventDefault();
    setShow(true);
  };

  const toggleJsonForm = () => {
    setShowJsonForm(!showJsonForm);
  };

  return <>
    <IconButton icon={'eye'} onClick={handleClick} style={{ background: 'transparent' }}/>
    <Modal show={show} setShow={setShow} cid={modalCid}>
      <ModalHeader title={formState.name}>
        <button className={'button-info'} onClick={toggleJsonForm} style={{ float: 'right' }}
                disabled={state.lockJsonForm}>
          {showJsonForm ? 'Switch to form view' : 'Switch to code view'}
        </button>
      </ModalHeader>
      <ModalBody maximize={true} show={show}>
        <Grid>
          <Cell size={'1'}>
            <Textarea label={'JSON Dump'} disabled={true} value={jsonFormState} rows={jsonRows}
                      style={{ cursor: 'text' }}/>
          </Cell>
        </Grid>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Close</button>
      </ModalFooter>
    </Modal>
  </>;
};

interface ActionProps {
  view: FormState;
  permission: string;
  modalCidSuffix: string;
}

const Actions = ({ view, permission, modalCidSuffix }: ActionProps) => {
  const isAdminUser = permission === 'rw' || !frontendConfig.authenticationEnabled;

  return <>
    <ViewButton view={view} modalCid={`modal-content-view-${modalCidSuffix}`}/>
    {
      isAdminUser
        ? <>
          &nbsp;
          <DeleteButton view={view} modalCid={`modal-content-delete-${modalCidSuffix}`}/>
        </>
        : null
    }
  </>;
};

export default Actions;
