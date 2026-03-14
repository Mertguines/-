// task.c - для PT планирования
#include "rtos_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

// Внешние глобальные переменные
extern TCB TaskQueue[MAX_TASKS];
extern SemaphoreCB ResourceQueue[MAX_RESOURCES];
extern int CurrentTask;
extern int ReadyList[MAX_TASKS];
extern int ReadyCount;
extern int InISR;
extern int OSStarted;
extern int ActiveTaskCount;
extern int TotalActivations;
extern int MaxReadyTasks;

// Внешние функции из os.c
extern void AddToReadyList(int task_id);
extern void RemoveFromReadyList(int task_id);
extern void Dispatch(void);
extern void SortReadyList(void);

// Контекст переключения (для setjmp/longjmp)
jmp_buf *TaskContext[MAX_TASKS];
jmp_buf SchedulerContext;

//----------------------------------------------------------------
// ИНИЦИАЛИЗАЦИЯ ЗАДАЧИ (с приоритетом вместо дедлайна)
//----------------------------------------------------------------
void InitTask(int task_id, void (*task_func)(void), int priority, int stack_size) {
    if (task_id < 0 || task_id >= MAX_TASKS) {
        printf("ERROR: Cannot init task %d - invalid ID\n", task_id);
        return;
    }
    
    // Инициализируем TCB
    TaskQueue[task_id].id = task_id;
    TaskQueue[task_id].state = TASK_SUSPENDED;
    TaskQueue[task_id].priority = priority;
    TaskQueue[task_id].original_priority = priority;  // Сохраняем для HLP
    TaskQueue[task_id].entry_point = task_func;
    TaskQueue[task_id].stack_size = stack_size;
    TaskQueue[task_id].waiting_semaphore = -1;
    TaskQueue[task_id].waiting_resource = -1;
    TaskQueue[task_id].pip_waiting_resource = -1;
    TaskQueue[task_id].held_count = 0;
    TaskQueue[task_id].resource_holder = -1;
    TaskQueue[task_id].activation_count = 0;
    TaskQueue[task_id].preemption_count = 0;
    TaskQueue[task_id].execution_time = 0;
    
    // Инициализируем массивы захваченных ресурсов
    for (int i = 0; i < MAX_HELD_RESOURCES; i++) {
        TaskQueue[task_id].held_semaphores[i] = -1;
        TaskQueue[task_id].held_resources[i] = -1;
    }
    
    // Выделяем память под стек
    if (stack_size > 0) {
        TaskQueue[task_id].stack_base = malloc(stack_size);
        if (TaskQueue[task_id].stack_base == NULL) {
            printf("ERROR: Failed to allocate stack for task %d\n", task_id);
            return;
        }
        TaskQueue[task_id].stack_pointer = TaskQueue[task_id].stack_base;
    } else {
        TaskQueue[task_id].stack_base = NULL;
        TaskQueue[task_id].stack_pointer = NULL;
    }
    
    // Выделяем память для контекста
    TaskContext[task_id] = (jmp_buf *)malloc(sizeof(jmp_buf));
    
    printf("InitTask: Task %d initialized with priority %d\n", task_id, priority);
}

//----------------------------------------------------------------
// АКТИВАЦИЯ ЗАДАЧИ
//----------------------------------------------------------------
void ActivateTask(TTask task) {
    if (!OSStarted) {
        printf("ERROR: ActivateTask called before StartOS\n");
        return;
    }
    
    if (task < 0 || task >= MAX_TASKS) {
        printf("ERROR: ActivateTask: invalid task ID %d\n", task);
        return;
    }
    
    if (TaskQueue[task].id == -1) {
        printf("ERROR: ActivateTask: task %d not initialized\n", task);
        return;
    }
    
    // Проверяем, не активна ли уже задача
    if (TaskQueue[task].state != TASK_SUSPENDED) {
        printf("ERROR: ActivateTask: task %d is not suspended (state=%d)\n", 
               task, TaskQueue[task].state);
        return;
    }
    
    // Проверка на максимальное количество активных задач
    if (ActiveTaskCount >= MAX_ACTIVE_TASKS) {
        printf("ERROR: Maximum active tasks (%d) reached, cannot activate task %d\n", 
               MAX_ACTIVE_TASKS, task);
        return;
    }
    
    printf("ActivateTask: Activating task %d (priority=%d)\n", 
           task, TaskQueue[task].priority);
    
    // Переводим задачу в состояние READY
    TaskQueue[task].state = TASK_READY;
    TaskQueue[task].activation_count++;
    TotalActivations++;
    ActiveTaskCount++;
    
    // Добавляем в список готовности
    AddToReadyList(task);
    
    // Если мы не в ISR, проверяем необходимость вытеснения
    if (!InISR) {
        if (CurrentTask != -1) {
            // Если новая задача имеет более высокий приоритет (меньше число)
            if (TaskQueue[task].priority < TaskQueue[CurrentTask].priority) {
                printf("Preemption: New task %d (prio=%d) has higher priority than current %d (prio=%d)\n",
                       task, TaskQueue[task].priority,
                       CurrentTask, TaskQueue[CurrentTask].priority);
                Dispatch();
            }
        } else if (CurrentTask == -1) {
            // Нет текущей задачи, запускаем первую готовую
            Dispatch();
        }
    } else {
        printf("ActivateTask from ISR - scheduling deferred to LeaveISR\n");
    }
}

