import React, { ReactNode } from "react";

interface ModalBodyProps {
  children?: ReactNode;
}

const ModalBody = ({ children }: ModalBodyProps) =>
  <div className="modal-body" style={{
    maxHeight: '90vh',
    overflowX: 'hidden'
  }}>
    {children}
  </div>;

export default ModalBody;
