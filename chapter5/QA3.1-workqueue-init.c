5.3 workqueue工作队列
	工作队列机制是除了软中断和tasklet以外最常用的一种下半部机制。其基本原理是把work(需要推迟执行的函数)交由一个内核线程来执行，它总是在进程上下文中执行。
工作队列的优点是利用进程上下文来执行中断下半部操作，因此工作队列允许重新调度和睡眠，是异步执行的进程上下文，另外它还能解决软中断和tasklet执行时间过长导致
系统实时性下降等问题。
	针对长期测试workqueue发现的内核线程数量太多、并发性比较差、死锁等问题，Linux 2.6.36提出了CMWQ(concurrency-managed workqueues)，CMWQ提出了工作线程池的概念
(worker-pool)，一共有两种
	第一种，BOUND类型，可以理解为Per-CPU类型，每个CPU都有worker-pool
	第二种，UNBOUND类型，即不和具体CPU绑定
	这两种worker-pool都会定义两个线程池，一个是普通优先级work使用，另一个给高优先级work使用
5.3.1 初始化工作队列
	workqueue机制最小的调度单元是work item，本文称为work/工作任务。
struct work_struct {
    atomic_long_t data;//低位-work标志位，剩余的位-存放上一次运行的worker_pool的ID号或pool_workqueue的指针，存放的内容由WORK_STRUCT_PWQ标志位来决定
    struct list_head entry;//把work挂到其它队列上
    work_func_t func;//工作任务的处理函数
};
	work运行在内核线程中，这个内核线程在本文称为工作线程或worker(工人)，work就是工人的工作
struct worker {
    struct work_struct  *current_work;//当前正在处理的work
    work_func_t     current_func;//当前正在执行的work回调函数
    struct pool_workqueue   *current_pwq;//当前work所属的pool_workqueue
    struct list_head    scheduled;//所有被调度并准备执行的work都挂入该链表中

    struct task_struct  *task;//该工作线程的task_struct数据结构
    struct worker_pool  *pool;//该工作线程所属的worker_pool
    struct list_head    node;//可以把该worker挂入到worker_pool->workers链表中
    int         id;//工作线程的ID号
};
	worker-pool或者工作线程池，它是Per-CPU变量，简化后
struct worker_pool {
    spinlock_t      lock;//保护worker-pool的自选锁
    int         cpu;//DOUND类型的workqueue对应绑定的CPU ID；UNBOUND类型，等于-1
    int         node;//UNBOUND类型的workqueue，node表示该worker-pool所属内存节点的ID编号
    int         id;//worker-pool的ID号
    unsigned int        flags;
    struct list_head    worklist;//pending状态的work会挂入该链表中
    int         nr_workers;//工作线程的数量
    int         nr_idle;//处于idle状态的工作线程的数量
    struct list_head    idle_list;//处于idle状态的工作线程会挂入该链表中
    struct list_head    workers;//该worker-pool管理的工作线程会挂入该链表中
    struct workqueue_attrs  *attrs;//工作线程的属性
    atomic_t        nr_running ____cacheline_aligned_in_smp;/*统计计数，用于管理worker的创建和销毁，表示正在运行中的woker数量。在进程调度器中
唤醒进程时(try_to_wake_up())，为其他CPU可能会同时访问该成员，该成员频繁在多核之间读写，因此让该成员独占一个缓冲行，避免多核CPU在读写该成员时引发
其他临近成员“颠簸”现象。
*/
    struct rcu_head     rcu;//RCU锁
} ____cacheline_aligned_in_smp;
	连接workqueue和worker-pool的枢纽，pool_workqueue数据结构，简化后
struct pool_workqueue {
    struct worker_pool  *pool;//指向worker-pool指针
    struct workqueue_struct *wq;//指向所属的工作队列
    int         nr_active;//活跃的work数量
    int         max_active;//活跃的work最大数量
    struct list_head    delayed_works;//链表头，被延迟执行的works可以挂入该链表
    struct rcu_head     rcu;//rcu锁
} __aligned(1 << WORK_STRUCT_FLAG_BITS);/*1<<8，256字节对齐，这样方便把该数据结构指针的bit[8:31]位放到work->data中，work->data的低8位存放一些标志位，
见set_work_pwq()/get_work_pwq()*/
	系统中所有的工作队列，无论系统默认还是驱动创建的，他们共享一组worker_pool。对于BOUND类型的工作队列，每个CPU只有两个工作线程池，每个工作线程池可以和
