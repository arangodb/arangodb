import { mutate } from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";

export const useDeleteView = ({ name }: { name: string }) => {
  const handleDelete = async () => {
    try {
      const { encoded: encodedViewName } = encodeHelper(name);
      const result = await getApiRouteForCurrentDB().delete(
        `/view/${encodedViewName}`
      );

      if (result.body.error) {
        window.arangoHelper.arangoError(
          "Failure",
          `Got unexpected server response: ${result.body.errorMessage}`
        );
      } else {
        mutate("/view");
        window.App.navigate("#views", { trigger: true });
        window.arangoHelper.arangoNotification(
          "Success",
          `Deleted View: ${name}`
        );
      }
    } catch (e: any) {
      window.arangoHelper.arangoError(
        "Failure",
        `Got unexpected server response: ${e.message}`
      );
    }
  };

  return { onDelete: handleDelete };
};
