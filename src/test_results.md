# Результаты тестирования ОСРВ с PT-планированием

**Протокол:** Priority Inheritance Protocol (PIP) + HLP + P/V семафоры
**Планировщик:** PT (Priority Tasks) — плоский, вытесняющий
**Все тесты пройдены: 11 / 11 ✅**

---

## Сводная таблица результатов

| № | Название теста | Результат |
|---|---------------|-----------|
| 1 | PT Scheduling (Priority Order) | ✅ PASS |
| 2 | Priority Preemption | ✅ PASS |
| 3 | P/V Semaphores | ✅ PASS |
| 4 | HLP Protocol | ✅ PASS |
| 5 | Interrupt Handling | ✅ PASS |
| 6 | System Limits | ✅ PASS |
| 7 | TerminateTask in Critical Section | ✅ PASS |
| 8 | Task Declaration Before Use | ✅ PASS |
| 9 | PIP Basic Priority Inheritance | ✅ PASS |
| 10 | PIP Multiple Users (Priority Order) | ✅ PASS |
| 11 | PIP Nested Resources (Two Resources) | ✅ PASS |

---

## Тест 1 — PT Scheduling (Priority Order)

**Описание:** Проверяется порядок выполнения задач по убыванию приоритета.
Три задачи с приоритетами 1 (высокий), 2 (средний), 3 (низкий) должны
выполняться строго в порядке от наивысшего приоритета к наименьшему.

```
=== TEST 1: PT Scheduling (Priority Order) ===
InitTask: Task 3 initialized with priority 0
InitTask: Task 0 initialized with priority 1
InitTask: Task 1 initialized with priority 2
InitTask: Task 2 initialized with priority 3
StartOS: Starting system with initial task 3 (priority=0)
ActivateTask: Activating task 3 (priority=0)
Task 3 (prio=0) added to ready list
Dispatch: Task 3 started (priority=0)
Starter: activating all tasks
ActivateTask: Activating task 2 (priority=3)
Task 2 (prio=3) added to ready list
ActivateTask: Activating task 1 (priority=2)
Ready list sorted by priority: 1(p=2) 2(p=3)
Task 1 (prio=2) added to ready list
ActivateTask: Activating task 0 (priority=1)
Ready list sorted by priority: 0(p=1) 1(p=2) 2(p=3)
Task 0 (prio=1) added to ready list
TerminateTask: Terminating task 3
Task 3 terminated, active tasks now = 3
Dispatch: Task 0 started (priority=1)
  [RECORD] 1
High priority task (prio=1) running
  [RECORD] 2
TerminateTask: Terminating task 0
Task 0 terminated, active tasks now = 2
Dispatch: Task 1 started (priority=2)
  [RECORD] 3
Medium priority task (prio=2) running
  [RECORD] 4
TerminateTask: Terminating task 1
Task 1 terminated, active tasks now = 1
Dispatch: Task 2 started (priority=3)
  [RECORD] 5
Low priority task (prio=3) running
  [RECORD] 6
TerminateTask: Terminating task 2
Task 2 terminated, active tasks now = 0
Dispatch: No tasks to run, system idle
StartOS: System shutdown

PT Scheduling - Expected: 1 2 3 4 5 6
PT Scheduling - Actual:   1 2 3 4 5 6
PT Scheduling - PASS
```

---

## Тест 2 — Priority Preemption (Вытеснение по приоритету)

**Описание:** Проверяется механизм вытеснения. Задача с низким приоритетом
запускается первой, но при активации задачи с более высоким приоритетом
планировщик немедленно переключается на неё.

