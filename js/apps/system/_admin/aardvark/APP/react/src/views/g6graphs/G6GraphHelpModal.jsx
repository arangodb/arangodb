/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useState, forwardRef, useRef, useImperativeHandle } from 'react';
import Modal, { ModalBody, ModalFooter, ModalHeader } from '../../components/modal/Modal';
import { QuestionCircleOutlined } from '@ant-design/icons';

export const G6GraphHelpModal = forwardRef((props, ref) => {
  const [show, setShow] = useState(false);

  const showG6GraphHelp = (event) => {
    event.preventDefault();
    setShow(true);
  };

  useImperativeHandle(ref, () => ({
    showG6GraphHelpFromParent(somedata) {
      console.log('somedata: ', somedata);
      setShow(true);
      //alert("Hello from Child Component")
    },
  }));

  return <>
    <a href={'#g6'} onClick={showG6GraphHelp} className="pull-right">
      <QuestionCircleOutlined />
    </a>
    <Modal show={show} setShow={setShow}>
      <ModalHeader title={'Graph Viewer Help'}/>
      <ModalBody>
        <dl>
          <dt>Graphs</dt>
          <dd>
            If you are new to graphs and displaying graph data... have a look at our <strong><a target={'_blank'} rel={'noreferrer'} href={'https://www.arangodb.com/docs/stable/graphs.html'}>graph documentation</a></strong>.
          </dd>
          <dt>Graph Course</dt>
          <dd>
            We also offer a course with the title <strong><a target={'_blank'} rel={'noreferrer'} href={'https://www.arangodb.com/learn/graphs/graph-course/'}>Get Started with Graphs in ArangoDB</a></strong> for free.
          </dd>
          <dt>Quick Start</dt>
          <dd>
            To start immediately, the explanationn of our <strong><a target={'_blank'} rel={'noreferrer'} href={'https://www.arangodb.com/docs/stable/graphs.html#example-graphs'}>example graphs</a></strong> might help very well.
          </dd>
        </dl>
      </ModalBody>
      <ModalFooter>
        <button className="button-close" onClick={() => setShow(false)}>Close</button>
      </ModalFooter>
    </Modal>
  </>;
});
