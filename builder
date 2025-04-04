#!/bin/python3

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

# Course catalog structure:
#     tasks - A directory containing all files with tasks. Each task
#        must be represented as a single file with the extension '.c'
#        or '.cpp' or a directory. The naming rule for files and
#        directories is as follows: 'task_N[_comment][.c | .cpp]',
#        where N is the task number.
#     execs - директоия, содержащая бинарные файлы проекта. Названия файлов
#         совпадают с названиями в директории tasks.



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

# -------------------------------------------------------------------------

import subprocess
import argparse
import time
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
    
    def is_cpp_file(self) -> bool:
        '''Return True if .cpp file, False if .c, overwise raise error'''
        if len(self.path) > 2 and self.path[-2:] == '.c':
            return False
        elif len(self.path) > 4 and self.path[-4:] == '.cpp':
            return True
        raise Exception("Unknown file type")

    def get_path(self) -> str:
        return self.path
    
    def get_name(self) -> str:
        return self.name
    
    def get_exec_path(self) -> str:
        return f'execs/{self.name}.out'


# ---------------------------------------------------------------- handlers
def compile_task(args: argparse.Namespace) -> None:
    print('compiling...')

    gcc_preset_options = '-g'

    task = Task(args.task)
    os.system('{} {} {} -o {}'.format(
            'g++' if task.is_cpp_file() else 'gcc',
            gcc_preset_options if not args.no_presets else '' + \
            ' ' + ' '.join(args.flags) if args.flags is not None else ' ',
            task.get_path(),
            task.get_exec_path()
        )
    )

def mpi_compile_task(args: argparse.Namespace) -> None:
    print('compiling...')

    task = Task(args.task)

    os.system('{} {} {} -o {}'.format(
            'mpic++' if task.is_cpp_file() else 'mpicc',
            # '-g',
            '-g',
            task.get_path(),
            task.get_exec_path()
        )
    )

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
    compile_task(args)
    run_task(args)

def mpi_comp_and_run(args: argparse.Namespace) -> None:
    mpi_compile_task(args)
    mpi_run_task(args)

def open_vim(args: argparse.Namespace) -> None:
    os.system(f'vim {Task(args.task).get_path()} +{args.n}')

def open_vscode(args: argparse.Namespace) -> None:
    os.system(f'code -g {Task(args.task).get_path()}:{args.n}')

def run_sbatch(args: argparse.Namespace):
    result = subprocess.run(['whoami'], capture_output=True, text=True)
    if result.stdout != 'rt20250211\n':
        print('Error: You cannot run this not from the claster')
        return
    
    args_tx = ' '.join(args.args) if args.args is not None else ''
    script_text =                                                         \
        f"#!/bin/sh\n"                                                    \
        f"#SBATCH -n {args.n}\n"                                          \
        f"#SBATCH -o ../logs/out_%j.txt\n"                                \
        f"#SBATCH -e ../logs/err_%j.txt\n"                                \
        f"mpiexec -np {args.n} {Task(args.task).get_exec_path()} {args_tx}"
    
    os.system(f'echo "{script_text}" > ../tmp/sbatch_script.sh')
    os.system(f'cat ../tmp/sbatch_script.sh')
    result = subprocess.run(
        ['sbatch', '../tmp/sbatch_script.sh'],
        capture_output=True,
        text=True
    )
    out_num = result.stdout[20:-1]
    print(result.stdout)
    mult = 0.25
    while subprocess.run(
        ['squeue', '-j', out_num],
        capture_output=True,
        text=True
    ).stdout.count('\n') != 1:
        mult *= 2
        if mult > args.wt:
            print("Error: waiting time exceeded")
            return
        print(f'waiting {mult}s')
        time.sleep(mult)

    print('Program output:')
    os.system(f'cat ../logs/out_{out_num}.txt')

# -------------------------------------------------------------------------

