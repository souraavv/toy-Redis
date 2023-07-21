# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -Wextra -O2 -g

# Source files
SRCS_SERVER = server.cpp
SRCS_CLIENT = client.cpp

# Output files
TARGET_SERVER = server
TARGET_CLIENT = client

# Rules
all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): $(SRCS_SERVER)
	$(CXX) $(CXXFLAGS) $(SRCS_SERVER) -o $(TARGET_SERVER)

$(TARGET_CLIENT): $(SRCS_CLIENT)
	$(CXX) $(CXXFLAGS) $(SRCS_CLIENT) -o $(TARGET_CLIENT)

clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT)
