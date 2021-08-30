import { CSSProperties, ReactNode, useEffect, useRef } from "react";
import { createPortal, render, unmountComponentAtNode } from "react-dom";
import picoModal from 'picomodal';
import { uniqueId } from 'lodash';

interface ModalProps {
  show: boolean;
  children?: ReactNode;
  setShow: (show: boolean) => void;
}

function renderChildren (children: ReactNode, contentDivId: string) {
  const contentDiv = document.getElementById(contentDivId) as HTMLElement;

  render(createPortal(children, contentDiv, contentDivId), contentDiv);
}

const Modal = ({ show, setShow, children }: ModalProps) => {
  const contentDivId = useRef(uniqueId('modal-content-'));
  const modal = useRef(picoModal({
    content: `<div id='${contentDivId.current}'></div>`,
    closeButton: false,
    modalStyles: (styles: CSSProperties) => {
      styles.zIndex = 9000;
    },
    overlayStyles: (styles: CSSProperties) => {
      styles.zIndex = 8999;
    }
  })
    .afterCreate(() => {
      renderChildren(children, contentDivId.current); // First render of children.
    })
    .afterClose(() => {
      setShow(false);
    }));

  useEffect(() => {
    if (modal.current.isVisible()) {
      renderChildren(children, contentDivId.current); // Re-render children on any child update. Important!
    }
  }, [children]);

  useEffect(() => {
    if (show) {
      modal.current.show();
    } else {
      modal.current.close();
    }
  }, [show]);

  useEffect(() => {
    return () => {
      try {
        // eslint-disable-next-line react-hooks/exhaustive-deps
        unmountComponentAtNode(document.getElementById(contentDivId.current) as HTMLElement);
        // eslint-disable-next-line react-hooks/exhaustive-deps
        modal.current.destroy();
      } catch (e) {
      }
    };
  }, []);

  return null;
};

export default Modal;
export { default as ModalFooter } from './ModalFooter';
export { default as ModalHeader } from './ModalHeader';
export { default as ModalBody } from './ModalBody';