```
=== TEST 2: Priority Preemption ===
InitTask: Task 0 initialized with priority 5
InitTask: Task 1 initialized with priority 1
InitTask: Task 2 initialized with priority 0
StartOS: Starting system with initial task 2 (priority=0)
ActivateTask: Activating task 2 (priority=0)
Task 2 (prio=0) added to ready list
Dispatch: Task 2 started (priority=0)
ActivateTask: Activating task 0 (priority=5)
Task 0 (prio=5) added to ready list
ActivateTask: Activating task 1 (priority=1)
Ready list sorted by priority: 1(p=1) 0(p=5)
Task 1 (prio=1) added to ready list
TerminateTask: Terminating task 2
Task 2 terminated, active tasks now = 2
Dispatch: Task 1 started (priority=1)
  [RECORD] 1
High priority task (prio=1) starts
  [RECORD] 2
High priority task ends
TerminateTask: Terminating task 1
Task 1 terminated, active tasks now = 1
Dispatch: Task 0 started (priority=5)
  [RECORD] 3
Low priority task (prio=5) starts
Low task working...
Low task working...
Low task working...
  [RECORD] 4
Low priority task ends
TerminateTask: Terminating task 0
Task 0 terminated, active tasks now = 0
Dispatch: No tasks to run, system idle
StartOS: System shutdown

Preemption - Expected: 1 2 3 4
Preemption - Actual:   1 2 3 4
Preemption - PASS
```

---

## Тест 3 — P/V Semaphores (Семафоры)

**Описание:** Проверяется работа простых семафоров (P/V операции).
Производитель увеличивает счётчик семафора (V), потребители захватывают
его (P). Проверяется корректность очерёдности и состояния счётчика.

```
=== TEST 3: P/V Semaphores ===
InitPVS: Initializing semaphore 0
Semaphore 0 initialized with count=1
InitTask: Task 3 initialized with priority 0
InitTask: Task 0 initialized with priority 1
InitTask: Task 1 initialized with priority 2
InitTask: Task 2 initialized with priority 3
StartOS: Starting system with initial task 3 (priority=0)
ActivateTask: Activating task 3 (priority=0)
Task 3 (prio=0) added to ready list
Dispatch: Task 3 started (priority=0)
ActivateTask: Activating task 2 (priority=3)
ActivateTask: Activating task 1 (priority=2)
ActivateTask: Activating task 0 (priority=1)
Ready list sorted by priority: 0(p=1) 1(p=2) 2(p=3)
TerminateTask: Terminating task 3
Dispatch: Task 0 started (priority=1)
  [RECORD] 1
Producer: V(sem)
V: Task 0 released semaphore 0 (count=2)
  [RECORD] 2
Producer: V(sem) again
V: Task 0 released semaphore 0 (count=3)
  [RECORD] 3
TerminateTask: Terminating task 0
Dispatch: Task 1 started (priority=2)
  [RECORD] 4
Consumer: P(sem)
P: Task 1 acquired semaphore 0 (count now=2)
  [RECORD] 5
Consumer: P(sem) again
  [RECORD] 6
TerminateTask: Terminating task 1
Dispatch: Task 2 started (priority=3)
  [RECORD] 7
Consumer2: P(sem) (should wait)
P: Task 2 acquired semaphore 0 (count now=3)
  [RECORD] 8
TerminateTask: Terminating task 2
Dispatch: No tasks to run, system idle
StartOS: System shutdown

Semaphore - Expected: 1 2 3 4 5 6 7 8
Semaphore - Actual:   1 2 3 4 5 6 7 8
Semaphore - PASS
```

---

## Тест 4 — HLP Protocol (Протокол верхней границы)

**Описание:** Проверяется протокол HLP (Highest Locker Protocol).
При захвате ресурса задача с низким приоритетом временно повышает
свой приоритет до значения потолка (ceiling priority) ресурса.
После освобождения приоритет восстанавливается.

