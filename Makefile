# December 10, 2017
# Makefile - generic
# For a full walkthrough of how this makefile works, see Learn C Hard Way Ch 28 
# Examples:
#		make alone compiles all libraries: .a, .so, all tests, and the full GUI program
#		make dev compiles all as above but also makes sure debug flags are in
#		make tests compiles tests and libs to run the tests; then runs runtests.sh
#		make clean removes everything so forces new compilation
#		make check looks into the sources to see which files have dangerous funcs
#		make install runs "all" then moves lib into given dir; default is /usr/local
#		make build just makes sure the build and bin folders exist in project tree
#				 build is used by other targets for the most part; not intended for user


CPPFLAGS=-g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Isrc -rdynamic -DNDEBUG $(OPTFLAGS)
LDLIBS=-ldl $(OPTFLAGS)
PREFIX?=/usr/local

SOURCES=$(wildcard src/lib/**/*.cpp src/lib/*.cpp)
OBJECTS=$(patsubst %.cpp,%.o,$(SOURCES))

TEST_SRC=$(wildcard tests/*_tests.cpp)
TESTS=$(patsubst %.cpp,%,$(TEST_SRC))

PROGRAMS_SRC=$(wildcard bin/*.cpp)
PROGRAMS=$(patsubst %.cpp,%,$(PROGRAMS_SRC))

TARGET=build/libPaqij.a
SO_TARGET=$(patsubst %.a,%.so,$(TARGET))

GUI_SRC=$(wildcard src/GUI/**/*.cpp src/GUI/*.cpp)
GUI_TARGET=$(patsubst %.cpp,%,$(GUI_SRC))

# The Target Build
all: $(TARGET) $(SO_TARGET) tests $(GUI_TARGET) $(PROGRAMS) 

dev: CPPFLAGS=-g -O2 -Wall -Isrc -Wall -Wextra $(OPTFLAGS)
dev: all # dev is for when you want a public build, no debugging code, fast

$(TARGET): CPPFLAGS += -fPIC
$(TARGET): build $(OBJECTS)
	ar rcs $@ $(OBJECTS)
	ranlib $@
$(SO_TARGET): $(TARGET) $(OBJECTS)
	$(CC) -shared -o $@ $(OBJECTS)

build:
	@mkdir -p build
	@mkdir -p bin

$(GUI_TARGET): LDLIBS += -L/usr/X11R6/lib -lX11 -lm -lasound $(TARGET)

$(PROGRAMS): LDLIBS += $(TARGET)

# The Unit Tests
.PHONY: tests
tests: LDLIBS += -lbsd $(TARGET)
tests: $(TESTS)
	sh ./tests/runtests.sh

# The Cleaner
clean:
	rm -rf build $(OBJECTS) $(TESTS) $(PROGRAMS) $(GUI_TARGET)
	rm -f tests/tests.log
	find . -iname "*.gc*" -exec rm {} \;
	rm -rf `find . -iname "*.dSYM" -print`

# The Install
install: all
	install -d $(DESTDIR)/$(PREFIX)/lib/
	install $(TARGET)/$(DESTDIR)/$(PREFIX)/lib/

#The Checker
check:
	@echo Files with potentially dangerous functions.
	@egrep '[^_.>a-zA-Z0-9](str(n?cpy|n?cat|xfrm|n?dup|str|pbrk|tok|_)\
            |stpn?cpy|a?sn?printf|byte_)' $(SOURCES) || true
