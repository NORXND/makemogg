CC = gcc
CXX = g++
CFLAGS = -O2 -std=c11
CXXFLAGS = -O2 -std=c++17

SRCS = makemogg_lib.cpp aes.c VorbisEncrypter.cpp OggMap.cpp oggvorbis.cpp CCallbacks.cpp
LIBNAME = makemogg

ifeq ($(OS),Windows_NT)
    SHARED_LIB = $(LIBNAME).dll
    SHARED_FLAGS = -shared -static-libgcc -static-libstdc++
    PICFLAG =
    RM = del /Q
    CFLAGS += -DMAKEMOGG_EXPORTS
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        SHARED_LIB = lib$(LIBNAME).dylib
        SHARED_FLAGS = -dynamiclib
        PICFLAG = -fPIC
    else
        SHARED_LIB = lib$(LIBNAME).so
        SHARED_FLAGS = -shared
        PICFLAG = -fPIC
    endif
    RM = rm -f
endif

# Object files for shared library need -fPIC
SHARED_OBJS = $(SRCS:.cpp=.so.o)
SHARED_OBJS := $(SHARED_OBJS:.c=.so.o)

all: $(SHARED_LIB)

$(SHARED_LIB): $(SHARED_OBJS)
	$(CXX) $(CXXFLAGS) $(SHARED_FLAGS) -o $(SHARED_LIB) $(SHARED_OBJS)

%.so.o: %.cpp
	$(CXX) $(CXXFLAGS) $(PICFLAG) -c $< -o $@

%.so.o: %.c
	$(CC) $(CFLAGS) $(PICFLAG) -c $< -o $@

clean:
	$(RM) $(SHARED_LIB) *.so.o
