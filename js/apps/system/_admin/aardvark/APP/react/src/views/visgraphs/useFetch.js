/* global arangoFetch */
import {useState, useEffect} from "react";


export function useFetch(uri) {
  const [data, setData] = useState("")
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState("")

  useEffect(() => {
    if(!uri) return;
    arangoFetch(uri)
    .then((data) => data.json())
    .then(setData)
    .then(() => setLoading(false))
    .catch(setError)
  }, [uri]);

  return {loading, data, error};
}
