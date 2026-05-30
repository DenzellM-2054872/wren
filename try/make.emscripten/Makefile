# Copied from projects/make and modified for emscripten
# To run, make sure emsdk is in your path (our sourced via their scripts)
# run `emmake make`

ifndef config
  config=release_32bit
endif

ifndef verbose
  SILENT = @
endif

ifeq ($(config),debug_32bit)
  fodi_config = debug_32bit
  fodi_try_config = debug_32bit

else ifeq ($(config),release_32bit)
  fodi_config = release_32bit
  fodi_try_config = release_32bit

else
  $(error "invalid configuration $(config)")
endif

PROJECTS := fodi fodi_try

.PHONY: all clean help $(PROJECTS) 

all: $(PROJECTS)

fodi:
ifneq (,$(fodi_config))
	@echo "==== Building fodi ($(fodi_config)) ===="
	@${MAKE} --no-print-directory -C . -f fodi.make config=$(fodi_config)
endif

fodi_try: fodi
ifneq (,$(fodi_try_config))
	@echo "==== Building fodi_try ($(fodi_try_config)) ===="
	@${MAKE} --no-print-directory -C . -f fodi_try.make config=$(fodi_try_config)
endif

clean:
	@${MAKE} --no-print-directory -C . -f fodi.make clean
	@${MAKE} --no-print-directory -C . -f fodi_try.make clean

help:
	@echo "Usage: make [config=name] [target]"
	@echo ""
	@echo "CONFIGURATIONS:"
	@echo "  release_32bit"
	@echo "  debug_32bit"
	@echo ""
	@echo "TARGETS:"
	@echo "   all (default)"
	@echo "   clean"
	@echo "   fodi"
	@echo "   fodi_try"
	@echo ""
