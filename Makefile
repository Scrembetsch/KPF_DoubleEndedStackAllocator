all:
	clang++ KPF_DoubleEndedStackAllocator/src/*.cpp -g -Wall -Wextra -pedantic -std=c++14 -o DoubleEndedStackAllocator
