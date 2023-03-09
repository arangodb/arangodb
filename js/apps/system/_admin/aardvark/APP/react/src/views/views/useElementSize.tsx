import { useCallback, useLayoutEffect, useState } from "react";

interface Size {
  width: number;
  height: number;
}

function useElementSize<T extends HTMLElement = HTMLDivElement>(): [
  (node: T | null) => void,
  Size
] {
  // Mutable values like 'ref.current' aren't valid dependencies
  // because mutating them doesn't re-render the component.
  // Instead, we use a state as a ref to be reactive.
  const [ref, setRef] = useState<T | null>(null);
  const [size, setSize] = useState<Size>({
    width: 0,
    height: 0
  });

  // Prevent too many rendering using useCallback
  const handleSize = useCallback(() => {
    setSize({
      width: ref ? ref.offsetWidth : 0,
      height: ref ? ref.offsetHeight : 0
    });

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [ref && ref.offsetHeight, ref && ref.offsetWidth]);

  document.addEventListener("resize", handleSize);

  useLayoutEffect(() => {
    handleSize();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [ref && ref.offsetHeight, ref && ref.offsetWidth]);

  return [setRef, size];
}

export default useElementSize;
