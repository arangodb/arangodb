import React, { Dispatch, MouseEvent, useEffect, useReducer, useRef, useState } from 'react';
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../components/modal/Modal";
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import useSWR, { mutate } from "swr";
import { FormState, ViewProperties } from "./constants";
import { DispatchArgs, State } from '../../utils/constants';
import { Cell, Grid } from "../../components/pure-css/grid";
import Textarea from "../../components/pure-css/form/Textarea";
import { cloneDeep, noop, omit, pick } from 'lodash';
import { IconButton } from "../../components/arango/buttons";
import BaseForm from "./forms/BaseForm";
import LinkPropertiesForm from "./forms/LinkPropertiesForm";
import ViewPropertiesForm from "./forms/ViewPropertiesForm";
import { getReducer } from "../../utils/helpers";
import JsonForm from "./forms/JsonForm";

declare var frontendConfig: { [key: string]: any };
declare var arangoHelper: { [key: string]: any };

type ButtonProps = {
  view: FormState;
  modalCid: string;
}

const EditButton = ({ view, modalCid }: ButtonProps) => {
  const initialState = useRef<State<FormState>>({
    formState: cloneDeep(view) as FormState,
    formCache: cloneDeep(view),
    show: false,
    showJsonForm: false,
    lockJsonForm: false,
    renderKey: ''
  });
  const [state, dispatch] = useReducer(getReducer<FormState>(initialState.current), initialState.current);
  const { data } = useSWR(state.show
    ? `/view/${view.name}/properties`
    : null, (path) => getApiRouteForCurrentDB().get(path));
  const cacheRef = useRef({});

  useEffect(() => {
    const fullView = data
      ? omit(data.body, 'error', 'code', 'id', 'globallyUniqueId', 'minScore', 'consolidationPolicy.minScore')
      : view;

    initialState.current.formCache = cloneDeep(fullView);

    dispatch({
      type: 'setFormState',
      formState: cloneDeep(fullView) as FormState
    });
  }, [data, view]);

  const toggleJsonForm = () => {
    dispatch({ type: state.showJsonForm ? 'hideJsonForm' : 'showJsonForm' });
  };

  const formState = state.formState;

  const handleEdit = async () => {
    const route = getApiRouteForCurrentDB();
    let result;
    let error = false;

    try {
      if (formState.name !== view.name) {
        result = await route.put(`/view/${view.name}/rename`, {
          name: formState.name
        });

        if (result.body.error) {
          arangoHelper.arangoError('Failure', `Got unexpected server response: ${result.body.errorMessage}`);
          error = true;
        }
      }

      if (!error) {
        const properties = pick(formState, 'consolidationIntervalMsec', 'commitIntervalMsec',
          'cleanupIntervalStep', 'links', 'consolidationPolicy');
        result = await route.patch(`/view/${formState.name}/properties`, properties);

        if (result.body.error) {
          arangoHelper.arangoError('Failure', `Got unexpected server response: ${result.body.errorMessage}`);
        } else {
          arangoHelper.arangoNotification('Success', `Updated View: ${result.body.name}`);
          await mutate('/view');

          dispatch({ type: 'reset' });
        }
      }
    } catch (e) {
      arangoHelper.arangoError('Failure', `Got unexpected server response: ${e.message}`);
    }
  };

  return <>
    <IconButton icon={'pencil'} style={{ background: 'transparent' }}
                onClick={() => dispatch({ type: 'show' })}/>
    <Modal show={state.show} setShow={(show) => dispatch({ type: show ? 'show' : 'reset' })} cid={modalCid}>
      <ModalHeader title={'Edit View'} minWidth={'90vw'}>
        <Grid>
          <Cell size={'2-3'}/>
          <Cell size={'1-3'}>
            <button className={'button-info'} onClick={toggleJsonForm} disabled={state.lockJsonForm}
                    style={{
                      float: 'right'
                    }}>
              {state.showJsonForm ? 'Switch to form view' : 'Switch to code view'}
            </button>
          </Cell>
        </Grid>
      </ModalHeader>
      <ModalBody maximize={true} show={state.show}>
        <Grid>
          {
            state.showJsonForm
              ? <Cell size={'1'}>
                <JsonForm formState={formState} dispatch={dispatch} renderKey={state.renderKey} isEdit={true}/>
              </Cell>
              : <>
                <Cell size={'1'}>
                  <fieldset>
                    <legend style={{ fontSize: '12pt' }}>Basic</legend>
                    <BaseForm formState={formState} dispatch={dispatch}/>
                  </fieldset>
                </Cell>
                <Cell size={'1'}>
                  <fieldset>
                    <legend style={{ fontSize: '12pt' }}>Link Properties</legend>
                    <LinkPropertiesForm formState={formState} dispatch={dispatch} cache={cacheRef.current}/>
                  </fieldset>
                </Cell>
                <Cell size={'1'}>
                  <fieldset>
                    <legend style={{ fontSize: '12pt' }}>View Properties</legend>
                    <ViewPropertiesForm formState={formState as ViewProperties} isEdit={true}
                                        dispatch={dispatch as Dispatch<DispatchArgs<ViewProperties>>}/>
                  </fieldset>
                </Cell>
              </>
          }
        </Grid>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => dispatch({ type: 'reset' })}>Close</button>
        <button className="button-warning" style={{ float: 'right' }} onClick={handleEdit}
                disabled={state.lockJsonForm}>
          Update
        </button>
      </ModalFooter>
    </Modal>
  </>;
};

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
      <ModalHeader title={formState.name} minWidth={'90vw'}>
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
          &nbsp;
          <EditButton view={view} modalCid={`modal-content-edit-${modalCidSuffix}`}/>
        </>
        : null
    }
  </>;
};

export default Actions;
