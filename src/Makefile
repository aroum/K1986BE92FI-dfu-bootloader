# Forward make targets from src/ directory to repository root Makefile
TARGETS := $(filter-out _all,$(MAKECMDGOALS))

.PHONY: _all $(TARGETS)

_all:
	@$(MAKE) -C .. all

$(TARGETS):
	@$(MAKE) -C .. $@
