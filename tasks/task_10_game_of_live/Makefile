
FLAGS := -O3

comp:
	gcc $(FLAGS) game.c -o game.out
mcomp:
	mpicc $(FLAGS) -DGTYPE=1 -DGCOUNT=3 -DGMODE=1 game.c -o game.out
run:
	./game.out
mrun:
	mpiexec -n 2 ./game.out

car: comp run
mcar: mcomp mrun

