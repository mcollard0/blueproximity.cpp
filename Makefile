CXX = g++
CXXFLAGS = -Wall -O3 -std=c++17
LDFLAGS = -lbluetooth

# Auto-detect number of processors and use nproc-2
NPROCS := $(shell nproc)
JOBS := $(shell expr $(NPROCS) - 2)
MAKEFLAGS += -j$(JOBS)

TARGET = BlueProximity
SRCS = main.cpp BlueProximity.cpp ConfigFile.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Setting capabilities..."
	-sudo setcap 'cap_net_raw,cap_net_admin+eip' $(TARGET) || echo "Warning: Failed to set capabilities."

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
