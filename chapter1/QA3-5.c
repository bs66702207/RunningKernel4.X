3-5内存屏障：
(1)编译时，编译器优化导致内存乱序访问。
(2)运行时，多CPU间交互引起的内存乱序访问。
从ARMv7指令集开始，ARM提供3条内存屏障指令，严格程度由小到大分别为
DMB -- Data Memory Barrier
DSB -- Data synchronization Barrier
ISB -- Instruction synchronization Barrier

例1: 假设有两个CPU核A和B，同时访问Addr1和Addr2地址
Core A:
	STR R0, [Addr1]
	LDR R1, [Addr2]

Core B:
	STR R2, [Addr2]
	LDR R3, [Addr1]
这将出现四种可能，A得到旧值，B也得到旧值；A得到旧值，B得到新值；A得到新值，B得到旧值；A得到新值，B也得到新值

例2: 假设Core A写入新数据到Msg地址，Core B需要判断flag标志后才读入新数据
Core A:
	STR R0, [Msg]
	STR R1, [Flag]

Core B:
	Poll_loop:
		LDR R1, [Flag]
		CMP R1, #0
		BEQ Poll_loop
		LDR R0, [Msg]
Core B可能因为乱序执行的原因先读入Msg，然后读取Flag；需要在Core B的LDR R0, [Msg]之前插入DMB指令来保证先判断Flag，直到满足条件后再读取[Msg]

例3: 在一个设备驱动中，写入一个命令到外设寄存器中，然后等待状态的变化
STR R0, [Addr]
DSB		@插入DSB指令，强制让他写命令完成，然后执行读取Flag的判断循环
Poll_loop:
	LDR R1, [Flag]
	CMP R1, #0
	BEQ Poll_loop
