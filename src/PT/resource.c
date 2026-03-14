// resource.c - для PT планирования
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

// Внешние функции
extern void AddToReadyList(int task_id);
extern void RemoveFromReadyList(int task_id);
extern void Dispatch(void);
extern void SortReadyList(void);

//========================================================================
// P/V СЕМАФОРЫ (без изменений, работают одинаково для EDF и PT)
//========================================================================

// Инициализация семафора
void InitPVS(TSemaphore S) {
    if (S < 0 || S >= MAX_RESOURCES) {
        printf("ERROR: InitPVS: invalid semaphore ID %d\n", S);
        return;
    }
    
    printf("InitPVS: Initializing semaphore %d\n", S);
    
    ResourceQueue[S].id = S;
    ResourceQueue[S].count = 1;  // По умолчанию двоичный семафор
    ResourceQueue[S].initial_count = 1;
    ResourceQueue[S].waiting_count = 0;
    ResourceQueue[S].max_waiting = 0;
    
    for (int i = 0; i < MAX_TASKS; i++) {
        ResourceQueue[S].waiting_tasks[i] = -1;
    }
    
    printf("Semaphore %d initialized with count=%d\n", S, ResourceQueue[S].count);
}

// Инициализация семафора с заданным значением
void InitPVSWithCount(TSemaphore S, int initial_count) {
    if (S < 0 || S >= MAX_RESOURCES) {
        printf("ERROR: InitPVSWithCount: invalid semaphore ID %d\n", S);
        return;
    }
    
    if (initial_count < 0) {
        printf("ERROR: InitPVSWithCount: initial count cannot be negative\n");
        return;
    }
    
    printf("InitPVSWithCount: Initializing semaphore %d with count=%d\n", S, initial_count);
    
    ResourceQueue[S].id = S;
    ResourceQueue[S].count = initial_count;
    ResourceQueue[S].initial_count = initial_count;
    ResourceQueue[S].waiting_count = 0;
    ResourceQueue[S].max_waiting = 0;
    
    for (int i = 0; i < MAX_TASKS; i++) {
        ResourceQueue[S].waiting_tasks[i] = -1;
    }
}

// Добавить задачу в очередь ожидания семафора
static void AddToSemWaitingQueue(int sem_id, int task_id) {
    SemaphoreCB *sem = &ResourceQueue[sem_id];
    
    if (sem->waiting_count < MAX_TASKS) {
        sem->waiting_tasks[sem->waiting_count] = task_id;
        sem->waiting_count++;
        
        if (sem->waiting_count > sem->max_waiting) {
            sem->max_waiting = sem->waiting_count;
        }
        
        printf("Task %d added to semaphore %d waiting queue (len=%d)\n", 
               task_id, sem_id, sem->waiting_count);
    }
}

// Удалить задачу из очереди ожидания семафора (FIFO)
static int RemoveFromSemWaitingQueue(int sem_id) {
    SemaphoreCB *sem = &ResourceQueue[sem_id];
    
    if (sem->waiting_count == 0) return -1;
    
    int task_id = sem->waiting_tasks[0];
    
    for (int i = 0; i < sem->waiting_count - 1; i++) {
        sem->waiting_tasks[i] = sem->waiting_tasks[i + 1];
    }
    sem->waiting_tasks[sem->waiting_count - 1] = -1;
    sem->waiting_count--;
    
    printf("Task %d removed from semaphore %d waiting queue (remaining=%d)\n", 
           task_id, sem_id, sem->waiting_count);
    
    return task_id;
}

// Проверить, удерживает ли задача данный семафор
static int TaskHoldsSemaphore(int task_id, int sem_id) {
    TCB *task = &TaskQueue[task_id];
    
    for (int i = 0; i < task->held_count; i++) {
        if (task->held_semaphores[i] == sem_id) return 1;
    }
    return 0;
}

// Добавить семафор в список захваченных задачей
static void AddSemToHeldList(int task_id, int sem_id) {
    TCB *task = &TaskQueue[task_id];
    
    if (task->held_count >= MAX_HELD_RESOURCES) {
        printf("ERROR: Task %d exceeded max held resources\n", task_id);
        return;
    }
    
    task->held_semaphores[task->held_count] = sem_id;
    task->held_count++;
    
    printf("Task %d now holds semaphore %d (total held=%d)\n", 
           task_id, sem_id, task->held_count);
}