def parse_args() -> None:
    parser = argparse.ArgumentParser(
        description='This is the builder for this project.',
        epilog=''
    )
    subparsers = parser.add_subparsers(title='commands')


    subparser = subparsers.add_parser(
        'compile',
        aliases=['c', 'comp'],
        help='Compile task using gcc compiler'
    )
    subparser.add_argument(
        'task',
        type=str,
        help='Full name of the task or task number'
    )
    subparser.add_argument(
        '--no_presets',
        action='store_true',
        help='Do not use preset flags'
    )
    subparser.add_argument(
        '--flags',
        nargs='+',
        help='Set flags for the compiler'
    )
    subparser.set_defaults(
        func=compile_task
    )

    subparser = subparsers.add_parser(
        'mpi_compile',
        aliases=['mc', 'mcomp'],
        help='Compile task using mpi compiler'
    )
    subparser.add_argument(
        'task',
        type=str,
        help='Full name of the task or task number'
    )
    subparser.add_argument(
        '--flags',
        nargs='+',
        help='Set flags for the compiler'
    )
    subparser.set_defaults(
        func=mpi_compile_task
    )

    subparser = subparsers.add_parser(
        'run',
        aliases=['r'],
        help='Run the program in the normal way'
    )
    subparser.add_argument(
        'task',
        type=str,
        help='Full name of the task or task number'
    )
    subparser.add_argument(
        '--args',
        nargs='+',
        help='Set args for the running program'
    )
    subparser.set_defaults(
        func=run_task
    )

    subparser = subparsers.add_parser(
        'mpi_run',
        aliases=['mr'],
        help='Run the program in using mpi environment'
    )
    subparser.add_argument('task', type=str, help='Full name of the task or task number')
    subparser.add_argument('-np', type=int, default=1, help='Number of processes')
    subparser.add_argument('--args', nargs='+', help='Set args for the running program')
    subparser.set_defaults(func=mpi_run_task)

    subparser = subparsers.add_parser(
        'compile_and_run',
        aliases=['car'],
        help='Compile task using gcc compiler and run'
    )
    subparser.add_argument(
        'task',
        type=str,
        help='Full name of the task or task number'
    )
    subparser.add_argument(
        '--no_presets',
        action='store_true',
        help='Do not use preset flags'
    )
    subparser.add_argument(
        '--flags',
        nargs='+',
        help='Set flags for the compiler'
    )
    subparser.add_argument(
        '--args',
        nargs='+',
        help='Set args for the running program'
    )
    subparser.set_defaults(
        func=comp_and_run
    )

    subparser = subparsers.add_parser(
        'mpi_compile_and_run',
        aliases=['mcar'],
        help='Compile and run the program in using mpi environment'
    )
    subparser.add_argument('task', type=str, help='Full name of the task or task number')
    subparser.add_argument('-np', type=int, default=1, help='Number of processes')
    subparser.add_argument('--args', nargs='+', help='Set args for the compiler')
    subparser.set_defaults(func=mpi_comp_and_run)

    subparser = subparsers.add_parser(
        'vim',
        help='Open task with vim'
    )
    subparser.add_argument('task', type=str, help='Full name of the task or task number')
    subparser.add_argument('-n', type=int, default=1, help='Number of processes')
    subparser.set_defaults(func=open_vim)

    subparser = subparsers.add_parser(
        'code',
        help='Open task with vscode'
    )
    subparser.add_argument('task', type=str, help='Full name of the task or task number')
    subparser.add_argument('-n', type=int, default=1, help='Number of processes')
    subparser.set_defaults(func=open_vscode)

    subparser = subparsers.add_parser(
        'run_sbatch',
        help='Send task to the queue'
    )
    subparser.add_argument(
        'task',
        type=str,
        help='Full name of the task or task number'
    )
    subparser.add_argument(
        '-n',
        type=str,
        help='Number of processes'
    )
    subparser.add_argument(
        '-wt',
        type=int,
        default=20,
        help='Set waiting time'
    )
    subparser.add_argument(
        '--args',
        nargs='+',
        help='Set flags for the compiler'
    )
    subparser.set_defaults(
        func=run_sbatch
    )

    return parser.parse_args()

if __name__ == '__main__':
    args = parse_args()
    args.func(args)
