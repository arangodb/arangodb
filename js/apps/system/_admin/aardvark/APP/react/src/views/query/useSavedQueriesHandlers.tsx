import { useDisclosure } from "@chakra-ui/react";
import { mutate } from "swr";
import { getCurrentDB } from "../../utils/arangoClient";
import {
  QueryType,
  useFetchUserSavedQueries
} from "./editor/useFetchUserSavedQueries";

export const useSavedQueriesHandlers = ({
  queryValue,
  queryBindParams,
  storageKey
}: {
  queryValue: string;
  queryBindParams: { [key: string]: string };
  queryName?: string;
  storageKey: string;
}) => {
  const { savedQueries, isLoading: isFetchingQueries } =
    useFetchUserSavedQueries({
      storageKey
    });
  const onSaveAs = async (queryName: string) => {
    const newQueries = [
      ...(savedQueries || []),
      { name: queryName, value: queryValue, parameter: queryBindParams }
    ];
    await patchQueries({
      queries: newQueries,
      onSuccess: () => {
        window.arangoHelper.arangoNotification(`Saved query: ${queryName}`);
      },
      storageKey
    });
    mutate("/savedQueries");
  };
  const onSave = async (queryName: string) => {
    await mutate("/savedQueries");
    if (queryName) {
      const newQueries = savedQueries?.map(query => {
        if (query.name === queryName) {
          return {
            name: queryName,
            value: queryValue,
            parameter: queryBindParams,
            modified_at: Date.now()
          };
        }
        return query;
      });
      await patchQueries({
        queries: newQueries || [],
        onSuccess: () => {
          window.arangoHelper.arangoNotification(`Updated query: ${queryName}`);
        },
        storageKey
      });
      mutate("/savedQueries");
    }
  };

  const onDelete = async (queryName: string) => {
    const newQueries = savedQueries?.filter(query => query.name !== queryName);
    await patchQueries({
      queries: newQueries || [],
      onSuccess: () => {
        window.arangoHelper.arangoNotification(`Deleted query: ${queryName}`);
      },
      storageKey
    });
    mutate("/savedQueries");
  };
  const {
    isOpen: isSaveAsModalOpen,
    onOpen: onOpenSaveAsModal,
    onClose: onCloseSaveAsModal
  } = useDisclosure();
  return {
    onSaveAs,
    onSave,
    onDelete,
    savedQueries,
    isFetchingQueries,
    isSaveAsModalOpen,
    onOpenSaveAsModal,
    onCloseSaveAsModal
  };
};

const patchQueries = async ({
  queries,
  onSuccess,
  storageKey
}: {
  queries: QueryType[];
  onSuccess: () => void;
  storageKey: string;
}) => {
  if (window.frontendConfig.ldapEnabled) {
    localStorage.setItem(storageKey, JSON.stringify(queries));
    onSuccess();
    return Promise.resolve();
  }
  const currentDB = getCurrentDB();
  try {
    await currentDB.request({
      method: "PATCH",
      path: `/_api/user/${encodeURIComponent(window.App.currentUser)}`,
      body: {
        extra: {
          queries
        }
      }
    });
    onSuccess();
    mutate("/savedQueries");
  } catch (e: any) {
    const message = e.message || e.response.body.errorMessage;
    window.arangoHelper.arangoError(`Could not save query. Error - ${message}`);
  }
};
