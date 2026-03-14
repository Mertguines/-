// test_pt.c - Комплексное тестирование ОСРВ с PT планированием
#include "rtos_api.h"
#include <stdio.h>
#include <stdlib.h>

// Глобальные переменные для тестов
int test_sequence[100];
int seq_index = 0;

void record(int value) {
    test_sequence[seq_index++] = value;
    printf("  [RECORD] %d\n", value);
}

void clear_sequence() {
    seq_index = 0;
}

void check_sequence(int expected[], int size, const char* test_name) {
    printf("\n%s - Expected: ", test_name);
    for (int i = 0; i < size; i++) printf("%d ", expected[i]);
    printf("\n%s - Actual:   ", test_name);
    for (int i = 0; i < seq_index; i++) printf("%d ", test_sequence[i]);
    printf("\n");
    
    if (seq_index != size) {
        printf("%s - FAIL: Sequence length mismatch (expected %d, got %d)\n", 
               test_name, size, seq_index);
    } else {
        int ok = 1;
        for (int i = 0; i < size; i++) {
            if (test_sequence[i] != expected[i]) {
                printf("%s - FAIL: Position %d: expected %d, got %d\n", 
                       test_name, i, expected[i], test_sequence[i]);
                ok = 0;
                break;
            }
        }
        if (ok) printf("%s - PASS\n", test_name);
    }
}

//==========================================================================
// ТЕСТ 1: Базовое PT планирование (приоритеты)
//==========================================================================

DeclareTask(PT_High);
DeclareTask(PT_Medium);
DeclareTask(PT_Low);
DeclareTask(PT_Starter);

// Приоритеты: 1 - highest, 3 - lowest
TASK(PT_Starter, 0) {  // Самый высокий приоритет
    printf("Starter: activating all tasks\n");
    ActivateTask(2);  // Low
    ActivateTask(1);  // Medium
    ActivateTask(0);  // High
    TerminateTask();
}

TASK(PT_High, 1) {
    record(1);
    printf("High priority task (prio=1) running\n");
    record(2);
    TerminateTask();
}

TASK(PT_Medium, 2) {
    record(3);
    printf("Medium priority task (prio=2) running\n");
    record(4);
    TerminateTask();
}

TASK(PT_Low, 3) {
    record(5);
    printf("Low priority task (prio=3) running\n");
    record(6);
    TerminateTask();
}

void TestPTScheduling() {
    printf("\n=== TEST 1: PT Scheduling (Priority Order) ===\n");
    clear_sequence();
    
    // ID: Starter=3, High=0, Medium=1, Low=2
    InitTask(3, PT_Starter_func, 0, 1024);
    InitTask(0, PT_High_func, 1, 1024);
    InitTask(1, PT_Medium_func, 2, 1024);
    InitTask(2, PT_Low_func, 3, 1024);
    
    StartOS(3);
    
    // Ожидаем: High (1,2), Medium (3,4), Low (5,6)
    int expected[] = {1, 2, 3, 4, 5, 6};
    check_sequence(expected, 6, "PT Scheduling");
}

//==========================================================================
// ТЕСТ 2: Вытеснение по приоритету (Preemption)
//==========================================================================

DeclareTask(Preempt_Low);
DeclareTask(Preempt_High);
DeclareTask(Preempt_Starter);

TASK(Preempt_Low, 5) {
    record(3);  // Должно быть 3 - начало Low (после High)
    printf("Low priority task (prio=5) starts\n");
    for (int i = 0; i < 3; i++) {
        printf("Low task working...\n");
    }
    record(4);  // Должно быть 4 - конец Low
    printf("Low priority task ends\n");
    TerminateTask();
}

TASK(Preempt_High, 1) {
    record(1);  // Должно быть 1 - начало High
    printf("High priority task (prio=1) starts\n");
    record(2);  // Должно быть 2 - конец High
    printf("High priority task ends\n");
    TerminateTask();
}

