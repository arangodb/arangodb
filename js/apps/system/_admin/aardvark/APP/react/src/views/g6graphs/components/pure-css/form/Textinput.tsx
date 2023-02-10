import React, { ReactNode, useEffect, useState } from 'react';
import styled from 'styled-components';
import { uniqueId } from 'lodash';
import PlainLabel from "./PlainLabel";

type StyledTextinputProps = {
  type: 'text' | 'number' | 'color';
  [key: string]: any;
};

const StyledTextinput = styled.input.attrs(({ type, width, height, disabled, style, ...rest }: StyledTextinputProps) => ({
  type,
  ...rest
}))`
  &&& {
    width: ${props => props.width || '50%'};
    height: ${props => props.height || 'auto'};
    margin-right: 8px;
    color: #555555;
    border: 2px solid rgba(140, 138, 137, 0.25);
    border-radius: 4px;
    background-color: #fff;
    box-shadow: none;
    outline: none;
    outline-color: transparent;
    outline-style: none;
  }
  &:disabled {
    background-color: #eeeeee !important;
  }
`;

type TextinputProps = {
  id?: string;
  label?: ReactNode;
  disabled?: boolean;
  style?: string;
  width?: string;
  height?: string;
} & StyledTextinputProps;

const Textinput = ({ id, label, disabled, style, width, height, ...rest }: TextinputProps) => {
  const [thisId, setThisId] = useState(id || uniqueId('textinput-'));

  useEffect(() => {
    if (id) {
      setThisId(id);
    }
  }, [id]);


  if (style === "graphviewer") {
    return <>
      {label ? <div style={{ 'color': '#ffffff', 'width': '150px' }}>{label}</div> : null}
      <StyledTextinput id={thisId} disabled={disabled} width={width} height={height} {...rest} />
    </>;
  }

  return <>
    {label ? <PlainLabel htmlFor={thisId}>{label}</PlainLabel> : null}
    <StyledTextinput id={thisId} disabled={disabled} width={width} height={height} {...rest} />
  </>;
};

export default Textinput;
