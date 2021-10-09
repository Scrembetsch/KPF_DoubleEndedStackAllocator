all:
	clang++ KPF_DoubleEndedStackAllocator/src/*.cpp -g -Wall -Werror -Wextra -pedantic -std=c++14 -o DoubleEndedStackAllocator