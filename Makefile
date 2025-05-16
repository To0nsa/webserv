# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/04/26 16:00:00 by nlouis            #+#    #+#              #
#    Updated: 2025/05/16 19:08:31 by nlouis           ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

# Compiler settings
CXX        := c++
CXXFLAGS   := -Wall -Wextra -Werror -I include
CXXFLAGS   += -std=c++17
DEBUGFLAGS := -g3 -O0 -DDEBUG
OPTFLAGS   := -O3

# Sanitizer flags
ASAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer
UBSAN_FLAGS:= -fsanitize=undefined -fno-omit-frame-pointer

# Executable output
NAME       := webserv
BINDIR     := bin
TARGET     := $(BINDIR)/$(NAME)

# Source, object, and dependency files
SRCDIR     := src
OBJDIR     := objs
DEPDIR     := deps

SRCS       := $(shell find $(SRCDIR) -name "*.cpp")
OBJS       := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS       := $(patsubst $(SRCDIR)/%.cpp,$(DEPDIR)/%.d,$(SRCS))

# Tests
TESTDIR    := tests
TESTSRCS   := $(shell find $(TESTDIR) -name "*.cpp" 2>/dev/null)
TESTBINS   := $(patsubst $(TESTDIR)/%.cpp,$(BINDIR)/tests/%, $(TESTSRCS))
TESTOBJS   := $(patsubst $(TESTDIR)/%.cpp,$(OBJDIR)/tests/%.o,$(TESTSRCS))
TESTDEPS   := $(patsubst $(TESTDIR)/%.cpp,$(DEPDIR)/tests/%.d,$(TESTSRCS))

# Colors
GREEN      := \033[0;32m
CYAN       := \033[0;36m
YELLOW     := \033[1;33m
RED        := \033[0;31m
RESET      := \033[0m

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

# Prepare necessary directories
prepare_dirs:
	@mkdir -p $(BINDIR) $(BINDIR)/tests $(OBJDIR) $(DEPDIR)

# Build rules
all: prepare_dirs $(TARGET)

$(TARGET): $(OBJS)
	@$(CXX) $(CXXFLAGS) $^ -o $@
	@echo "$(CYAN)🚀 Built executable:$(RESET) $(TARGET)"

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@) $(DEPDIR)/$(dir $(patsubst $(OBJDIR)/%,%,$@))
	@if [ "$(FAST)" = "1" ]; then \
		$(CXX) $(CXXFLAGS) -c $< -o $@; \
	else \
		$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@; \
	fi
	@echo "$(GREEN)🛠️  Compiled:$(RESET) $<"

OBJS_NO_MAIN := $(filter-out %/main.o, $(OBJS))
$(BINDIR)/tests/%: $(TESTDIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $< $(OBJS_NO_MAIN) -o $@
	@echo "$(GREEN)🛠️  Built test executable:$(RESET) $@"

$(BINDIR)/tests/slow_%: $(TESTDIR)/slow_%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $< -o $@
	@echo "$(GREEN)🛠️  Built standalone test:$(RESET) $@"

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

debug_ubsan:
	@$(MAKE) MODE=debug SAN=ubsan

release:
	@$(MAKE) MODE=release

fast:
	@$(MAKE) FAST=1

# Run shortcut
run: all
	@./$(TARGET)

# Test shortcut
test: prepare_dirs $(TARGET) $(TESTBINS)
	@echo "$(CYAN)🧪 Launching web server in background...$(RESET)"
	@./$(TARGET) & echo $$! > .webserv_test.pid

	@sleep 1  # give the server a second to bind to port

	@echo "$(CYAN)🧪 Running all tests...$(RESET)"
	@if [ -z "$(TESTBINS)" ]; then \
		echo "$(YELLOW)⚠️  No test files found. Skipping tests.$(RESET)"; \
	else \
		FAILED=0; \
		for test_bin in $(TESTBINS); do \
			echo "$(CYAN)➡️  Running $$test_bin$(RESET)"; \
			./$$test_bin || { echo "$(RED)❌ Test failed: $$test_bin$(RESET)"; FAILED=1; }; \
		done; \
		if [ $$FAILED -eq 0 ]; then \
			echo "$(GREEN)🏆 All tests passed successfully!$(RESET)"; \
		else \
			echo "$(RED)❌ Some tests failed.$(RESET)"; \
			kill `cat .webserv_test.pid` >/dev/null 2>&1 || true; \
			rm -f .webserv_test.pid; \
			exit 1; \
		fi \
	fi

	@echo "$(CYAN)🧹 Shutting down test server...$(RESET)"
	@kill `cat .webserv_test.pid` >/dev/null 2>&1 || true
	@rm -f .webserv_test.pid

	@echo "$(CYAN)🧪 Running Python CGI tests...$(RESET)"
	@python3 tests/test_cgi.py || { \
		echo "$(RED)❌ Python CGI tests failed.$(RESET)"; \
		kill `cat .webserv_test.pid` >/dev/null 2>&1 || true; \
		rm -f .webserv_test.pid; \
		exit 1; \
	}

# Code Quality Targets
format:
	@echo "$(CYAN)🎨 Formatting source files...$(RESET)"
	@find src include tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

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

.PHONY: all clean fclean re debug debug_asan debug_tsan debug_ubsan release run test sanitize fast help prepare_dirs

# Include dependency files unless FAST
ifeq ($(FAST),)
-include $(DEPS) $(TESTDEPS)
endif
