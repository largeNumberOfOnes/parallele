#!/bin/python3

# This is attempt to 

# Задача: написать систему сборки задач по курсу параллельного програмирования 
# Структура каталога курса:
#     tasks - директоия, содержащая все файлы с заданиями. Каждое задание
#         должно быть представленно в виде одного файла с расширением '.c' или
#         '.cpp' или директорией. Правило наименования для файлов и директорий 
#         следующее: 'task_N_M[_comment][.c | .cpp]' где N - номер занятия,   
#         M - номер задачи.
#     execs - директоия, содержащая бинарные файлы проекта. Названия файлов
#         совпадают с названиями в директории tasks.
#     slibs - директоия, содержащая заголовочные файлы библиотек.

# Правила запуска сборщика:
#     ./script.py {c | r | car} <task> [args]
#     поле task может содержать имя задачи или шаблон вида 'task <N> <M>'
#     , где N - номер занятия, M - номер задачи.
#     args:
#         -n      number of processes

# task 1 4 # task 4 from pull 1


# import os
# os.system('ls')

# import subprocess
# output = subprocess.run(['ls', '-l'], capture_output=True, text=True)
# print(output.stdout)

# You can select task by printing task_N or by full name.

# Нада учесть выбор компилятора, языка и режима сборки

# План
#    Написать сборщик
#    Проверить нулики
#    Создать репозиторий
#    Протестировать всё на сервере

import argparse
import sys
import os

def search_task_name(task: str) -> str:
    files = os.listdir('./tasks')
    if task not in files:
        return ''
    return 'tasks/' + task
    # print(files)

# parser = argparse.ArgumentParser(
#     prog='ProgramName',
#     description='What the program does',
#     epilog='Text at the bottom of help'
# )

def compile_task(task: str, mpi: bool=False) -> None:
    print('compiling...')
    if mpi:
        # os.system(f'mpicc -g {task}')
        os.system(f'mpic++ -g {task}')
    else:
        os.system(f'g++ -g {task}')

def run_task(binary: str, nproc: int = 1, args: str = '', mpi: bool=False) -> None:
    print('running...')
    if mpi:
        os.system(f'mpiexec -n {nproc} ./{binary} {args}')
    else:
        os.system(f'./{binary} {args}')

def parse_args():
    pass

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        prog='ProgramName',
        description='What the program does',
        epilog='Text at the bottom of help'
    )
    parser.add_argument('mode', choices=('c', 'r', 'car', 'mcar', 'mr'), help='mode of execution')
    parser.add_argument('task', type=str, help='name of task')
    # subparsers = parser.add_subparsers(title='subcommands',
    #                                description='valid subcommands',
    #                                help='additional help')
    parser.add_argument('-nproc', type=int, help='number of processes')
    parser.add_argument('-args', type=str, help='args to the program')
    args = parser.parse_args()
    task = search_task_name(args.task)
    binary = 'a.out'
    do_dict = {
        'c':   lambda: compile_task(task),
        'r':   lambda: run_task(binary, args.nproc),
        'car': lambda: (
                compile_task(task),
                run_task(binary, args.nproc, args.args)
            ),
        'mcar': lambda: (
                compile_task(task, mpi=True),
                run_task(binary, args.nproc, args.args, mpi=True)
            ),
        'mr': lambda: (
                run_task(binary, args.nproc, args.args, mpi=True)
            ),
    }
    do_dict[args.mode]()
