3.6 EAS绿色节能调度器
	Energy Aware Scheduling
    PELT在辨别进程负载的变化上显得有些迟钝，比如:
(1) PELT使用一个32毫秒的衰减时间，大约213毫秒才能把之前的负载忘记掉，也就是历史负载清零，原文做了一个实验，一个进程工作20ms，休眠20ms，180ms后，
    经过PELT统计的CPU利用率最高只有60%，这个对于手持移动设备来说是非常不好的，因为有些小活会突然变成大活，PELT在辨别负载变化上显得有些迟钝。
(2) 对于睡眠或阻塞的进程，PELT还会继续计算其衰减负载，但是这些“历史”负载对下一次唤醒其实没有什么用处，因此会拖延减低CPU频率的速度，从而增加CPU功耗。
    针对上述问题，可以使用新的计算进程负载的WALT(Window Assisted Load Tracking)，该算法已经被Android社区采纳，并且在Android 7.x中已经采用，
但是官方Linux内核还没有采纳。
	现在Linux内核中关于电源管理的几个重要模块都比较独立，CFS调度器，cpuidle模块(Menu算法)，cpufreq模块(Interactive算法)等。
	HMP调度器脱离主流Linux内核，自带CPU拓扑关系；EAS调度器则使用主流Linux内核中的CPU调度域拓扑关系。
	Kernel默认采用的PELT算法在计算负载时只考虑了CPU利用率，没有考虑不同CPU频率和不同CPU架构计算能力的差异所导致的负载计算的不准确性。
	Kernel默认的调度器也没有考虑不同CPU架构在功耗方面的差异。
	因此，设计一种考虑CPU频率和计算能力的负载算法(Frequency and capacity invariant load tracking)显得很有必要，来看一下EAS调度算法的架构图
          ————————————————————————————
         |       进程CFS调度器        |
         | (功耗和性能俱佳的调度算法) |
          ————————————————————————————
                /            \
               /  选择下一个  \
              /      状态      \
             /                  \
            /                    \
 ———————————                     ———————————
|CPUidle模块|                   |CPUfreq模块|
|   核心    |                   |    核心   |
| 平台驱动  |                   |  平台驱动 |
 ———————————                     ———————————
	不同的场景对调度器有不同的需求: 性能优先、功耗优先、实时性优先。

3.6.1能效模型
    EAS绿色节能调度器基于能效模型(Energy Model)。能效模型需要考虑CPU的计算能力(capacity)和功耗(energy)两方面的因素。
include/linux/sched.h//代码取自项目代码
struct capacity_state {
    unsigned long cap;  /* capacity - calculated by energy driver，表示计算机的计算能力*/
    unsigned long frequency;/* frequency */
    unsigned long power;    /* power consumption at this frequency，此计算能力下的CPU功耗*/
};

struct idle_state {
    unsigned long power;     /* power consumption in this idle state，CPU进入idle状态的功耗*/
};

struct sched_group_energy {
    unsigned int nr_idle_states;    /* number of idle states，idle状态(C-state)状态的个数，CPU Power states*/
    struct idle_state *idle_states; /* ptr to idle state array，指向一个idle 状态的数组*/
    unsigned int nr_cap_states; /* number of capacity states，P state状态的个数， CPU Performance states*/
    struct capacity_state *cap_states; /* ptr to capacity state array，DVFS OPP 频率-电压*/
};

truct sched_domain_topology_level {
...
    sched_domain_energy_f energy;//增加了一个函数指针energy
...
};

static struct sched_domain_topology_level arm64_topology[] = {
#ifdef CONFIG_SCHED_MC
    { cpu_coregroup_mask, cpu_corepower_flags, cpu_core_energy, SD_INIT_NAME(MC) },//cpu_core_energy()获取MC等级的能效模型
#endif
    { cpu_cpu_mask, cpu_cpu_flags, cpu_cluster_energy, SD_INIT_NAME(DIE) },//cpu_cluster_energy()获取DIE等级的能效模型
    { NULL, },
};

struct sched_group_energy *sge_array[NR_CPUS][NR_SD_LEVELS];//系统定义了一个全局的struct sched_group_energy二维数组用于表示每个SDTL层级下各个CPU的能效数据

start_kernel()->kernel_init_freeable()->smp_prepare_cpus()->init_cpu_topology()->init_sched_energy_costs()/*查找DTS文件，把对应的计算能力数据和功耗数据存放在cap_states，
                                                                                                          idle_states存放idle的功耗数据，具体数据总结在了doc里*/
    DTS中给出的功耗数据只是一个量化值，而不是实际的瓦数。目前在ARM架构中，一个cluster里的所有CPU只能同时调频电压，所以sdm845.dtsi也给出了cluster在各个频率下的功耗值。
	上述的CPU能效数据，需要在做芯片验证时由硬件工程师和软件工程师实验得出，一般由各个SoC芯片厂商提供。
	EAS调度器提出了两个概念，分别是FIE(Frequency Invairent Engine)和CIE(CPU Invariant Engine)。FIE是在计算CPU负载时考虑CPU频率的变化，CIE考虑不同CPU架构的计算能力对负载的影响，
