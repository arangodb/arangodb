OUT=../out/$(VS)-getopt-

.PHONY: all all_bits all_config install install_bits clean

all:
	$(MAKE) all_bits BITS=32 TARGET="$(TARGET)"
	$(MAKE) all_bits BITS=64 TARGET="$(TARGET) Win64"

all_bits:
	rm -rf Build$(BITS)
	mkdir Build$(BITS)

	$(MAKE) all_config CONFIG=RelWithDebInfo BITS=$(BITS) TARGET="$(TARGET)"
	$(MAKE) all_config CONFIG=Debug BITS=$(BITS) TARGET="$(TARGET)"

all_config:
	@echo "--------------------------------------------------------------------------------"
	@echo "Build: $(CONFIG) for $(BITS) bits"
	@echo "--------------------------------------------------------------------------------"
	@echo

	cd Build$(BITS) && cmake -G "$(TARGET)" ..
	cd Build$(BITS) && cmake --build . --config $(CONFIG)

install:
	$(MAKE) install_bits BITS=32
	$(MAKE) install_bits BITS=64

install_bits:
	rm -rf "$(OUT)$(BITS)"

	mkdir "$(OUT)$(BITS)"
	mkdir "$(OUT)$(BITS)/lib"
	mkdir "$(OUT)$(BITS)/include"

	cp -a Build$(BITS)/RelWithDebInfo "$(OUT)$(BITS)/lib"
	cp -a Build$(BITS)/getopt.dir/RelWithDebInfo/*.pdb "$(OUT)$(BITS)/lib/RelWithDebInfo/getopt.pdb"
	cp -a Build$(BITS)/Debug "$(OUT)$(BITS)/lib"

	cp *.h "$(OUT)$(BITS)/include"

clean:
	rm -rf Build32 Build64
