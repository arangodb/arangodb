export const encodeHelper = (value?: string) => {
  const normalized = value?.normalize();
  const encoded = normalized ? encodeURIComponent(normalized) : "";
  const advanced = normalized
    ? encodeURIComponent(normalized.replace(/%/g, "%25"))
    : "";
  return { encoded, normalized, advanced };
};
