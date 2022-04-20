import React, { ReactNode } from "react";
import styled from "styled-components";

const StyledLegend = styled.legend`
  font-size: 14px;
  border-bottom: none;
  line-height: normal;
  color: inherit;
  margin: 12px;
`;

type FieldsetProps = {
  children: ReactNode;
  legend?: string;
};

const Fieldset = ({ children, legend }: FieldsetProps) => (
  <fieldset>
    {legend ? <StyledLegend>{legend}</StyledLegend> : null}
    {children}
  </fieldset>
);

export default Fieldset;
