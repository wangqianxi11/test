

CXX = g++
CFLAGS = -std=c++17 -O2 -Wall -g 

TARGET = server
OBJS = ../code/log/*.cpp ../code/pool/*.cpp ../code/timer/*.cpp \
       ../code/http/*.cpp ../code/server/*.cpp  ../code/processing/*.cpp\
       ../code/buffer/*.cpp ../code/main.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o ../bin/$(TARGET)  -lpthread -lmysqlclient -lcpprest -lssl -lcrypto -lhiredis -lredis++ -lbcrypt

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)


