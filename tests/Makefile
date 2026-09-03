#-------------------------------------------------------------------------------
# Every test RPL at once. See README.md for what each one proves.
#
#   make
#   make deploy WIIU_IP=192.168.68.145 [TITLE_ID=0005000010143500]
#   make clean
#
# chain_test builds two RPLs from one source, and chain_a is built EXCLUSIVE so
# the pair tests the "one of them will not share" case. Build it without that
# (cd chain_test && make both) for the plain chaining case.
#-------------------------------------------------------------------------------
.PHONY: all clean deploy

WIIU_IP  ?=
TITLE_ID ?= 0005000010143500

all:
	@$(MAKE) --no-print-directory -C release_test
	@$(MAKE) --no-print-directory -C chain_test NAME=chain_a EXCLUSIVE=1
	@$(MAKE) --no-print-directory -C chain_test NAME=chain_b

clean:
	@$(MAKE) --no-print-directory -C release_test clean
	@$(MAKE) --no-print-directory -C chain_test clean

# One transfer at a time: the console's FTP server handles a single connection.
deploy: all
	@if [ -z "$(WIIU_IP)" ]; then echo "deploy: set WIIU_IP=<console ip>"; exit 1; fi
	@$(MAKE) --no-print-directory -C release_test deploy WIIU_IP=$(WIIU_IP) TITLE_ID=$(TITLE_ID)
	@$(MAKE) --no-print-directory -C chain_test   deploy NAME=chain_a EXCLUSIVE=1 WIIU_IP=$(WIIU_IP) TITLE_ID=$(TITLE_ID)
	@$(MAKE) --no-print-directory -C chain_test   deploy NAME=chain_b WIIU_IP=$(WIIU_IP) TITLE_ID=$(TITLE_ID)
	@echo
	@echo "  All three .rpl files are now in sd:/wiiu/rpl-loader/$(TITLE_ID)/."
	@echo "  Remove the ones you are not testing: they hook the same function"
	@echo "  and each log means something different depending on what else is there."
	@echo
