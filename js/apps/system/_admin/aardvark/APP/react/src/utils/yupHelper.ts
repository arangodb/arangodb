import * as Yup from "yup";

export function getNormalizedByteLengthTest(
  maxByteLength: number,
  message: string
): Yup.TestFunction<string | undefined, any> {
  return function (value) {
    // your logic
    const { path, createError } = this;
    // ...
    const normalizedValue = value?.normalize("NFC");
    const byteLength = normalizedValue ? new Blob([normalizedValue]).size : 0;
    if (byteLength <= maxByteLength) {
      return true;
    }
    return createError({
      path,
      message
    });
  };
}
