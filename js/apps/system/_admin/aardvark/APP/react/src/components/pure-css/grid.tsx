import React, { ReactNode } from 'react';

type ChildProp = {
  children: ReactNode;
  [key: string]: any;
};

export const Grid = ({ children, ...rest }: ChildProp) => <div className={'pure-g'} {...rest}>{children}</div>;

export type CellSize =
  '1-5'
  | '2-5'
  | '3-5'
  | '4-5'
  | '1'
  | '1-1'
  | '5-5'
  | '1-24'
  | '1-12'
  | '2-24'
  | '3-24'
  | '1-8'
  | '4-24'
  | '1-6'
  | '5-24'
  | '1-4'
  | '6-24'
  | '7-24'
  | '1-3'
  | '8-24'
  | '3-8'
  | '9-24'
  | '5-12'
  | '10-24'
  | '11-24'
  | '1-2'
  | '12-24'
  | '13-24'
  | '7-12'
  | '14-24'
  | '5-8'
  | '15-24'
  | '2-3'
  | '16-24'
  | '17-24'
  | '3-4'
  | '18-24'
  | '19-24'
  | '5-6'
  | '20-24'
  | '7-8'
  | '21-24'
  | '11-12'
  | '22-24'
  | '23-24'
  | '24-24';

type CellProps = ({
  size: CellSize;
  [key: string]: any;
}) & ChildProp;

export const Cell = ({ size, children, ...rest }: CellProps) => {
  const sizes = [
    `pure-u-${size}`,
    `pure-u-md-${size}`,
    `pure-u-lg-${size}`,
    `pure-u-xl-${size}`
  ];
  const classes = sizes.join(' ');

  return <div className={classes} {...rest}>{children}</div>;
};
