import React, { useContext } from "react";
import { UrlParametersContext } from "./url-parameters-context";

const FetchData = () => {
  const urlParameters = useContext(UrlParametersContext);

  const callApi = () => {
    console.log("Pokemon: ", urlParameters.pokemon);
    fetch(`https://pokeapi.co/api/v2/pokemon/${urlParameters.pokemon}`)
      .then((res) => res.json())
      .then((data) => console.log(data))
      .catch((err) => console.error(err));
  };

  return (
    <>
      <button
        onClick={() => {
          console.log("Call API with these urlParameters: ", urlParameters);
          callApi();
        }}
      >
        Call API (based on current urlParameters from context)
      </button>
      <hr />
    </>
  );
};

export default FetchData;
