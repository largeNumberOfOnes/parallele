#!/bin/bash

cmp <(./get_e.py $1) <(./builder mr task_4_exp.cpp -n $2 -args $1)