// Удалить семафор из списка захваченных задачей
static void RemoveSemFromHeldList(int task_id, int sem_id) {
    TCB *task = &TaskQueue[task_id];
    
    for (int i = 0; i < task->held_count; i++) {
        if (task->held_semaphores[i] == sem_id) {
            for (int j = i; j < task->held_count - 1; j++) {
                task->held_semaphores[j] = task->held_semaphores[j + 1];
            }
            task->held_semaphores[task->held_count - 1] = -1;
            task->held_count--;
            
            printf("Task %d released semaphore %d (remaining held=%d)\n", 
                   task_id, sem_id, task->held_count);
            return;
        }
    }
    
    printf("ERROR: Task %d does not hold semaphore %d\n", task_id, sem_id);
}

// P операция
void P(TSemaphore S) {
    if (!OSStarted) {
        printf("ERROR: P called before StartOS\n");
        return;
    }
    
    if (S < 0 || S >= MAX_RESOURCES) {
        printf("ERROR: P: invalid semaphore ID %d\n", S);
        return;
    }
    
    if (ResourceQueue[S].id == -1) {
        printf("ERROR: P: semaphore %d not initialized\n", S);
        return;
    }
    
    if (CurrentTask == -1) {
        printf("ERROR: P called with no current task\n");
        return;
    }
    
    int task = CurrentTask;
    SemaphoreCB *sem = &ResourceQueue[S];
    
    printf("P: Task %d trying to acquire semaphore %d (count=%d)\n", 
           task, S, sem->count);
    
    // Проверка на повторный захват того же семафора
    if (TaskHoldsSemaphore(task, S)) {
        printf("ERROR: Task %d trying to acquire semaphore %d it already holds\n", 
               task, S);
        return;
    }
    
    if (sem->count > 0) {
        // Семафор свободен
        sem->count--;
        AddSemToHeldList(task, S);
        printf("P: Task %d acquired semaphore %d (count now=%d)\n", 
               task, S, sem->count);
    } else {
        // Семафор занят - блокируем задачу
        printf("P: Semaphore %d busy, blocking task %d\n", S, task);
        
        RemoveFromReadyList(task);
        TaskQueue[task].state = TASK_WAITING;
        TaskQueue[task].waiting_semaphore = S;
        AddToSemWaitingQueue(S, task);
        
        CurrentTask = -1;
        
        printf("P: Task %d blocked, switching to next task\n", task);
        Dispatch();
        
        // Когда задача возобновится, она продолжит здесь
        printf("P: Task %d resumed with semaphore %d\n", task, S);
    }
}

// V операция
void V(TSemaphore S) {
    if (!OSStarted) {
        printf("ERROR: V called before StartOS\n");
        return;
    }
    
    if (S < 0 || S >= MAX_RESOURCES) {
        printf("ERROR: V: invalid semaphore ID %d\n", S);
        return;
    }
    
    if (ResourceQueue[S].id == -1) {
        printf("ERROR: V: semaphore %d not initialized\n", S);
        return;
    }
    
    if (CurrentTask == -1) {
        printf("ERROR: V called with no current task\n");
        return;
    }
    
    int task = CurrentTask;
    SemaphoreCB *sem = &ResourceQueue[S];
    
    printf("V: Task %d releasing semaphore %d (count=%d, waiting=%d)\n", 
           task, S, sem->count, sem->waiting_count);
    
    // Проверка, что задача действительно владеет семафором
    /*if (!TaskHoldsSemaphore(task, S)) {
        printf("ERROR: Task %d does not hold semaphore %d\n", task, S);
        return;
    }*/
    
    RemoveSemFromHeldList(task, S);
    sem->count++;
    
    // Если есть ожидающие задачи, разблокируем одну
    if (sem->waiting_count > 0) {
        int waiting_task = RemoveFromSemWaitingQueue(S);
        
        if (waiting_task != -1) {
            printf("V: Unblocking task %d waiting for semaphore %d\n", 
                   waiting_task, S);
            
            TaskQueue[waiting_task].state = TASK_READY;
            TaskQueue[waiting_task].waiting_semaphore = -1;
            AddToReadyList(waiting_task);
            
            sem->count--;  // Передаем семафор разблокированной задаче
        }
    }
    
    printf("V: Task %d released semaphore %d (count=%d)\n", task, S, sem->count);
    
    // Проверка на вытеснение (если разблокирована задача с higher priority)
    if (!InISR && sem->waiting_count > 0) {
        if (ReadyCount > 0) {
            int next_task = ReadyList[0];
            if (next_task != CurrentTask && 
                TaskQueue[next_task].priority < TaskQueue[CurrentTask].priority) {
                printf("V: Higher priority task unblocked, preempting current task\n");
                Dispatch();
            }
        }
    }
}

