CC := g++
CFLAGS := -std=c++11 -I/usr/include
LDFLAGS := -lpthread 

SRCS := logger.cpp main.cpp

OBJS := $(SRCS:.cpp=.o)

EXEC := log_util

.PHONY: all clean

dummy: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJS) $(EXEC)