WFLAGS = -Wall -Wextra
RFLAFS = -O3 -DNDEBUG
DFLAFS = -g -fsanitize=address

lint:
	cppcheck --language=c++ -q main.cpp global_stack_alg.cpp stack.h range.h

comp:
	g++ $(RFLAFS) $(WFLAGS) main.cpp global_stack_alg.cpp
	# g++ $(DFLAFS) $(WFLAGS) main.cpp global_stack_alg.cpp

run:
	./a.out

car: comp run

test_stack:
	g++ -std=c++20 -g test_stack.cpp
	./a.out

