import { DispatchArgs, FormProps } from "../../../utils/constants";
import { FormState, LinkProperties } from "../constants";
import React, { Dispatch, useEffect, useState } from "react";
import { chain, difference, isEmpty, isNull, map } from "lodash";
import { useLinkState } from "../helpers";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import NewLink from "../Components/NewLink";
import LinkView from "../Components/LinkView";
import { useShow, useShowUpdate, useField } from "../Contexts/LinkContext";
import FieldView from "../Components/FieldView";

const LinkPropertiesForm = ({
  formState,
  dispatch,
  disabled,
  view
}: FormProps<FormState>) => {
  const [collection, setCollection, addDisabled, links] = useLinkState(
    formState,
    "links"
  );
  const { data } = useSWR(["/collection", "excludeSystem=true"], (path, qs) =>
    getApiRouteForCurrentDB().get(path, qs)
  );
  const [options, setOptions] = useState<string[]>([]);
  const [link, setLink] = useState<string>();
  const show = useShow();
  const setShow = useShowUpdate();
  const field = useField();

  useEffect(() => {
    if (data) {
      const linkKeys = chain(links)
        .omitBy(isNull)
        .keys()
        .value();
      const collNames = map(data.body.result, "name");
      const tempOptions = difference(collNames, linkKeys).sort();

      setOptions(tempOptions);
    }
  }, [data, links]);

  const linkVal = chain(links)
    .omitBy(isNull)
    .keys()
    .value()[0];

  const updateCollection = (value: string | number) => {
    setCollection(value);
  };

  const getLink = (str: string) => {
    let formatedStr;
    if (str !== "") {
      const toReturn = str.split("[")[1].replace("]", "");
      if (toReturn.includes(".")) {
        formatedStr = toReturn.split(".")[0];
      } else {
        formatedStr = toReturn;
      }
    }
    return formatedStr;
  };

  const handleShowField = () => {};
  const addLink = () => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${collection}]`,
        value: {}
      }
    });

    setCollection("");
    setShow("ViewChild");
    setLink(collection);
  };

  console.log(links);
  return disabled && isEmpty(links) ? (
    <span>No links found.</span>
  ) : (
    <>
      {!disabled && show === "AddNew" && (
        <NewLink
          view={view}
          disabled={addDisabled || !options.includes(collection)}
          addLink={addLink}
          collection={collection}
          updateCollection={updateCollection}
          options={options}
        />
      )}

      {show === "ViewParent" && (
        <LinkView
          view={view}
          link={linkVal}
          links={links}
          dispatch={
            (dispatch as unknown) as Dispatch<DispatchArgs<LinkProperties>>
          }
          disabled={disabled}
        />
      )}
      {show === "ViewChild" && (
        <LinkView
          view={view}
          link={link}
          links={links}
          dispatch={
            (dispatch as unknown) as Dispatch<DispatchArgs<LinkProperties>>
          }
          disabled={disabled}
        />
      )}

      {show === "ViewField" && field !== null && (
        <FieldView
          view={view}
          fields={field.fields}
          disabled={disabled}
          dispatch={
            (dispatch as unknown) as Dispatch<DispatchArgs<LinkProperties>>
          }
          basePath={field.basePath}
          viewField={handleShowField}
          fieldName={field.field}
          link={getLink(field.basePath)}
        />
      )}
      {/* {map(links, (properties, coll) => {
          return properties ? (
            <tr key={coll} style={{ borderBottom: "1px  solid #DDD" }}>
              <ArangoTD seq={disabled ? 1 : 2}>
                <LinkPropertiesInput
                  formState={properties}
                  disabled={disabled || !properties}
                  dispatch={
                    (dispatch as unknown) as Dispatch<
                      DispatchArgs<LinkProperties>
                    >
                  }
                  basePath={`links[${coll}]`}
                />
              </ArangoTD>

              <ArangoTD seq={disabled ? 0 : 1}>{coll}</ArangoTD>
              {disabled ? null : (
                <ArangoTD seq={0} valign={"middle"}>
                  <IconButton
                    icon={"trash-o"}
                    type={"danger"}
                    onClick={getLinkRemover(coll)}
                  />
                </ArangoTD>
              )}
            </tr>
          ) : null;
        })} */}
    </>
  );
};

export default LinkPropertiesForm;
