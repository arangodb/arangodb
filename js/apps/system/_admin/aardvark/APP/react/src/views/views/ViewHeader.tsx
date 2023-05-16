import { Box } from "@chakra-ui/react";
import React from "react";
import { FormProps } from "../../utils/constants";
import { DeleteButtonWrap } from "./Actions";
import { FormState } from "./constants";
import CopyFromInput from "./forms/inputs/CopyFromInput";
import { SaveArangoSearchViewButton } from "./arangoSearchView/SaveArangoSearchViewButton";
import { EditableViewNameField } from "./searchAliasView/EditableViewNameField";
import { ViewPropertiesType } from "./searchAliasView/useFetchViewProperties";

export const ViewHeader = ({
  formState,
  updateName,
  isAdminUser,
  views,
  dispatch,
  changed,
  name,
  setChanged
}: {
  formState: FormState;
  updateName: (name: string) => void;
  isAdminUser: boolean;
  views: FormState[];
  changed: boolean;
  name: string;
  setChanged: (changed: boolean) => void;
} & Pick<FormProps<FormState>, "dispatch">) => {
  return (
    <Box
      position="sticky"
      top="0"
      backgroundColor="gray.100"
      borderBottom="2px solid"
      borderColor={"gray.300"}
      padding="4"
      zIndex="900"
    >
      <Box display="grid" gap="4" gridTemplateRows={"30px 1fr"}>
        <EditableViewNameField
          isAdminUser={isAdminUser}
          isCluster={window.frontendConfig.isCluster}
          setCurrentName={updateName}
          view={formState as ViewPropertiesType}
        />
        {isAdminUser && views.length ? (
          <Box display="flex" alignItems="center">
            <CopyFromInput
              views={views}
              dispatch={dispatch}
              formState={formState}
            />
            <Box display="flex" ml="auto" flexWrap="wrap">
              <SaveArangoSearchViewButton
                view={formState}
                oldName={name}
                setChanged={setChanged}
                disabled={!isAdminUser || !changed}
              />
              <DeleteButtonWrap disabled={!isAdminUser} view={formState} />
            </Box>
          </Box>
        ) : null}
      </Box>
    </Box>
  );
};