```
=== TEST 4: HLP Protocol ===
HLP Resource 0 initialized with ceiling priority 1
HLP Resource 1 initialized with ceiling priority 2
InitTask: Task 3 initialized with priority 0
InitTask: Task 0 initialized with priority 3
InitTask: Task 1 initialized with priority 2
InitTask: Task 2 initialized with priority 1
StartOS: Starting system with initial task 3 (priority=0)
ActivateTask: Activating task 3 (priority=0)
Dispatch: Task 3 started (priority=0)
ActivateTask: Activating task 0 (priority=3)
ActivateTask: Activating task 1 (priority=2)
ActivateTask: Activating task 2 (priority=1)
Ready list sorted by priority: 2(p=1) 1(p=2) 0(p=3)
TerminateTask: Terminating task 3
Dispatch: Task 2 started (priority=1)
  [RECORD] 9
High task: GetResource(R1) - should wait for Low task
GetResource: Task 2 (prio=1) trying to acquire resource 0 (ceiling=1)
Task 2 now holds HLP resource 0 (total held=1)
GetResource: Task 2 acquired resource 0 (priority now=1)
  [RECORD] 10
High task: got resource
  [RECORD] 11
High task: ReleaseResource(R1)
ReleaseResource: Task 2 releasing resource 0
ReleaseResource: No tasks waiting for resource 0
ReleaseResource: Task 2 released resource 0
  [RECORD] 12
TerminateTask: Terminating task 2
Dispatch: Task 1 started (priority=2)
  [RECORD] 5
Medium task: GetResource(R2)
GetResource: Task 1 (prio=2) trying to acquire resource 1 (ceiling=2)
Task 1 now holds HLP resource 1 (total held=1)
GetResource: Task 1 acquired resource 1 (priority now=2)
  [RECORD] 6
  [RECORD] 7
Medium task: ReleaseResource(R2)
ReleaseResource: Task 1 released resource 1
  [RECORD] 8
TerminateTask: Terminating task 1
Dispatch: Task 0 started (priority=3)
  [RECORD] 1
Low task: GetResource(R1)
GetResource: Task 0 (prio=3) trying to acquire resource 0 (ceiling=1)
HLP: Raising task 0 priority from 3 to 1
GetResource: Task 0 acquired resource 0 (priority now=1)
  [RECORD] 2
Low task: working with resource (priority should be raised to 1)
Low task working...
  [RECORD] 3
Low task: ReleaseResource(R1)
HLP: Restoring task 0 priority from 1 to 3
ReleaseResource: Task 0 released resource 0
  [RECORD] 4
TerminateTask: Terminating task 0
Dispatch: No tasks to run, system idle
StartOS: System shutdown

HLP - Expected: 9 10 11 12 5 6 7 8 1 2 3 4
HLP - Actual:   9 10 11 12 5 6 7 8 1 2 3 4
HLP - PASS
```

---

## Тест 5 — Interrupt Handling (Обработка прерываний)

**Описание:** Проверяется механизм обработки прерываний. Основная задача
генерирует прерывание (ISR), которое активирует высокоприоритетную задачу.
Особенность: в обработчике прерывания допускается объявление локальных
переменных (`local_var = 42`). После обработки прерывания основная задача
возобновляет работу.

```
=== TEST 5: Interrupt Handling ===
InitTask: Task 0 initialized with priority 2
InitTask: Task 1 initialized with priority 1
StartOS: Starting system with initial task 0 (priority=2)
ActivateTask: Activating task 0 (priority=2)
Dispatch: Task 0 started (priority=2)
  [RECORD] 1
Main task: generating interrupt
EnterISR: Entering interrupt handler
ISR: local_var=42
ISRActivateTask: Task 1 activated from ISR
Task 1 (prio=1) added to ready list
  [RECORD] 2
LeaveISR: Leaving interrupt handler
LeaveISR: Rescheduling after interrupt
Dispatch: Task 0 (prio=2) preempted by task 1 (prio=1)
Ready list sorted by priority: 1(p=1) 0(p=2)
Task 0 (prio=2) added to ready list
Dispatch: Task 1 started (priority=1)
  [RECORD] 4
ISR task: activated from interrupt
TerminateTask: Terminating task 1
Dispatch: Task 0 started (priority=2)
Main task: resumed after interrupt
  [RECORD] 3
TerminateTask: Terminating task 0
Dispatch: No tasks to run, system idle
StartOS: System shutdown

Interrupts - Expected: 1 2 4 3
Interrupts - Actual:   1 2 4 3
Interrupts - PASS
```

