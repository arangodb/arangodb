import React, { ReactNode } from "react";

export const FieldNameTag = ({
  children
}: {
  children: ReactNode;
}) => {
  return <div style={{
    background: '#e4e4e4',
    padding: '2px 4px',
    display: 'inline-block',
    borderRadius: '4px',
    marginRight: '4px'
  }}>
    {children}
  </div>
}