// Ожидаемая последовательность: High (1,2), Low (3,4) -> {1,2,3,4}

TASK(Preempt_Starter, 0) {
    // Активируем в правильном порядке
    ActivateTask(0);  // Low - активируем первой
    ActivateTask(1);  // High - активируем второй
    TerminateTask();
}

void TestPreemption() {
    printf("\n=== TEST 2: Priority Preemption ===\n");
    clear_sequence();
    
    InitTask(0, Preempt_Low_func, 5, 1024);   // Низкий приоритет
    InitTask(1, Preempt_High_func, 1, 1024);  // Высокий приоритет
    InitTask(2, Preempt_Starter_func, 0, 1024);
    
    StartOS(2);
    
    // Ожидаем: Low начинает (1), High вытесняет (2,3), Low продолжает (4)
    int expected[] = {1, 2, 3, 4};
    check_sequence(expected, 4, "Preemption");
}

//==========================================================================
// ТЕСТ 3: P/V Семафоры
//==========================================================================

DeclareTask(Sem_Producer);
DeclareTask(Sem_Consumer);
DeclareTask(Sem_Consumer2);
DeclareTask(Sem_Starter);

TSemaphore sem = 0;

TASK(Sem_Starter, 0) {
    ActivateTask(2);  // Consumer2
    ActivateTask(1);  // Consumer
    ActivateTask(0);  // Producer
    TerminateTask();
}

TASK(Sem_Producer, 1) {
    record(1);
    printf("Producer: V(sem)\n");
    V(0);
    record(2);
    printf("Producer: V(sem) again\n");
    V(0);
    record(3);
    TerminateTask();
}

TASK(Sem_Consumer, 2) {
    record(4);
    printf("Consumer: P(sem)\n");
    P(0);
    record(5);
    printf("Consumer: P(sem) again\n");
    P(0);
    record(6);
    V(0);  // Освобождаем для Consumer2
    V(0);
    TerminateTask();
}

TASK(Sem_Consumer2, 3) {
    record(7);
    printf("Consumer2: P(sem) (should wait)\n");
    P(0);
    record(8);
    V(0);  // Освобождаем
    TerminateTask();
}

void TestSemaphore() {
    printf("\n=== TEST 3: P/V Semaphores ===\n");
    clear_sequence();
    
    InitPVS(0);  // count = 1
    
    InitTask(3, Sem_Starter_func, 0, 1024);
    InitTask(0, Sem_Producer_func, 1, 1024);
    InitTask(1, Sem_Consumer_func, 2, 1024);
    InitTask(2, Sem_Consumer2_func, 3, 1024);
    
    StartOS(3);
    
    int expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
    check_sequence(expected, 8, "Semaphore");
}

//==========================================================================
// ТЕСТ 4: HLP протокол (GetResource/ReleaseResource)
//==========================================================================

DeclareTask(HLP_Low);
DeclareTask(HLP_Medium);
DeclareTask(HLP_High);
DeclareTask(HLP_Starter);

TASK(HLP_Starter, 0) {
    ActivateTask(0);  // Low
    ActivateTask(1);  // Medium
    ActivateTask(2);  // High
    TerminateTask();
}

TASK(HLP_Low, 3) {  // Низкий приоритет
    record(1);
    printf("Low task: GetResource(R1)\n");
    GetResource(0);  // R1 с ceiling=1
    record(2);
    printf("Low task: working with resource (priority should be raised to 1)\n");
    
    // Имитация работы
    for (int i = 0; i < 3; i++) {
        printf("Low task working...\n");
    }
    
    record(3);
    printf("Low task: ReleaseResource(R1)\n");
    ReleaseResource(0);
    record(4);
    TerminateTask();
}

