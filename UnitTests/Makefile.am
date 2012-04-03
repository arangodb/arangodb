SUBDIRS = .

PID := $(shell echo $$PPID)
TMP_DIR := "/tmp/avocadodb.$(PID)"
PID_FILE := "/tmp//avocadodb.$(PID).pid"

.PHONY: start-avocadodb stop-avocadodb

start-avocadodb:
	echo $(PID)
	echo $(TMP_DIR)
