# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/04/26 16:00:00 by nlouis            #+#    #+#              #
#    Updated: 2025/04/27 22:27:13 by nlouis           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

# Compiler settings
CXX       := c++
CXXFLAGS  := -Wall -Wextra -Werror -I include
DEBUGFLAGS:= -g3 -O0 -DDEBUG
OPTFLAGS  := -O3

# Sanitizer flags
ASAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer
TSAN_FLAGS := -fsanitize=thread -fno-omit-frame-pointer
UBSAN_FLAGS:= -fsanitize=undefined -fno-omit-frame-pointer

# Executable output
NAME      := webserv
BINDIR    := bin
TARGET    := $(BINDIR)/$(NAME)

# Source, object, and dependency files
SRCDIR    := src
OBJDIR    := objs
DEPDIR    := deps

SRCS      := $(shell find $(SRCDIR) -name "*.cpp")
OBJS      := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS      := $(patsubst $(SRCDIR)/%.cpp,$(DEPDIR)/%.d,$(SRCS))

# Tests
TESTDIR    := tests
TESTSRCS   := $(shell find $(TESTDIR) -name "*.cpp" 2>/dev/null)
TESTBINS   := $(patsubst $(TESTDIR)/%.cpp,$(BINDIR)/tests/%, $(TESTSRCS))
TESTOBJS   := $(patsubst $(TESTDIR)/%.cpp,$(OBJDIR)/tests/%.o,$(TESTSRCS))
TESTDEPS   := $(patsubst $(TESTDIR)/%.cpp,$(DEPDIR)/tests/%.d,$(TESTSRCS))

