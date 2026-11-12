# ---------------------------------------------------------------------------
#  Unity build: each test and benchmark #includes the src/*/*.c it needs, so
#  each binary is one translation unit. main.c uses dist/mule.h instead.
#
#    make            -> debug build   (build/debug/main)
#    make release    -> release build (build/release/main), and the benchmarks
#    make run        -> build + run debug on data/pg2554.txt (ARGS=... to change)
#    make test       -> build + run every tests/test_*.c
#    make test T=x   -> run only the tests named x (space-separated list)
#    make dist       -> regenerate dist/mule.h, the single-header library
#    make docs       -> Doxygen HTML reference (build/docs/html/index.html)
#    make examples   -> compile-check notes/str-examples/ against dist/mule.h
#    make version    -> print the version in src/base/base.h
#    make bench      -> micro-benchmarks of str_append and friends
#    make bench-corpus -> benchmark on Gutenberg books, Pizza & Chili
#                       English, source code and XML, and the 1 GB enwik9
#                       Wikipedia dump (downloaded on first use), and
#                       compare the StrView functions against string.h
#    make bench-map  -> main.c's word-frequency phases and the map's index on
#                       the same corpus
#    make bench-scale  -> benchmark on generated 10, 100 and 1000 MiB files
#                       and time the main program on them with hyperfine
#    make format     -> clang-format every source file in place
#    make clean      -> remove build/, including generated scale files
# ---------------------------------------------------------------------------

CC     ?= cc
TARGET := main
BUILD  := build

TEST_SRCS := $(wildcard tests/test_*.c)
BENCH_SRCS := $(wildcard bench/bench_*.c)

# --- Flags -----------------------------------------------------------------
WARN := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
        -Wdouble-promotion -Wformat=2 -Wundef \
        -Wcast-align -Wcast-qual -Wnull-dereference -Wstrict-prototypes \
        -Wmissing-prototypes -Wmissing-declarations -Wpointer-arith \
        -Wwrite-strings -Wswitch-enum -Wimplicit-fallthrough \
        -Wvla -Walloca
CPPFLAGS += -Isrc -Idist -MMD -MP

# Keep each flag in $(1) only if $(CC) compiles and links with it silently.
cc-options = $(strip $(foreach f,$(1),$(shell echo 'int main(void){return 0;}' | \
               $(CC) $(f) -x c - -o /dev/null 2>&1 | grep -q . || echo '$(f)')))

# C11 is the floor gcc, clang and MSVC share. The code is written as C99.
STD := -std=c11

# GCC-only warnings, and hardening that only applies to ELF/GNU ld.
GCC_WARN   := -Wformat-overflow=2 -Wduplicated-cond -Wduplicated-branches \
              -Wlogical-op
ELF_HARDEN := -fno-plt -s -Wl,-O1 -Wl,--as-needed -Wl,-z,relro -Wl,-z,now
WARN       += $(call cc-options,$(GCC_WARN))

# Sanitizers are used if the toolchain can link them (override: make SANITIZE=)
SANITIZE ?= $(shell echo 'int main(void){return 0;}' | \
              $(CC) -fsanitize=address,undefined -x c - -o /dev/null \
              2>/dev/null && echo address,undefined)
SAN_FLAGS := $(if $(SANITIZE),-fsanitize=$(SANITIZE) -fno-sanitize-recover=all)

DEBUG_FLAGS   := $(STD) $(WARN) -O0 -g3 -fno-omit-frame-pointer \
                 -fstack-protector-strong $(SAN_FLAGS) -DDEBUG

RELEASE_FLAGS := $(STD) $(WARN) -Werror -O3 -march=native -flto=auto \
                 -fstack-protector-strong -D_FORTIFY_SOURCE=3 -DNDEBUG \
                 $(call cc-options,$(ELF_HARDEN))

# --- Targets ---------------------------------------------------------------
DEBUG_BIN   := $(BUILD)/debug/$(TARGET)
RELEASE_BIN := $(BUILD)/release/$(TARGET)
TEST_BINS   := $(TEST_SRCS:tests/%.c=$(BUILD)/tests/%)
BENCH_BINS  := $(BENCH_SRCS:bench/%.c=$(BUILD)/bench/%)