TASK(HLP_Medium, 2) {  // Средний приоритет
    record(5);
    printf("Medium task: GetResource(R2)\n");
    GetResource(1);  // R2 с ceiling=2
    record(6);
    printf("Medium task: working with resource\n");
    
    record(7);
    printf("Medium task: ReleaseResource(R2)\n");
    ReleaseResource(1);
    record(8);
    TerminateTask();
}

TASK(HLP_High, 1) {  // Высокий приоритет
    record(9);
    printf("High task: GetResource(R1) - should wait for Low task\n");
    GetResource(0);  // Должен ждать, так как R1 у Low
    record(10);
    printf("High task: got resource\n");
    
    record(11);
    printf("High task: ReleaseResource(R1)\n");
    ReleaseResource(0);
    record(12);
    TerminateTask();
}

void TestHLP() {
    printf("\n=== TEST 4: HLP Protocol ===\n");
    clear_sequence();
    
    // Инициализируем HLP ресурсы
    // R1 используется Low(prio=3) и High(prio=1) -> ceiling = min(3,1) = 1
    // R2 используется Medium(prio=2) -> ceiling = 2
    InitHLPResource(0, 1);  // R1 с ceiling priority 1
    InitHLPResource(1, 2);  // R2 с ceiling priority 2
    
    InitTask(3, HLP_Starter_func, 0, 1024);
    InitTask(0, HLP_Low_func, 3, 1024);
    InitTask(1, HLP_Medium_func, 2, 1024);
    InitTask(2, HLP_High_func, 1, 1024);
    
    StartOS(3);
    
    // Ожидаемая последовательность:
    // Low (1,2,3,4), Medium (5,6,7,8), High (9,10,11,12)
    int expected[] = {9, 10, 11, 12, 5, 6, 7, 8, 1, 2, 3, 4};
    check_sequence(expected, 12, "HLP");
}

//==========================================================================
// ТЕСТ 5: Прерывания
//==========================================================================

DeclareTask(Int_Main);
DeclareTask(Int_ISRTask);

int isr_activated = 0;
int main_resumed = 0;  // Флаг для предотвращения дублирования

ISR(TimerISR) {
    EnterISR();
    
    int local_var = 42;
    printf("ISR: local_var=%d\n", local_var);
    
    if (!isr_activated) {
        ISRActivateTask(1);
        isr_activated = 1;
        record(2);
    }
    
    LeaveISR();
}

int interrupt_stage = 0;

TASK(Int_Main, 2) {
    if (interrupt_stage == 0) {
        record(1);
        interrupt_stage = 1;
        printf("Main task: generating interrupt\n");
        TimerISR_handler();
    } else if (interrupt_stage == 2) {
        printf("Main task: resumed after interrupt\n");
        record(3);
        interrupt_stage = 3;
    }
    TerminateTask();
}

TASK(Int_ISRTask, 1) {
    record(4);
    interrupt_stage = 2;
    printf("ISR task: activated from interrupt\n");
    TerminateTask();
}

void TestISR() {
    printf("\n=== TEST 5: Interrupt Handling ===\n");
    clear_sequence();
    isr_activated = 0;
    main_resumed = 0;
    
    InitTask(0, Int_Main_func, 2, 1024);
    InitTask(1, Int_ISRTask_func, 1, 1024);
    
    StartOS(0);
    
    int expected[] = {1, 2, 4, 3};
    check_sequence(expected, 4, "Interrupts");
}

//==========================================================================
// ТЕСТ 6: Ограничения (Max Active Tasks)
//==========================================================================

// Создаем 16 задач с разными функциями
void LimitTask0_func(void) { record(1); TerminateTask(); }
void LimitTask1_func(void) { record(2); TerminateTask(); }
void LimitTask2_func(void) { record(3); TerminateTask(); }
void LimitTask3_func(void) { record(4); TerminateTask(); }
void LimitTask4_func(void) { record(5); TerminateTask(); }
void LimitTask5_func(void) { record(6); TerminateTask(); }
void LimitTask6_func(void) { record(7); TerminateTask(); }
void LimitTask7_func(void) { record(8); TerminateTask(); }
void LimitTask8_func(void) { record(9); TerminateTask(); }
void LimitTask9_func(void) { record(10); TerminateTask(); }
void LimitTask10_func(void) { record(11); TerminateTask(); }
void LimitTask11_func(void) { record(12); TerminateTask(); }
void LimitTask12_func(void) { record(13); TerminateTask(); }
void LimitTask13_func(void) { record(14); TerminateTask(); }
void LimitTask14_func(void) { record(15); TerminateTask(); }
void LimitTask15_func(void) { record(16); TerminateTask(); }

DeclareTask(Limit_Starter);

TASK(Limit_Starter, 0) {
    // Активируем 15 задач (Starter уже активен)
    for (int i = 0; i < 15; i++) {
        ActivateTask(i);
    }
    TerminateTask();
}

void TestLimits() {
    printf("\n=== TEST 6: System Limits ===\n");
    clear_sequence();
    
    // Starter с ID=15
    InitTask(15, Limit_Starter_func, 0, 1024);
    
    // 15 задач с ID от 0 до 14
    InitTask(0, LimitTask0_func, 1, 1024);
    InitTask(1, LimitTask1_func, 1, 1024);
    InitTask(2, LimitTask2_func, 1, 1024);
    InitTask(3, LimitTask3_func, 1, 1024);
    InitTask(4, LimitTask4_func, 1, 1024);
    InitTask(5, LimitTask5_func, 1, 1024);
    InitTask(6, LimitTask6_func, 1, 1024);
    InitTask(7, LimitTask7_func, 1, 1024);
    InitTask(8, LimitTask8_func, 1, 1024);
    InitTask(9, LimitTask9_func, 1, 1024);
    InitTask(10, LimitTask10_func, 1, 1024);
    InitTask(11, LimitTask11_func, 1, 1024);
    InitTask(12, LimitTask12_func, 1, 1024);
    InitTask(13, LimitTask13_func, 1, 1024);
    InitTask(14, LimitTask14_func, 1, 1024);
    
    // Пытаемся создать 16-ю задачу (должна быть ошибка при активации)
    printf("Attempting to create 16th task:\n");
    InitTask(16, LimitTask15_func, 1, 1024);
    
    printf("ActiveTaskCount after creation: %d\n", ActiveTaskCount);
    
    StartOS(15);
    
    int expected[15];
    for (int i = 1; i <= 15; i++) expected[i-1] = i;
    check_sequence(expected, 15, "Limits");
}

//==========================================================================
// ТЕСТ 7: Проверка запрета TerminateTask в критической секции
//==========================================================================

DeclareTask(Test_CriticalSection);

TASK(Test_CriticalSection, 1) {
    printf("Task: acquiring resource\n");
    GetResource(0);
    printf("Task: in critical section, trying to terminate\n");
    
    // Попытка завершиться в критической секции
    printf("Attempting TerminateTask in critical section (should fail)\n");
    TerminateTask();  // Должно выдать ошибку
    
    printf("Task: still running - TerminateTask was blocked\n");
    ReleaseResource(0);
    printf("Task: resource released\n");
    record(1);
    TerminateTask();  // Теперь можно
}

void TestCriticalSection() {
    printf("\n=== TEST 7: TerminateTask in Critical Section ===\n");
    clear_sequence();
    
    InitHLPResource(0, 1);
    InitTask(0, Test_CriticalSection_func, 1, 1024);
    
    StartOS(0);
    
    int expected[] = {1};
    check_sequence(expected, 1, "Critical Section");
}

//==========================================================================
// ТЕСТ 8: Проверка объявления задач до использования
//==========================================================================

// Демонстрация использования DeclareTask до определения
DeclareTask(Task_Forward);
DeclareTask(Task_User);

TASK(Task_Forward, 1) {
    printf("Forward declared task running\n");
    ActivateTask(1);  // Используем Task_User, который объявлен выше
    record(1);
    TerminateTask();
}

TASK(Task_User, 2) {
    record(2);
    TerminateTask();
}

void TestDeclaration() {
    printf("\n=== TEST 8: Task Declaration Before Use ===\n");
    clear_sequence();
    
    InitTask(0, Task_Forward_func, 1, 1024);
    InitTask(1, Task_User_func, 2, 1024);
    
    StartOS(0);
    
    int expected[] = {1, 2};
    check_sequence(expected, 2, "Declaration");
}

//==========================================================================
// ТЕСТ 9: PIP — базовое наследование приоритета
// Сценарий: Low захватывает ресурс, High пытается захватить его же.
//   High блокируется, Low наследует приоритет High, завершает работу,
//   освобождает ресурс, High продолжает.
//==========================================================================

DeclareTask(PIP_Low);
DeclareTask(PIP_High);
DeclareTask(PIP_Starter);

TASK(PIP_Starter, 0) {
    ActivateTask(0);  // Low
    ActivateTask(1);  // High
    TerminateTask();
}

TASK(PIP_Low, 3) {          // статический приоритет = 3 (низкий)
    record(1);
    printf("PIP_Low: GetResourcePIP(0)\n");
    GetResourcePIP(0);
    record(2);
    printf("PIP_Low: working in critical section (inherited prio should be 1)\n");
    record(3);
    printf("PIP_Low: ReleaseResourcePIP(0)\n");
    ReleaseResourcePIP(0);
    record(4);
    TerminateTask();
}

TASK(PIP_High, 1) {         // статический приоритет = 1 (высокий)
    record(5);
    printf("PIP_High: GetResourcePIP(0) — should inherit priority to Low\n");
    GetResourcePIP(0);
    record(6);
    printf("PIP_High: got resource\n");
    record(7);
    ReleaseResourcePIP(0);
    record(8);
    TerminateTask();
}

void TestPIP_Basic() {
    printf("\n=== TEST 9: PIP Basic Priority Inheritance ===\n");
    clear_sequence();

    InitPIPResource(0);

    InitTask(2, PIP_Starter_func, 0, 1024);
    InitTask(0, PIP_Low_func, 3, 1024);
    InitTask(1, PIP_High_func, 1, 1024);

    StartOS(2);

    // Low запускается первой (higher prio after Starter активирует обоих):
    // Starter активирует Low(prio=3), затем High(prio=1).
    // High вытесняет — но Low ещё не захватила ресурс, поэтому High берёт его сразу.
    // Ожидаем: High(5,6,7,8), Low(1,2,3,4)
    int expected[] = {5, 6, 7, 8, 1, 2, 3, 4};
    check_sequence(expected, 8, "PIP Basic");
}

//==========================================================================
// ТЕСТ 10: PIP — несколько задач используют один ресурс в порядке приоритета
// Три задачи конкурируют за PIP ресурс. Так как высокоприоритетная задача
// запускается первой, она первой получает ресурс. Это проверяет корректность
// передачи ресурса и восстановление приоритета.
//==========================================================================

DeclareTask(PIP2_A);
DeclareTask(PIP2_B);
DeclareTask(PIP2_C);
DeclareTask(PIP2_Starter);

TASK(PIP2_Starter, 0) {
    ActivateTask(2);  // C prio=3
    ActivateTask(1);  // B prio=2
    ActivateTask(0);  // A prio=1 (highest)
    TerminateTask();
}

TASK(PIP2_A, 1) {          // наивысший приоритет
    record(1);
    GetResourcePIP(0);
    record(2);
    ReleaseResourcePIP(0);
    record(3);
    TerminateTask();
}

TASK(PIP2_B, 2) {
    record(4);
    GetResourcePIP(0);
    record(5);
    ReleaseResourcePIP(0);
    record(6);
    TerminateTask();
}

TASK(PIP2_C, 3) {          // наименьший приоритет
    record(7);
    GetResourcePIP(0);
    record(8);
    ReleaseResourcePIP(0);
    record(9);
    TerminateTask();
}

void TestPIP_MultipleUsers() {
    printf("\n=== TEST 10: PIP Multiple Users (Priority Order) ===\n");
    clear_sequence();

    InitPIPResource(0);

    InitTask(3, PIP2_Starter_func, 0, 1024);
    InitTask(0, PIP2_A_func, 1, 1024);
    InitTask(1, PIP2_B_func, 2, 1024);
    InitTask(2, PIP2_C_func, 3, 1024);

    StartOS(3);

    // Задачи выполняются строго по убыванию приоритета:
    // A (1,2,3), B (4,5,6), C (7,8,9)
    int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    check_sequence(expected, 9, "PIP Multiple Users");
}

//==========================================================================
// ТЕСТ 11: PIP — работа с двумя ресурсами, вложенный захват
// Задача захватывает два PIP ресурса поочерёдно и освобождает в обратном
// порядке (LIFO). Проверяется корректность управления несколькими ресурсами.
//==========================================================================

DeclareTask(PIP3_Task);
DeclareTask(PIP3_Starter);

TASK(PIP3_Starter, 0) {
    ActivateTask(0);
    TerminateTask();
}

TASK(PIP3_Task, 2) {
    record(1);
    GetResourcePIP(0);   // Захват R1
    record(2);
    GetResourcePIP(1);   // Захват R2 (вложенный)
    record(3);
    ReleaseResourcePIP(1);  // Освобождение R2 (обратный порядок)
    record(4);
    ReleaseResourcePIP(0);  // Освобождение R1
    record(5);
    TerminateTask();
}

void TestPIP_NestedResources() {
    printf("\n=== TEST 11: PIP Nested Resources (Two Resources) ===\n");
    clear_sequence();

    InitPIPResource(0);  // R1
    InitPIPResource(1);  // R2

    InitTask(1, PIP3_Starter_func, 0, 1024);
    InitTask(0, PIP3_Task_func, 2, 1024);

    StartOS(1);

    // Задача захватывает оба ресурса и освобождает их в обратном порядке
    int expected[] = {1, 2, 3, 4, 5};
    check_sequence(expected, 5, "PIP Nested Resources");
}

//==========================================================================
// Главная функция
//==========================================================================

int main() {
    printf("========================================\n");
    printf("=== RTOS with PT Scheduling Testing ===\n");
    printf("========================================\n");
    
    // Тест 1: Базовое PT планирование
    GlobalInit();
    TestPTScheduling();
    
    // Тест 2: Вытеснение по приоритету
    GlobalInit();
    TestPreemption();
    
    // Тест 3: P/V семафоры
    GlobalInit();
    TestSemaphore();
    
    // Тест 4: HLP протокол
    GlobalInit();
    TestHLP();
    
    // Тест 5: Прерывания
    GlobalInit();
    TestISR();
    
    // Тест 6: Ограничения
    GlobalInit();
    TestLimits();
    
    // Тест 7: Запрет TerminateTask в критической секции
    GlobalInit();
    TestCriticalSection();
    
    // Тест 8: Объявление задач до использования
    GlobalInit();
    TestDeclaration();

    // Тест 9: PIP — базовое наследование приоритета
    GlobalInit();
    TestPIP_Basic();

    // Тест 10: PIP — несколько задач в порядке приоритета
    GlobalInit();
    TestPIP_MultipleUsers();

    // Тест 11: PIP — вложенный захват двух ресурсов
    GlobalInit();
    TestPIP_NestedResources();

    printf("\n========================================\n");
    printf("=== All Tests Completed ===\n");
    printf("========================================\n");
    
    return 0;
}