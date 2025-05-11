
FLAGS := -g

comp:
	gcc $(FLAGS) game.c -o game.out
mcomp:
	mpicc $(FLAGS) -DGTYPE=1 -DGCOUNT=1 game.c -o game.out
run:
	./game.out
mrun:
	mpiexec -n 8 ./game.out

car: comp run
mcar: mcomp mrun

