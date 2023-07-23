# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -Wextra -O2 -g

# Source files
CLIENT_SRC = client.cpp
SERVER_SRC = server.cpp
HASHTABLE_SRC = hashtable.cpp

# Object files
CLIENT_OBJ = $(CLIENT_SRC:.cpp=.o)
SERVER_OBJ = $(SERVER_SRC:.cpp=.o)
HASHTABLE_OBJ = $(HASHTABLE_SRC:.cpp=.o)

# DLL name and options
HASHTABLE_DLL = libhashtable.so
DLL_CXXFLAGS = -shared -fPIC

# Targets
CLIENT_TARGET = client
SERVER_TARGET = server

# Default target
all: $(HASHTABLE_DLL) $(CLIENT_TARGET) $(SERVER_TARGET)

# Compile client
$(CLIENT_OBJ): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -c $(CLIENT_SRC) -o $(CLIENT_OBJ)

# Compile server
$(SERVER_OBJ): $(SERVER_SRC) hashtable.h
	$(CXX) $(CXXFLAGS) -c $(SERVER_SRC) -o $(SERVER_OBJ)

# Compile hashtable object for DLL
$(HASHTABLE_OBJ): $(HASHTABLE_SRC) hashtable.h
	$(CXX) $(DLL_CXXFLAGS) $(CXXFLAGS) -c $(HASHTABLE_SRC) -o $(HASHTABLE_OBJ)

# Create hashtable DLL
$(HASHTABLE_DLL): $(HASHTABLE_OBJ)
	$(CXX) $(DLL_CXXFLAGS) $(CXXFLAGS) $(HASHTABLE_OBJ) -o $(HASHTABLE_DLL)

# Link client with the hashtable DLL
$(CLIENT_TARGET): $(CLIENT_OBJ) $(HASHTABLE_DLL)
	$(CXX) $(CXXFLAGS) $(CLIENT_OBJ) -L. -lhashtable -o $(CLIENT_TARGET)

# Link server with the hashtable DLL
$(SERVER_TARGET): $(SERVER_OBJ) $(HASHTABLE_DLL)
	$(CXX) $(CXXFLAGS) $(SERVER_OBJ) -L. -lhashtable -o $(SERVER_TARGET)

# Clean intermediate object files, DLL, and executables
clean:
	rm -f $(CLIENT_OBJ) $(SERVER_OBJ) $(HASHTABLE_OBJ) $(CLIENT_TARGET) $(SERVER_TARGET) $(HASHTABLE_DLL)
