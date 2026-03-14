// os.c - для PT планирования
#include "rtos_api.h"
#include <stdio.h>
#include <stdlib.h>

// Внешние глобальные переменные
extern TCB TaskQueue[MAX_TASKS];
extern SemaphoreCB ResourceQueue[MAX_RESOURCES];
extern HLPResource HLPResourceQueue[MAX_RESOURCES];
extern int CurrentTask;
extern int ReadyList[MAX_TASKS];
extern int ReadyCount;
extern int InISR;
extern int OSStarted;
extern int ActiveTaskCount;
extern int TotalPreemptions;
extern int MaxReadyTasks;

//----------------------------------------------------------------
// СОРТИРОВКА ПО ПРИОРИТЕТУ (вместо дедлайна)
//----------------------------------------------------------------
void SortReadyList(void) {
    if (ReadyCount <= 1) return;
    
    // Сортировка вставками по приоритету (меньше = выше)
    for (int i = 1; i < ReadyCount; i++) {
        int key = ReadyList[i];
        int j = i - 1;
        
        // Сравниваем приоритеты - чем меньше число, тем выше приоритет
        while (j >= 0 && TaskQueue[ReadyList[j]].priority > TaskQueue[key].priority) {
            ReadyList[j + 1] = ReadyList[j];
            j--;
        }
        ReadyList[j + 1] = key;
    }
    
    // Обновляем статистику
    if (ReadyCount > MaxReadyTasks) {
        MaxReadyTasks = ReadyCount;
    }
    
    // Отладочный вывод
    printf("Ready list sorted by priority: ");
    for (int i = 0; i < ReadyCount; i++) {
        printf("%d(p=%d) ", ReadyList[i], TaskQueue[ReadyList[i]].priority);
    }
    printf("\n");
}

//----------------------------------------------------------------
// ПЛАНИРОВЩИК - выбирает задачу с наивысшим приоритетом
//----------------------------------------------------------------
int Schedule(void) {
    if (ReadyCount == 0) {
        return -1;  // Нет готовых задач
    }
    
    // Первая задача в отсортированном списке имеет наивысший приоритет
    // (наименьшее значение priority)
    return ReadyList[0];
}

//----------------------------------------------------------------
// ДИСПЕТЧЕР - переключение контекста
//----------------------------------------------------------------
void Dispatch(void) {
    int next_task = Schedule();
    
    if (next_task == -1) {
        printf("Dispatch: No tasks to run, system idle\n");
        CurrentTask = -1;
        return;
    }
    
    if (next_task == CurrentTask) {
        return;  // Та же задача, переключение не нужно
    }
    
    // Статистика вытеснения
    if (CurrentTask != -1 && TaskQueue[CurrentTask].state == TASK_RUNNING) {
        TaskQueue[CurrentTask].preemption_count++;
        TotalPreemptions++;
        printf("Dispatch: Task %d (prio=%d) preempted by task %d (prio=%d)\n", 
               CurrentTask, TaskQueue[CurrentTask].priority,
               next_task, TaskQueue[next_task].priority);
        TaskQueue[CurrentTask].state = TASK_READY;
        AddToReadyList(CurrentTask);
    }
    
    // Убираем новую задачу из списка готовности
    for (int i = 0; i < ReadyCount; i++) {
        if (ReadyList[i] == next_task) {
            for (int j = i; j < ReadyCount - 1; j++) {
                ReadyList[j] = ReadyList[j + 1];
            }
            ReadyList[ReadyCount - 1] = -1;
            ReadyCount--;
            break;
        }
    }
    
    // Запускаем новую задачу
    CurrentTask = next_task;
    TaskQueue[CurrentTask].state = TASK_RUNNING;
    
    printf("Dispatch: Task %d started (priority=%d)\n", 
           CurrentTask, TaskQueue[CurrentTask].priority);
    
    // Вызов функции задачи
    if (TaskQueue[CurrentTask].entry_point) {
        TaskQueue[CurrentTask].entry_point();
    } else {
        printf("ERROR: Task %d has no entry point\n", CurrentTask);
        TerminateTask();
    }
}

