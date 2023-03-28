import { MutableRefObject, useEffect, useState } from "react";
import useResizeObserver from "@react-hook/resize-observer";

export const useElementPosition = (
  target: MutableRefObject<HTMLElement | null>
) => {
  const [position, setPosition] = useState<{ top: number; left: number }>();
  useEffect(() => {
    const rect = target.current?.getBoundingClientRect();
    const { top, left } = rect || {};
    setPosition({
      top: top || 0,
      left: left || 0
    });

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
  useResizeObserver(target, () => {
    const { top, left } = target.current?.getBoundingClientRect() || {};
    setPosition({
      top: top || 0,
      left: left || 0
    });
  });

  return position;
};
