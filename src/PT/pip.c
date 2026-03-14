// pip.c - Priority Inheritance Protocol (PIP) for PT scheduling
//
// PIP: When a high-priority task H blocks on a resource held by a
// lower-priority task L, task L temporarily inherits H's priority.
// This inheritance is transitive: if L is itself blocked, the priority
// is propagated further up the chain.
// Priority is restored when the resource is released.
//
#include "rtos_api.h"
#include <stdio.h>

extern TCB TaskQueue[MAX_TASKS];
extern PIPResource PIPResourceQueue[MAX_RESOURCES];
extern int CurrentTask;
extern int ReadyList[MAX_TASKS];
extern int ReadyCount;
extern int InISR;
extern int OSStarted;

extern void AddToReadyList(int task_id);
extern void RemoveFromReadyList(int task_id);
extern void Dispatch(void);
extern void SortReadyList(void);

//------------------------------------------------------------------------
// InitPIPResource
//------------------------------------------------------------------------
void InitPIPResource(TResource res) {
    if (res < 0 || res >= MAX_RESOURCES) {
        printf("ERROR: InitPIPResource: invalid resource ID %d\n", res);
        return;
    }

    PIPResourceQueue[res].id = res;
    PIPResourceQueue[res].holder_task = -1;
    PIPResourceQueue[res].waiting_count = 0;
    PIPResourceQueue[res].initialized = 1;

    for (int i = 0; i < MAX_TASKS; i++)
        PIPResourceQueue[res].waiting_tasks[i] = -1;

    printf("PIP Resource %d initialized\n", res);
}

//------------------------------------------------------------------------
// ApplyInheritance
// Sets the holder's priority to the highest waiter's priority (if
// necessary) and propagates transitively if the holder is itself blocked.
//------------------------------------------------------------------------
static void ApplyInheritance(int res_id) {
    PIPResource *res = &PIPResourceQueue[res_id];
    if (res->holder_task == -1 || res->waiting_count == 0) return;

    int holder = res->holder_task;
    // waiting_tasks is sorted: index 0 has highest priority (lowest number)
    int best_waiter_prio = TaskQueue[res->waiting_tasks[0]].priority;

    if (best_waiter_prio >= TaskQueue[holder].priority) return; // no change needed

    printf("PIP: Task %d inherits priority %d (was %d) due to resource %d\n",
           holder, best_waiter_prio, TaskQueue[holder].priority, res_id);

    TaskQueue[holder].priority = best_waiter_prio;
    SortReadyList();

    // Transitive inheritance: if holder is blocked on another PIP resource,
    // propagate to that resource's holder as well.
    if (TaskQueue[holder].state == TASK_WAITING) {
        int blocked_on = TaskQueue[holder].pip_waiting_resource;
        if (blocked_on >= 0 && blocked_on < MAX_RESOURCES &&
            PIPResourceQueue[blocked_on].initialized) {
            ApplyInheritance(blocked_on);
        }
    }
}

//------------------------------------------------------------------------
// RestoreInheritedPriority
// After releasing a resource, restore the task's priority to its
// original value (or to the highest priority still needed for other
// PIP resources it holds).
//------------------------------------------------------------------------
static void RestoreInheritedPriority(int task_id) {
    TCB *task = &TaskQueue[task_id];

    // Start from the original (static) priority
    int restored = task->original_priority;

    // If this task still holds other PIP resources, keep any needed inheritance
    for (int r = 0; r < MAX_RESOURCES; r++) {
        if (!PIPResourceQueue[r].initialized) continue;
        if (PIPResourceQueue[r].holder_task != task_id) continue;
        if (PIPResourceQueue[r].waiting_count == 0) continue;

        int waiter_prio = TaskQueue[PIPResourceQueue[r].waiting_tasks[0]].priority;
        if (waiter_prio < restored)
            restored = waiter_prio;
    }

    if (restored != task->priority) {
        printf("PIP: Restoring task %d priority from %d to %d\n",
               task_id, task->priority, restored);
        task->priority = restored;
        SortReadyList();
    }
}

//------------------------------------------------------------------------
// Add task to PIP waiting queue (sorted by priority, ascending = highest first)
//------------------------------------------------------------------------
static void AddToPIPWaitingQueue(int res_id, int task_id) {
    PIPResource *res = &PIPResourceQueue[res_id];

    if (res->waiting_count >= MAX_TASKS) {
        printf("ERROR: PIP resource %d waiting queue full\n", res_id);
        return;
    }

    res->waiting_tasks[res->waiting_count++] = task_id;

    // Insertion-sort by priority (smaller number = higher priority at index 0)
    for (int i = res->waiting_count - 1; i > 0; i--) {
        if (TaskQueue[res->waiting_tasks[i]].priority <
            TaskQueue[res->waiting_tasks[i - 1]].priority) {
            int tmp = res->waiting_tasks[i];
            res->waiting_tasks[i] = res->waiting_tasks[i - 1];
            res->waiting_tasks[i - 1] = tmp;
        } else {
            break;
        }
    }

    printf("Task %d added to PIP resource %d waiting queue (total=%d)\n",
           task_id, res_id, res->waiting_count);
}

