3.2 CFS调度器
	目前Linux内核中默认实现了4种调度策略，分别为deadline、realtime、CFS和idle，它们分别使用struct sched_class来定义调度类。
	这4种调度类通过next指针串联在一起，用户空间程序可以使用调度策略API函数(sched_setscheduler())来设定用户进程的调度策略。
include/uapi/linux/sched.h
#define SCHED_NORMAL	0 //CFS调度器
#define SCHED_FIFO		1 //realtime调度器
#define SCHED_RR		2 //realtime调度器
#define SCHED_BATCH		3 //CFS调度器
#define SCHED_IDLE		5 //idle调度器
#define SCHED_DEADLINE	6 //deadline调度器

3.2.1 权重计算
	调度实体数据结构名 struct sched_entity
(1)进程权重、优先级和vruntime
	0~139表示进程优先级，数值越低优先级越高。0~99给实时进程使用，100~139给普通进程使用。另外在用户空间有一个传统变量nice值映射到普通进程的优先级，即100~139。
struct task_struct {
	int prio;
	int static_prio; //静态优先级，进程启动时分配，不会随着时间而改变，用户可以通过nice或sched_setscheduler等系统调用来修改该值
	int normal_prio; /*基于static_prio和调度策略计算出来的优先级，在创建时会继承父进程的normal_prio，
					   对于普通进程normal_prio = static_prio；对于实时进程，会根据rt_priority重新计算normal_prio，详见effective_prio()函数*/
	unsigned int rt_priority; // 进程的实时优先级
}
	记录调度实体的权重信息(weight):
struct load_weight {//已经内嵌在了sched_entity，代码经常使用p->se.load来获取进程p的权重信息
	unsigned long weight;//调度实体的权重
	u32 inv_weight;	//inverse weight的缩写，它是权重的一个中间计算结果
};
	nice值的范围是从-20~19，进程默认的nice值为0，nice越高则优先级越低；进程每降低一个nice级别，优先级则提高一个级别，相应的进程多获得10%的CPU时间，反之亦然，这个10%是相对累加的，如下表
static const int prio_to_weight[40] = {
 /* -20 */     88761,     71755,     56483,     46273,     36291,
 /* -15 */     29154,     23254,     18705,     14949,     11916,
 /* -10 */      9548,      7620,      6100,      4904,      3906,
 /*  -5 */      3121,      2501,      1991,      1586,      1277,
 /*   0 */      1024,       820,       655,       526,       423,
 /*   5 */       335,       272,       215,       172,       137,
 /*  10 */       110,        87,        70,        56,        45,
 /*  15 */        36,        29,        23,        18,        15,
};
	根据上面的表格举例，进程A和进程B的nice值都是0，那么权重值都是1024，它们获得的CPU时间都是50%，计算公式为1024/(1024+1024)=50%。假设A的nice+1，即A的nice=1，B的nice保持不变，那么
A = 820/(1024+820) = 45%，B = 1024/(1024+820) = 55%。这里是约等于。
	另外一张表：
static const u32 prio_to_wmult[40] = {
 /* -20 */     48388,     59856,     76040,     92818,    118348,
 /* -15 */    147320,    184698,    229616,    287308,    360437,
 /* -10 */    449829,    563644,    704093,    875809,   1099582,
 /*  -5 */   1376151,   1717300,   2157191,   2708050,   3363326,
 /*   0 */   4194304,   5237765,   6557202,   8165337,  10153587,
 /*   5 */  12820798,  15790321,  19976592,  24970740,  31350126,
 /*  10 */  39045157,  49367440,  61356676,  76695844,  95443717,
 /*  15 */ 119304647, 148102320, 186737708, 238609294, 286331153,
};
	prio_to_wmult[]表的计算公式: inv_weight = 2的32次方 / weight，权重被倒转了，作用是为后面计算方便。内核提供一个函数set_load_weight，来查询这两个表，然后把值放在p->se.load中
	CFS调度器计算虚拟时间的核心函数calc_delta_fair(): vruntime = (delta_exec * nice_0_weight) / weight
	vruntime -- 虚拟进程运行时间，delta_exec -- 实际运行时间，nice_0_weight -- nice为0的权重值，weight -- 该进程的权重值。
	这么看的话，如果进程的权重越高，那么其虚拟时间就会越小(相对真实时间)，也就是说虚拟时钟比真实时钟跑得慢，但是可以获得比较多的运行时间。
	上面的计算公式涉及到了浮点运算，那么需要inv_weight的带入转换，vruntime = (delta_exec * nice_0_weight * inv_weight) >> shift，prio_to_wmult[]这张表预先做好了除法，所以效率得以提高。

