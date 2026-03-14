// global.c - для PT планирования
#include "rtos_api.h"
#include <stdio.h>
#include <stdlib.h>

// Массивы объектов ОС
TCB TaskQueue[MAX_TASKS];
SemaphoreCB ResourceQueue[MAX_RESOURCES];
HLPResource HLPResourceQueue[MAX_RESOURCES];
PIPResource PIPResourceQueue[MAX_RESOURCES];

// Текущее состояние системы
int CurrentTask = -1;

// Флаги состояния
int OSInitialized = 0;
int OSStarted = 0;
int InISR = 0;

// Планировщик
int ReadyList[MAX_TASKS];
int ReadyCount = 0;

// Статистика и ограничения
int ActiveTaskCount = 0;
int TotalActivations = 0;
int TotalPreemptions = 0;
int MaxReadyTasks = 0;

// Функция инициализации
void GlobalInit(void) {
    // Инициализация TaskQueue
    for (int i = 0; i < MAX_TASKS; i++) {
        TaskQueue[i].id = -1;
        TaskQueue[i].state = TASK_SUSPENDED;
        TaskQueue[i].priority = 0;
        TaskQueue[i].original_priority = 0;
        TaskQueue[i].entry_point = NULL;
        TaskQueue[i].stack_pointer = NULL;
        TaskQueue[i].stack_base = NULL;
        TaskQueue[i].stack_size = 0;
        TaskQueue[i].waiting_semaphore = -1;
        TaskQueue[i].waiting_resource = -1;
        TaskQueue[i].pip_waiting_resource = -1;
        TaskQueue[i].held_count = 0;
        TaskQueue[i].resource_holder = -1;
        TaskQueue[i].activation_count = 0;
        TaskQueue[i].preemption_count = 0;
        TaskQueue[i].execution_time = 0;
        
        for (int j = 0; j < MAX_HELD_RESOURCES; j++) {
            TaskQueue[i].held_semaphores[j] = -1;
            TaskQueue[i].held_resources[j] = -1;
        }
    }
    
    // Инициализация семафоров
    for (int i = 0; i < MAX_RESOURCES; i++) {
        ResourceQueue[i].id = -1;
        ResourceQueue[i].count = 0;
        ResourceQueue[i].initial_count = 0;
        ResourceQueue[i].waiting_count = 0;
        ResourceQueue[i].max_waiting = 0;
        for (int j = 0; j < MAX_TASKS; j++) {
            ResourceQueue[i].waiting_tasks[j] = -1;
        }
    }
    
    // Инициализация HLP ресурсов
    for (int i = 0; i < MAX_RESOURCES; i++) {
        HLPResourceQueue[i].id = -1;
        HLPResourceQueue[i].ceiling_priority = 0;
        HLPResourceQueue[i].holder_task = -1;
        HLPResourceQueue[i].original_priority = 0;
        HLPResourceQueue[i].waiting_count = 0;
        HLPResourceQueue[i].initialized = 0;
        for (int j = 0; j < MAX_TASKS; j++) {
            HLPResourceQueue[i].waiting_tasks[j] = -1;
        }
    }
    
    // Инициализация PIP ресурсов
    for (int i = 0; i < MAX_RESOURCES; i++) {
        PIPResourceQueue[i].id = -1;
        PIPResourceQueue[i].holder_task = -1;
        PIPResourceQueue[i].waiting_count = 0;
        PIPResourceQueue[i].initialized = 0;
        for (int j = 0; j < MAX_TASKS; j++) {
            PIPResourceQueue[i].waiting_tasks[j] = -1;
        }
    }

    // Инициализация ReadyList
    for (int i = 0; i < MAX_TASKS; i++) {
        ReadyList[i] = -1;
    }
    
    // Сброс глобальных переменных
    CurrentTask = -1;
    OSInitialized = 1;
    OSStarted = 0;
    InISR = 0;
    ReadyCount = 0;
    ActiveTaskCount = 0;
    TotalActivations = 0;
    TotalPreemptions = 0;
    MaxReadyTasks = 0;
    
    printf("GlobalInit: System initialized for PT scheduling\n");
}