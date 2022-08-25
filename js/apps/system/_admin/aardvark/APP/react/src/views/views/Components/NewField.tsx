import React, { ChangeEvent, MouseEvent, useContext, useState } from "react";
import { ViewContext } from "../constants";
import { useRouteMatch } from "react-router-dom";
import { useLinkState } from "../helpers";
import Textbox from "../../../components/pure-css/form/Textbox";
import Modal, { ModalBody, ModalFooter, ModalHeader } from "../../../components/modal/Modal";
import ToolTip from "../../../components/arango/tootip";

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
    <ToolTip title="Add field" setArrow={true}>
      <button className={'btn btn-link'} onClick={handleClick}>
        <i className="fa fa-plus-circle" id="addField"/>
      </button>
    </ToolTip>
    <Modal show={show} setShow={setShow} cid={'add-field'}>
      <ModalHeader title={'Add Field'} width={600} minWidth={'unset'}/>
      <ModalBody show={show} innerStyle={{
        minWidth: 'unset'
      }}>
        <Textbox placeholder={'Field'} type={'text'} value={newField} onChange={updateNewField}/>
        {newField && addDisabled ? <div style={{ color: 'red' }}>This field already exists</div> : null}
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Close</button>
        <button className="button-success" style={{ float: 'right' }} onClick={addField}>Add</button>
      </ModalFooter>
    </Modal>
  </>;
};

export default NewField;
