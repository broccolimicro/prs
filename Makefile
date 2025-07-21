NAME          = prs
DEPEND        = sch phy boolean interpret_boolean parse_expression parse_ucs parse common
TEST_DEPEND   = interpret_prs interpret_boolean parse_prs parse_spice parse_dot parse_expression parse_ucs parse sch phy boolean common

COVERAGE ?= 0

ifeq ($(COVERAGE),0)
CXXFLAGS = -std=c++20 -g -Wall -fmessage-length=0 -O2
LDFLAGS  =
else
CXXFLAGS = -std=c++20 -g -Wall -fmessage-length=0 -O0 --coverage -fprofile-arcs -ftest-coverage
LDFLAGS  = --coverage -fprofile-arcs -ftest-coverage 
endif

SRCDIR        = $(NAME)
INCLUDE_PATHS = $(DEPEND:%=-I../%) -I.
LIBRARY_PATHS =
LIBRARIES     =

SOURCES	     := $(shell mkdir -p $(SRCDIR); find $(SRCDIR) -name '*.cpp')
OBJECTS	     := $(SOURCES:%.cpp=build/%.o)
DEPS         := $(shell mkdir -p build/$(SRCDIR); find build/$(SRCDIR) -name '*.d')
TARGET	      = lib$(NAME).a

TESTDIR       = tests

ifndef GTEST
override GTEST=../../googletest
endif

TEST_INCLUDE_PATHS = -I$(GTEST)/googletest/include $(TEST_DEPEND:%=-I../%) -I../gdstk/include $(shell python3-config --includes) -I.
TEST_LIBRARY_PATHS = -L$(GTEST)/build/lib $(TEST_DEPEND:%=-L../%) -L.
TEST_LIBRARIES = -l$(NAME) $(TEST_DEPEND:%=-l%) -pthread -lgtest

TESTS        := $(shell mkdir -p $(TESTDIR); find $(TESTDIR) -name '*.cpp')
TEST_OBJECTS := $(TESTS:%.cpp=build/%.o) build/$(TESTDIR)/gtest_main.o
TEST_DEPS    := $(shell mkdir -p build/$(TESTDIR); find build/$(TESTDIR) -name '*.d')
TEST_TARGET   = test

ifeq ($(OS),Windows_NT)
    CXXFLAGS += -D WIN32
    ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
        CXXFLAGS += -D AMD64
    else
        ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
            CXXFLAGS += -D AMD64
        endif
        ifeq ($(PROCESSOR_ARCHITECTURE),x86)
            CXXFLAGS += -D IA32
        endif
    endif
    LIBRARIES += -l:libgdstk.a -l:libclipper.a -l:libqhullstatic_r.a -lz
    LIBRARY_PATHS += -L../gdstk/build/lib -L../gdstk/build/lib64
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        CXXFLAGS += -D LINUX
        LIBRARIES += -l:libgdstk.a -l:libclipper.a -l:libqhullstatic_r.a -lz
        LIBRARY_PATHS += -L../gdstk/build/lib -L../gdstk/build/lib64
    endif
    ifeq ($(UNAME_S),Darwin)
        CXXFLAGS += -D OSX -mmacos-version-min=15.0 -Wno-missing-braces
        INCLUDE_PATHS += -I$(shell brew --prefix qhull)/include
        LIBRARY_PATHS += -L../gdstk/build/lib -L$(shell brew --prefix qhull)/lib
        LIBRARIES     += -lgdstk -lclipper -lqhullstatic_r -lz
        LDFLAGS	      += -Wl,-rpath,/opt/homebrew/opt/python@3.15/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.14/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.13/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.12/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.11/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.10/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3.09/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python@3/Frameworks/Python.framework/Versions/Current/lib \
-Wl,-rpath,/opt/homebrew/opt/python/Frameworks/Python.framework/Versions/Current/lib
    endif
    UNAME_P := $(shell uname -p)
    ifeq ($(UNAME_P),x86_64)
        CXXFLAGS += -D AMD64
    endif
    ifneq ($(filter %86,$(UNAME_P)),)
        CXXFLAGS += -D IA32
    endif
    ifneq ($(filter arm%,$(UNAME_P)),)
        CXXFLAGS += -D ARM
    endif
endif


all: lib

lib: $(TARGET)

tests: lib $(TEST_TARGET)

coverage: clean
	$(MAKE) COVERAGE=1 tests
	./$(TEST_TARGET) || true  # Continue even if tests fail
	lcov --capture --directory build/$(SRCDIR) --output-file coverage.info
	lcov --ignore-errors unused --remove coverage.info '/usr/include/*' '*/googletest/*' '*/tests/*' --output-file coverage_filtered.info
	genhtml coverage_filtered.info --output-directory coverage_report

$(TARGET): $(OBJECTS)
	ar rvs $(TARGET) $(OBJECTS)

build/$(SRCDIR)/%.o: $(SRCDIR)/%.cpp 
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) -MM -MF $(patsubst %.o,%.d,$@) -MT $@ -c $<
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) -c -o $@ $<

$(TEST_TARGET): $(TEST_OBJECTS) $(OBJECTS) $(TARGET)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_LIBRARY_PATHS) $(TEST_OBJECTS) $(TEST_LIBRARIES) -o $(TEST_TARGET)

build/$(TESTDIR)/%.o: $(TESTDIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(TEST_INCLUDE_PATHS) -MM -MF $(patsubst %.o,%.d,$@) -MT $@ -c $<
	$(CXX) $(CXXFLAGS) $(TEST_INCLUDE_PATHS) $< -c -o $@

build/$(TESTDIR)/gtest_main.o: $(GTEST)/googletest/src/gtest_main.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(TEST_INCLUDE_PATHS) $< -c -o $@

include $(DEPS) $(TEST_DEPS)

clean:
	rm -rf build $(TARGET) $(TEST_TARGET) coverage.info coverage_filtered.info coverage_report *.gcda *.gcno

clean-test:
	rm -rf build/$(TESTDIR) $(TEST_TARGET)

clean-coverage:
	rm -rf coverage.info coverage_filtered.info coverage_report *.gcda *.gcno
