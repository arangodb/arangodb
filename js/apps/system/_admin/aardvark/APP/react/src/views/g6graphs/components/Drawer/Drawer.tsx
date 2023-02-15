import React, { ReactNode } from "react";
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
  children?: ReactNode | null | JSX.Element;
  open?: boolean;
  position?: "left" | "right";
}
const Drawer = ({
  open,
  position = "left",
  children
}: IDrawer) => {

  return (
    <div className={`side-drawer ${position} ${open ? "open" : ""} `}>
      {children}
    </div>
  );
};

export default Drawer;
