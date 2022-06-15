/**
 * @brief 由 BIOS 中断 E820 获取的物理内存空间信息结构体
 * 物理内存空间信息保存在线性地址 0xffff800000007e00 处
 *
 * 物理内存空间的类型：
 * 1:RAM,
 * 2:ROM or Reserved,
 * 3:ACPI Reclaim Memory,
 * 4:ACPI NVS Memory,
 * Others:Undefine
 */
struct E820 {
  unsigned long address; // 起始地址
  unsigned long length;  // 内存空间长度
  unsigned int type;     // 内存类型
} __attribute__((packed));
