CC=clang
ADDITIONAL_REPL_CFLAGS=

all: release

############################################################
# files
############################################################
OSE_DIR=../..
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
	sys/ose_term\
	ose_print
OSE_FILES=$(foreach f,$(OSE_FILES_BASENAMES),$(OSE_DIR)/$(f))

REPL_FILES=ose_repl linenoise

############################################################
# Derived files
############################################################
$(OSE_DIR)/sys/ose_endian.h:
	cd $(OSE_DIR) && $(MAKE) sys/ose_endian.h

ose_version.h:
	$(MAKE) -f $(OSE_DIR)/Makefile ose_version.h

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
	-DHAVE_OSE_VERSION_H

INCLUDES=-I. -I$(OSE_DIR)

CFLAGS_DEBUG=-Wall -DOSE_CONF_DEBUG -O0 -glldb -fsanitize=undefined
CFLAGS_RELEASE=-Wall -O3

LDFLAGS=-lm -ldl -rdynamic

############################################################
# Targets
############################################################
.PHONY: release
release: CFLAGS=$(CFLAGS_RELEASE) $(INCLUDES) $(DEFINES)
release: ose

.PHONY: debug
debug: CFLAGS=$(CFLAGS_DEBUG) $(INCLUDES) $(DEFINES)
debug: ose


ose: CMD=$(CC) $(CFLAGS) $(LDFLAGS) -o ose \
		$(OSE_FILES:=.c) $(REPL_FILES:=.c)
ose: $(OSE_FILES:=.c) $(REPL_FILES:=.c) $(OSE_DIR)/sys/ose_endian.h ose_version.h
	$(CMD)

.PHONY: clean
clean:
	rm -rf ose *.dSYM ose_version.h