# Colors
GREEN     := \033[0;32m
CYAN      := \033[0;36m
YELLOW    := \033[1;33m
RED       := \033[0;31m
RESET     := \033[0m

# Default goal
.DEFAULT_GOAL := all

# Mode control
MODE ?= release
SAN ?= none

ifeq ($(MODE),debug)
	CXXFLAGS += $(DEBUGFLAGS)
	ifeq ($(SAN),asan)
		CXXFLAGS += $(ASAN_FLAGS)
	endif
	ifeq ($(SAN),tsan)
		CXXFLAGS += $(TSAN_FLAGS)
	endif
	ifeq ($(SAN),ubsan)
		CXXFLAGS += $(UBSAN_FLAGS)
	endif
else
	CXXFLAGS += $(OPTFLAGS)
endif

# Build rules
all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BINDIR)
	@$(CXX) $(CXXFLAGS) $^ -o $@
	@echo "$(CYAN)🚀 Built executable:$(RESET) $(TARGET)"

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(@D)
	@if [ "$(FAST)" != "1" ]; then mkdir -p $(DEPDIR)/$(dir $(patsubst $(SRCDIR)/%,%,$<)); fi
	@if [ "$(FAST)" = "1" ]; then \
		$(CXX) $(CXXFLAGS) -c $< -o $@; \
	else \
		$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@ && mv $(basename $@).d $(DEPDIR)/$(notdir $(basename $@)).d; \
	fi
	@echo "$(GREEN)🛠️  Compiled:$(RESET) $<"

# Build each test individually
$(BINDIR)/tests/%: $(TESTDIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $< -o $@
	@echo "$(GREEN)🛠️  Built test executable:$(RESET) $@"

# Cleaning
clean:
	@rm -rf $(OBJDIR) $(DEPDIR)
	@echo "$(YELLOW)🧹 Cleaned object and dependency files.$(RESET)"

fclean: clean
	@rm -rf $(TARGET) $(BINDIR) build
	@echo "$(YELLOW)🗑️  Completely removed executables, binaries, build/, and tests.$(RESET)"

re: fclean all

# Modes
debug:
	@$(MAKE) MODE=debug

debug_asan:
	@$(MAKE) MODE=debug SAN=asan

debug_tsan:
	@$(MAKE) MODE=debug SAN=tsan

debug_ubsan:
	@$(MAKE) MODE=debug SAN=ubsan

release:
	@$(MAKE) MODE=release

# Fast build (no dependency checking)
fast:
	@$(MAKE) FAST=1

# Run shortcut
run: all
	@./bin/webserv

# Test shortcut
test: $(TARGET) $(TESTBINS)
	@echo "$(CYAN)🧪 Running all tests...$(RESET)"
	@if [ -z "$(TESTBINS)" ]; then \
		echo "$(YELLOW)⚠️  No test files found. Skipping tests.$(RESET)"; \
	else \
		for test_bin in $(TESTBINS); do \
			echo "$(CYAN)➡️  Running $$test_bin$(RESET)"; \
			./$$test_bin || (echo "$(RED)❌ Test failed: $$test_bin$(RESET)" && exit 1); \
		done; \
		echo "$(GREEN)🏆 All tests passed successfully!$(RESET)"; \
	fi

# Sanitize target
sanitize:
	@echo "$(CYAN)🔬 Building and testing with AddressSanitizer...$(RESET)"
	@$(MAKE) debug_asan
	@ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:abort_on_error=1:fast_unwind_on_malloc=0:suppressions=$(PWD)/.asanignore" ./bin/webserv & sleep 2 ; kill $$! || true

	@echo "$(CYAN)🔬 Building and testing with ThreadSanitizer...$(RESET)"
	@$(MAKE) debug_tsan
	@TSAN_OPTIONS="suppressions=$(PWD)/.asanignore:halt_on_error=1" ./bin/webserv & sleep 2 ; kill $$! || true

	@echo "$(CYAN)🔬 Building and testing with UndefinedBehaviorSanitizer...$(RESET)"
	@$(MAKE) debug_ubsan
	@UBSAN_OPTIONS="suppressions=$(PWD)/.asanignore:halt_on_error=1:print_stacktrace=1" ./bin/webserv & sleep 2 ; kill $$! || true

	@echo "$(GREEN)🏆 All sanitizer builds completed successfully!$(RESET)"

# Code Quality Targets
format:
	@echo "$(CYAN)🎨 Formatting source files...$(RESET)"
	@clang-format -i src/**/*.cpp include/**/*.hpp tests/**/*.cpp

tidy:
	@echo "$(CYAN)🔍 Running clang-tidy analysis...$(RESET)"
	@clang-tidy src/**/*.cpp tests/**/*.cpp -- -Iinclude -std=c++20

# Help target
help:
	@echo "$(CYAN)📦 Build Targets:$(RESET)"
	@echo "  $(GREEN)make$(RESET)                → Build in release mode (optimized) 🚀"
	@echo "  $(GREEN)make release$(RESET)        → Force build in release mode 🏎️"
	@echo "  $(GREEN)make debug$(RESET)          → Build in debug mode (-g, no optimization) 🐞"
	@echo "  $(GREEN)make debug_asan$(RESET)     → Debug with AddressSanitizer 🛡️"
	@echo "  $(GREEN)make debug_tsan$(RESET)     → Debug with ThreadSanitizer 🔥"
	@echo "  $(GREEN)make debug_ubsan$(RESET)    → Debug with UndefinedBehaviorSanitizer 🚨"
	@echo "  $(GREEN)make fast$(RESET)           → Fast build without dependency scanning ⚡"
	@echo ""
	@echo "$(CYAN)🚀 Run Targets:$(RESET)"
	@echo "  $(GREEN)make run$(RESET)            → Build and run the web server 🚀"
	@echo "  $(GREEN)make test$(RESET)           → Build and run all tests in tests/ 🧪"
	@echo "  $(GREEN)make sanitize$(RESET)       → Build and short-run under sanitizers 🔬"
	@echo ""
	@echo "$(CYAN)🧹 Code Quality Targets:$(RESET)"
	@echo "  $(GREEN)make format$(RESET)         → Format all source files 🎨"
	@echo "  $(GREEN)make tidy$(RESET)           → Run clang-tidy static analysis 🔍"
	@echo ""
	@echo "$(CYAN)🧹 Cleaning Targets:$(RESET)"
	@echo "  $(GREEN)make clean$(RESET)          → Remove object and dependency files 🧹"
	@echo "  $(GREEN)make fclean$(RESET)         → Remove everything (objs, deps, executables, build/) 🗑️"
	@echo "  $(GREEN)make re$(RESET)             → Clean and rebuild everything 🔁"
	@echo ""
	@echo "$(CYAN)📚 Other:$(RESET)"
	@echo "  $(GREEN)make help$(RESET)           → Show this help message 📚"

.PHONY: all clean fclean re debug debug_asan debug_tsan debug_ubsan release run test sanitize fast help

# Include dependency files unless FAST
ifeq ($(FAST),)
-include $(DEPS) $(TESTDEPS)
endif
