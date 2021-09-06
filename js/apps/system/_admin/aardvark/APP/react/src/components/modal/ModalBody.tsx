import React, { CSSProperties, ReactNode, useEffect, useRef } from "react";

interface ModalBodyProps {
  children?: ReactNode;
  innerStyle?: CSSProperties;
  maximize?: boolean;
  show?: boolean;
}

function getBorderAndMarginHeightOffset (style: CSSStyleDeclaration) {
  let offset = 0;

  offset += parseFloat(style.marginTop) || 0;
  offset += parseFloat(style.marginBottom) || 0;
  offset += parseFloat(style.borderTop) || 0;
  offset += parseFloat(style.borderBottom) || 0;

  return offset;
}

function getPaddingHeightOffset (style: CSSStyleDeclaration) {
  let offset = 0;

  offset += parseFloat(style.paddingTop) || 0;
  offset += parseFloat(style.paddingBottom) || 0;

  return offset;
}

const ModalBody = ({ children, innerStyle = {}, maximize = false, show = false }: ModalBodyProps) => {
  const innerRef = useRef(null);

  useEffect(() => {
    if (maximize && show) {
      const body = innerRef.current as unknown as HTMLDivElement;
      const parent = body.parentElement as HTMLElement;
      const grandParent = parent.parentElement as HTMLElement;
      const gpStyle = window.getComputedStyle(grandParent);
      const pStyle = window.getComputedStyle(parent);

      let offset = 0;
      offset += getPaddingHeightOffset(gpStyle);
      offset += getBorderAndMarginHeightOffset(pStyle);
      offset += getPaddingHeightOffset(pStyle);

      parent.childNodes.forEach(child => {
        const htmlChild = child as HTMLElement;
        const hcStyle = window.getComputedStyle(htmlChild);

        if (child === body) {
          offset += getPaddingHeightOffset(hcStyle);
        } else {
          offset += htmlChild.clientHeight;
        }

        offset += getBorderAndMarginHeightOffset(hcStyle);
      });

      const maxGrandParentHeight = window.innerHeight * 0.9;
      const bodyHeight = Math.floor(maxGrandParentHeight - offset);

      body.style.height = `${bodyHeight}px`;
      body.style.maxHeight = 'unset';
      grandParent.style.height = gpStyle.maxHeight;
      grandParent.style.overflowY = 'hidden';
    }
  }, [maximize, show]);

  const style = Object.assign({ minWidth: '60vw' }, innerStyle);

  return <div className="modal-body" style={style} ref={innerRef}>
    {children}
  </div>;
};

export default ModalBody;
