EXAMPLE_CFLAGS = \
	-I$(top_srcdir)/src/bson \
	-I$(top_builddir)/src/bson


noinst_PROGRAMS += bson-to-json
bson_to_json_SOURCES = examples/bson-to-json.c
bson_to_json_CPPFLAGS = $(EXAMPLE_CFLAGS)
bson_to_json_LDADD = libbson-1.0.la


noinst_PROGRAMS += bson-validate
bson_validate_SOURCES = examples/bson-validate.c
bson_validate_CPPFLAGS = $(EXAMPLE_CFLAGS)
bson_validate_LDADD = libbson-1.0.la


noinst_PROGRAMS += bcon-speed
bcon_speed_SOURCES = examples/bcon-speed.c
bcon_speed_CPPFLAGS = $(EXAMPLE_CFLAGS)
bcon_speed_LDADD = libbson-1.0.la


noinst_PROGRAMS += bcon-col-view
bcon_col_view_SOURCES = examples/bcon-col-view.c
bcon_col_view_CPPFLAGS = $(EXAMPLE_CFLAGS)
bcon_col_view_LDADD = libbson-1.0.la


noinst_PROGRAMS += json-to-bson
json_to_bson_SOURCES = examples/json-to-bson.c
json_to_bson_CPPFLAGS = $(EXAMPLE_CFLAGS)
json_to_bson_LDADD = libbson-1.0.la

noinst_PROGRAMS += bson-metrics
bson_metrics_SOURCES = examples/bson-metrics.c
bson_metrics_CPPFLAGS = $(EXAMPLE_CFLAGS)
bson_metrics_LDADD = -lm libbson-1.0.la

