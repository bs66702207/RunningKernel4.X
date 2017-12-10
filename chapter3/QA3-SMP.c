3.3 SMP负载均衡
    SMP Symmetric Muti-Processor 对称多处理器
3.3.1 CPU域初始化
    CPU域的分类
    超线程(SMT, Simultaneous MultiThreading)，CONFIG_SCHED_SMT，一个物理核心可以有两个执行线程，被称为超线程技术。
超线程使用相同CPU资源且共享L1 cache，迁移进程不会影响Cache利用率。
    多核(MC)，CONFIG_SCHED_MC，每个物理核心独享L1 cache，多个物理核心可以组成一个cluster，cluster里的CPU共享L2 cache。
    处理器(SoC)，内核称为DIE(晶元——拥有集成电路的硅晶片)，SoC级别
struct sched_domain_topology_level {//描述CPU层次关系，SDTL层级
    sched_domain_mask_f mask;//函数指针，用于指定某个SDTL层级的cpumask位图
    sched_domain_flags_f sd_flags;//函数指针，用于指定某个SDTL层级的标志位
    int flags;
    struct sd_data data;
}

static struct sched_domain_topology_level default_topology[] = {//CPU物理域的层次结构
#ifdef CONFIG_SCHED_SM//ARM架构目前不支持T
    { cpu_smt_mask, cpu_smt_flags, SD_INIT_NAME(SMT) },
#endif
#ifdef CONFIG_SCHED_MC//通常配置
    { cpu_coregroup_mask, cpu_core_flags, SD_INIT_NAME(MC) },
#endif
    { cpu_cpu_mask, SD_INIT_NAME(DIE) },
    { NULL, },
};

    内核对CPU的管理是通过bitmap类型变量来管理的，并且定义了possible、present、online和active这4种状态。
cpu_possible_mask: 系统中有多少个可以运行(现在运行或者将来某个时间点运行)的CPU核心。
cpu_online_mask: 系统中有多少个正在处于运行状态(online)的CPU核心。
cpu_present_mask: 系统中有多少个具备online条件的CPU核心，它们不一定都处于online状态，有的CPU核心可能被热插拔了。
cpu_active_mask: 系统中有多少个活跃的CPU核心。
    系统启动时即开始构建拓扑关系start_kernel()->rest_init()->kernel_init()->kernel_init_freeable()->sched_init_smp()->init_sched_domain()
    cpu_possible_mask初始化start_kernel()->setup_arch()->arm_dt_init_cpu_maps()
    假设在一个4核处理中，每个CPU核心拥有独立L1 Cache且不支持超线程技术，分成两个簇Cluster0和Cluster1，每个簇包含两个物理CPU核，簇中的CPU核
共享L2 Cache，其示意图如下
 ———————————————————————————————
| 4核处理器                     |
|   Cluster0      CLuster1      |
|   ———————————   ———————————   |
|  | cpu0|cpu1 | | cpu2|cpu3 |  |
|  |—————————— | | ——————————|  |
|  |  L2 Cache | | L2 Cache  |  |
|   ———————————   ———————————   |
 ———————————————————————————————
    其在Linux内核里调度域和调度组的拓扑关系图如下:
/**************************************************DIE等级**************************************************/
    domain_die_0                domain_die_1                domain_die_2                domain_die_3
span[cpu0|cpu1|cpu2|cpu3]   span[cpu0|cpu1|cpu2|cpu3]   span[cpu0|cpu1|cpu2|cpu3]   span[cpu0|cpu1|cpu2|cpu3]

                    group_die_0                                             group_die_1
                    [cpu0|cpu1]                                             [cpu2|cpu3]
/**********************************************MC等级********************************************************/
                domain_mc_0       domain_mc_1           domain_mc_2       domain_mc_3
              span[cpu0|cpu1]   span[cpu0|cpu1]       span[cpu2|cpu3]   span[cpu2|cpu3]

                 group_mc_0        group_mc_1            group_mc_2        group_mc_3
                   [cpu0]            [cpu1]                [cpu2]            [cpu3]
    每个SDTL层级为每个CPU都分配了对应的调度域和调度组，以CPU0为例:
(1)对于DIE级别，CPU0的对应的调度域是domain_die_0，该调度域管辖着4个CPU并包含两个调度组，分别为group_die_0和group_die_1。
    调度组group_die_0管辖CPU0和CPU1。
    调度组group_die_1管辖CPU2和CPU3。
(2)对于MC级别，CPU0对应的调度域是domain_mc_0，该调度域管辖着CPU0和CPU1并包含两个调度组，分别为group_mc_0和group_mc_1。
    调度组group_mc_0管辖CPU0
    调度组group_mc_1管辖CPU1
    在DIE级别中，调度组只有group_die_0和group_die_1，这是因为只有调度域的第一个CPU才会去建立DIE层级的调度组。
    除此之外，还有两层关系：
