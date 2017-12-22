5.1 Linux中断管理机制
1. P609的图5.3相当重要，通过struct irq_desc *desc = irq_to_desc(irq);转换后，可以获得irq_data、irq_chip、irq_domain等各种信息