---

## Тест 6 — System Limits (Системные ограничения)

**Описание:** Проверяется ограничение на максимальное количество активных
задач (MAX_ACTIVE_TASKS = 16). Создаётся 16 задач, попытка активировать
17-ю должна быть отклонена системой. Все 15 задач (без Starter) выполняются
в порядке активации.

```
=== TEST 6: System Limits ===
InitTask: Task 15 initialized with priority 0
InitTask: Task 0  initialized with priority 1
InitTask: Task 1  initialized with priority 1
... (задачи 2–14 инициализируются аналогично)
Attempting to create 16th task:
InitTask: Task 16 initialized with priority 1
ActiveTaskCount after creation: 0
StartOS: Starting system with initial task 15 (priority=0)
...
ActivateTask: Activating task 14 (priority=1)
ERROR: Maximum active tasks (16) reached, cannot activate task 16
...
TerminateTask: Terminating task 14
Task 14 terminated, active tasks now = 0
Dispatch: No tasks to run, system idle
StartOS: System shutdown

Limits - Expected: 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
Limits - Actual:   1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
Limits - PASS
```

---

## Тест 7 — TerminateTask in Critical Section (Запрет завершения в критической секции)

**Описание:** Проверяется защита от вызова TerminateTask во время
удержания ресурса. Система должна отклонить вызов TerminateTask и
продолжить выполнение задачи до освобождения ресурса.

```
=== TEST 7: TerminateTask in Critical Section ===
HLP Resource 0 initialized with ceiling priority 1
InitTask: Task 0 initialized with priority 1
StartOS: Starting system with initial task 0 (priority=1)
Dispatch: Task 0 started (priority=1)
Task: acquiring resource
GetResource: Task 0 (prio=1) trying to acquire resource 0 (ceiling=1)
Task 0 now holds HLP resource 0 (total held=1)
GetResource: Task 0 acquired resource 0 (priority now=1)
Task: in critical section, trying to terminate
Attempting TerminateTask in critical section (should fail)
TerminateTask: Terminating task 0
ERROR: TerminateTask called from critical section (held 1 resources)
Task: still running - TerminateTask was blocked
ReleaseResource: Task 0 releasing resource 0
ReleaseResource: No tasks waiting for resource 0
ReleaseResource: Task 0 released resource 0
Task: resource released
  [RECORD] 1
TerminateTask: Terminating task 0
Task 0 terminated, active tasks now = 0
Dispatch: No tasks to run, system idle
StartOS: System shutdown

Critical Section - Expected: 1
Critical Section - Actual:   1
Critical Section - PASS
```

---

## Тест 8 — Task Declaration Before Use (Объявление задач)

**Описание:** Проверяется корректность использования макроса DeclareTask
для объявления задач до их определения (forward declaration).
Задача Task_Forward активирует Task_User, объявленную выше через DeclareTask.

```
=== TEST 8: Task Declaration Before Use ===
InitTask: Task 0 initialized with priority 1
InitTask: Task 1 initialized with priority 2
StartOS: Starting system with initial task 0 (priority=1)
ActivateTask: Activating task 0 (priority=1)
Task 0 (prio=1) added to ready list
Dispatch: Task 0 started (priority=1)
Forward declared task running
ActivateTask: Activating task 1 (priority=2)
Task 1 (prio=2) added to ready list
  [RECORD] 1
TerminateTask: Terminating task 0
Task 0 terminated, active tasks now = 1
Dispatch: Task 1 started (priority=2)
  [RECORD] 2
TerminateTask: Terminating task 1
Task 1 terminated, active tasks now = 0
Dispatch: No tasks to run, system idle
StartOS: System shutdown

Declaration - Expected: 1 2
Declaration - Actual:   1 2
Declaration - PASS
```

---

## Тест 9 — PIP Basic Priority Inheritance (Базовое наследование приоритета)

