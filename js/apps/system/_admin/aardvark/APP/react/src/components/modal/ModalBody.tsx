import React, { CSSProperties, ReactNode, useEffect, useRef } from "react";

interface ModalBodyProps {
  children?: ReactNode;
  innerStyle?: CSSProperties;
  maximize?: boolean;
}

const ModalBody = ({ children, innerStyle = {}, maximize = false }: ModalBodyProps) => {
  const innerRef = useRef(null);

  useEffect(() => {
    if (maximize) {
      const body = innerRef.current as unknown as HTMLDivElement;
      const parent = body.parentElement as HTMLElement;
      const header = parent.firstElementChild as HTMLElement;
      const footer = parent.lastElementChild as HTMLElement;
      const maxParentHeight = window.innerHeight * 0.9;
      const bodyHeight = maxParentHeight - header.offsetHeight - footer.offsetHeight;

      body.style.height = `${bodyHeight}px`;
    }
  }, [maximize]);

  const style = Object.assign({ minWidth: '50vw' }, innerStyle);

  return <div className="modal-body" style={style} ref={innerRef}>
    {children}
  </div>;
};

export default ModalBody;
