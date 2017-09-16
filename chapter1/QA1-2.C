1.请简述精简指令集RISC和复杂指令集CISC的区别
	RISC - Reduced Instruction Set Computer
	CISC - Complex Instruction Set Computer
	一段代码编译之后，80%的内容会使用到处理器的简单指令，而20%的内容会使用复杂指令，基于这种思想，将指令集和处理器重新设计，在新的设计中只保留了常用的简单指令，这样处理器不需要浪费太多的晶体管去做那些很复杂又很少使用的复杂指令。通常，简单指令大部分时间都能在一个cycle内完成，基于这种思想的指令集叫作RISC指令集，以前的指令集叫作CISC。

2.请简述数值0x12345678在大小端字节序处理器的存储器中的存储方式
Big-endian，高字节在低地址
/*0000430*/: 1234 5678
/*0000440*/: 
Little-endian，高字节在高地址
/*0000430*/: 7845 3412
/*0000440*/: 

int checkCpuEndian(void)//true-little, false-big
{
	union w
	{
		int a;
		char b;
	} c;
	c.a = 1;
	return (c.b == 1);
}