(2)CPU负载
	Linux 3.8内核以后进程的负载计算不仅考虑权重，而且跟踪每个调度实体的负载情况，该方法称为PELT(Pre-entity Load Tracking)。sched_entity中有一个 struct sched_avg
struct sched_avg {
    u32 runnable_avg_sum, runnable_avg_period;// runnable_avg_sum -- 就绪队列里的负载均衡值(运行时间se->on_rq=1 + 等待时间se->on_rq=0)；runnable_avg_period -- 在系统的总时间，负载均衡值
    u64 last_runnable_update;
    s64 decay_count;
    unsigned long load_avg_contrib;//进程平均负载的贡献度，对于那些长时间不活动而突然短时间访问CPU的进程或者访问磁盘被阻塞等待的进程，其load_avg_contrib要比CPU密集型的进程小很多，例如矩阵乘法运算的密集型进程。对于前者runnable_avg_sum远小于runnable_avg_period，对于后者它们几乎相等。
};
	我们把1毫秒(准确来说是1024微秒，为了方便移位操作)的时间跨度算成一个周期，称为period，简称PI。一个调度实体(进程或者调度组)在一个PI周期内对系统负载贡献除了权重外，还有在PI周期内可运行的
时间(runnable_time)，包括运行时间或等待CPU时间。一个理想的计算方式: 假设Li是一个调度实体在第i个周期内的负载贡献，那么一个调度实体的负载总和计算公式如下:
	L = L0 + L1*y + L2*y^2 + L3*y^3 + ... + L32*y^32 + ...
	调度实体的负载需要考虑时间因素，也就是其在过去一段时间的表现，y是一个预先选定好的衰减系统，y^32约等于0.5。
	该公式有简化计算方式: 过去贡献总和乘以衰减系数y，并加上当前时间点的负载L0即可。
	为处理器计算方便，A/B = [A*(2^32/B)] >> 32，内核定义了一张(2^32/B)的表
static const u32 runnable_avg_yN_inv[] = {
    0xffffffff, 0xfa83b2da, 0xf5257d14, 0xefe4b99a, 0xeac0c6e6, 0xe5b906e6,
    0xe0ccdeeb, 0xdbfbb796, 0xd744fcc9, 0xd2a81d91, 0xce248c14, 0xc9b9bd85,
    0xc5672a10, 0xc12c4cc9, 0xbd08a39e, 0xb8fbaf46, 0xb504f333, 0xb123f581,
    0xad583ee9, 0xa9a15ab4, 0xa5fed6a9, 0xa2704302, 0x9ef5325f, 0x9b8d39b9,
    0x9837f050, 0x94f4efa8, 0x91c3d373, 0x8ea4398a, 0x8b95c1e3, 0x88980e80,
    0x85aac367, 0x82cd8698,
};
	假设当前进程的负载贡献度是100，要求计算过去第32毫秒的负载。Load = (100*runnable_avg_yN_inv[31]) >> 32，最后计算结果为51。
	换算后的衰减因子:
static const u32 runnable_avg_yN_org[] = {
	0.999, 0.978, 0.957, 0.937, 0.917, 0.897,
	0.878, 0.859, 0.840, 0.822, 0.805, 0.787,
	...
	...
	...
	0.522, 0.510,
};
	为了计算更加方便更加方便，内核又维护了一个表:
static const u32 runnable_avg_yN_sum[] = {
        0, 1002, 1982, 2941, 3880, 4798, 5697, 6576, 7437, 8279, 9103,
     9909,10698,11470,12226,12966,13690,14398,15091,15769,16433,17082,
    17718,18340,18949,19545,20128,20698,21256,21802,22336,22859,23371,
};
	验证一下，n=2时，1024*(runnable_avg_yN_org[1] + runnable_avg_yN_org[2]) = 1981.44，约等于runnable_avg_yN_sum[2]，如果n小于等于LOAD_AVG_PERIOD(32)，那么直接在runnable_avg_yN_sum[]取值；
如果n>LOAD_AVG_MAX_N(345)，那么直接取极限值LOAD_AVG_MAX(477742)；如果n的范围为32~345，那么每次递进32个衰减周期进行计算，然后把不能凑成32个周期的单独计算并累加。见函数__compute_runnable_contrib(n);
	running_avg_sum的计算，简单归纳如下：running_avg_sum = prev_avg_sum +  ∑   decay,
                                                                         period
	period -- 上一次统计到当前统计经历的周期个数；prev_avg_sum -- 上一次统计时runnable_avg_sum在period+1个周期的衰减值；decay -- period个周期的衰减值和。runnable_avg_period的计算类似。
	如果一个进程在就绪队列里等待很长时间才被调度，period很大，相当于之前的统计值被清0了。
	load_avg_contrib的计算公式如下：
					runnable_avg_sum * weight
