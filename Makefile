comp_all: comp_client comp_server

comp_client:
	gcc tasks/task_10_game_of_live/client.c -o execs/client.out

comp_server:
	gcc tasks/task_10_game_of_live/server.c -o execs/server.out

run_server:
	sbatch --output=logs/out.txt --error=logs/err.txt --ntasks=1 run.sh

scp_gol:
	scp -P 10195 -r Makefile rt20250211@proxy2.cod.phystech.edu:parallele/Makefile
	scp -P 10195 -r run.sh rt20250211@proxy2.cod.phystech.edu:parallele/run.sh
	scp -P 10195 -r tasks/task_10_game_of_live/client.c rt20250211@proxy2.cod.phystech.edu:parallele/tasks/task_10_game_of_live/client.c
	scp -P 10195 -r tasks/task_10_game_of_live/server.c rt20250211@proxy2.cod.phystech.edu:parallele/tasks/task_10_game_of_live/server.c
