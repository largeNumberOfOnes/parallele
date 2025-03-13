# CC = mpicc

comp:
	mpicc tasks/task_5_send_test.c
	# mpic++ task_4_exp.cpp

run:
	mpiexec -n 2 ./a.out 100

cr: comp run
	# ---
