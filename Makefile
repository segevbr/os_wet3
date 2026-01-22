# Compiler and Flags
CXX = g++
# Strict compilation flags as required by the PDF (Source 192, 194)
# NOTE: -DNDEBUG disables assert(). For debugging your tests, remove -DNDEBUG temporarily.
CXXFLAGS = -std=c++11 -Wall -Werror -pedantic-errors -pthread
LDFLAGS = -pthread

# Targets
TARGET = my_tests
# Assumes your test file is named 'my_tests.cpp'. Change if you named it 'main.cpp'
OBJS = customAllocator.o my_tests.o

# Default rule: build the executable
all: $(TARGET)

# Link the object files to create the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

# Compile the allocator
customAllocator.o: customAllocator.cpp customAllocator.h
	$(CXX) $(CXXFLAGS) -c customAllocator.cpp

# Compile the tests
my_tests.o: my_tests.cpp customAllocator.h
	$(CXX) $(CXXFLAGS) -c my_tests.cpp

# Clean up build files
clean:
	rm -f $(TARGET) *.o

# Helper to run tests immediately
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run