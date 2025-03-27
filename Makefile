NAME          = prs
DEPEND        = sch phy boolean interpret_boolean parse_expression parse_ucs parse common
TEST_DEPEND   = interpret_prs prs interpret_boolean parse_prs parse_spice parse_dot parse_expression parse_ucs parse sch phy boolean common

CXXFLAGS      = -std=c++17 -O2 -g -Wall -fmessage-length=0
LDFLAGS       =  

SRCDIR        = $(NAME)
INCLUDE_PATHS = $(DEPEND:%=-I../%) -I.
LIBRARY_PATHS =
LIBRARIES     =

SOURCES	     := $(shell mkdir -p $(SRCDIR); find $(SRCDIR) -name '*.cpp')
OBJECTS	     := $(SOURCES:%.cpp=build/%.o)
DEPS         := $(shell mkdir -p build/$(SRCDIR); find build/$(SRCDIR) -name '*.d')
TARGET        = lib$(NAME).a

TESTDIR       = tests
GTEST        := ../../googletest
TEST_INCLUDE_PATHS = -I$(GTEST)/googletest/include $(TEST_DEPEND:%=-I../%) -I.
TEST_LIBRARY_PATHS = -L$(GTEST)/build/lib $(TEST_DEPEND:%=-L../%) -L.
TEST_LIBRARIES = $(TEST_DEPEND:%=-l%) -pthread -lgtest

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
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        CXXFLAGS += -D LINUX
    endif
    ifeq ($(UNAME_S),Darwin)
        CXXFLAGS += -D OSX -mmacos-version-min=12.0 -Wno-varargs
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

$(TARGET): $(OBJECTS)
	ar rvs $(TARGET) $(OBJECTS)

build/$(SRCDIR)/%.o: $(SRCDIR)/%.cpp 
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) -MM -MF $(patsubst %.o,%.d,$@) -MT $@ -c $<
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(INCLUDE_PATHS) -c -o $@ $<

$(TEST_TARGET): $(TEST_OBJECTS) $(OBJECTS) $(TARGET)
	$(CXX) $(CXXFLAGS) $(TEST_LIBRARY_PATHS) $(TEST_OBJECTS) $(TEST_LIBRARIES) -o $(TEST_TARGET)

build/$(TESTDIR)/%.o: $(TESTDIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(TEST_INCLUDE_PATHS) -MM -MF $(patsubst %.o,%.d,$@) -MT $@ -c $<
	$(CXX) $(CXXFLAGS) $(TEST_INCLUDE_PATHS) $< -c -o $@

build/$(TESTDIR)/gtest_main.o: $(GTEST)/googletest/src/gtest_main.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(TEST_INCLUDE_PATHS) $< -c -o $@

include $(DEPS) $(TEST_DEPS)

clean:
	rm -rf build $(TARGET) $(TEST_TARGET)

cleantest:
	rm -rf build/$(TESTDIR) $(TEST_TARGET)
