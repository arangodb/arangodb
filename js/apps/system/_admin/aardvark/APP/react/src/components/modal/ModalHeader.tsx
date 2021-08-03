import React from "react";

interface ModalHeaderProps {
  title: string;
}

const ModalHeader = ({ title }: ModalHeaderProps) =>
  <>
    <div className="modal-header">
      <span className="arangoHeader">{title}</span>
    </div>
    <hr/>
  </>;

export default ModalHeader;
