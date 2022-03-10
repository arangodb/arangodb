import React, { ReactNode } from "react";
import { Cell, Grid } from "../pure-css/grid";

interface ModalHeaderProps {
  title: string;
  children?: ReactNode;
  minWidth?: string;
}

const ModalHeader = ({ title, children, minWidth = '60vw' }: ModalHeaderProps) =>
  <>
    <div className="modal-header" style={{ minWidth }}>
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
