ArangoSearch Analyzers
======================

To simplify query syntax ArangoSearch provides a concept of named analyzers
which are merely aliases for type+configuration of IResearch analyzers.  In the
future, users will be able to specify their own named analyzers.  For now,
ArangoDB comes with the following analyzers:

* `identity`
  treat the value as an atom

* `text_de`
  tokenize the value into case-insensitive word stems as per the German locale,
  do not discard any stopwords

* `text_en`
  tokenize the value into case-insensitive word stems as per the English locale,
  do not discard any stopwords

* `text_es`
  tokenize the value into case-insensitive word stems as per the Spanish locale,
  do not discard any stopwords

* `text_fi`
  tokenize the value into case-insensitive word stems as per the Finnish locale,
  do not discard any stopwords

* `text_fr`
  tokenize the value into case-insensitive word stems as per the French locale,
  do not discard any stopwords

* `text_it`
  tokenize the value into case-insensitive word stems as per the Italian locale,
  do not discard any stopwords

* `text_nl`
  tokenize the value into case-insensitive word stems as per the Dutch locale,
  do not discard any stopwords

* `text_no`
  tokenize the value into case-insensitive word stems as per the Norwegian
  locale, do not discard any stopwords

* `text_pt`
  tokenize the value into case-insensitive word stems as per the Portuguese
  locale, do not discard any stopwords

* `text_ru`
  tokenize the value into case-insensitive word stems as per the Russian locale,
  do not discard any stopwords

* `text_sv`
  tokenize the value into case-insensitive word stems as per the Swedish locale,
  do not discard any stopwords

* `text_zh`
  tokenize the value into word stems as per the Chinese locale
