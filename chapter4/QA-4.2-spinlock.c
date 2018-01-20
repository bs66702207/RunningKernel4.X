4.2 spinlock
    操作系统中的锁机制分为两类，忙等待和休眠等待，spinlock属于前者，当无法获取spinlock锁时会不断尝试，直到获取锁为止。
1.  模型简化，spinlock的数据结构next和owner，假设某个饭店只有一张饭桌，开始营业时，next和owner都是0。
客户A来了，next和owner都是0，说明此时锁没有人持有，此时饭店没有客户，A的等号牌是0，直接进餐，next++；
客户B来了，next=1，owner=0，说明锁被人持有，B需要等待，等号牌是1，next++；
客户C来了，等号牌是2，next++；
A吃完买单了，owner++，owner变成了1，B的等号牌是1，那么B可以进餐了，以此类推。
	最终的实现依赖arch的代码，而且arch代码也是按照上面这个模型做的，其等待指令是WFE指令。
	ARM体系结构中WFI(Wait for interrupt)和WFE(Wait for event)都是让ARM进入standby睡眠模式，WFI是直到有WFI唤醒事件发生才会唤醒CPU，WFE是直到有WFE唤醒事件发生，这两类事件大部分相同，
唯一不同在于WFE可以被其他CPU上的SEV指令唤醒，SEV指令用于修改Event寄存器的指令。
2.  spin_lock：调用preempt_disabe关闭内核抢占
    spin_lock_irq：增加调用local_irq_disable关闭本地CPU中断
    spin_lock_irqsave：保存本地CPU irq状态
3.  spin_lock和raw_spin_lock，spin_lock直接调用raw_spin_lock，这如果有RT-patch，它允许spinlock锁的临界区内被抢占，其临界区内允许进程睡眠等待，这样会导致spinlock语义被修改。
	如果没有打上RT-patch，spin_lock就是直接调用raw_spin_lock。
	https://rt.wiki.kernel.org/
