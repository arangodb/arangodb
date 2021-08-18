import React, { ReactNode } from "react";
import { Cell, Grid } from "styled-css-grid";

interface ModalHeaderProps {
  title: string;
  children?: ReactNode;
}

const ModalHeader = ({ title, children }: ModalHeaderProps) =>
  <>
    <div className="modal-header">
      <Grid columns={3}>
        <Cell width={1}>
          <span className="arangoHeader">{title}</span>
        </Cell>
        <Cell width={2}>
          {children}
        </Cell>
      </Grid>
    </div>
    <hr style={{ marginBottom: 0 }}/>
  </>;

export default ModalHeader;
