# try-run
# Usage: option = $(call try-run, $(CC)...-o "$$TMP",option-ok,otherwise)
# Exit code chooses option. "$$TMP" serves as a temporary file and is
# automatically cleaned up.
try-run = $(shell set -e;		\
	TMP=".$$$$.tmp";	\
	TMPO=".$$$$.o";	\
	if ($(1)) >/dev/null 2>&1;	\
	then echo "$(2)";		\
	else echo "$(3)";		\
	fi;				\
	rm -f "$$TMP" "$$TMPO")

# as-option
# Usage: cflags-y += $(call as-option,-Wa$(comma)-isa=foo,)

as-option = $(call try-run,\
	$(CC) $(CFLAGS) $(1) -c -x assembler /dev/null -o "$$TMP",$(1),$(2))

# as-instr
# Usage: cflags-y += $(call as-instr,instr,option1,option2)

as-instr = $(call try-run,\
	printf "%b\n" "$(1)" | $(CC) $(AFLAGS) -c -x assembler -o "$$TMP" -,$(2),$(3))

# __cc-option
# Usage: MY_CFLAGS += $(call __cc-option,$(CC),$(MY_CFLAGS),-march=winchip-c6,-march=i586)
__cc-option = $(call try-run,\
	$(1) -Werror $(2) $(3) -c -x c /dev/null -o "$$TMP",$(3),$(4))

# Do not attempt to build with gcc plugins during cc-option tests.
# (And this uses delayed resolution so the flags will be up to date.)
CC_OPTION_CFLAGS = $(filter-out $(GCC_PLUGINS_CFLAGS),$(CFLAGS))

# cc-option
# Usage: cflags-y += $(call cc-option,-march=winchip-c6,-march=i586)
cc-option = $(call __cc-option, $(CC),\
	$(CFLAGS) $(CC_OPTION_CFLAGS),$(1),$(2))

# cc-option-yn
# Usage: flag := $(call cc-option-yn,-march=winchip-c6)
cc-option-yn = $(call try-run,\
	$(CC) -Werror $(CFLAGS) $(CC_OPTION_CFLAGS) $(1) -c -x c /dev/null -o "$$TMP",y,n)

# cc-disable-warning
# Usage: cflags-y += $(call cc-disable-warning,unused-but-set-variable)
cc-disable-warning = $(call try-run,\
	$(CC) -Werror $(CFLAGS) $(CC_OPTION_CFLAGS) -W$(strip $(1)) -c -x c /dev/null -o "$$TMP",-Wno-$(strip $(1)))

