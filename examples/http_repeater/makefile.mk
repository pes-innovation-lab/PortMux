CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2

SRCS = ./interceptor.cpp ./functions.cpp
HEADERS = ./headers.hpp
OBJ = $(SRCS:.cpp=.o)
EXEC = repeater

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) $(OBJ) -o $(EXEC)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)