//------------------------------------------------------------------------
// GetResourcePIP
//------------------------------------------------------------------------
void GetResourcePIP(TResource res) {
    if (!OSStarted) {
        printf("ERROR: GetResourcePIP called before StartOS\n");
        return;
    }
    if (res < 0 || res >= MAX_RESOURCES) {
        printf("ERROR: GetResourcePIP: invalid resource ID %d\n", res);
        return;
    }
    if (!PIPResourceQueue[res].initialized) {
        printf("ERROR: GetResourcePIP: resource %d not initialized\n", res);
        return;
    }
    if (CurrentTask == -1) {
        printf("ERROR: GetResourcePIP called with no current task\n");
        return;
    }

    int task = CurrentTask;
    PIPResource *resource = &PIPResourceQueue[res];

    printf("GetResourcePIP: Task %d (prio=%d) acquiring PIP resource %d\n",
           task, TaskQueue[task].priority, res);

    if (resource->holder_task == task) {
        printf("ERROR: Task %d already holds PIP resource %d\n", task, res);
        return;
    }

    if (resource->holder_task == -1) {
        // Resource free
        resource->holder_task = task;
        printf("GetResourcePIP: Task %d acquired PIP resource %d\n", task, res);
    } else {
        // Resource busy — block and apply priority inheritance
        int holder = resource->holder_task;

        printf("GetResourcePIP: Resource %d held by task %d, blocking task %d\n",
               res, holder, task);

        // Block the current task
        RemoveFromReadyList(task);
        TaskQueue[task].state = TASK_WAITING;
        TaskQueue[task].pip_waiting_resource = res;

        AddToPIPWaitingQueue(res, task);

        // Apply priority inheritance (and transitively)
        ApplyInheritance(res);

        CurrentTask = -1;
        Dispatch();

        // Resumes here after ReleaseResourcePIP passes the resource
        printf("GetResourcePIP: Task %d resumed, holds PIP resource %d\n", task, res);
    }
}

//------------------------------------------------------------------------
// ReleaseResourcePIP
//------------------------------------------------------------------------
void ReleaseResourcePIP(TResource res) {
    if (!OSStarted) {
        printf("ERROR: ReleaseResourcePIP called before StartOS\n");
        return;
    }
    if (res < 0 || res >= MAX_RESOURCES) {
        printf("ERROR: ReleaseResourcePIP: invalid resource ID %d\n", res);
        return;
    }
    if (!PIPResourceQueue[res].initialized) {
        printf("ERROR: ReleaseResourcePIP: resource %d not initialized\n", res);
        return;
    }
    if (CurrentTask == -1) {
        printf("ERROR: ReleaseResourcePIP called with no current task\n");
        return;
    }

    int task = CurrentTask;
    PIPResource *resource = &PIPResourceQueue[res];

    if (resource->holder_task != task) {
        printf("ERROR: Task %d does not hold PIP resource %d\n", task, res);
        return;
    }

    printf("ReleaseResourcePIP: Task %d releasing PIP resource %d\n", task, res);

    resource->holder_task = -1;

    // Restore the releasing task's priority (may still hold other PIP resources)
    RestoreInheritedPriority(task);

    // Wake up the highest-priority waiter and give it the resource
    if (resource->waiting_count > 0) {
        int next_task = resource->waiting_tasks[0];

        // Shift the queue
        for (int i = 0; i < resource->waiting_count - 1; i++)
            resource->waiting_tasks[i] = resource->waiting_tasks[i + 1];
        resource->waiting_tasks[--resource->waiting_count] = -1;

        printf("ReleaseResourcePIP: Passing resource %d to task %d\n", res, next_task);

        resource->holder_task = next_task;
        TaskQueue[next_task].state = TASK_READY;
        TaskQueue[next_task].pip_waiting_resource = -1;
        AddToReadyList(next_task);

        // Apply inheritance for the new holder if other tasks still wait
        ApplyInheritance(res);

        // Preempt current task if new holder has higher priority
        if (TaskQueue[next_task].priority < TaskQueue[task].priority) {
            printf("ReleaseResourcePIP: Task %d (prio=%d) preempts task %d (prio=%d)\n",
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
        printf("ReleaseResourcePIP: No tasks waiting for resource %d\n", res);
    }
}