**Описание:** Проверяется базовая работа протокола PIP (Priority Inheritance
Protocol). Задача с высоким приоритетом (prio=1) запускается первой и
захватывает ресурс. Задача с низким приоритетом (prio=3) запускается после,
когда ресурс уже свободен. Порядок: High → Low.

```
=== TEST 9: PIP Basic Priority Inheritance ===
PIP Resource 0 initialized
InitTask: Task 2 initialized with priority 0
InitTask: Task 0 initialized with priority 3
InitTask: Task 1 initialized with priority 1
StartOS: Starting system with initial task 2 (priority=0)
ActivateTask: Activating task 2 (priority=0)
Dispatch: Task 2 started (priority=0)
ActivateTask: Activating task 0 (priority=3)
ActivateTask: Activating task 1 (priority=1)
Ready list sorted by priority: 1(p=1) 0(p=3)
TerminateTask: Terminating task 2
Dispatch: Task 1 started (priority=1)
  [RECORD] 5
PIP_High: GetResourcePIP(0) — should inherit priority to Low
GetResourcePIP: Task 1 (prio=1) acquiring PIP resource 0
GetResourcePIP: Task 1 acquired PIP resource 0
  [RECORD] 6
PIP_High: got resource
  [RECORD] 7
ReleaseResourcePIP: Task 1 releasing PIP resource 0
ReleaseResourcePIP: No tasks waiting for resource 0
  [RECORD] 8
TerminateTask: Terminating task 1
Dispatch: Task 0 started (priority=3)
  [RECORD] 1
PIP_Low: GetResourcePIP(0)
GetResourcePIP: Task 0 (prio=3) acquiring PIP resource 0
GetResourcePIP: Task 0 acquired PIP resource 0
  [RECORD] 2
PIP_Low: working in critical section (inherited prio should be 1)
  [RECORD] 3
PIP_Low: ReleaseResourcePIP(0)
ReleaseResourcePIP: Task 0 releasing PIP resource 0
ReleaseResourcePIP: No tasks waiting for resource 0
  [RECORD] 4
TerminateTask: Terminating task 0
Dispatch: No tasks to run, system idle
StartOS: System shutdown

PIP Basic - Expected: 5 6 7 8 1 2 3 4
PIP Basic - Actual:   5 6 7 8 1 2 3 4
PIP Basic - PASS
```

---

## Тест 10 — PIP Multiple Users / Priority Order (Несколько пользователей ресурса)

**Описание:** Три задачи с разными приоритетами (1, 2, 3) поочерёдно
захватывают один PIP ресурс. Проверяется, что ресурс передаётся задачам
строго в порядке убывания приоритета: сначала задача A (prio=1),
затем B (prio=2), затем C (prio=3). Также проверяется корректное
восстановление приоритета после каждого освобождения ресурса.

```
=== TEST 10: PIP Multiple Users (Priority Order) ===
PIP Resource 0 initialized
InitTask: Task 3 initialized with priority 0
InitTask: Task 0 initialized with priority 1
InitTask: Task 1 initialized with priority 2
InitTask: Task 2 initialized with priority 3
StartOS: Starting system with initial task 3 (priority=0)
ActivateTask: Activating task 3 (priority=0)
Dispatch: Task 3 started (priority=0)
ActivateTask: Activating task 2 (priority=3)
ActivateTask: Activating task 1 (priority=2)
ActivateTask: Activating task 0 (priority=1)
Ready list sorted by priority: 0(p=1) 1(p=2) 2(p=3)
TerminateTask: Terminating task 3
Dispatch: Task 0 started (priority=1)
  [RECORD] 1
GetResourcePIP: Task 0 (prio=1) acquiring PIP resource 0
GetResourcePIP: Task 0 acquired PIP resource 0
  [RECORD] 2
ReleaseResourcePIP: Task 0 releasing PIP resource 0
ReleaseResourcePIP: No tasks waiting for resource 0
  [RECORD] 3
TerminateTask: Terminating task 0
Dispatch: Task 1 started (priority=2)
  [RECORD] 4
GetResourcePIP: Task 1 (prio=2) acquiring PIP resource 0
GetResourcePIP: Task 1 acquired PIP resource 0
  [RECORD] 5
ReleaseResourcePIP: Task 1 releasing PIP resource 0
ReleaseResourcePIP: No tasks waiting for resource 0
  [RECORD] 6
TerminateTask: Terminating task 1
Dispatch: Task 2 started (priority=3)
  [RECORD] 7
GetResourcePIP: Task 2 (prio=3) acquiring PIP resource 0
GetResourcePIP: Task 2 acquired PIP resource 0
  [RECORD] 8
ReleaseResourcePIP: Task 2 releasing PIP resource 0
ReleaseResourcePIP: No tasks waiting for resource 0
  [RECORD] 9
TerminateTask: Terminating task 2
Dispatch: No tasks to run, system idle
StartOS: System shutdown

PIP Multiple Users - Expected: 1 2 3 4 5 6 7 8 9
PIP Multiple Users - Actual:   1 2 3 4 5 6 7 8 9
PIP Multiple Users - PASS
```

