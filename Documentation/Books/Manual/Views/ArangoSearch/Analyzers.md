ArangoSearch Analyzers
======================

To simplify query syntax ArangoSearch provides a concept of named analyzers
which are merely aliases for type+configuration of IResearch analyzers.  In the
future, users will be able to specify their own named analyzers.  For now,
ArangoDB comes with the following analyzers:

- `identity`<br/>
  treat the value as an atom

- `text_de`<br/>
  tokenize the value into case-insensitive word stems as per the German locale,<br/>
  do not discard any stopwords

- `text_en`<br/>
  tokenize the value into case-insensitive word stems as per the English locale,<br/>
  do not discard any stopwords

- `text_es`<br/>
  tokenize the value into case-insensitive word stems as per the Spanish locale,<br/>
  do not discard any stopwords

- `text_fi`<br/>
  tokenize the value into case-insensitive word stems as per the Finnish locale,<br/>
  do not discard any stopwords

- `text_fr`<br/>
  tokenize the value into case-insensitive word stems as per the French locale,<br/>
  do not discard any stopwords

- `text_it`<br/>
  tokenize the value into case-insensitive word stems as per the Italian locale,<br/>
  do not discard any stopwords

- `text_nl`<br/>
  tokenize the value into case-insensitive word stems as per the Dutch locale,<br/>
  do not discard any stopwords

- `text_no`<br/>
  tokenize the value into case-insensitive word stems as per the Norwegian<br/>
  locale, do not discard any stopwords

- `text_pt`<br/>
  tokenize the value into case-insensitive word stems as per the Portuguese<br/>
  locale, do not discard any stopwords

- `text_ru`<br/>
  tokenize the value into case-insensitive word stems as per the Russian locale,<br/>
  do not discard any stopwords

- `text_sv`<br/>
  tokenize the value into case-insensitive word stems as per the Swedish locale,<br/>
  do not discard any stopwords

- `text_zh`<br/>
  tokenize the value into word stems as per the Chinese locale
