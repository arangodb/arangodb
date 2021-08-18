import React, { ReactNode } from 'react';
import styled from 'styled-components';
import { uniqueId } from 'lodash';
import PlainLabel from "./PlainLabel";

const StyledTextarea = styled.textarea`
  width: 90%;
`;

type TextareaProps = {
  id?: string;
  label: ReactNode;
  [key: string]: any;
};

const Textarea = ({ id, label, ...rest }: TextareaProps) => {
  if (!id) {
    id = uniqueId('textbox-');
  }

  return <>
    <PlainLabel htmlFor={id}>{label}</PlainLabel>
    <StyledTextarea id={id} {...rest}/>
  </>;
};

export default Textarea;
