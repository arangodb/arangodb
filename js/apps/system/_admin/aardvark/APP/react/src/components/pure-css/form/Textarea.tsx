import React, { ReactNode } from 'react';
import styled from 'styled-components';
import { uniqueId } from 'lodash';
import PlainLabel from "./PlainLabel";

const StyledTextarea = styled.textarea`
  width: 90%;
  resize: vertical;
`;

type TextareaProps = {
  id?: string;
  label: ReactNode;
  disabled?: boolean;
  [key: string]: any;
};

const Textarea = ({ id, label, disabled, ...rest }: TextareaProps) => {
  if (!id) {
    id = uniqueId('textbox-');
  }

  return <>
    <PlainLabel htmlFor={id}>{label}</PlainLabel>
    <StyledTextarea id={id} disabled={disabled} {...rest}/>
  </>;
};

export default Textarea;
