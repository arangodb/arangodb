import React, { ReactNode } from "react";

interface ModalBodyProps {
  children?: ReactNode;
}

const ModalBody = ({ children }: ModalBodyProps) =>
  <div className="modal-body" style={{ minWidth: '50vw' }}>
    {children}
  </div>;

export default ModalBody;
