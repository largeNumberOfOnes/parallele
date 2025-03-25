#!/bin/python3

# This is attempt to 

# Задача: написать систему сборки задач по курсу параллельного програмирования 
# Структура каталога курса:
#     tasks - директоия, содержащая все файлы с заданиями. Каждое задание
#         должно быть представленно в виде одного файла с расширением '.c' или
#         '.cpp' или директорией. Правило наименования для файлов и директорий 
#         следующее: 'task_N[_comment][.c, .cpp]' где N - номер задачи.
#     execs - директоия, содержащая бинарные файлы проекта. Названия файлов
#         совпадают с названиями в директории tasks.
#     slibs - директоия, содержащая заголовочные файлы библиотек.

# Правила запуска сборщика:
#     ./script.py {c, r, car, mc, mr, mcar} <task> [args]
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

# TODO
#    Add support for directory-tasks

import argparse
import sys
import os



class Task:
    def __init__(self, task_pat: str) -> None:
        
        dir_cont = os.listdir('tasks')
        try:
            n = int(task_pat)
            for obj in dir_cont:
                pat = f'task_{n}'
                if pat == obj[:len(pat)] and obj[len(pat)] in ['.', '_']:
                    self.name = obj[:obj.rfind('.')]
                    self.path = 'tasks/' + obj
                    return
            else:
                raise('No such task')
        except:
            for obj in dir_cont:
                if task_pat == obj:
                    self.name = obj[:obj.rfind('.')]
                    self.path = 'tasks/' + obj
                    return
            else:
                raise('No such task')
    
    def get_path(self) -> str:
        return self.path
    
    def get_name(self) -> str:
        return self.name
    
    def get_exec_path(self) -> str:
        return f'execs/{self.name}.out'

def compile_task(args: argparse.Namespace) -> None:
    print('compiling...')
    args = vars(args)
    
    comp = ''
    flags = ''
    inp_name = ''
    out_name = ''

    if not args['no_presets']:
        flags += ' -g'
    if args['args'] is not None:
        flags += ' ' + ' '.join(args['flags'])
    
    task = Task(args['task'])
    inp_name = task.get_path()
    out_name = f'execs/{task.get_name()}.out'

    if task.get_name()[-2:] == '.c':
        comp = 'gcc'
    else:
        comp = 'g++'

    os.system(f'{comp} {flags} {inp_name} -o {out_name}')

def mpi_compile_task(args: argparse.Namespace) -> None:
    print('compiling...')
    args = vars(args)
    
    comp = ''
    inp_name = ''
    out_name = ''

    task = Task(args['task'])
    inp_name = task.get_path()
    out_name = f'execs/{task.get_name()}.out'

    if task.get_name()[-2:] == '.c':
        comp = 'mpicc'
    else:
        comp = 'mpic++'

    os.system(f'{comp} {inp_name} -o {out_name}')

def run_task(args: argparse.Namespace) -> None:
    print('running...')

    os.system('./{} {}'.format(
            Task(args.task).get_exec_path(),
            ' '.join(args.args) if args.args is not None else ''
        )
    )

def mpi_run_task(args: argparse.Namespace) -> None:
    print('running...')

    os.system('mpiexec -n {} ./{} {}'.format(
            args.np,
            Task(args.task).get_exec_path(),
            ' '.join(args.args) if args.args is not None else ''
        )
    )

def comp_and_run(args: argparse.Namespace) -> None:
    run_task(args)
    compile_task(args)

def mpi_comp_and_run(args: argparse.Namespace) -> None:
    mpi_compile_task(args)
    mpi_run_task(args)

def open_vim(args: argparse.Namespace) -> None:
    os.system(f'vim {Task(args.task).get_path()} +{args.n}')

def open_vscode(args: argparse.Namespace) -> None:
    os.system(f'code -g {Task(args.task).get_path()}:{args.n}')

def parse_args() -> None:
    parser = argparse.ArgumentParser(
        prog='ProgramName',
        description='What the program does',
        epilog='Text at the bottom of help'
    )
    subparsers = parser.add_subparsers(title='commands')


    subparsers_comp = subparsers.add_parser(
        'compile',
        aliases=['c', 'comp'],
        help='Compile task using gcc compiler'
    )
    subparsers_comp.add_argument(
        'task',
        type=str,
        help='Full name of the task or task number'
    )
    subparsers_comp.add_argument(
        '--no_presets',
        action='store_true',
        help='Do not use preset flags'
    )
    subparsers_comp.add_argument(
        '--flags',
        nargs='+',
        help='Set flags for the compiler'
    )
    subparsers_comp.set_defaults(
        func=compile_task
    )


    subparsers_comp = subparsers.add_parser(
        'mpi_compile',
        aliases=['mc', 'mcomp'],
        help='Compile task using mpi compiler'
    )
    subparsers_comp.add_argument(
        'task',
        type=str,
        help='Full name of the task or task number'
    )
    subparsers_comp.set_defaults(
        func=mpi_compile_task
    )

    subparsers_comp = subparsers.add_parser(
        'run',
        aliases=['r'],
        help='Run the program in the normal way'
    )
    subparsers_comp.add_argument('task', type=str, help='Full name of the task or task number')
    subparsers_comp.add_argument('--args', nargs='+', help='Set flags for the running program')
    subparsers_comp.set_defaults(func=run_task)

    subparsers_comp = subparsers.add_parser(
        'mpi_run',
        aliases=['mr'],
        help='Run the program in using mpi environment'
    )
    subparsers_comp.add_argument('task', type=str, help='Full name of the task or task number')
    subparsers_comp.add_argument('-np', type=int, default=1, help='Number of processes')
    subparsers_comp.add_argument('--args', nargs='+', help='Set flags for the running program')
    subparsers_comp.set_defaults(func=mpi_run_task)

    subparsers_comp = subparsers.add_parser(
        'mpi_compile_and_run',
        aliases=['mcar'],
        help='Compile and run the program in using mpi environment'
    )
    subparsers_comp.add_argument('task', type=str, help='Full name of the task or task number')
    subparsers_comp.add_argument('-np', type=int, default=1, help='Number of processes')
    subparsers_comp.add_argument('--args', nargs='+', help='Set flags for the compiler')
    subparsers_comp.set_defaults(func=mpi_comp_and_run)

    subparsers_comp = subparsers.add_parser(
        'vim',
        help='Open task with vim'
    )
    subparsers_comp.add_argument('task', type=str, help='Full name of the task or task number')
    subparsers_comp.add_argument('-n', type=int, default=1, help='Number of processes')
    subparsers_comp.set_defaults(func=open_vim)

    subparsers_comp = subparsers.add_parser(
        'code',
        help='Open task with vscode'
    )
    subparsers_comp.add_argument('task', type=str, help='Full name of the task or task number')
    subparsers_comp.add_argument('-n', type=int, default=1, help='Number of processes')
    subparsers_comp.set_defaults(func=open_vscode)

    return parser.parse_args()

if __name__ == '__main__':
    args = parse_args()
    args.func(args)