例如在相同频率下，ARM公司的大小核架构CPU其计算能力是不同的。
struct rq {//为了体现FIE和CIE的概念，在struct rq中添加了如下两个成员
...
    unsigned long cpu_capacity;//cpu_capacity_orig计算能力扣除DL和RT调度类之后剩余的CFS调度类的计算能力，代码里常用capacity_of()获取当前CPU的CFS调度类计算能力
    unsigned long cpu_capacity_orig;//CPU在DTS中定义的最高的计算能力 * 该CPU和系统中最高频率的比值，845中已经写死了是1024，代码里常用capacity_orig_of()获取
...
}
struct sched_group_capacity {//也多了一个成员
...
	unsigned long capacity;//同cpu_capacity
...
}
	sched_init_smp()->init_sched_domains()->build_sched_domains()->init_sched_groups_capacity()->update_group_capacity()->update_cpu_capacity()
static void update_cpu_capacity(struct sched_domain *sd, int cpu)
{
    unsigned long capacity = arch_scale_cpu_capacity(sd, cpu);
    ...
    cpu_rq(cpu)->cpu_capacity_orig = capacity;
    ...
}
	arch_scale_cpu_capacity()->scale_cpu_capacity()
static DEFINE_PER_CPU(unsigned long, cpu_scale) = SCHED_CAPACITY_SCALE;//1024
unsigned long scale_cpu_capacity(struct sched_domain *sd, int cpu)
{   
#ifdef CONFIG_CPU_FREQ //defined
    unsigned long max_freq_scale = cpufreq_scale_max_freq_capacity(cpu);//#define SCHED_CAPACITY_SCALE    (1L << SCHED_CAPACITY_SHIFT) = 1024

    return per_cpu(cpu_scale, cpu) * max_freq_scale >> SCHED_CAPACITY_SHIFT;//获取当前CPU的最高频率和系统里所有CPU中最高频率的一个比值，CPU的计算能力和CPUfreq模块联系在一起，即FIE概念。
                                                                            //845: cpu_scale * 1024>>1024，就是cpu_scale=2014，写死了
#else
    return per_cpu(cpu_scale, cpu);
#endif
}

3.6.2 WALT算法
	WALT(Window Assisted Load Tracking)算法是以时间窗口(window based view of time)为单位的跟踪进程CPU利用率并计算下一个窗口期望的运行时间的一种新算法，
主要解决PELT算法中进程负载计算的问题，
	例如历史负载反应导致反应慢等。window指时间窗口，或者统计窗口，这是可配置的值，默认是20毫秒。
struct task_struct {
...
#ifdef CONFIG_SCHED_WALT//845已经配置
    struct ravg ravg;
    u32 init_load_pct;
    u64 last_wake_ts;
    u64 last_switch_out_ts;
    u64 last_cpu_selected_ts;
    struct related_thread_group *grp;
    struct list_head grp_list;
    u64 cpu_cycles;
    bool misfit;
#endif
...
}
struct ravg {
    u64 mark_start;//是标记进程开始的一个新的统计窗口，例如，在进程唤醒、开始运行或者被抢占时都会开始一个新的统计窗口
    u32 sum, demand;//sum-进程在当前统计窗口已经运行的WALT时间，它由运行和等待时间组成，并且和CPU频率有关；
                    //demand-根据过去几个统计窗口来判断当前进程最大的负载请求值，该值可以通过新的CPUFreq Governor接口来调整CPU频率
    u32 coloc_demand;
    u32 sum_history[RAVG_HIST_SIZE_MAX];//维护过去5个统计窗口的统计数据。睡眠时间不计算在窗口时间中，RAVG_HIST_SIZE_MAX = 5
    u32 *curr_window_cpu, *prev_window_cpu;//curr_window_cpu - 当前窗口中，任务对CPUs的cpu busy time的贡献度
    u32 curr_window, prev_window;//curr_window_cpu的总和
    u16 active_windows;
    u32 pred_demand;//任务的预测当前cpu bust time
    u8 busy_buckets[NUM_BUSY_BUCKETS];//历史busy time分成不同的buckets用来预测
};
	进程在创建时会初始化WALT相关数据结构，sched_fork()->init_new_task_load()，
