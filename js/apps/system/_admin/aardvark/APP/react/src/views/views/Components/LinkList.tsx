import React, { useContext, useState } from "react";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../components/arango/table";
import { IconButton } from "../../../components/arango/buttons";
import { FormState, ViewContext, ViewProps } from "../constants";
import Modal, { ModalFooter, ModalHeader } from "../../../components/modal/Modal";
import { Link as HashLink, useRouteMatch } from 'react-router-dom';
import { SaveButton } from "../Actions";
import { chain, size } from "lodash";

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
  const validLinks = chain(formState.links || {}).toPairs().filter(pair => pair[1] !== null).value();

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
            size(validLinks)
              ? validLinks.map(pair =>
                <tr key={pair[0]}>
                  <ArangoTD seq={0}>{pair[0]}</ArangoTD>
                  <ArangoTD seq={1}>
                    <DeleteButton collection={pair[0]} modalCid={`modal-content-view-${pair[0]}`}/>
                    <HashLink to={`/${pair[0]}`}>
                      <IconButton icon={"edit"} type={"warning"}/>
                    </HashLink>
                  </ArangoTD>
                </tr>
              )
              : <tr>
                <ArangoTD seq={0} colSpan={2}>No links.</ArangoTD>
              </tr>
          }
          </tbody>
          {
            isAdminUser
              ? <tfoot>
              <tr>
                <ArangoTD seq={0} colSpan={2}>
                  <HashLink to={`${match.url}_add`}>
                    <i className={'fa fa-plus-circle'}/>
                  </HashLink>
                </ArangoTD>
              </tr>
              </tfoot>
              : null
          }
        </ArangoTable>
        {
          isAdminUser && changed
            ? <SaveButton view={formState} oldName={name} setChanged={setChanged}/>
            : null
        }
      </div>
    </div>
  );
};

export default LinkList;