//----------------------------------------------------------------
// ЗАВЕРШЕНИЕ ЗАДАЧИ
//----------------------------------------------------------------
void TerminateTask(void) {
    if (!OSStarted) {
        printf("ERROR: TerminateTask called before StartOS\n");
        return;
    }
    
    if (CurrentTask == -1) {
        printf("ERROR: TerminateTask called with no current task\n");
        return;
    }
    
    int task = CurrentTask;
    
    printf("TerminateTask: Terminating task %d\n", task);
    
    // Проверка: нельзя завершаться в критической секции
    if (TaskQueue[task].held_count > 0) {
        printf("ERROR: TerminateTask called from critical section (held %d resources)\n", 
               TaskQueue[task].held_count);
        return;
    }
    
    // Уменьшаем счетчик активных задач
    if (TaskQueue[task].state != TASK_SUSPENDED) {
        ActiveTaskCount--;
    }
    
    // Удаляем задачу из списка готовности
    RemoveFromReadyList(task);
    
    // Освобождаем стек
    if (TaskQueue[task].stack_base != NULL) {
        free(TaskQueue[task].stack_base);
        TaskQueue[task].stack_base = NULL;
        TaskQueue[task].stack_pointer = NULL;
    }
    
    // Освобождаем контекст
    if (TaskContext[task] != NULL) {
        free(TaskContext[task]);
        TaskContext[task] = NULL;
    }
    
    // Переводим задачу в состояние SUSPENDED
    TaskQueue[task].state = TASK_SUSPENDED;
    TaskQueue[task].waiting_semaphore = -1;
    TaskQueue[task].waiting_resource = -1;
    TaskQueue[task].held_count = 0;
    TaskQueue[task].resource_holder = -1;
    
    printf("Task %d terminated, active tasks now = %d\n", task, ActiveTaskCount);
    
    // Текущей задачи больше нет
    CurrentTask = -1;
    
    // Запускаем следующую задачу
    Dispatch();
}

//----------------------------------------------------------------
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
//----------------------------------------------------------------
int GetCurrentTask(void) {
    return CurrentTask;
}

int GetReadyCount(void) {
    return ReadyCount;
}

int IsTaskValid(int task_id) {
    if (task_id < 0 || task_id >= MAX_TASKS) return 0;
    return (TaskQueue[task_id].id != -1);
}

TTaskState GetTaskState(int task_id) {
    if (!IsTaskValid(task_id)) return -1;
    return TaskQueue[task_id].state;
}

void PrintTaskInfo(int task_id) {
    if (!IsTaskValid(task_id)) {
        printf("Task %d: invalid\n", task_id);
        return;
    }
    
    const char *state_str;
    switch (TaskQueue[task_id].state) {
        case TASK_SUSPENDED: state_str = "SUSPENDED"; break;
        case TASK_READY: state_str = "READY"; break;
        case TASK_RUNNING: state_str = "RUNNING"; break;
        case TASK_WAITING: state_str = "WAITING"; break;
        default: state_str = "UNKNOWN";
    }
    
    printf("Task %d: state=%s, priority=%d, act_count=%d, held=%d\n",
           task_id, state_str, TaskQueue[task_id].priority,
           TaskQueue[task_id].activation_count, TaskQueue[task_id].held_count);
}