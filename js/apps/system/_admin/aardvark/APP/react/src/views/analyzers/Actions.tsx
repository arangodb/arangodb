import { pick } from 'lodash';
import React, { MouseEvent, useState } from 'react';
import { mutate } from "swr";
import { IconButton } from "../../components/arango/buttons";
import { Modal, ModalBody, ModalFooter, ModalHeader } from "../../components/modal";
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { State } from '../../utils/constants';
import { userIsAdmin } from '../../utils/usePermissions';
import { FormState } from "./constants";
import { ViewAnalyzerModal } from './ViewAnalyzerModal';

type ButtonProps = {
  analyzer: FormState;
  modalCid: string;
}

const DeleteButton = ({ analyzer }: ButtonProps) => {
  const [show, setShow] = useState(false);
  const [forceDelete, setForceDelete] = useState(false);

  const toggleForce = () => {
    setForceDelete(!forceDelete);
  };

  const handleDelete = async () => {
    try {
      const result = await getApiRouteForCurrentDB().delete(`/analyzer/${analyzer.name}`, { force: forceDelete });

      if (result.body.error) {
        window.arangoHelper.arangoError('Failure', `Got unexpected server response: ${result.body.errorMessage}`);
      } else {
        window.arangoHelper.arangoNotification('Success', `Deleted Analyzer: ${analyzer.name}`);
        await mutate('/analyzer');
      }
    } catch (e: any) {
      window.arangoHelper.arangoError('Failure', `Got unexpected server response: ${e.message}`);
    }
  };

  return <>
    <IconButton icon={'trash-o'} style={{ background: 'transparent' }} onClick={() => setShow(true)} />
    <Modal isOpen={show} onClose={() => setShow(false)}>
      <ModalHeader>
        Delete Analyzer {analyzer.name}?
      </ModalHeader>
      <ModalBody>
        <p>
          This Analyzer might be in use. Deleting it will impact the Views using it. Clicking on
          the <b>Delete</b> button will only Delete the Analyzer if it is not in use.
        </p>
        <p>
          Select the <b>Force Delete</b> option below if you want to delete the Analyzer even if it is being
          used.
        </p>
        <br />
        <label htmlFor={'force-delete'} className="pure-checkbox">
          <input id={'force-delete'} type={'checkbox'} checked={forceDelete} onChange={toggleForce}
            style={{ width: 'auto' }} /> Force Delete
        </label>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Close</button>
        <button className="button-danger" style={{ float: 'right' }} onClick={handleDelete}>Delete</button>
      </ModalFooter>
    </Modal>
  </>;
};

const ViewButton = ({ analyzer }: ButtonProps) => {
  const [show, setShow] = useState(false);
  const [showJsonForm, setShowJsonForm] = useState(false);

  const state: State<FormState> = {
    formState: pick(analyzer, 'name', 'type', 'features', 'properties') as FormState,
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

  const formState = state.formState;
  let jsonFormState = '';
  let jsonRows = 1;
  if (showJsonForm) {
    jsonFormState = JSON.stringify(formState, null, 4);
    jsonRows = jsonFormState.split('\n').length;
  }

  return (
    <>
      <IconButton
        icon={"eye"}
        onClick={handleClick}
        style={{ background: "transparent" }}
      />
      <ViewAnalyzerModal
        isOpen={show}
        onClose={() => setShow(false)}
        formState={formState}
        toggleJsonForm={toggleJsonForm}
        showJsonForm={showJsonForm}
        jsonFormState={jsonFormState}
        jsonRows={jsonRows}
      />
    </>
  );
};

interface ActionProps {
  analyzer: FormState;
  permission: string;
  modalCidSuffix: string;
}

const Actions = ({ analyzer, permission, modalCidSuffix }: ActionProps) => {
  const isUserDefined = analyzer.name.includes('::');
  const isSameDB = isUserDefined
    ? analyzer.name.split('::')[0] === window.frontendConfig.db
    : window.frontendConfig.db === '_system';
  const isAdminUser = userIsAdmin(permission);
  const canDelete = isUserDefined && isSameDB && isAdminUser;

  return <>
    <ViewButton analyzer={analyzer} modalCid={`modal-content-view-${modalCidSuffix}`} />
    {
      canDelete
        ? <>
          &nbsp;
          <DeleteButton analyzer={analyzer} modalCid={`modal-content-delete-${modalCidSuffix}`} />
        </>
        : null
    }
  </>;
};

export default Actions;
