import React, { ReactNode } from "react";

interface ModalFooterProps {
  children?: ReactNode;
}

const ModalFooter = ({ children }: ModalFooterProps) =>
  <>
    <hr style={{ marginTop: 0 }}/>
    <div className="modal-footer">
      {children}
    </div>
  </>;

export default ModalFooter;
