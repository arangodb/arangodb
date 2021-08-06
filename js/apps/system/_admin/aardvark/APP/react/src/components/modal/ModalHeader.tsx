import React, { ReactNode } from "react";

interface ModalHeaderProps {
  title: string;
  children?: ReactNode;
}

const ModalHeader = ({ title, children }: ModalHeaderProps) =>
  <>
    <div className="modal-header">
      <div className={'pure-g'}>
        <div className={'pure-u-8-24 pure-u-md-8-24 pure-u-lg-8-24 pure-u-xl-8-24'}>
          <span className="arangoHeader">{title}</span>
        </div>
        <div className={'pure-u-16-24 pure-u-md-16-24 pure-u-lg-16-24 pure-u-xl-16-24'}>
          {children}
        </div>
      </div>
    </div>
    <hr style={{ marginBottom: 0 }}/>
  </>;

export default ModalHeader;
