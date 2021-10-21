import React, { MouseEvent, useState } from 'react';
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../components/modal/Modal";
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import useSWR, { mutate } from "swr";
import { FormState, ViewProperties } from "./constants";
import { State } from '../../utils/constants';
import { Cell, Grid } from "../../components/pure-css/grid";
import Textarea from "../../components/pure-css/form/Textarea";
import { noop, omit } from 'lodash';
import { IconButton } from "../../components/arango/buttons";
import BaseForm from "./forms/BaseForm";
import LinkPropertiesForm from "./forms/LinkPropertiesForm";
import ViewPropertiesForm from "./forms/ViewPropertiesForm";

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
  const [showJsonForm, setShowJsonForm] = useState(false);
  const { data } = useSWR(show
    ? `/view/${view.name}/properties`
    : null, (path) => getApiRouteForCurrentDB().get(path));

  const fullView = data ? omit(data.body, 'error', 'code') : view;

  const state: State<FormState> = {
    formState: fullView as FormState,
    formCache: {},
    show: false,
    showJsonForm,
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

  let formState = state.formState;
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
                <Cell size={'1'}>
                  <fieldset>
                    <legend style={{ fontSize: '12pt' }}>Basic</legend>
                    <BaseForm formState={formState} dispatch={noop} disabled={true}/>
                  </fieldset>
                </Cell>
                <Cell size={'1'}>
                  <fieldset>
                    <legend style={{ fontSize: '12pt' }}>Link Properties</legend>
                    <LinkPropertiesForm formState={formState} dispatch={noop} disabled={true}/>
                  </fieldset>
                </Cell>
                <Cell size={'1'}>
                  <fieldset>
                    <legend style={{ fontSize: '12pt' }}>View Properties</legend>
                    <ViewPropertiesForm formState={formState as ViewProperties} dispatch={noop}
                                        disabled={true}/>
                  </fieldset>
                </Cell>
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