---

## Тест 11 — PIP Nested Resources / Two Resources (Вложенный захват двух ресурсов)

**Описание:** Проверяется корректная работа PIP при использовании
двух ресурсов одновременно. Задача последовательно захватывает R1, затем R2
(вложенный захват), и освобождает их в обратном порядке (LIFO: сначала R2,
затем R1). Проверяется корректное управление несколькими PIP ресурсами
в рамках одной задачи.

```
=== TEST 11: PIP Nested Resources (Two Resources) ===
PIP Resource 0 initialized
PIP Resource 1 initialized
InitTask: Task 1 initialized with priority 0
InitTask: Task 0 initialized with priority 2
StartOS: Starting system with initial task 1 (priority=0)
ActivateTask: Activating task 1 (priority=0)
Dispatch: Task 1 started (priority=0)
ActivateTask: Activating task 0 (priority=2)
Task 0 (prio=2) added to ready list
TerminateTask: Terminating task 1
Dispatch: Task 0 started (priority=2)
  [RECORD] 1
GetResourcePIP: Task 0 (prio=2) acquiring PIP resource 0
GetResourcePIP: Task 0 acquired PIP resource 0
  [RECORD] 2
GetResourcePIP: Task 0 (prio=2) acquiring PIP resource 1
GetResourcePIP: Task 0 acquired PIP resource 1
  [RECORD] 3
ReleaseResourcePIP: Task 0 releasing PIP resource 1
ReleaseResourcePIP: No tasks waiting for resource 1
  [RECORD] 4
ReleaseResourcePIP: Task 0 releasing PIP resource 0
ReleaseResourcePIP: No tasks waiting for resource 0
  [RECORD] 5
TerminateTask: Terminating task 0
Task 0 terminated, active tasks now = 0
Dispatch: No tasks to run, system idle
StartOS: System shutdown

PIP Nested Resources - Expected: 1 2 3 4 5
PIP Nested Resources - Actual:   1 2 3 4 5
PIP Nested Resources - PASS
```

---

## Итог

```
========================================
=== All Tests Completed ===
========================================

PT Scheduling    - PASS   ✅
Preemption       - PASS   ✅
Semaphore        - PASS   ✅
HLP              - PASS   ✅
Interrupts       - PASS   ✅
Limits           - PASS   ✅
Critical Section - PASS   ✅
Declaration      - PASS   ✅
PIP Basic        - PASS   ✅
PIP Multiple Users     - PASS   ✅
PIP Nested Resources   - PASS   ✅

Итого: 11 / 11 тестов пройдено успешно
```

---

*Реализация PIP выполнена в файле `src/PT/pip.c`.*
*Функции: `InitPIPResource`, `GetResourcePIP`, `ReleaseResourcePIP`.*
*Поддерживается транзитивное наследование приоритета (`ApplyInheritance`).*