//========================================================================
// HLP РЕСУРСЫ (GetResource/ReleaseResource) - ИЗМЕНЕНО для PT
//========================================================================

// Инициализация HLP ресурса
void InitHLPResource(TResource res, int ceiling_priority) {
    if (res < 0 || res >= MAX_RESOURCES) {
        printf("ERROR: InitHLPResource: invalid resource ID %d\n", res);
        return;
    }
    
    HLPResourceQueue[res].id = res;
    HLPResourceQueue[res].ceiling_priority = ceiling_priority;
    HLPResourceQueue[res].holder_task = -1;
    HLPResourceQueue[res].original_priority = 0;
    HLPResourceQueue[res].waiting_count = 0;
    HLPResourceQueue[res].initialized = 1;
    
    for (int i = 0; i < MAX_TASKS; i++) {
        HLPResourceQueue[res].waiting_tasks[i] = -1;
    }
    
    printf("HLP Resource %d initialized with ceiling priority %d\n", 
           res, ceiling_priority);
}

// Добавить задачу в очередь ожидания HLP ресурса (сортированная по приоритету)
static void AddToHLPWaitingQueue(int res_id, int task_id) {
    HLPResource *res = &HLPResourceQueue[res_id];
    
    if (res->waiting_count < MAX_TASKS) {
        res->waiting_tasks[res->waiting_count] = task_id;
        res->waiting_count++;
        
        // Сортировка по приоритету (меньше число = выше приоритет)
        for (int i = res->waiting_count - 1; i > 0; i--) {
            if (TaskQueue[res->waiting_tasks[i]].priority < 
                TaskQueue[res->waiting_tasks[i-1]].priority) {
                int temp = res->waiting_tasks[i];
                res->waiting_tasks[i] = res->waiting_tasks[i-1];
                res->waiting_tasks[i-1] = temp;
            }
        }
        
        printf("Task %d added to HLP resource %d waiting queue (pos %d)\n", 
               task_id, res_id, res->waiting_count - 1);
    }
}

// Удалить задачу из очереди ожидания HLP ресурса (берем первую с highest priority)
static int RemoveFromHLPWaitingQueue(int res_id) {
    HLPResource *res = &HLPResourceQueue[res_id];
    
    if (res->waiting_count == 0) return -1;
    
    int task_id = res->waiting_tasks[0];
    
    for (int i = 0; i < res->waiting_count - 1; i++) {
        res->waiting_tasks[i] = res->waiting_tasks[i + 1];
    }
    res->waiting_tasks[res->waiting_count - 1] = -1;
    res->waiting_count--;
    
    printf("Task %d removed from HLP resource %d waiting queue\n", 
           task_id, res_id);
    
    return task_id;
}

// Проверить, удерживает ли задача HLP ресурс
static int TaskHoldsHLPResource(int task_id, int res_id) {
    TCB *task = &TaskQueue[task_id];
    
    for (int i = 0; i < task->held_count; i++) {
        if (task->held_resources[i] == res_id) return 1;
    }
    return 0;
}

// Добавить HLP ресурс в список захваченных задачей
static void AddHLPToHeldList(int task_id, int res_id) {
    TCB *task = &TaskQueue[task_id];
    
    if (task->held_count >= MAX_HELD_RESOURCES) {
        printf("ERROR: Task %d exceeded max held resources\n", task_id);
        return;
    }
    
    task->held_resources[task->held_count] = res_id;
    task->held_count++;
    
    printf("Task %d now holds HLP resource %d (total held=%d)\n", 
           task_id, res_id, task->held_count);
}

// Удалить HLP ресурс из списка захваченных задачей
static void RemoveHLPFromHeldList(int task_id, int res_id) {
    TCB *task = &TaskQueue[task_id];
    
    for (int i = 0; i < task->held_count; i++) {
        if (task->held_resources[i] == res_id) {
            for (int j = i; j < task->held_count - 1; j++) {
                task->held_resources[j] = task->held_resources[j + 1];
            }
            task->held_resources[task->held_count - 1] = -1;
            task->held_count--;
            return;
        }
    }
}

