import { usePrevious } from "@chakra-ui/react";
import hotkeys from "hotkeys-js";
import { isEqual } from "lodash";
import React from "react";
import { useQueryContext } from "./QueryContextProvider";

export const QueryKeyboardShortcutProvider = ({
  children
}: {
  children: React.ReactNode;
}) => {
  const { onExecute, onExplain, onOpenSpotlight, queryValue, queryBindParams } =
    useQueryContext();
  const prevQueryBindParams = usePrevious(queryBindParams);
  const areParamsEqual = prevQueryBindParams
    ? isEqual(queryBindParams, prevQueryBindParams)
    : true;
  React.useEffect(() => {
    hotkeys.filter = function (event) {
      var target = event.target;
      var tagName = (target as any)?.tagName;
      return tagName === "INPUT" || tagName === "TEXTAREA";
    };

    hotkeys("ctrl+enter,command+enter", () => {
      onExecute({ queryValue, queryBindParams });
    });

    hotkeys("ctrl+space", onOpenSpotlight);

    hotkeys("ctrl+shift+enter,command+shift+enter", () => {
      onExplain({
        queryValue,
        queryBindParams
      });
    });
    return () => {
      hotkeys.unbind();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [onExecute, queryValue, areParamsEqual]);
  return <>{children}</>;
};
