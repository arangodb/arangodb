import React, { ReactNode } from "react";
import { Cell, Grid } from "../pure-css/grid";

interface ModalHeaderProps {
  title: string;
  children?: ReactNode;
}

const ModalHeader = ({ title, children }: ModalHeaderProps) =>
  <>
    <div className="modal-header" style={{ minWidth: '60vw' }}>
      <Grid>
        <Cell size={'1-4'}>
          <span className="arangoHeader">{title}</span>
        </Cell>
        <Cell size={'3-4'}>
          {children}
        </Cell>
      </Grid>
    </div>
    <hr style={{ marginBottom: 0 }}/>
  </>;

export default ModalHeader;
