/**
 * adapted from here: https://github.com/lukasbach/chakra-ui-contextmenu/blob/main/src/ContextMenu.tsx
 * Modifications made to control open/close & position from outside
 */
import {
  Menu,
  MenuButton,
  MenuButtonProps,
  MenuProps,
  Portal,
  PortalProps
} from "@chakra-ui/react";
import * as React from "react";
import { useCallback, useEffect, useState } from "react";

export interface ContextMenuProps {
  isOpen: boolean;
  onClose: () => void;
  position: { left: string; top: string };
  renderMenu: (ref: React.MutableRefObject<any>) => JSX.Element | null;
  menuProps?: Omit<MenuProps, "children"> & { children?: React.ReactNode };
  portalProps?: Omit<PortalProps, "children"> & { children?: React.ReactNode };
  menuButtonProps?: MenuButtonProps;
}

export function ContextMenu(props: ContextMenuProps) {
  const [isRendered, setIsRendered] = useState(false);
  const [isDeferredOpen, setIsDeferredOpen] = useState(false);

  useEffect(() => {
    if (props.isOpen) {
      setTimeout(() => {
        setIsRendered(true);
        setTimeout(() => {
          setIsDeferredOpen(true);
        });
      });
    } else {
      setIsDeferredOpen(false);
      const timeout = setTimeout(() => {
        setIsRendered(props.isOpen);
      }, 1000);
      return () => clearTimeout(timeout);
    }
  }, [props.isOpen]);
  const ref = React.useRef(null);

  const onCloseHandler = useCallback(() => {
    props.menuProps?.onClose?.();
    props.onClose();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
  if (!isRendered) {
    return <></>;
  }
  return (
    <Portal {...props.portalProps}>
      <Menu
        closeOnBlur
        isOpen={isDeferredOpen}
        gutter={0}
        {...props.menuProps}
        onClose={onCloseHandler}
      >
        <MenuButton
          aria-hidden={true}
          w={1}
          h={1}
          style={{
            position: "absolute",
            left: props.position.left,
            top: props.position.top,
            cursor: "default"
          }}
          {...props.menuButtonProps}
        />
        {props.renderMenu(ref)}
      </Menu>
    </Portal>
  );
}
