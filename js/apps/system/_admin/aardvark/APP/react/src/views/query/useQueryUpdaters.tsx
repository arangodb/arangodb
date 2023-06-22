import { mutate } from "swr";
import { getCurrentDB } from "../../utils/arangoClient";
import { QueryType } from "./editor/useFetchUserSavedQueries";

export const useQueryUpdaters = ({
  queryValue,
  queryBindParams,
  savedQueries,
  onCloseSaveAsModal
}: {
  queryValue: string;
  queryBindParams: { [key: string]: string };
  queryName?: string;
  savedQueries?: QueryType[];
  onCloseSaveAsModal: () => void;
}) => {
  const onSaveAs = async (queryName: string) => {
    const newQueries = [
      ...(savedQueries || []),
      { name: queryName, value: queryValue, parameter: queryBindParams }
    ];
    await patchQueries({
      queries: newQueries,
      queryName
    });
    onCloseSaveAsModal();
  };
  const onSave = async (queryName: string) => {
    await mutate("/savedQueries");
    if (queryName) {
      const newQueries = savedQueries?.map(query => {
        if (query.name === queryName) {
          return {
            name: queryName,
            value: queryValue,
            parameter: queryBindParams
          };
        }
        return query;
      });
      await patchQueries({
        queries: newQueries || [],
        queryName
      });
    }
  };
  return { onSaveAs, onSave };
};

const patchQueries = async ({
  queries,
  queryName
}: {
  queries: QueryType[];
  queryName: string;
}) => {
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
    window.arangoHelper.arangoNotification(`Saved query: ${queryName}`);
    mutate("/savedQueries");
  } catch (e: any) {
    const message = e.message || e.response.body.errorMessage;
    window.arangoHelper.arangoError(`Could not save query. Error - ${message}`);
  }
};
