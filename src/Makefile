# 编译器
CXX = g++

# 编译选项
CXXFLAGS = -Iinclude -O2 -DDEBUG -Wall -g

# 链接选项
LDFLAGS = -O2

# 目标文件
TARGET = main

# 源文件
SRCS = $(wildcard *.cpp)

# 生成的对象文件
OBJS = $(SRCS:.cpp=.o)

# 默认目标
all: $(TARGET)

# 链接
$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

# 编译
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean