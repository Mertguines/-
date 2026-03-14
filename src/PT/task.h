#ifndef TASK_H
#define TASK_H

typedef enum {
    TASK_SUSPENDED,  // Приостановлена (неактивна)
    TASK_READY,      // Готова к выполнению
    TASK_RUNNING,    // Выполняется
    TASK_WAITING     // Ожидает семафор
} TTaskState;

// Структура задачи (TCB)
typedef struct TCB {
    // Идентификация
    int id;
    
    // Состояние
    TTaskState state;
    
    // Параметры EDF
    int deadline;
    int relative_deadline;
    int activation_time;
    
    // Контекст задачи
    void (*entry_point)(void);
    void *stack_pointer;
    void *stack_base;
    int stack_size;
    
    // Семафоры (P/V)
    int waiting_semaphore;
    
    // Ресурсы (HLP)
    int waiting_resource;
    int held_resources[MAX_HELD_RESOURCES];
    int held_count;
    int original_deadline;
    int resource_holder;
    
    // Статистика
    int activation_count;
    int preemption_count;
    int execution_time;
};


#endif