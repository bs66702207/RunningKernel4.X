5.3.2 创建工作队列
    最常见的创建工作队列的API #define alloc_workqueue(fmt, flags, max_active, args...)，其他类似。
flags:
    (1)WQ_UNBOUND: 没有绑定CPU，这类work会牺牲一部分性能，适用于一些应用会在不同的CPU上跳跃、长时间运行的CPU消耗类型的应用(WQ_CPU_INTENSIVE)。
    (2)WQ_FREEZABLE: 在系统的suspend过程中，要让worker完成当前的work，才完成进程冻结，并且在这个过程中不会再新开始一个work的执行。
    (3)WQ_MEM_RECLAIN: 内存紧张时，创建worker可能会失败，这时候会有rescuer内核线程去接管这种情况。
    (4)WQ_HIGHPRI: 高优先级的worker-pool，即比较低的nice值。
    (5)WQ_CPU_INTENSIVE: 特别消耗CPU资源的一类work，这类work的执行会得到系统进程调度器的监管。
    (6)__WQ_ORDERED: 同一时间只能执行一个work item。
max_active:
    它决定每个CPU最多可以有多少个work挂入一个工作队列，通常对与BOUND类型的wq，max_active最大可以512，如果传入max_active=0，则表示指定256;
对于UNBOUND类型的wq，max_active可以取512和4*num_possible_cpus()之间的最大值。通常驱动开发都是max_active=0。
    alloc_workqueue()->__alloc_workqueue_key()
WQ_POWER_EFFICIENT: 考虑系统的功耗，对于BOUND类型的workqueue，它是Per-CPU变量，会利用cache的局部性原理来提高性能，也就是说它不会进行CPU之间的
迁移，也不希望进程调度器打扰；设置UNBOUND类型的wq后，究竟选择哪个CPU上唤醒交由进程调度器决定。Per-CPU的wq会让idle状态的CPU从idle状态唤醒，从而
增加功耗。如果系统配置了WQ_POWER_EFFICIENT_DEFAULT选项，那么创建wq会把标记了WQ_POWER_EFFICIENT_DEFAULT的wq设置成UNBOUND类型，这样进程调度器可以参与选择哪个CPU来执行。
ps: WQ_POWER_EFFICIENT_DEFAULT=y，只是不想让CPU固定地睡眠、唤醒、睡眠、唤醒，由调度器来决定选择哪个CPU唤醒比较好。
    接下来是分配一个workqueue_struct数据结构并初始化。

