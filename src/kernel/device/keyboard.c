#include "keyboard.h"
#include "APIC.h"
#include "interrupt.h"
#include "lib.h"
#include "mm.h"

// 全局键盘输入缓冲区
static struct keyboard_inputbuffer *p_kb = NULL;
// 记录是否按下 shift ctrl alt
static int shift_l, shift_r, ctrl_l, ctrl_r, alt_l, alt_r;

// 键盘的中断处理函数
void keyboard_handler(unsigned long nr, unsigned long parameter,
                      struct pt_regs *regs) {
  // 读取键盘扫描码
  unsigned char x = io_in8(PORT_KB_DATA);
  color_printk(WHITE, BLACK, "(K,%02x)", x);

  // 缓冲区已满，循环覆盖
  if (p_kb->p_head == p_kb->buf + KB_BUF_SIZE)
    p_kb->p_head = p_kb->buf;

  *p_kb->p_head = x;
  p_kb->count++;
  p_kb->p_head++; // head 前进
}

/**
 * @brief 从全局键盘输入缓冲区中读取一个键盘扫描码
 */
unsigned char get_scancode() {
  unsigned char ret = 0;

  // 忙等，直到有未解析字符
  if (p_kb->count == 0) {
    while (!p_kb->count)
      nop();
  }

  if (p_kb->p_tail == p_kb->buf + KB_BUF_SIZE)
    p_kb->p_tail = p_kb->buf;

  ret = *p_kb->p_tail;
  p_kb->count--;
  p_kb->p_tail++; // tail 前进

  return ret;
}

/**
 * 解析键盘输入缓冲区中的键盘扫描码
 */
void analysis_keycode() {
  unsigned char x = 0;
  int i;
  int key = 0;
  int make = 0;

  x = get_scancode();

  if (x == 0xE1) { // 0xE1 开头的 pausebreak 键
    key = PAUSEBREAK;
    for (i = 1; i < 6; i++)
      if (get_scancode() != pausebreak_scode[i]) {
        key = 0;
        break;
      }
  } else if (x == 0xE0) { // 0xE0 开头的功能键
    x = get_scancode();

    switch (x) {
    case 0x2A: // press printscreen

      if (get_scancode() == 0xE0)
        if (get_scancode() == 0x37) {
          key = PRINTSCREEN;
          make = 1;
        }
      break;

    case 0xB7: // UNpress printscreen

      if (get_scancode() == 0xE0)
        if (get_scancode() == 0xAA) {
          key = PRINTSCREEN;
          make = 0;
        }
      break;

    case 0x1d: // press right ctrl

      ctrl_r = 1;
      key = OTHERKEY;
      break;

    case 0x9d: // UNpress right ctrl

      ctrl_r = 0;
      key = OTHERKEY;
      break;

    case 0x38: // press right alt

      alt_r = 1;
      key = OTHERKEY;
      break;

    case 0xb8: // UNpress right alt

      alt_r = 0;
      key = OTHERKEY;
      break;

    default:
      key = OTHERKEY;
      break;
    }
  }

  if (key == 0) { // 处理普通按键
    unsigned int *keyrow = NULL;
    int column = 0;
    // 判断当前键盘扫描码描述的是按下状态还是抬起状态
    make = (x & FLAG_BREAK ? 0 : 1);
    // 找到按键对应的字符
    keyrow = &keycode_map_normal[(x & 0x7F) * MAP_COLS];
    // 是否按下 shift
    if (shift_l || shift_r)
      column = 1;

    key = keyrow[column];

    switch (x & 0x7F) {
    case 0x2a: // SHIFT_L:
      shift_l = make;
      key = 0;
      break;

    case 0x36: // SHIFT_R:
      shift_r = make;
      key = 0;
      break;

    case 0x1d: // CTRL_L:
      ctrl_l = make;
      key = 0;
      break;

    case 0x38: // ALT_L:
      alt_l = make;
      key = 0;
      break;

    default:
      if (!make)
        key = 0;
      break;
    }

    if (key)
      color_printk(RED, BLACK, "(K:%c)\t", key);
  }
}

hw_int_controller keyboard_int_controller = {.enable = IOAPIC_enable,
                                             .disable = IOAPIC_disable,
                                             .install = IOAPIC_install,
                                             .uninstall = IOAPIC_uninstall,
                                             .ack = IOAPIC_edge_ack};
/**
 * 初始化键盘驱动程序
 */
void keyboard_init() {
  struct IO_APIC_RET_entry entry;
  unsigned long i, j;

  // 初始化键盘输入缓冲区
  p_kb = (struct keyboard_inputbuffer *)kmalloc(
      sizeof(struct keyboard_inputbuffer), 0);
  p_kb->p_head = p_kb->buf;
  p_kb->p_tail = p_kb->buf;
  p_kb->count = 0;
  memset(p_kb->buf, 0, KB_BUF_SIZE);
  // 配置 IO APIC 的 RTE1
  // 将键盘设备的中断请求投递至 Local APIC ID 为 0 的处理器核心（BSP处理器）
  entry.vector = 0x21;
  entry.deliver_mode = APIC_ICR_IOAPIC_Fixed;
  entry.dest_mode = ICR_IOAPIC_DELV_PHYSICAL;
  entry.deliver_status = APIC_ICR_IOAPIC_Idle;
  entry.polarity = APIC_IOAPIC_POLARITY_HIGH;
  entry.irr = APIC_IOAPIC_IRR_RESET;
  entry.trigger = APIC_ICR_IOAPIC_Edge;
  entry.mask = APIC_ICR_IOAPIC_Masked;
  entry.reserved = 0;
  entry.destination.physical.reserved1 = 0;
  entry.destination.physical.phy_dest = 0;
  entry.destination.physical.reserved2 = 0;

  wait_KB_write();
  io_out8(PORT_KB_CMD, KBCMD_WRITE_CMD);
  wait_KB_write();
  io_out8(PORT_KB_DATA, KB_INIT_MODE);
  // 重复 1m 次空操作，拖延时间，等待低俗的键盘控制器执行完控制命令
  for (i = 0; i < 1000; i++)
    for (j = 0; j < 1000; j++)
      nop();

  shift_l = 0;
  shift_r = 0;
  ctrl_l = 0;
  ctrl_r = 0;
  alt_l = 0;
  alt_r = 0;

  // 注册键盘的中断处理函数
  register_irq(0x21, &entry, &keyboard_handler, (unsigned long)p_kb,
               &keyboard_int_controller, "ps/2 keyboard");
}

void keyboard_exit() {
  unregister_irq(0x21);
  kfree(p_kb);
}