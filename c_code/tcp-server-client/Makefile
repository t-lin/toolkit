#CPPFLAGS +=
LDFLAGS += -lpthread
CXXFLAGS += -std=gnu++17 -O0 -g -Wall -Wsign-compare -Wconversion -Wpedantic
CXX=g++

SERVER_BINNAME = tcp-server
CLIENT_BINNAME = tcp-client

all: $(SERVER_BINNAME) $(CLIENT_BINNAME)

$(SERVER_BINNAME): tcp-server.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

$(CLIENT_BINNAME): tcp-client.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(SERVER_BINNAME) $(CLIENT_BINNAME)
