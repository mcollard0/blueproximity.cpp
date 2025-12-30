CXX = g++
CXXFLAGS = -Wall -O3 -std=c++17
LDFLAGS = -lbluetooth

TARGET = BlueProximity
SRCS = main.cpp BlueProximity.cpp ConfigFile.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
