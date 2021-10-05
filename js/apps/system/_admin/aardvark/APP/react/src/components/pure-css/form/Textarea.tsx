import React, { ReactNode, useEffect, useState } from 'react';
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
  const [thisId, setThisId] = useState(id || uniqueId('textarea-'));

  useEffect(() => {
    if (id) {
      setThisId(id);
    }
  }, [id]);

  return <>
    <PlainLabel htmlFor={thisId}>{label}</PlainLabel>
    <StyledTextarea id={thisId} disabled={disabled} {...rest}/>
  </>;
};

export default Textarea;
