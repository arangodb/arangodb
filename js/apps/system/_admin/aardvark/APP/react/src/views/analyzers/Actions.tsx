import React, { MouseEvent, useState } from 'react';
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../components/modal/Modal";
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { mutate } from "swr";

declare var frontendConfig: { [key: string]: any };
declare var arangoHelper: { [key: string]: any };

const route = getApiRouteForCurrentDB();

interface Analyzer {
  name: string;
}

interface ButtonProps {
  analyzer: Analyzer;
}

const DeleteButton = ({ analyzer }: ButtonProps) => {
  const [showDelete, setShowDelete] = useState(false);
  const [forceDelete, setForceDelete] = useState(false);

  const toggleShow = () => {
    setShowDelete(!showDelete);
  };

  const toggleForce = () => {
    setForceDelete(!forceDelete);
  };

  const handleDelete = async () => {
    toggleShow();

    const result = await route.delete(`/analyzer/${analyzer.name}`, { force: forceDelete });
    if (result.body.error) {
      arangoHelper.arangoError('Failure', `Got unexpected server response: ${result.body.errorMessage}`);
    } else {
      await mutate('/analyzer');
    }
  };

  return <>
    <button className={'pure-button'} onClick={() => setShowDelete(true)}>
      <i className={'fa fa-trash-o'}/>
    </button>
    <Modal show={showDelete} toggle={toggleShow}>
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
        <label htmlFor={'force-delete'}>
          Force Delete: <input id={'force-delete'} type={'checkbox'} checked={forceDelete}
                               onChange={toggleForce} style={{ width: 'auto' }}/>
        </label>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={toggleShow}>Close</button>
        <button id="confirmDeleteBtn" className="button-danger" style={{ float: 'right' }}
                onClick={handleDelete}>Delete
        </button>
      </ModalFooter>
    </Modal>
  </>;
};

const ViewButton = ({ analyzer }: ButtonProps) => {
  const [showView, setShowView] = useState(false);

  const toggle = () => {
    setShowView(!showView);
  };

  const handleClick = (event: MouseEvent<HTMLElement>) => {
    event.preventDefault();

    // console.log(analyzer);
    setShowView(true);
  };

  return <>
    <button className={'pure-button'} onClick={handleClick}>
      <i className={'fa fa-eye'}/>
    </button>
    <Modal show={showView} toggle={toggle}>
      <span>Are you sure?! {analyzer.name}</span>
    </Modal>
  </>;
};

interface ActionProps {
  analyzer: Analyzer;
  permission: string;
}

const Actions = ({ analyzer, permission }: ActionProps) => {
  const isUserDefined = analyzer.name.includes('::');
  const isSameDB = isUserDefined
    ? analyzer.name.split('::')[0] === frontendConfig.db
    : frontendConfig.db === '_system';
  const isAdminUser = permission === 'rw';
  const canDelete = isUserDefined && isSameDB && isAdminUser;

  return <>
    <ViewButton analyzer={analyzer}/>
    {canDelete ? <>&nbsp;<DeleteButton analyzer={analyzer}/></> : null}
  </>;
};

export default Actions;
