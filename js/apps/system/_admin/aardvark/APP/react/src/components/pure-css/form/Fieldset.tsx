import React, { ReactNode } from "react";
import styled from "styled-components";

const StyledLegend = styled.legend`
  font-size: 14px;
  border-bottom: none;
  line-height: normal;
  color: inherit;
  margin: 12px 12px 12px 0;
  width: fit-content;
`;

type FieldsetProps = {
  children: ReactNode;
  legend?: string;
  [key: string]: any;
};

const Fieldset = ({ children, legend, ...rest }: FieldsetProps) => (
  <fieldset {...rest}>
    {legend ? <StyledLegend>{legend}</StyledLegend> : null}
    {children}
  </fieldset>
);

export default Fieldset;
