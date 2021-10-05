import React, { Dispatch, useReducer } from 'react';
import { FormState, ViewProperties } from "./constants";
import { DispatchArgs, State } from '../../utils/constants';
import { cloneDeep, uniqueId } from "lodash";
import { getApiRouteForCurrentDB } from "../../utils/arangoClient";
import { mutate } from "swr";
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../components/modal/Modal";
import { Cell, Grid } from "../../components/pure-css/grid";
import CopyFromInput from "./forms/inputs/CopyFromInput";
import JsonForm from "./forms/JsonForm";
import BaseForm from "./forms/BaseForm";
import LinkPropertiesForm from "./forms/LinkPropertiesForm";
import ViewPropertiesForm from "./forms/ViewPropertiesForm";
import { getReducer } from "../../utils/helpers";

declare var arangoHelper: { [key: string]: any };

const initialFormState: FormState = {
  name: '',
  type: 'arangosearch'
};

const initialState: State<FormState> = {
  formState: cloneDeep(initialFormState),
  formCache: cloneDeep(initialFormState),
  show: false,
  showJsonForm: false,
  lockJsonForm: false,
  renderKey: uniqueId('force_re-render_')
};

interface AddViewProps {
  views: FormState[];
}

const AddView = ({ views }: AddViewProps) => {
  const [state, dispatch] = useReducer(getReducer<FormState>(initialState), initialState);

  const formState = state.formState;

  const handleAdd = async () => {
    try {
      const result = await getApiRouteForCurrentDB().post('/view', formState);

      if (result.body.error) {
        arangoHelper.arangoError('Failure', `Got unexpected server response: ${result.body.errorMessage}`);
      } else {
        arangoHelper.arangoNotification('Success', `Created View: ${result.body.name}`);
        await mutate('/view');

        dispatch({ type: 'reset' });
      }
    } catch (e) {
      arangoHelper.arangoError('Failure', `Got unexpected server response: ${e.message}`);
    }
  };

  const toggleJsonForm = () => {
    dispatch({ type: state.showJsonForm ? 'hideJsonForm' : 'showJsonForm' });
  };

  return <>
    <button className={'pure-button'} onClick={() => dispatch({ type: 'show' })} style={{
      background: 'transparent',
      color: 'white',
      paddingLeft: 0,
      paddingTop: 0
    }}>
      <i className="fa fa-plus-circle"/> Add View
    </button>
    <Modal show={state.show} setShow={(show) => dispatch({ type: show ? 'show' : 'reset' })}
           key={`${views.length}-${state.show}`} cid={'modal-content-add-view'}>
      <ModalHeader title={'Create View'}>
        <Grid>
          <Cell size={'2-3'}>
            <CopyFromInput views={views} dispatch={dispatch}/>
          </Cell>
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
                <JsonForm formState={formState} dispatch={dispatch} renderKey={state.renderKey}/>
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
                    {/* <LinkPropertiesForm formState={formState} dispatch={dispatch}/>*/}
                    <LinkPropertiesForm/>
                  </fieldset>
                </Cell>
                <Cell size={'1'}>
                  <fieldset>
                    <legend style={{ fontSize: '12pt' }}>View Properties</legend>
                    <ViewPropertiesForm formState={formState as ViewProperties}
                                        dispatch={dispatch as Dispatch<DispatchArgs<ViewProperties>>}/>
                  </fieldset>
                </Cell>
              </>
          }
        </Grid>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => dispatch({ type: 'reset' })}>Close</button>
        <button className="button-success" style={{ float: 'right' }} onClick={handleAdd}
                disabled={state.lockJsonForm}>
          Create
        </button>
      </ModalFooter>
    </Modal>
  </>;
};

export default AddView;
