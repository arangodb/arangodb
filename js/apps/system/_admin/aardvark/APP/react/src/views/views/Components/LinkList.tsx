import React, { MouseEventHandler, useContext, useEffect, useState } from "react";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../components/arango/table";
import { IconButton } from "../../../components/arango/buttons";
import Link from "./Link";
import { ViewContext } from "../ViewLinksReactView";
import { FormState } from "../constants";
import { handleSave } from "../Actions";
import Modal, { ModalFooter, ModalHeader } from "../../../components/modal/Modal";

type DeleteButtonProps = {
  collection: string;
  modalCid: string;
  setLinkRemoved: (val: boolean) => void;
};

const DeleteButton = ({ collection, modalCid, setLinkRemoved }: DeleteButtonProps) => {
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
    setLinkRemoved(true);
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

type CollProps = {
  formState: FormState;
  addClick: MouseEventHandler<HTMLElement>;
  viewLink: (link: {} | []) => void;
  icon: string;
};

const LinkList = ({ formState, addClick, icon, viewLink }: CollProps) => {
  const [linkRemoved, setLinkRemoved] = useState(false);
  const links = formState.links;
  const checkLinks = (links: any) => {
    let linksArr = [];
    if (links) {
      for (const l in links) {
        linksArr.push({
          name: l,
          link: links[l]
        });
      }
    }
    return linksArr;
  };

  const linksArr = checkLinks(links);

  useEffect(() => {
    if (linkRemoved) {
      handleSave({
        view: formState,
        oldName: formState.name
      }).then(() => setLinkRemoved(false));
    }
  }, [formState, linkRemoved]);

  return (
    <div className="contentIn" id="indexHeaderContent">
      <ArangoTable className={"edit-index-table arango-table"}>
        <thead>
        <tr className="figuresHeader">
          <ArangoTH seq={0}>Link Name</ArangoTH>
          <ArangoTH seq={1}>Properties</ArangoTH>
          <ArangoTH seq={1}>Root Analyzers</ArangoTH>
          <ArangoTH seq={3}>Action</ArangoTH>
        </tr>
        </thead>

        <tbody>
        {links && linksArr.filter(p => p.link !== null)
          .map((p, key) => (
            <Link
              name={p.name}
              analyzers={p.link.analyzers}
              includeAllFields={p.link.includeAllFields}
              action={
                <>
                  <DeleteButton collection={p.name} modalCid={`modal-content-view-${key}`}
                                setLinkRemoved={setLinkRemoved}/>
                  <IconButton
                    icon={"eye"}
                    type={"warning"}
                    onClick={() => viewLink(p.name)}
                  />
                </>
              }
              key={key}
              linkKey={key}
            />
          ))}
        </tbody>
        <tfoot>
        <tr>
          <ArangoTD seq={0}> </ArangoTD>
          <ArangoTD seq={1}> </ArangoTD>
          <ArangoTD seq={2}> </ArangoTD>
          <ArangoTD seq={3}>
            <i className={`fa ${icon}`} onClick={addClick}/>
          </ArangoTD>
        </tr>
        </tfoot>
      </ArangoTable>

      <div id="modal-dialog">
        <div className="modal-footer" style={{ border: "none" }}/>
      </div>
    </div>
  );
};

export default LinkList;