// GetResource - реализация HLP для PT
void GetResource(TResource res) {
    if (!OSStarted) {
        printf("ERROR: GetResource called before StartOS\n");
        return;
    }
    
    if (res < 0 || res >= MAX_RESOURCES) {
        printf("ERROR: GetResource: invalid resource ID %d\n", res);
        return;
    }
    
    if (!HLPResourceQueue[res].initialized) {
        printf("ERROR: GetResource: resource %d not initialized\n", res);
        return;
    }
    
    if (CurrentTask == -1) {
        printf("ERROR: GetResource called with no current task\n");
        return;
    }
    
    int task = CurrentTask;
    HLPResource *resource = &HLPResourceQueue[res];
    
    printf("GetResource: Task %d (prio=%d) trying to acquire resource %d (ceiling=%d)\n", 
           task, TaskQueue[task].priority, res, resource->ceiling_priority);
    
    // Проверка на вложенный захват того же ресурса
    if (resource->holder_task == task) {
        printf("ERROR: Task %d trying to acquire resource %d it already holds\n", 
               task, res);
        return;
    }
    
    // Проверка на максимальное количество захваченных ресурсов
    if (TaskQueue[task].held_count >= MAX_HELD_RESOURCES) {
        printf("ERROR: Task %d exceeded max held resources (%d)\n", 
               task, MAX_HELD_RESOURCES);
        return;
    }
    
    if (resource->holder_task == -1) {
        // Ресурс свободен - захватываем
        resource->holder_task = task;
        resource->original_priority = TaskQueue[task].priority;
        
        // Если приоритет задачи ниже (число больше), чем ceiling_priority,
        // повышаем приоритет (уменьшаем число)
        if (TaskQueue[task].priority > resource->ceiling_priority) {
            printf("HLP: Raising task %d priority from %d to %d\n", 
                   task, TaskQueue[task].priority, resource->ceiling_priority);
            
            TaskQueue[task].original_priority = TaskQueue[task].priority;
            TaskQueue[task].priority = resource->ceiling_priority;
            TaskQueue[task].resource_holder = res;
            
            // Пересортировываем список готовности, так как приоритет изменился
            if (TaskQueue[task].state == TASK_READY || 
                TaskQueue[task].state == TASK_RUNNING) {
                SortReadyList();
            }
        }
        
        AddHLPToHeldList(task, res);
        printf("GetResource: Task %d acquired resource %d (priority now=%d)\n", 
               task, res, TaskQueue[task].priority);
        
    } else {
        // Ресурс занят - блокируем задачу
        printf("GetResource: Resource %d busy by task %d, blocking task %d\n", 
               res, resource->holder_task, task);
        
        RemoveFromReadyList(task);
        TaskQueue[task].state = TASK_WAITING;
        TaskQueue[task].waiting_resource = res;
        AddToHLPWaitingQueue(res, task);
        
        CurrentTask = -1;
        
        printf("GetResource: Task %d blocked, switching to next task\n", task);
        Dispatch();
        
        printf("GetResource: Task %d resumed with resource %d\n", task, res);
    }
}

