import { ReactNode, useEffect, useRef } from "react";
import { createPortal, render, unmountComponentAtNode } from "react-dom";
import picoModal from 'picomodal';
import { noop, uniqueId } from 'underscore';

interface ModalProps {
  show: boolean;
  children?: ReactNode;
  toggle: () => void;
}

const Modal = ({ show, toggle, children }: ModalProps) => {
  const contentDivId = useRef(uniqueId('modal-container-'));
  const modal = useRef({ destroy: noop });

  useEffect(() => {
    if (show) {
      modal.current = picoModal({
        content: `<div id='${contentDivId.current}'></div>`,
        closeButton: false
      })
        .afterCreate(() => {
          render(createPortal(children, document.getElementById(contentDivId.current) as HTMLElement),
            document.getElementById(contentDivId.current) as HTMLElement);
        })
        .afterClose(() => {
          unmountComponentAtNode(document.getElementById(contentDivId.current) as HTMLElement);
          modal.current.destroy();
          toggle();
        })
        .show();
    }

    return () => {
      try {
        // eslint-disable-next-line react-hooks/exhaustive-deps
        unmountComponentAtNode(document.getElementById(contentDivId.current) as HTMLElement);
        modal.current.destroy();
      } catch (e) {
      }
    };
  }, [children, show, toggle]);

  return null;
};

export default Modal;
export { default as ModalFooter } from './ModalFooter';
export { default as ModalHeader } from './ModalHeader';
export { default as ModalBody } from './ModalBody';
