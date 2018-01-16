4. 并发与同步
    临界区(critical region)是指访问和操作共享数据的代码段，这些资源无法同时被多个执行线程访问，访问临界区的执行线程或代码路径称为并发源。
4.1 原子操作和内存屏障
4.1.1 原子操作
	比如简单的i++操作，如果使用spinlock的方式来保证i++操作的原子性，会增加开销，kernel提供了atomic_t类型的原子变量，它的实现依赖不同的体系结构
typedef struct {
    int counter;
} atomic_t;
	内核还提供atomic_inc(v)等操作原子变量的函数
4.1.2 内存屏障
	第一章介绍过
