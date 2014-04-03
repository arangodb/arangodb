MAN3 = \
	doc/bson_oid_t.3 \
	doc/bson_append_array.3 \
	doc/bson_append_binary.3 \
	doc/bson_append_bool.3 \
	doc/bson_append_code.3 \
	doc/bson_append_date_time.3 \
	doc/bson_append_dbpointer.3 \
	doc/bson_append_document.3 \
	doc/bson_append_double.3 \
	doc/bson_append_int32.3 \
	doc/bson_append_int64.3 \
	doc/bson_append_iter.3 \
	doc/bson_append_maxkey.3 \
	doc/bson_append_minkey.3 \
	doc/bson_append_now_utc.3 \
	doc/bson_append_null.3 \
	doc/bson_append_oid.3 \
	doc/bson_append_regex.3 \
	doc/bson_append_symbol.3 \
	doc/bson_append_timestamp.3 \
	doc/bson_append_undefined.3 \
	doc/bson_append_utf8.3 \
	doc/bson_as_json.3 \
	doc/bson_compare.3 \
	doc/bson_concat.3 \
	doc/bson_context_destroy.3 \
	doc/bson_context_get_default.3 \
	doc/bson_context_new.3 \
	doc/bson_copy_to_excluding.3 \
	doc/bson_copy_to.3 \
	doc/bson_copy.3 \
	doc/bson_count_keys.3 \
	doc/bson_destroy.3 \
	doc/bson_equal.3 \
	doc/bson_free.3 \
	doc/bson_get_data.3 \
	doc/bson_get_monotonic_time.3 \
	doc/bson_has_field.3 \
	doc/bson_init_static.3 \
	doc/bson_init.3 \
	doc/bson_iter_t.3 \
	doc/bson_iter_array.3 \
	doc/bson_iter_as_bool.3 \
	doc/bson_iter_as_int64.3 \
	doc/bson_iter_binary.3 \
	doc/bson_iter_bool.3 \
	doc/bson_iter_code.3 \
	doc/bson_iter_codewscope.3 \
	doc/bson_iter_date_time.3 \
	doc/bson_iter_dbpointer.3 \
	doc/bson_iter_document.3 \
	doc/bson_iter_double.3 \
	doc/bson_iter_dup_utf8.3 \
	doc/bson_iter_find_case.3 \
	doc/bson_iter_find_descendant.3 \
	doc/bson_iter_find.3 \
	doc/bson_iter_int32.3 \
	doc/bson_iter_int64.3 \
	doc/bson_iter_key.3 \
	doc/bson_iter_next.3 \
	doc/bson_iter_oid.3 \
	doc/bson_iter_overwrite_bool.3 \
	doc/bson_iter_overwrite_double.3 \
	doc/bson_iter_overwrite_int32.3 \
	doc/bson_iter_overwrite_int64.3 \
	doc/bson_iter_recurse.3 \
	doc/bson_iter_regex.3 \
	doc/bson_iter_symbol.3 \
	doc/bson_iter_timestamp.3 \
	doc/bson_iter_time_t.3 \
	doc/bson_iter_timeval.3 \
	doc/bson_iter_type.3 \
	doc/bson_iter_utf8.3 \
	doc/bson_iter_visit_all.3 \
	doc/bson_malloc0.3 \
	doc/bson_malloc.3 \
	doc/bson_new.3 \
	doc/bson_oid_compare.3 \
	doc/bson_oid_copy.3 \
	doc/bson_oid_equal.3 \
	doc/bson_oid_get_time_t.3 \
	doc/bson_oid_hash.3 \
	doc/bson_oid_is_valid.3 \
	doc/bson_oid_to_string.3 \
	doc/bson_reader_destroy.3 \
	doc/bson_reader_new_from_data.3 \
	doc/bson_reader_new_from_fd.3 \
	doc/bson_reader_read.3 \
	doc/bson_reader_set_read_func.3 \
	doc/bson_reader_tell.3 \
	doc/bson_realloc.3 \
	doc/bson_realloc_ctx.3 \
	doc/bson_reinit.3 \
	doc/bson_sized_new.3 \
	doc/bson_strdup.3 \
	doc/bson_strdup_printf.3 \
	doc/bson_strdupv_printf.3 \
	doc/bson_string_append_c.3 \
	doc/bson_string_append_printf.3 \
	doc/bson_string_append.3 \
	doc/bson_string_append_unichar.3 \
	doc/bson_string_free.3 \
	doc/bson_string_new.3 \
	doc/bson_string_truncate.3 \
	doc/bson_strndup.3 \
	doc/bson_uint32_to_string.3 \
	doc/bson_utf8_escape_for_json.3 \
	doc/bson_utf8_from_unichar.3 \
	doc/bson_utf8_get_char.3 \
	doc/bson_utf8_next_char.3 \
	doc/bson_utf8_validate.3 \
	doc/bson_validate.3 \
	doc/bson_visitor_t.3 \
	doc/bson_writer_t.3 \
	doc/bson_writer_begin.3 \
	doc/bson_writer_destroy.3 \
	doc/bson_writer_end.3 \
	doc/bson_writer_get_length.3 \
	doc/bson_writer_new.3 \
	doc/bson_writer_rollback.3 \
	doc/bson_zero_free.3


MAN7 = \
	$(top_srcdir)/doc/libbson.7

MAN_DOC = $(MAN3) $(MAN7)

MAN_TXT = \
	$(MAN3:%.3=%.txt) \
	$(MAN7:%.7=%.txt)

MAN_HTML = $(MAN_TXT:%.txt=%.html)

if INSTALL_MAN
dist_man_MANS = $(MAN_DOC)
endif

EXTRA_DIST += $(top_srcdir)/doc/asciidoc.conf $(MAN_TXT)
if BUILD_DOC
EXTRA_DIST += $(MAN_HTML)
endif

MAINTAINERCLEANFILES = $(MAN_DOC) $(MAN_HTML)

doc : $(top_srcdir)/doc/asciidoc.conf $(DOC_EXAMPLES) $(MAN_DOC) $(MAN_HTML)
dist-hook : doc

if BUILD_DOC
SUFFIXES=.html .txt .xml .3 .7

EXAMPLE_DIR = $(abs_top_srcdir)/examples

.txt.html:
	$(ASCIIDOC) -d manpage -b xhtml11 -f $(top_srcdir)/doc/asciidoc.conf \
           -aexamples=$(EXAMPLE_DIR) -abson_version=@PACKAGE_VERSION@ -o$@ $<
.txt.xml:
	$(ASCIIDOC) -d manpage -b docbook -f $(top_srcdir)/doc/asciidoc.conf \
           -aexamples=$(EXAMPLE_DIR) -abson_version=@PACKAGE_VERSION@ \
	   -abson_manname=$(*F) -o$@ $<
.xml.3:
	$(XMLTO) man -o $(top_srcdir)/doc $<
.xml.7:
	$(XMLTO) man -o $(top_srcdir)/doc $<

CLEANFILES += $(MAN3) $(MAN7)
endif