多个workerqueue对应，每个workerqueue也只能对应这几个工作线程池。工作队列数据结构简化后，
struct workqueue_struct {
    struct list_head    pwqs;//所有的pool-workqueue数据结构都挂入链表中
    struct list_head    list;//系统定义一个全局的链表workqueues，所有的workqueue挂入该链表
    struct list_head    maydays;//所有rescue状态下的pool-workqueue挂入该链表
    struct worker       *rescuer;//rescue内核线程，内存紧张时创建新的工作线程可能会失败，如果设置了WQ_MEM_RECLAIM，那么rescuer线程会接管这种情况

    struct workqueue_attrs  *unbound_attrs;//UNBOUND类型属性
    struct pool_workqueue   *dfl_pwq;//指向UNBOUND类型的pool_workqueue

    char            name[WQ_NAME_LEN];//该workqueue的名字

    unsigned int        flags ____cacheline_aligned;//标志位经常被不同CPU访问，因此要和cache line对齐。标志位包括WQ_UNBOUND、WQ_HIGHPRI、WQ_FREEZABLE等
    struct pool_workqueue __percpu *cpu_pwqs;//指向Per-CPU类型的pool_workqueue
};
	P655，workqueue、pool_workqueue和worker_pool之间的关系：
[system_wq] [system_highpri_wq] ... [user_create_workqueue|BOUND]         |        [system_unbound_wq] .. [user_create_workqueue|UNBOUND]
————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
[pool_workqueue] [pool_workqueue]    [pool_workqueue] [pool_workqueue]            [pool_workqueue] [pool_workqueue] [pool_workqueue] [pool_workqueue]
————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
[worker_pool] [worker_pool_highpri]  [worker_pool] [worker_pool_highpri]          [worker_pool] [worker_pool_highpri]  [worker_pool] [worker_pool_highpri]
           CPU0                                 CPU1                                        内存节点0                            内存节点n
	(1)一个work挂入workqueue中，最终还要通过系统共享的worker-pool中的worker(工作线程)来处理其回调函数
	(2)workqueue需要找到一个合适的worker-pool，然后从worker-pool中分配一个合适的worker(工作线程)
	(3)pool_workqueue是桥梁作用，类似人力资源池
	在系统启动时，会通过init_workqueues()来初始化几个系统默认的workqueue。
for (i = 0; i < NR_STD_WORKER_POOLS; i++) {//创建UNBOUND类型和ordered类型的workqueue属性，ordered类型的wq表示同一时刻只能有一个work item在运行
    ...
}
system_wq = alloc_workqueue("events", 0, 0);//普通优先级BOUND工作队列，默认工作队列
system_highpri_wq = alloc_workqueue("events_highpri", WQ_HIGHPRI, 0);//高优先级BOUND工作队列
system_long_wq = alloc_workqueue("events_long", 0, 0);
system_unbound_wq = alloc_workqueue("events_unbound", WQ_UNBOUND, WQ_UNBOUND_MAX_ACTIVE);//UNBOUND工作队列
system_freezable_wq = alloc_workqueue("events_freezable", WQ_FREEZABLE, 0);//FREEZABL类型工作队列
system_power_efficient_wq = alloc_workqueue("events_power_efficient", WQ_POWER_EFFICIENT, 0);//省电类型工作队列
system_freezable_power_efficient_wq = alloc_workqueue("events_freezable_power_efficient", WQ_FREEZABLE | WQ_POWER_EFFICIENT, 0);

    init_workqueues()->create_worker()创建工作线程
(1)worker名字一般是 kworker/ + CPU_ID + work_id，如果是高优先级的wq，即nice值小于0，那么还要加上H，例如，ps查看后
root      67    2     0      0              0 0000000000 S kworker/7:0H
root      89    2     0      0              0 0000000000 S kworker/4:1
(2)UNBOUND类型的工作线程，名字为 kworker/u + CPU_ID + work_id，例如
root      121   2     0      0              0 0000000000 S kworker/u17:0

