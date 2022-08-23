import React, { ChangeEvent, MouseEvent, useContext, useState } from "react";
import { ViewContext } from "../constants";
import { useRouteMatch } from "react-router-dom";
import { useLinkState } from "../helpers";
import { IconButton } from "../../../components/arango/buttons";
import Textbox from "../../../components/pure-css/form/Textbox";
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../../components/modal/Modal";

const NewField = () => {
  const [show, setShow] = useState(false);
  const { dispatch, formState } = useContext(ViewContext);
  const match = useRouteMatch();

  const fragments = match.url.slice(1).split('/');
  let basePath = `links[${fragments[0]}]`;
  for (let i = 1; i < fragments.length; ++i) {
    basePath += `.fields[${fragments[i]}]`;
  }

  const [newField, setNewField, addDisabled] = useLinkState(formState, `${basePath}.fields`);

  const addField = () => {
    dispatch({
      type: "setField",
      field: {
        path: `fields[${newField}]`,
        value: {}
      },
      basePath
    });
    setShow(false);
    setNewField('');
  };

  const updateNewField = (event: ChangeEvent<HTMLInputElement>) => {
    setNewField(event.target.value);
  };

  const handleClick = (event: MouseEvent<HTMLElement>) => {
    event.preventDefault();
    setShow(true);
  };

  return <>
    <IconButton icon={'plus'} onClick={handleClick} type={'warning'}>Add Field</IconButton>
    <Modal show={show} setShow={setShow} cid={'add-field'}>
      <ModalHeader title={'Add Field'}/>
      <ModalBody show={show}>
        <Textbox placeholder={'Field'} type={'text'} value={newField} onChange={updateNewField}/>
        { newField && addDisabled ? <div style={{ color: 'red' }}>This field already exists</div> : null }
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Close</button>
        <button className="button-success" style={{ float: 'right' }} onClick={addField}>Add</button>
      </ModalFooter>
    </Modal>
  </>;
};

export default NewField;
