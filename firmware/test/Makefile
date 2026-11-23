# Host unit tests for RobotCarCore: plain asserts, no framework.
# Usage: make -C firmware/test

CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -pedantic
CORE_SRC := ../libraries/RobotCarCore/src/RobotCarCore.cpp
CORE_HDR := ../libraries/RobotCarCore/src

all: robot_car_core_test

robot_car_core_test: test_robot_car_core.cpp $(CORE_SRC)
	$(CXX) $(CXXFLAGS) -I$(CORE_HDR) $^ -o $@

run: robot_car_core_test
	./robot_car_core_test

clean:
	rm -f robot_car_core_test

.PHONY: all run clean
