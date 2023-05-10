import { MutableRefObject, useEffect, useState } from "react";
import useResizeObserver from "@react-hook/resize-observer";

export const useElementPosition = (
  target: MutableRefObject<HTMLElement | null>
) => {
  const [position, setPosition] = useState<DOMRect>();
  useEffect(() => {
    const rect = target.current?.getBoundingClientRect();
    setPosition(rect);

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
  useResizeObserver(target, () => {
    const rect = target.current?.getBoundingClientRect();
    setPosition(rect);
  });

  return position;
};