//----------------------------------------------------------------
// УПРАВЛЕНИЕ СПИСКОМ ГОТОВНОСТИ
//----------------------------------------------------------------
void AddToReadyList(int task_id) {
    if (task_id < 0 || task_id >= MAX_TASKS) return;
    if (TaskQueue[task_id].state != TASK_READY) return;
    
    // Проверяем, нет ли уже задачи в списке
    for (int i = 0; i < ReadyCount; i++) {
        if (ReadyList[i] == task_id) return;
    }
    
    if (ReadyCount < MAX_TASKS) {
        ReadyList[ReadyCount] = task_id;
        ReadyCount++;
        SortReadyList();
        
        printf("Task %d (prio=%d) added to ready list\n", 
               task_id, TaskQueue[task_id].priority);
    }
}

void RemoveFromReadyList(int task_id) {
    for (int i = 0; i < ReadyCount; i++) {
        if (ReadyList[i] == task_id) {
            for (int j = i; j < ReadyCount - 1; j++) {
                ReadyList[j] = ReadyList[j + 1];
            }
            ReadyList[ReadyCount - 1] = -1;
            ReadyCount--;
            SortReadyList();
            return;
        }
    }
}

//----------------------------------------------------------------
// ПРЕРЫВАНИЯ
//----------------------------------------------------------------
void EnterISR(void) {
    printf("EnterISR: Entering interrupt handler\n");
    InISR = 1;
}

void LeaveISR(void) {
    printf("LeaveISR: Leaving interrupt handler\n");
    InISR = 0;
    
    // При выходе из прерывания проверяем необходимость перепланирования
    if (ReadyCount > 0) {
        int next_task = ReadyList[0];
        if (CurrentTask == -1 || 
            (next_task != CurrentTask && 
             TaskQueue[next_task].priority < TaskQueue[CurrentTask].priority)) {
            printf("LeaveISR: Rescheduling after interrupt\n");
            Dispatch();
        }
    }
}

void ISRActivateTask(TTask task) {
    if (!OSStarted || task < 0 || task >= MAX_TASKS) return;
    
    printf("ISRActivateTask: Task %d activated from ISR\n", task);
    
    if (TaskQueue[task].state == TASK_SUSPENDED) {
        TaskQueue[task].state = TASK_READY;
        TaskQueue[task].activation_count++;
        ActiveTaskCount++;
        AddToReadyList(task);
    }
}

//----------------------------------------------------------------
// УПРАВЛЕНИЕ ОС
//----------------------------------------------------------------
void StartOS(TTask task) {
    printf("StartOS: Starting system with initial task %d (priority=%d)\n", 
           task, TaskQueue[task].priority);
    
    OSStarted = 1;
    
    if (task >= 0 && task < MAX_TASKS && TaskQueue[task].state == TASK_SUSPENDED) {
        ActivateTask(task);
    } else {
        Dispatch();
    }
    
    printf("StartOS: System shutdown\n");
}

void ShutdownOS(void) {
    printf("ShutdownOS: System shutting down\n");
    OSStarted = 0;
}

//----------------------------------------------------------------
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
//----------------------------------------------------------------
void PrintSystemState(void) {
    printf("\n=== System State ===\n");
    printf("Current Task: %d (prio=%d)\n", 
           CurrentTask, 
           CurrentTask != -1 ? TaskQueue[CurrentTask].priority : -1);
    
    printf("Ready Tasks (%d): ", ReadyCount);
    for (int i = 0; i < ReadyCount; i++) {
        if (ReadyList[i] != -1) {
            printf("%d(prio=%d) ", ReadyList[i], TaskQueue[ReadyList[i]].priority);
        }
    }
    printf("\n");
    
    printf("Active Tasks: %d\n", ActiveTaskCount);
    printf("===================\n\n");
}