.PHONY: all debug release run test bench bench-corpus bench-map bench-scale \
        corpus dist docs examples format clean version
.DELETE_ON_ERROR:
all: debug compile_commands.json

debug:   $(DEBUG_BIN)
release: $(RELEASE_BIN) $(BENCH_BINS)

ARGS ?= data/pg2554.txt
run: debug
	./$(DEBUG_BIN) $(ARGS)

$(DEBUG_BIN): main.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(DEBUG_FLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@ $(LDLIBS)

$(RELEASE_BIN): main.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(RELEASE_FLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@ $(LDLIBS)

# Tests are unity builds too: each test file #includes the .c files it needs.
$(BUILD)/tests/%: tests/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(DEBUG_FLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@ $(LDLIBS)

# The single-header distribution. It is committed, so regenerate it with every
# change to src/. MODULES lists them in dependency order.
MODULES := base mem str err arr map rand time log fs args proc build
DIST    := dist/mule.h

dist: $(DIST)

$(DIST): tools/amalgamate.sh $(wildcard src/*/*.[ch])
	@mkdir -p $(@D)
	sh tools/amalgamate.sh $(MODULES) > $@

$(DEBUG_BIN) $(RELEASE_BIN) $(BUILD)/tests/test_dist: $(DIST)

# Compiling is enough: 10_dangling.c and 11_double_free.c are bugs on purpose.
examples: dist
	$(CC) -std=c11 -Idist -fsyntax-only $(wildcard notes/str-examples/*.c)

test: $(TEST_BINS)
	@pass=0; fail=0; \
	for t in $(TEST_BINS); do \
	    if ./$$t $(T); then pass=$$((pass+1)); else fail=$$((fail+1)); fi; \
	done; \
	echo "== $$pass test file(s) passed, $$fail failed =="; \
	[ $$fail -eq 0 ]

# Benchmarks use the release flags: sanitizers and -O0 would swamp the timings.
$(BUILD)/bench/%: bench/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(RELEASE_FLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@ $(LDLIBS)

$(BUILD)/bench/bench_corpus: LDLIBS += -lm

bench: $(BUILD)/bench/bench_str
	./$<

# --- Benchmark data ----------------------------------------------------------
# Corpora download once into data/corpus/, which `make clean` keeps. Scaled
# files are generated into build/scale/, which `make clean` removes.
CORPUS        := data/corpus
GUTENBERG_IDS := 2554 2701 1342 2600 2000
PIZZACHILI    := https://pizzachili.dcc.uchile.cl/texts
CORPUS_FILES  := $(CORPUS)/gutenberg.txt $(CORPUS)/english.200MB \
                 $(CORPUS)/sources.200MB $(CORPUS)/dblp.xml.200MB \
                 $(CORPUS)/enwik9
# bench_corpus peaks near 3.5x the largest file, so 4 GiB would need about
# 14 GB. Override SCALE_MIB for bigger runs on a machine that has room.
SCALE_MIB     ?= 10 100 1000
SCALE_TEXT    := $(SCALE_MIB:%=$(BUILD)/scale/text-%M)
SCALE_SEQ     := $(SCALE_MIB:%=$(BUILD)/scale/seq-%M)

comma := ,
empty :=
space := $(empty) $(empty)

corpus: $(CORPUS_FILES)

# Five books: Crime and Punishment, Moby-Dick, Pride and Prejudice, War and
# Peace, and Don Quijote in Spanish for non-ASCII UTF-8.
$(CORPUS)/gutenberg.txt:
	@mkdir -p $(@D)/gutenberg
	for id in $(GUTENBERG_IDS); do \
	    curl -fsSL -o $(@D)/gutenberg/pg$$id.txt \
	        https://www.gutenberg.org/cache/epub/$$id/pg$$id.txt || exit 1; \
	done
	cat $(GUTENBERG_IDS:%=$(@D)/gutenberg/pg%.txt) > $@

$(CORPUS)/english.200MB:
	@mkdir -p $(@D)
	curl -fsSL $(PIZZACHILI)/nlang/english.200MB.gz | gunzip > $@.tmp
	mv $@.tmp $@

$(CORPUS)/sources.200MB:
	@mkdir -p $(@D)
	curl -fsSL $(PIZZACHILI)/code/sources.200MB.gz | gunzip > $@.tmp
	mv $@.tmp $@

$(CORPUS)/dblp.xml.200MB:
	@mkdir -p $(@D)
	curl -fsSL $(PIZZACHILI)/xml/dblp.xml.200MB.gz | gunzip > $@.tmp
	mv $@.tmp $@

# The first 10^9 bytes of English Wikipedia's XML, from the Hutter Prize.
$(CORPUS)/enwik9:
	@mkdir -p $(@D)
	curl -fsSL -o $@.zip https://mattmahoney.net/dc/enwik9.zip
	unzip -p $@.zip enwik9 > $@.tmp
	rm $@.zip
	mv $@.tmp $@

# Exactly N MiB, cut with head -c. The loop stops once head closes the pipe.
$(BUILD)/scale/text-%M: $(CORPUS)/gutenberg.txt
	@mkdir -p $(@D)
	while cat $<; do :; done | head -c $$(( $* * 1048576 )) > $@

# BSD seq prints 1e+06 without an explicit format.
$(BUILD)/scale/seq-%M:
	@mkdir -p $(@D)
	seq -f %.0f 1 1000000000 | head -c $$(( $* * 1048576 )) > $@

bench-corpus: $(BUILD)/bench/bench_corpus $(BUILD)/bench/bench_views \
              $(CORPUS_FILES)
	./$(BUILD)/bench/bench_corpus $(CORPUS_FILES)
	./$(BUILD)/bench/bench_views $(CORPUS_FILES)

bench-map: $(BUILD)/bench/bench_map $(CORPUS_FILES)
	./$< $(CORPUS_FILES)

bench-scale: $(BUILD)/bench/bench_corpus $(RELEASE_BIN) $(SCALE_TEXT) \
             $(SCALE_SEQ)
	./$< --scale $(SCALE_TEXT)
	./$< --scale $(SCALE_SEQ)
	hyperfine -N --warmup 1 --export-markdown $(BUILD)/scale/main.md \
	    -L kind text,seq -L mib $(subst $(space),$(comma),$(SCALE_MIB)) \
	    '$(RELEASE_BIN) $(BUILD)/scale/{kind}-{mib}M'

# clangd reads this for include paths and -std; src/*/*.c resolve via the tests.
compile_commands.json: Makefile $(TEST_SRCS) $(BENCH_SRCS)
	@printf '[' > $@; sep=; \
	for f in main.c $(TEST_SRCS) $(BENCH_SRCS); do \
	    printf '%s\n  {"directory": "%s", "file": "%s", "command": "%s %s"}' \
	        "$$sep" '$(CURDIR)' "$$f" \
	        '$(CC) $(CPPFLAGS) $(DEBUG_FLAGS) $(CFLAGS)' "$$f" >> $@; \
	    sep=,; \
	done; printf '\n]\n' >> $@

# The version in src/base/base.h, for the docs header and the release.
VERSION := $(shell sed -n 's/^\#define MULE_VERSION "\(.*\)"/\1/p' src/base/base.h)

# HTML reference from README.md, docs/ and the doc comments in src/ (needs
# doxygen). The test harness is not in MODULES, so it is added last.
docs:
	@mkdir -p $(BUILD)/docs # doxygen before 1.10 does not create parents
	MULE_DOC_MODULES="$(MODULES:%=src/%) src/test" MULE_DOC_VERSION=$(VERSION) MULE_DOC_OUT=$(BUILD)/docs \
	    doxygen Doxyfile
	@echo "open $(BUILD)/docs/html/index.html"

version:
	@echo $(VERSION)

format:
	clang-format -i main.c $(wildcard src/*/*.[ch] tests/*.[ch] bench/*.[ch])

clean:
	rm -rf $(BUILD)

# Header/source dependency tracking (regenerated by -MMD on each build).
-include $(wildcard $(BUILD)/*/*.d)
