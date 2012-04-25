# -*- mode: Makefile; -*-

################################################################################
## FLEX
################################################################################

JsonParser/%.c: @srcdir@/JsonParser/%.l
	@top_srcdir@/config/flex-c.sh $(LEX) $@ $<

QL/%.c: @srcdir@/QL/%.l
	@top_srcdir@/config/flex-c.sh $(LEX) $@ $<

Ahuacatl/%.c: @srcdir@/Ahuacatl/%.l
	@top_srcdir@/config/flex-c.sh $(LEX) $@ $<

################################################################################
## FLEX++
################################################################################

V8/%.cpp: @srcdir@/V8/%.ll
	@top_srcdir@/config/flex-c++.sh $(LEX) $@ $<

JsonParserX/%.cpp: @srcdir@/JsonParserX/%.ll
	@top_srcdir@/config/flex-c++.sh $(LEX) $@ $<

################################################################################
## CLEANUP
################################################################################

CLEANUP += $(FLEX_FILES) $(FLEXXX_FILES)