(1)MC级别的SD(sched domain)中的parent指针指向父调度域(DIE)
(2)SD中的groups指针指向调度组链表

3.2.2 SMP负载均衡
    rebalance_domains(rq, idle)函数是负载均衡的核心入口。rq表示当前CPU的通用就绪队列，如果当前CPU是idle cpu，idle参数为CPU_IDLE，否则
为CPU_NO_IDLE。从当前CPU开始，从上到下遍历调度域，如果调度域没有设置SD_LOAD_BALANCE，表示该调度域不需要做负载均衡。最后的核心函数
    rebalance_domains()->load_banance()->should_we_balance()，首先查找当前调度组是否有空闲CPU(idle cpu)，如果有空闲CPU，那么变量banance_cpu
记录该CPU；如果没有空闲CPU，则返回调度组里第一个CPU。如果当前CPU是空闲CPU后者组里第一个CPU，那么当前CPU可以做负载均衡，即只有当前CPU是该调度
域中第一个CPU或者当前CPU是idle CPU才可以做负载均衡。举例说明，CPU0和CPU1同属一个调度域，CPU0和CPU1都不是idle cpu，那么只有CPU0运行load_banance()
才可以做负载均衡；假设CPU0不是idle cpu，CPU1处于idle状态，那么CPU1可以做负载均衡。
    rebalance_domains()->load_banance()->find_busiest_group()，简单归纳为如下步骤:
(1)首先遍历调度域中每个调度组，计算各个调度组中的平均负载等相关信息。
(2)根据平均负载，找出最繁忙的调度组
(3)获取本地调度组的平均负载(avg_load)和最繁忙调度组的平均负载，以及该调度域的平均负载
(4)本地调度组的平均负载大于最繁忙组的平均负载，或者本地调度组的平均负载大于调度域的平均负载，说明不适合做负载均衡，退出此次负载均衡处理
(5)根据最繁忙组的平均负载、调度域的平均负载和本地调度组的平均负载来计算该调度域的需要迁移的负载不均衡值
(6)找到并且从最繁忙组中的最繁忙的CPU中迁移进程到当前CPU，迁移过程中有些进程不能迁移：CPU位图限制、正在运行、cache-hot。
    load_balance主要流程总结如下：
(1)负载均衡从当前CPU开始，由下至上地遍历调度域，从最底层的调度域开始做负载均衡。
(2)允许做负载均衡的首要条件是当前CPU是该调度域中第一个CPU，或者当前CPU是idle CPU。
(3)在调度域中查找最繁忙的调度组，更新调度域和调度组的相关信息，最后计算出该调度域的不均衡负载值imbalance。
(4)在最繁忙的调度组中找出最繁忙的CPU，然后把繁忙CPU中的进程迁移到当前CPU上，迁移的负载量为不均衡负载值。

3.3.3 唤醒进程
    Linux内核中提供一个wake_up_process()函数API来唤醒进程，唤醒进程涉及应该由哪个CPU来运行唤醒进程，是当前CPU(wakepup CPU，因为它调用了
wake_up_process()函数)，还是该进程之前运行的CPU(prev_cpu):
    如果wakeup CPU和prev_cpu在同一个调度域且这个调度域包含SD_WAKE_AFFIN标志位，那么affine_sd调度域具有亲和性；
    当找到一个具有亲和性的调度域，且wake_up CPU和prev CPU不是同一个CPU，那么可以考虑使用wakeup CPU来唤醒进程，条件是如果wakeup CPU的负载
加上唤醒进程的负载比prev CPU的负载小，那么wakeup CPU是可以唤醒进程。
    优先选择idle CPU，如果没有找到idle CPU；如果prev CPU和wakeup CPU具有cache亲缘性，并且prev CPU也处于idle状态，那么选择prev CPU。
    对于没有设置SD_BALANCE_WAKE的情况，找到最悠闲的调度组和最悠闲的CPU来唤醒该进程。
    关于wake affine特性: CPU0-进程A在CPU0上唤醒CPU2-进程B，那么进程B在CPU0上唤醒并且运行，和进程A共享L1/L2 cache，性能会得到提升。但是android
的C/S软件架构，service和client是一对多的关系，一轮唤醒后，进程B、C、D将都在CPU0上运行，其他CPU都会空闲，这反而适得其反。
    针对这个问题，Linux 3.12提出了解决方案，加入了在task_struct中wakee_flips，如果A上次唤醒的不是B，那么wakee_flips++，以此表示很多进程依赖A的
唤醒。

3.3.4 调试
    SMP负载均衡提供了一个名为sched_migrate_task的tracepoint。
    抓取进程迁移相关信息，例如:
    trace-cmd record -e 'sched_wakeup*' -e sched_switch -e 'sched_migrate*'
    kernelshark trace.dat




