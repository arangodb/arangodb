import React, { CSSProperties, ReactNode } from "react";

type IconButtonProps = {
  icon: string;
  type?: 'neutral' | 'disabled' | 'header' | 'primary' | 'notification' | 'success' | 'info' | 'navbar'
    | 'danger' | 'warning' | 'inactive' | 'close' | 'default';
  size?: 'xsmall' | 'small' | 'large' | 'xlarge';
  children?: ReactNode;
  [key: string]: any;
};

const iconButtonSizeMap: { [key: string]: CSSProperties } = {
  xsmall: {
    fontSize: '70%'
  },
  small: {
    fontSize: '85%'
  },
  large: {
    fontSize: '110%'
  },
  xlarge: {
    fontSize: '125%'
  }
};

export const IconButton = ({ type, size, icon, children, ...rest }: IconButtonProps) => {
  const style: CSSProperties = rest.style || {};

  if (size) {
    Object.assign(style, iconButtonSizeMap[size]);
  }
  delete rest.style;

  return <button className={type ? `button-${type}` : 'pure-button'} style={style} {...rest}>
    <i className={`fa fa-${icon}`} style={{ marginLeft: 'auto' }}/>
    {
      children
        ? <>
          &nbsp;
          {children}
        </>
        : null
    }
  </button>;
};

