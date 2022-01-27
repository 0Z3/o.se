############################################################
# Variables setable from the command line:
#
# CCOMPILER (default: clang)
# DEBUG_SYMBOLS (default: DWARF)
# EXTRA_CFLAGS (default: none)
#
# for submodules that contain c++:
# CPPCOMPILER
# EXTRA_CPPFLAGS
############################################################

ifndef CCOMPILER
CC=clang
else
CC=$(CCOMPILER)
endif

.PHONY: all
all: release

MODULES=o.se.lined o.se.udp o.se.oscbn

.PHONY: all-modules all-modules-debug all-modules-clean

all-modules:
	for m in $(MODULES); do (cd "$$m" && CCOMPILER=$(CC) DEBUG_SYMBOLS=$(DEBUG_SYMBOLS) EXTRA_CFLAGS=$(EXTRA_CFLAGS) CPPCOMPILER=$(CPPCOMPILER) EXTRA_CPPFLAGS=$(EXTRA_CPPFLAGS) $(MAKE)); done

all-modules-debug:
	for m in $(MODULES); do (cd "$$m" && CCOMPILER=$(CC) DEBUG_SYMBOLS=$(DEBUG_SYMBOLS) EXTRA_CFLAGS=$(EXTRA_CFLAGS) CPPCOMPILER=$(CPPCOMPILER) EXTRA_CPPFLAGS=$(EXTRA_CPPFLAGS) $(MAKE) debug); done

all-modules-clean:
	for m in $(MODULES); do (cd "$$m" && CCOMPILER=$(CC) DEBUG_SYMBOLS=$(DEBUG_SYMBOLS) EXTRA_CFLAGS=$(EXTRA_CFLAGS) CPPCOMPILER=$(CPPCOMPILER) EXTRA_CPPFLAGS=$(EXTRA_CPPFLAGS) $(MAKE) clean); done

############################################################
# files
############################################################
LIBOSE_DIR=libose
OSE_FILES_BASENAMES=\
	ose\
	ose_util\
	ose_stackops\
	ose_context\
	ose_match\
	ose_assert\
	ose_symtab\
	ose_vm\
	ose_builtins\
	sys/ose_load\
	sys/ose_time\
	ose_print

OSE_FILES=$(foreach f,$(OSE_FILES_BASENAMES),$(LIBOSE_DIR)/$(f))

REPL_FILES=ose_repl ose_term

############################################################
# Derived files
############################################################
$(LIBOSE_DIR)/sys/ose_endian.h:
	cd $(LIBOSE_DIR) && $(MAKE) sys/ose_endian.h

ose_version.h:
	$(MAKE) -f $(LIBOSE_DIR)/Makefile ose_version.h

############################################################
# Flags and options
############################################################
VM_BUNDLE_SIZE=65536
VM_BUNDLE_SIZES=\
	-DOSE_CONF_VM_INPUT_SIZE=$(VM_BUNDLE_SIZE) \
	-DOSE_CONF_VM_STACK_SIZE=$(VM_BUNDLE_SIZE) \
	-DOSE_CONF_VM_ENV_SIZE=$(VM_BUNDLE_SIZE) \
	-DOSE_CONF_VM_CONTROL_SIZE=$(VM_BUNDLE_SIZE) \
	-DOSE_CONF_VM_DUMP_SIZE=$(VM_BUNDLE_SIZE) \
	-DOSE_CONF_VM_OUTPUT_SIZE=$(VM_BUNDLE_SIZE)

DEFINES=\
	$(VM_BUNDLE_SIZES) \
	-DHAVE_OSE_ENDIAN_H \
	-DHAVE_OSE_VERSION_H \
	-DOSEVM_LOOKUP=oserepl_lookup

INCLUDES=-I. -I$(LIBOSE_DIR)

CFLAGS_DEBUG=-Wall -DOSE_CONF_DEBUG -O0 -g$(DEBUG_SYMBOLS) $(EXTRA_CFLAGS) # -fsanitize=undefined
CFLAGS_RELEASE=-Wall -O3 $(EXTRA_CFLAGS)

LDFLAGS=-lm -ldl -rdynamic

############################################################
# Targets
############################################################
.PHONY: release
release: CFLAGS=$(CFLAGS_RELEASE) $(INCLUDES) $(DEFINES)
release: o.se

.PHONY: debug
debug: CFLAGS=$(CFLAGS_DEBUG) $(INCLUDES) $(DEFINES)
debug: o.se


o.se: CMD=$(CC) $(CFLAGS) $(LDFLAGS) -o o.se \
		$(OSE_FILES:=.c) $(REPL_FILES:=.c)
o.se: $(OSE_FILES:=.c) $(REPL_FILES:=.c) $(LIBOSE_DIR)/sys/ose_endian.h ose_version.h
	$(CMD)

.PHONY: clean
clean:
	rm -rf ose *.dSYM ose_version.h
