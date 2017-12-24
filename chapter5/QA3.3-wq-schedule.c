5.3.3 调度一个work
    kernel推荐驱动开发者使用默认的workqueue，而不是新创建workqueue。要使用默认的workqueue，首先要初始化一个work，内核提供了相应的宏INIT_WORK()
struct work_struct的data字段低8位存放一些标志位，
    WORK_STRUCT_PENDING_BIT = 0,//work正在pending执行
    WORK_STRUCT_DELAYED_BIT = 1,//work被延迟执行了
    WORK_STRUCT_PWQ_BIT = 2,//data指向pwqs数据结构的指针
    WORK_STRUCT_LINKED_BIT  = 3,//下一个work连接到该work上
    初始化完一个work后，就可以调用schedule_work()函数来把work挂入系统的默认BOUND类型的工作队列system_wq中，并没有开始实质调度work执行，只是把
work加入workqueue的PENDING链表中而已。
    接下来看worker如何处理work的，简化后的代码逻辑如下：
worker_thread()
{
recheck:
    if(不需要更多的工作线程？) {
        goto 睡眠;
     }
    if(需要创建更多的工作线程？&& 创建线程)
        goto recheck;
    do {
        处理工作;
    } (还有工作待完成 && 活跃的工作线程 <= 1)

睡眠:
    schedule();
}
5.3.4 取消一个work
    cancel_work_sync()，取消一个work，但会等待该work执行完毕。小心使用，cancel_work_sync()申请了锁A，如果work的回调函数也需要申请锁A，会死锁。
5.3.5 和调度器的交互
    worker进入执行时、被唤醒时，nr_running++
    worker推出执行时、睡眠时，nr_running--
