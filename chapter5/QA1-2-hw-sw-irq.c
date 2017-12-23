5.1 Linux中断管理机制
1. P609的图5.3相当重要，通过struct irq_desc *desc = irq_to_desc(irq);转换后，可以获得irq_data、irq_chip、irq_domain等各种信息

5.2 软中断和tasklet
5.2.1 硬件中断管理基本属于上半部的范畴，中断线程化属于下半部机制，例如软中断(SoftIRQ)、tasklet和工作队列(workqueue)等
enum
{
    HI_SOFTIRQ=0,//0, 最高优先级的软中断类型
    TIMER_SOFTIRQ,//1, Timer定时器的软中断
    NET_TX_SOFTIRQ,//2. 发送网络数据包的软中断
    NET_RX_SOFTIRQ,//3, 接收网络数据包的软中断
    BLOCK_SOFTIRQ,//4和5，用于块设备的软中断
    BLOCK_IOPOLL_SOFTIRQ,
    TASKLET_SOFTIRQ,//6, 专门为tasklet机制准备的软中断
    SCHED_SOFTIRQ,//7, 进程调度以及负载均衡
    HRTIMER_SOFTIRQ,//8, 高精度定时器
    RCU_SOFTIRQ,//9, 专门为RCU服务的软中断
    NR_SOFTIRQS
};

//类似硬件中断描述符数据结构irq_desc[]，L1(cache line)对齐
static struct softirq_action softirq_vec[NR_SOFTIRQS] __cacheline_aligned_in_smp;

struct softirq_action
{
    void    (*action)(struct softirq_action *);//对应软中断的回调函数
};

//软中断的状态信息
typedef struct {
    unsigned int __softirq_pending;
#ifdef CONFIG_SMP
    unsigned int ipi_irqs[NR_IPI];
#endif
} ____cacheline_aligned irq_cpustat_t;

//触发软中断的API接口
inline void raise_softirq_irqoff(unsigned int nr)//关闭本地中断，允许进程上下文中调用
void raise_softirq(unsigned int nr)
5.2.2 tasklet
	tasklet是利用软中断实现的一种下半部机制，本质上是软中断的一个变种，运行在软中断上下文中。
struct tasklet_struct
{
    struct tasklet_struct *next;//多个task串成一个链表
    unsigned long state;//TASKLET_STATE_SCHED-tasklet已经被调度，正准备运行；TASKLET_STATE_RUN-tasklet正在运行中
    atomic_t count;//为0表示tasklet处于激活状态；不为0表示该tasklet被禁止
    void (*func)(unsigned long);//tasklet处理程序，类似软中断中的action函数指针
    unsigned long data;//传递参数给tasklet处理函数
};

//每个CPU维护两个tasklet链表，一个用于普通优先级的tasklet_vec，另一个用于高优先级的tasklet_hi_vec
struct tasklet_head {
    struct tasklet_struct *head;
    struct tasklet_struct **tail;
};      
static DEFINE_PER_CPU(struct tasklet_head, tasklet_vec);//TASKLET_SOFTIRQ，tasklet_action
static DEFINE_PER_CPU(struct tasklet_head, tasklet_hi_vec);//HI_SOFTIRQ，tasklet_hi_action，网络驱动中用的比较多，和普通tasklet实现机制相同，本文以普通为例

tasklet_schedule()//作用: 在驱动程序中，调度tasklet

	软中断上下文优先级高于进程上下文，这样会影响高优先级任务的实时性，RT linux里有专家希望用workqueue替代tasklet。