// ReleaseResource - реализация HLP для PT
void ReleaseResource(TResource res) {
    if (!OSStarted) {
        printf("ERROR: ReleaseResource called before StartOS\n");
        return;
    }
    
    if (res < 0 || res >= MAX_RESOURCES) {
        printf("ERROR: ReleaseResource: invalid resource ID %d\n", res);
        return;
    }
    
    if (!HLPResourceQueue[res].initialized) {
        printf("ERROR: ReleaseResource: resource %d not initialized\n", res);
        return;
    }
    
    if (CurrentTask == -1) {
        printf("ERROR: ReleaseResource called with no current task\n");
        return;
    }
    
    int task = CurrentTask;
    HLPResource *resource = &HLPResourceQueue[res];
    
    printf("ReleaseResource: Task %d releasing resource %d\n", task, res);
    
    // Проверка, что задача действительно владеет ресурсом
    if (resource->holder_task != task) {
        printf("ERROR: Task %d does not hold resource %d\n", task, res);
        return;
    }
    
    RemoveHLPFromHeldList(task, res);
    
    // Восстанавливаем оригинальный приоритет, если он был изменен
    if (TaskQueue[task].resource_holder == res) {
        printf("HLP: Restoring task %d priority from %d to %d\n", 
               task, TaskQueue[task].priority, TaskQueue[task].original_priority);
        
        TaskQueue[task].priority = TaskQueue[task].original_priority;
        TaskQueue[task].resource_holder = -1;
        TaskQueue[task].original_priority = 0;
        
        // Пересортировываем список готовности
        if (TaskQueue[task].state == TASK_READY || 
            TaskQueue[task].state == TASK_RUNNING) {
            SortReadyList();
        }
    }
    
    // Освобождаем ресурс
    resource->holder_task = -1;
    
    // Если есть ожидающие задачи, выбираем следующую (с наивысшим приоритетом)
    if (resource->waiting_count > 0) {
        int next_task = RemoveFromHLPWaitingQueue(res);
        
        printf("ReleaseResource: Unblocking task %d waiting for resource %d\n", 
               next_task, res);
        
        TaskQueue[next_task].state = TASK_READY;
        TaskQueue[next_task].waiting_resource = -1;
        AddToReadyList(next_task);
        
        // Назначаем ресурс новой задаче
        resource->holder_task = next_task;
        resource->original_priority = TaskQueue[next_task].priority;
        
        // Проверяем, нужно ли повысить приоритет новой задачи
        if (TaskQueue[next_task].priority > resource->ceiling_priority) {
            printf("HLP: Raising task %d priority from %d to %d\n", 
                   next_task, TaskQueue[next_task].priority, resource->ceiling_priority);
            
            TaskQueue[next_task].original_priority = TaskQueue[next_task].priority;
            TaskQueue[next_task].priority = resource->ceiling_priority;
            TaskQueue[next_task].resource_holder = res;
        }
        
        // Проверяем, нужно ли вытеснить текущую задачу
        if (TaskQueue[next_task].priority < TaskQueue[task].priority) {
            printf("ReleaseResource: Task %d (prio=%d) has higher priority than current %d (prio=%d)\n",
                   next_task, TaskQueue[next_task].priority,
                   task, TaskQueue[task].priority);
            
            if (TaskQueue[task].state == TASK_RUNNING) {
                TaskQueue[task].state = TASK_READY;
                AddToReadyList(task);
            }
            
            CurrentTask = -1;
            Dispatch();
        }
    } else {
        printf("ReleaseResource: No tasks waiting for resource %d\n", res);
    }
    
    printf("ReleaseResource: Task %d released resource %d\n", task, res);
}

//========================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
//========================================================================

void PrintSemaphoreInfo(TSemaphore S) {
    if (S < 0 || S >= MAX_RESOURCES) {
        printf("Semaphore %d: invalid ID\n", S);
        return;
    }
    
    if (ResourceQueue[S].id == -1) {
        printf("Semaphore %d: not initialized\n", S);
        return;
    }
    
    SemaphoreCB *sem = &ResourceQueue[S];
    
    printf("Semaphore %d: count=%d, initial=%d, waiting=%d, max_waiting=%d\n",
           S, sem->count, sem->initial_count, sem->waiting_count, sem->max_waiting);
    
    if (sem->waiting_count > 0) {
        printf("  Waiting tasks: ");
        for (int i = 0; i < sem->waiting_count; i++) {
            printf("%d ", sem->waiting_tasks[i]);
        }
        printf("\n");
    }
}

void PrintResourceInfo(TResource res) {
    if (res < 0 || res >= MAX_RESOURCES) {
        printf("Resource %d: invalid ID\n", res);
        return;
    }
    
    if (!HLPResourceQueue[res].initialized) {
        printf("Resource %d: not initialized\n", res);
        return;
    }
    
    HLPResource *r = &HLPResourceQueue[res];
    
    printf("Resource %d: ceiling=%d, holder=%d, waiting=%d\n",
           res, r->ceiling_priority, r->holder_task, r->waiting_count);
    
    if (r->waiting_count > 0) {
        printf("  Waiting tasks (by priority): ");
        for (int i = 0; i < r->waiting_count; i++) {
            printf("%d(prio=%d) ", r->waiting_tasks[i], 
                   TaskQueue[r->waiting_tasks[i]].priority);
        }
        printf("\n");
    }
}