void init_new_task_load(struct task_struct *p, bool idle_task)
{
...
    init_load_windows = div64_u64((u64)init_load_pct *(u64)sched_ravg_window, 100);
...
}
	新创建进程第一次加入就绪队列时会开启一个统计窗口，wake_up_new_task(p)->mark_task_starting(p)，开启一个新的窗口时用mark_start记录当前时间
	新进程通过调用activate_task()加入到就绪队列，在加入就绪队列之前需要计算和更新该进程的平均负载情况。enqueue_entity_load_avg()用于更新平均负载，
__update_load_avg()是更新负载的核心函数。
	wake_up_new_task()->activate_task()->enqueue_task()->enqueue_task_fair()->__update_load_avg()
static __always_inline int __update_load_avg(u64 now, int cpu, struct sched_avg *sa, unsigned long weight, int running, struct cfs_rq *cfs_rq)
//weight--进程权重值
//running -- 当前进程是否在运行状态
//考虑了FIE和CIE的概念
scale_freq = arch_scale_freq_capacity(NULL, cpu);//获取当前CPU频率
scale_cpu = arch_scale_cpu_capacity(NULL, cpu);//获取CPU的最大计算能力
	最后，WALT运行时间计算公式如下:
                    cur_freq     cur_IPC
walt_time = delta * ————————— * —————————
                    max_freq     max_IPC
    delta - 上一次开始统计mark_start到当前统计窗口window_start的时间差为delta
    cur_freq - 当前CPU频率，
    max_freq - 系统中最大频率，
    cur_IPC - 当前CPU的计算效率，
    max_IPC - 系统中计算能力最强的CPU的计算效率，即大核的计算效率
    cur_IPC / max_IPC - 指上述提到的efficiency
	WALT算法统计窗口内的CPU运行时间，称为WALT time，本章也称为WALT CPU运行时间。那么什么是100%的WALT CPU运行时间呢?
	假设小核最高频率1GHZ，大核最高频率2GHz，并且大核计算能力(IPC)是小核的两倍，那么在大核上以最高频率奔跑一个统计窗口的时间称为100%WALT CPU使用时间，即20ms，
可以理解为SoC上单个CPU的最大功率是20ms。在小核CPU上以1GHz运行10ms，它的WALT CPU使用时间是多少呢
	小核WALT CPU时间 = 10*(1GHz/2GHz)*(小核IPC/大核IPC) = 10*0.5*0.5=2.5ms
	也就是说，虽然它在小核CPU以1GHz频率运行了10ms，但是以WALT衡量尺度，它相当于只运行了2.5毫秒，这就是所谓的可测量的Scale-invariant Load Tracking。
	在一个统计窗口里，SoC中计算能力最大的CPU以最高频率运行一个统计窗口的时间成为满载WALT Time，类似额定功率概念。WALT算法把不同CPU核心和不同CPU频率映射到
同一个度量尺度上。
	WALT算法demand取值规则，5个统计窗口                         Task demand值
          [8][13][3][5][10]                   取最大值----->          13
          [8][13][3][5][10]                   取平均值----->          7.8
          [8][13][3][5][10]                 取最近的值----->          10
          [8][13][3][5][10]       取平均和最近的最大值----->          10
	WALT默认使用最近值和平均值两者的最大值WINDOW_STATS_MAX_RECENT_AVG。
#define WINDOW_STATS_RECENT     0
#define WINDOW_STATS_MAX        1
#define WINDOW_STATS_MAX_RECENT_AVG 2
#define WINDOW_STATS_AVG        3
#define WINDOW_STATS_INVALID_POLICY 4
__read_mostly unsigned int sched_window_stats_policy = WINDOW_STATS_MAX_RECENT_AVG;
__read_mostly unsigned int sysctl_sched_window_stats_policy = WINDOW_STATS_MAX_RECENT_AVG;

	T0和T1会被统计进来，T2存放在ravg.sum里
     one window|->           n个统计窗口          <-|      本次更新
	|          |          |          |              |         |    |
         | T0  |<---------       T1      ---------->|<-    T2    ->|
     mark_start                               window_start    

3.6.3 唤醒进程
	比如一个四核处理器，分为cluster0(CPU0、CPU1)和cluster1(CPU2、CPU3)，其中CPU1和CPU3是空闲状态，他们有足够的计算能力容纳唤醒进程P，那么选择哪个运行P?
	需要注意的是，EAS的设计目标是功耗优先，它会将cluster/cpu的busy/idle状态下的计算能力还有功耗值考虑进来，选择一个刚刚好的CPU来运行进程P，那么答案是CPU1。

3.6.4 CPU动态调频
	把调频的governor cpufreq_sched加入到调度器中进行调用，从schedulter_tick开始，到具体的调度器，到负载比对，决定是否进行调频/调压。
