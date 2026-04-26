CXX ?= g++
CC ?= cc
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread
CFLAGS ?= -O2 -Wall -Wextra
CPPFLAGS ?=
LDFLAGS ?=
LDLIBS ?= -pthread

TARGET := boost_Server
CXX_SOURCES := boost_Server.cpp Auth.cpp StaticFiles.cpp
C_SOURCES := third_party/sqlite/sqlite-amalgamation-3530000/sqlite3.c
OBJECTS := $(CXX_SOURCES:.cpp=.o) $(C_SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(LDFLAGS) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJECTS)
