
FLAGS := -g

comp:
	gcc $(FLAGS) game.c -o game.out
mcomp:
	mpicc $(FLAGS) game.c -o game.out
run:
	./game.out
mrun:
	mpiexec -n 1 ./game.out

car: comp run
mcar: mcomp mrun

