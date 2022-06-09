import React, { useContext, useState } from "react";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../components/arango/table";
import { IconButton } from "../../../components/arango/buttons";
import { FormState, ViewContext, ViewProps } from "../constants";
import Modal, { ModalFooter, ModalHeader } from "../../../components/modal/Modal";
import { Link as HashLink, useRouteMatch } from 'react-router-dom';
import { SaveButton } from "../Actions";
import { chain } from "lodash";

type DeleteButtonProps = {
  collection: string;
  modalCid: string;
};

const DeleteButton = ({ collection, modalCid }: DeleteButtonProps) => {
  const [show, setShow] = useState(false);
  const { dispatch } = useContext(ViewContext);

  const removeLink = async () => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${collection}]`,
        value: null
      }
    });
  };

  return <>
    <IconButton icon={'trash-o'} type={'danger'} onClick={() => setShow(true)}/>
    <Modal show={show} setShow={setShow} cid={modalCid}>
      <ModalHeader title={`Delete Link "${collection}"?`}/>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Close</button>
        <button className="button-danger" style={{ float: 'right' }} onClick={removeLink}>Delete</button>
      </ModalFooter>
    </Modal>
  </>;
};

const LinkList = ({ name }: ViewProps) => {
  const { formState: fs, isAdminUser, changed, setChanged } = useContext(ViewContext);
  const match = useRouteMatch();

  const formState = fs as FormState;

  return (
    <div id="modal-dialog">
      <div className="modal-body">
        <ArangoTable className={"edit-index-table arango-table"}>
          <thead>
          <tr className="figuresHeader">
            <ArangoTH seq={0}>Collection</ArangoTH>
            <ArangoTH seq={1}>Actions</ArangoTH>
          </tr>
          </thead>

          <tbody>
          {
            chain(formState.links || {}).toPairs().filter(pair => pair[1] !== null).map((pair, key) =>
              <tr key={key}>
                <ArangoTD seq={0}>{pair[0]}</ArangoTD>
                <ArangoTD seq={1}>
                  <DeleteButton collection={pair[0]} modalCid={`modal-content-view-${key}`}/>
                  <HashLink to={`/${pair[0]}`}>
                    <IconButton icon={"eye"} type={"warning"}/>
                  </HashLink>
                </ArangoTD>
              </tr>
            ).value()
          }
          </tbody>
          <tfoot>
          <tr>
            <ArangoTD seq={0}> </ArangoTD>
            <ArangoTD seq={1}>
              <HashLink to={`${match.path}_add`}>
                <i className={'fa fa-plus-circle'}/>
              </HashLink>
            </ArangoTD>
          </tr>
          </tfoot>
        </ArangoTable>
      </div>
      {
        isAdminUser && changed
          ? <div className="modal-footer">
            <SaveButton view={formState} oldName={name} setChanged={setChanged}/>
          </div>
          : null
      }
    </div>
  );
};

export default LinkList;
