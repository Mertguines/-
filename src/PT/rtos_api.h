// rtos_api.h - для PT планирования
#ifndef RTOS_API_H
#define RTOS_API_H

// Типы данных (по спецификации)
typedef int TTask;
typedef int TResource;
typedef int TSemaphore;

// Ограничения из спецификации
#define MAX_TASKS 32
#define MAX_RESOURCES 16
#define MAX_ACTIVE_TASKS 16
#define MAX_HELD_RESOURCES 8
#define MAX_PRIORITIES 16

// Состояния задачи
typedef enum {
    TASK_SUSPENDED,
    TASK_READY,
    TASK_RUNNING,
    TASK_WAITING
} TTaskState;

// Структура задачи (TCB) - ИЗМЕНЕНО для PT
typedef struct TCB {
    // Идентификация
    int id;
    
    // Состояние
    TTaskState state;
    
    // Параметры PT (вместо EDF)
    int priority;              // Статический приоритет (меньше = выше)
    int original_priority;     // Для HLP (сохраняем исходный приоритет)
    
    // Контекст задачи
    void (*entry_point)(void);
    void *stack_pointer;
    void *stack_base;
    int stack_size;
    
    // Для P/V семафоров
    int waiting_semaphore;
    int held_semaphores[MAX_HELD_RESOURCES];
    
    // Для HLP ресурсов
    int waiting_resource;
    int held_resources[MAX_HELD_RESOURCES];
    int held_count;
    int resource_holder;       // ID ресурса, повысившего приоритет

    // Для PIP ресурсов
    int pip_waiting_resource;  // PIP ресурс, которого ждёт задача (-1 если нет)
    
    // Статистика
    int activation_count;
    int preemption_count;
    int execution_time;
} TCB;

// Структура семафора (для P/V)
typedef struct {
    int id;
    int count;
    int initial_count;
    int waiting_tasks[MAX_TASKS];
    int waiting_count;
    int max_waiting;
} SemaphoreCB;

// Структура HLP ресурса - ИЗМЕНЕНО для PT
typedef struct {
    int id;
    int ceiling_priority;       // Наивысший приоритет среди задач (меньше = выше)
    int holder_task;
    int original_priority;      // Исходный приоритет задачи-владельца
    int waiting_tasks[MAX_TASKS];
    int waiting_count;
    int initialized;
} HLPResource;

// Структура PIP ресурса (Priority Inheritance Protocol)
typedef struct {
    int id;
    int holder_task;
    int waiting_tasks[MAX_TASKS];
    int waiting_count;
    int initialized;
} PIPResource;

// Глобальные переменные
extern TCB TaskQueue[MAX_TASKS];
extern SemaphoreCB ResourceQueue[MAX_RESOURCES];
extern HLPResource HLPResourceQueue[MAX_RESOURCES];
extern PIPResource PIPResourceQueue[MAX_RESOURCES];
extern int CurrentTask;
extern int ReadyList[MAX_TASKS];
extern int ReadyCount;
extern int InISR;
extern int OSStarted;
extern int ActiveTaskCount;

// МАКРОСЫ - без изменений
#define DeclareTask(TaskID) extern void TaskID##_func(void)
#define DeclareResource(ResourceID, CP) /* ничего */
#define TASK(TaskID, priority) void TaskID##_func(void)
#define ISR(IsrID) void IsrID##_handler(void)

// Системные сервисы
void StartOS(TTask task);
void ShutdownOS(void);
void ActivateTask(TTask task);
void TerminateTask(void);
void ISRActivateTask(TTask task);
void EnterISR(void);
void LeaveISR(void);
void InitPVS(TSemaphore S);
void P(TSemaphore S);
void V(TSemaphore S);
void GetResource(TResource res);
void ReleaseResource(TResource res);
void InitPIPResource(TResource res);
void GetResourcePIP(TResource res);
void ReleaseResourcePIP(TResource res);

// Внутренние функции
void AddToReadyList(int task_id);
void RemoveFromReadyList(int task_id);
void Dispatch(void);
void SortReadyList(void);
void GlobalInit(void);

// Функции инициализации для тестов
void InitTask(int task_id, void (*task_func)(void), int priority, int stack_size);
void InitHLPResource(TResource res, int ceiling_priority);
void InitPIPResource(TResource res);

// Вспомогательные функции
int GetCurrentTask(void);
int GetReadyCount(void);
void PrintSystemState(void);
void PrintResourceInfo(TResource res);
void PrintSemaphoreInfo(TSemaphore S);

#endif