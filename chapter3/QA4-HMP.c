3.4 HMP调度器
    针对功耗问题，ARM公司提出了大小核的概念，big.LITTLE模型，该模型的主要目的是省电。
    big.LITTLE在计算机的术语中称为HMP(Heterogeneous Muti-Processing)，通俗的话来概括就是“用大核干重活，用小核干轻活”。

                           把小核调度域上负载最重的进程迁移到大核调度域的idle CPU上
     小核调度域         ——————————————————————————————————————————————————————————————>      大核调度域
Cortex-A53|Cortex-A53   <——————————————————————————————————————————————————————————————   Cortex-A72|Cortex-A72
                           把每个大核CPU上负载最轻的进程迁移到大核调度域中idle CPU上