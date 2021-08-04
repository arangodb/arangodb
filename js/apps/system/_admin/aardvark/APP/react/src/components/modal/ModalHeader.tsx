import React, { ReactNode } from "react";

interface ModalHeaderProps {
  title: string;
  children?: ReactNode;
}

const ModalHeader = ({ title, children }: ModalHeaderProps) =>
  <>
    <div className="modal-header">
      <span className="arangoHeader">{title}</span>
      {children}
    </div>
    <hr style={{ marginBottom: 0 }}/>
  </>;

export default ModalHeader;
