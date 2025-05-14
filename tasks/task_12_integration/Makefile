WFLAGS = -Wall -Wextra
RFLAFS = -O3 -DNDEBUG
DFLAFS = -g -fsanitize=address
MACRO = "-DFUNCTION=sin(1/x)" "-DINT_A=0.0001" "-DINT_B=1" "-DEPS=0.00000001" "-DPROC=4"

runs:
	sbatch --output=out.txt ./run_sc.sh

lint:
	cppcheck --language=c++ -q main.cpp global_stack_alg.cpp stack.h range.h

comp:
	g++ $(RFLAFS) $(WFLAGS) $(MACRO) main.cpp global_stack_alg.cpp
	# g++ $(DFLAFS) $(WFLAGS) main.cpp global_stack_alg.cpp

run:
	./a.out

car: comp run

test_stack:
	g++ -std=c++20 -g test_stack.cpp
	./a.out

