3.1 进程的诞生
	线程被称为轻量级进程，它是操作系统调度的最小单元，通常一个进程可以拥有多个线程。
	线程和进程的区别在于进程拥有独立的资源空间，而线程则共享进程的资源空间。
	Linux内核并没有对线程有特别的调度算法或定义特别的数据结构来标识线程，线程和进程都使用相同的进程PCB数据结构。
	内核里使用clone方法来创建线程。
3.1.1 init进程
	Linux内核在启动时会有一个init_task进程，它是系统所有进程的鼻祖，称为0号进程或idle进程，当系统没有进程需要调度时，调度器就会去执行idle进程。
	内核栈大小通常和体系结构相关，ARM32架构中内核栈大小是8KB，ARM64架构中内核栈大小是16KB。
	内核有一个常用的常量current用于获取当前进程task_struct数据结构，它利用了内核栈的特性。首先通过SP寄存器获取当前内核栈的地址，对齐后可以获取struct thread_info数据结构指针，
最后通过thread_info->task成员获取task_struct数据结构。
3.1.2 fork
	fork、vfork、clone这三个系统调用都是通过一个函数来实现，即do_fork()函数。
	  1. fork是重量级调用，为子进程建立了一个基于父进程的完整副本，然后子进程基于此运行。
	  2. vfork比fork多了两个标志位CLONE_VFORK和CLONE_VM，CLONE_VFORK表示父进程会被挂起，直至子进程释放虚拟内存资源；CLONE_VM表示父子进程运行在相同的内存空间中。
	  3. clone用于创建线程，并且通过寄存器从用户空间传递下来，通常会指定新的栈地址newsp。
	do_fork()主要调用copy_process()来创建一个新的进程：
	  1. User Namespace用于管理User ID和Group ID的映射，起到隔离User ID的作用。
	  2. POSIX协议规定在一个进程内部多个线程共享一个PID，为了满足POSIX协议，Linux内核实现了一个线程组的概念(thread group)，
		sys_getpid()系统调用返回线程组ID(tgid，thread group id)，sys_gettid()返回线程的PID。
	  3. 对于Linux内核来说，进程的鼻祖是idle进程，也称为swapper进程；但对用户空间来说，进程的鼻祖是init进程，所有用户空间进程都由init进程创建和派生。只有init进程才会设置SIGNAL_UNKILLABLE标志位。
	  4. 使用CLONE_PARENT创建兄弟进程，那么该进程无法由init进程回收，父进程idle进程也无能为力，因此它会变成僵尸进程。
	  5. 在没有PID namespace之前，进程唯一的标识是PID，在引入PID namespace之后，标识一个进程需要PID namespace和PID双重认证。
	  6. read_mostly修饰的变量会放入.data.read_mostly段中，在内核加载时放入相应的cache中，以便提高效率，笔者认为这可能是内核代码的遗漏。
	  7. dup_task_struct()函数会分配一个task_struct实例，其相关内容会复制到子进程中，task_struct数据结构中有一个成员flags用于存放重要的标志位，这里首先取消超级用户权限并告诉系统这不是一个worker线程
		(PF_WQ_WORKER)，worker线程由工作队列机制创建，另外设置PF_FORKNOEXEC标志位，这个进程暂时还不能执行。
	  8. 接下来是做内存空间、文件系统、信号系统、IO系统等核心内容的复制操作，这是fork进程的核心部分。
		8.1 copy_process()->sched_fork()，涉及调度相关的复制，而且要注意到get_cpu()的使用，关闭内核抢占，获取当前CPU的ID；还有关键地方要使用内存屏障smp_wmb()来保证代码执行完成。
		8.2 copy_files()，打开的文件信息复制；copy_fs()，fs_struct结构复制；copy_signal()，信号系统复制；copy_io()，IO复制；
		8.3 do_fork()->copy_process()->copy_mm()，内存空间复制；oldmm指父进程内存空间指针，oldmm为空则说明父进程没有自己的运行空间，只是一个寄人篱下的线程或内核线程。如果要创建一个和父进程共享内存空间
			的新进程，那么直接将新进程的mm指针指向父进程的mm数据结构即可。dup_mm()函数分配一个mm数据结构，然后从父进程中复制相关内容。

关于8.3的内存的复制，应该细化出来几个要点，但是因为工作需要，我是跳过第二章看的，所以这一块暂时搁浅，等学习第二章后，再细化这一块。

