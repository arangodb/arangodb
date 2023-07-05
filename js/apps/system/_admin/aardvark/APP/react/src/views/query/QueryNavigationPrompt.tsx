import { Button, ModalHeader, Stack } from "@chakra-ui/react";
import React, { useEffect, useRef } from "react";
import { Modal, ModalBody, ModalFooter } from "../../components/modal";
import { QueryResultType } from "./QueryContextProvider";

/**
 * Prevents navigation by patching Backbone's execute method.
 */
export const QueryNavigationPrompt = ({
  queryResults
}: {
  queryResults: QueryResultType[];
}) => {
  const [showPrompt, setShowPrompt] = React.useState(false);
  const [fragment, setFragment] = React.useState<string>("");
  const allowLeave = useRef(false);
  const preventNavigation = (isAnyQueryInProgress: boolean) => {
    window.onbeforeunload = () => {
      return isAnyQueryInProgress ? true : null;
    };
  };
  useEffect(() => {
    const isAnyQueryInProgress = queryResults.some(
      queryResult => queryResult.status === "loading"
    );
    preventNavigation(isAnyQueryInProgress);
    const originalExecute = window.App.execute;
    window.App.execute = function (...args: any[]) {
      if (!isAnyQueryInProgress || allowLeave.current) {
        return originalExecute.call(this, ...args);
      } else {
        // put URL back to original state
        const newFragment = (window.Backbone.history as any).fragment;
        setFragment(prevFragment => newFragment !== "queries" ? newFragment : prevFragment
        );
        window.history.replaceState(
          {},
          document.title,
          `${window.location.origin}${window.location.pathname}#queries`
        );
        setShowPrompt(true);
      }
    };
    return () => {
      window.App.execute = originalExecute;
    };
  }, [queryResults, allowLeave]);
  if (showPrompt) {
    return (
      <Modal
        isOpen
        onClose={() => {
          setShowPrompt(false);
        }}
      >
        <ModalHeader>Are you sure you want to navigate away?</ModalHeader>
        <ModalBody>
          A query is still in progress, if you leave now, it will be cancelled.
        </ModalBody>
        <ModalFooter>
          <Stack direction="row" spacing={4}>
            <Button
              onClick={() => {
                setShowPrompt(false);
              }}
            >
              Stay
            </Button>
            <Button
              onClick={() => {
                setShowPrompt(false);
                allowLeave.current = true;
                console.log("navigate to", fragment);
                window.App.navigate(fragment, { trigger: true });
              }}
              colorScheme="red"
            >
              Leave
            </Button>
          </Stack>
        </ModalFooter>
      </Modal>
    );
  }
  return null;
};
