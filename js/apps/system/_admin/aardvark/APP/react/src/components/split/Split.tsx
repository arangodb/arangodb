import React, { useState } from "react";
import ReactSplit, {
  ChildrenPattern,
  ComponentPattern,
  RenderPattern,
  SplitGridProps
} from "react-split-grid";

export const Split = (
  props: SplitGridProps &
    (ChildrenPattern | RenderPattern | ComponentPattern) & {
      storageKey: string;
    }
) => {
  const { storageKey, ...rest } = props;
  const localStorageSplitSize = localStorage.getItem(storageKey);
  const [gridTemplateColumns, setGridTemplateColumns] = useState(
    localStorageSplitSize || "1fr 10px 1fr"
  );
  return (
    <ReactSplit
      gridTemplateColumns={gridTemplateColumns}
      minSize={100}
      cursor="col-resize"
      onDrag={(_direction, _track, gridTemplateStyle) => {
        setGridTemplateColumns(gridTemplateStyle);
        localStorage.setItem(storageKey, gridTemplateStyle);
      }}
      {...rest}
    />
  );
};