load_avg_contrib = ————————————————————————————
					  runnable_avg_period
	runnable_avg_sum越接近runnable_avg_period，则平均负载越大，表示该调度实体一直在占用CPU。

3.2.2 进程创建
struct sched_entity {//调度实体
    struct load_weight  load;//权重
    struct rb_node  run_node;//在红黑树中的结点
    struct list_head    group_node;
    unsigned int    on_rq;//是否在就绪队列中接收调度
    u64 exec_start;//计算虚拟运行时间需要的信息
    u64 sum_exec_runtime;//计算虚拟运行时间需要的信息
    u64 vruntime;//虚拟运行时间
    u64 prev_sum_exec_runtime;//计算虚拟运行时间需要的信息
    u64 nr_migrations;
    ...
#ifdef CONFIG_SMP
    struct sched_avg    avg ____cacheline_aligned_in_smp;//负载信息
#endif
};
    do_fork()->sched_fork()->__sched_fork()函数会把新创建进程的调度实体se相关成员初始化为0，因为这些值不能用父进程，
子进程将来要参与调度，和父进程分道扬镳。
    do_fork()->sched_fork()->task_fork_fair()函数的参数p表示新创建的进程。task_struct里内嵌了sched_entity，因此可以通过task_struct中得到
该进程的调度实体。smp_processor_id()从当前进程thread_info结构中的cpu成员获取当前CPU id。系统中每个CPU有一个就绪队列(runqueue)，它是Per-CPU
类型，即每个CPU有一个struct rq数据结构。this_rq()可以获取当前CPU的就绪队列数据结构struct rq。
struct rq {
    unsigned int nr_running;
    struct load_weight load;//就绪队列的权重信息load
    struct cfs_rq cfs;//CFS调度器就绪队列数据结构
    struct rt_rq rt;//realtime调度器就绪队列数据结构
    struct dl_rq dl;//deadline调度器就绪队列数据结构
    struct task_struct *curr, *idle, *stop;
    u64 clock;
    u64 clock_task;
    int cpu;
    int online;
...
}

struct cfs_rq {
    struct load_weight load;
    unsigned int nr_running, h_nr_running;
    u64 exec_clock;
    u64 min_vruntime;
    struct sched_entity *curr, *next, *last, *skip;
    unsigned long runnable_load_avg, blocked_load_avg;
...
}
    CFS总是在红黑树中选择vruntime最小的进程进行调度，优先级高(nice低)的进程总会被优先选择，随着vruntime增长，优先级低的进程也会有机会运行。
3.2.3 进程调度
    __schedule()是调度器的核心函数，其作用是让调度器选择和切换到一个合适进程运行。调度的时机见原文吧。
3.2.4 scheduler tick
3.2.5 组调度
struct task_group {
    struct cgroup_subsys_state css;
#ifdef CONFIG_FAIR_GROUP_SCHED //还需要打开 CONFIG_CGROUP_SCHED
    struct sched_entity **se;
    struct cfs_rq **cfs_rq;
    unsigned long shares;

#ifdef  CONFIG_SMP
    atomic_long_t load_avg ____cacheline_aligned;
#endif
#endif

#ifdef CONFIG_RT_GROUP_SCHED
    struct sched_rt_entity **rt_se;
    struct rt_rq **rt_rq;
    struct rt_bandwidth rt_bandwidth;
#endif

    struct rcu_head rcu;
    struct list_head list;
    struct task_group *parent;
    struct list_head siblings;
    struct list_head children;
#ifdef CONFIG_SCHED_AUTOGROUP
    struct autogroup *autogroup;
#endif
    struct cfs_bandwidth cfs_bandwidth;
};
3.2.6 PELT算法改进
    存在问题: 一次更新只有一个调度实体的负载变化，而没有更新cfs_rq所有调度实体的负载变化。Linux 4.3已解决，并且考虑CPU频率因素。
3.2.7 小结
    1. 每个CPU有一个通用就绪队列struct rq。rq=this_rq()
    2. 每个task_struct中内嵌一个调度实体struct sched_entity se结构体。se=&p->se
    3. 每个通用就绪队列数据结构中内嵌CFS就绪队列、RT就绪队列和Deadline就绪队列结构体。cfs_rq=&rq->cfs
    4. 每个调度实体se内嵌一个权重struct load_weight load结构体。
    5. 每个调度实体se内嵌一个平均负载struct sched_av avg结构体。
    6. 每个调度实体se有一个vruntime成员表示该调度实体的虚拟时钟。
    7. 每个调度实体se有一个on_rq成员表示该调度实体是否在就绪队列中接受调度。
    ...

