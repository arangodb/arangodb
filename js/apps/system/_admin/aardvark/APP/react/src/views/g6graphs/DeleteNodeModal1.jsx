/* global arangoHelper, arangoFetch, frontendConfig */

import React, { ChangeEvent, useEffect, useState, forwardRef, useRef, useImperativeHandle } from 'react';
import Modal, { ModalBody, ModalFooter, ModalHeader } from '../../components/modal/Modal';
import { QuestionCircleOutlined } from '@ant-design/icons';
import { Cell, Grid } from "../../components/pure-css/grid";
import Textbox from "../../components/pure-css/form/Textbox";
import Checkbox from "../../components/pure-css/form/Checkbox";

export const DeleteNodeModal = ({ node, show, setShow }) => {
  //const [show, setShow] = useState(false);
  const [nodeId, setNodeId] = useState(node);

  useEffect(() => {
    console.log(`nodeId changed to ${nodeId}`);
  }, [nodeId]);

  const showDeleteNodeModal = (event) => {
    event.preventDefault();
    setShow(true);
  };

  const handleDelete = async () => {
    console.log("Delete node");
    /*
    try {
      const result = await getApiRouteForCurrentDB().post('/analyzer/', formState);

      if (result.body.error) {
        arangoHelper.arangoError('Failure', `Got unexpected server response: ${result.body.errorMessage}`);
      } else {
        arangoHelper.arangoNotification('Success', `Created Analyzer: ${result.body.name}`);
        await mutate('/analyzer');

        dispatch({ type: 'reset' });
      }
    } catch (e) {
      arangoHelper.arangoError('Failure', `Got unexpected server response: ${e.message}`);
    }
    */
  };

  return <>
    <a href={'#g6'} onClick={showDeleteNodeModal} className="pull-right">
      <QuestionCircleOutlined />
    </a>
    <Modal show={show} setShow={setShow}>
      <ModalHeader title={'Delete node'}/>
      <ModalBody>
        <Grid>
          <Cell size={'1-2'}>
            <strong>Node name: </strong> {nodeId}
          </Cell>
          <Cell size={'1-2'}>
            <Checkbox label={'Also delete edges?'} disabled={false} />
          </Cell>
        </Grid>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Cancel</button>
        <button className="button-success" style={{ float: 'right' }} onClick={handleDelete}>Delete</button>
      </ModalFooter>
    </Modal>
  </>;
};
