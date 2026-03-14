#!/bin/bash
gcc -o output/rtos_test PT/global.c PT/os.c PT/task.c PT/resource.c PT/pip.c PT/test.c -I./PT && output/rtos_test