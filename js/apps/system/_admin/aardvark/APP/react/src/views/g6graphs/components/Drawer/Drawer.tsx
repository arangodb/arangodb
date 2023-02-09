import React, { ReactNode, useEffect } from "react";
import "./styles.css";

/**
 * Props
 * Control -> open/close
 * Children
 * Events
 *  onClose
 *  onShow
 *
 */

interface IDrawer {
  onClose?: () => void;
  onOpen?: () => void;
  children?: ReactNode | null | JSX.Element;
  open?: boolean;
  position?: "left" | "right";
}
const Drawer = ({
  open,
  onClose,
  onOpen,
  position = "left",
  children
}: IDrawer) => {
  useEffect(() => {
    if (open) {
      onOpen && onOpen();
    } else if (!open) {
      onClose && onClose();
    }
  }, [open]);
  return (
    <div className={open ? `side-drawer open ${position}` : "drawer"}>
      {open ? children : null}
    </div>
  );
};

export default Drawer;
