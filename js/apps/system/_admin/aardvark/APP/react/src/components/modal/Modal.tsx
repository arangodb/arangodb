import { ReactNode, useEffect, useRef } from "react";
import { createPortal, render, unmountComponentAtNode } from "react-dom";
import picoModal from 'picomodal';
import { uniqueId } from 'underscore';

interface ModalProps {
  show: boolean;
  children?: ReactNode;
  setShow: (show: boolean) => void;
}

const Modal = ({ show, setShow, children }: ModalProps) => {
  const contentDivId = useRef(uniqueId('modal-content-'));
  const modal = useRef(picoModal({
    content: `<div id='${contentDivId.current}'></div>`,
    closeButton: false
  })
    .afterCreate(() => {
      render(createPortal(children, document.getElementById(contentDivId.current) as HTMLElement, contentDivId.current),
        document.getElementById(contentDivId.current) as HTMLElement);
    }));

  useEffect(() => {
    if (modal.current.isVisible()) {
      render(createPortal(children, document.getElementById(contentDivId.current) as HTMLElement, contentDivId.current),
        document.getElementById(contentDivId.current) as HTMLElement);
    }
  }, [children]);

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

  useEffect(() => {
    if (show) {
      modal.current.show();
    } else {
      modal.current.close();
      setShow(false);
    }
  }, [setShow, show]);

  return null;
};

export default Modal;
export { default as ModalFooter } from './ModalFooter';
export { default as ModalHeader } from './ModalHeader';
export { default as ModalBody } from './ModalBody';
