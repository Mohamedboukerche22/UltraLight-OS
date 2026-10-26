/* drivers/keyboard.c - PS/2 keyboard (scancode set 1), IRQ1
 *
 * Handles shift, ctrl, alt, caps lock, the 0xE0 extended prefix (arrows, home,
 * end, delete, ...) and feeds a lock-free single-producer/single-consumer
 * ring buffer that keyboard_poll() drains.
 */
#include "keyboard.h"
#include "idt.h"
#include "pic.h"
#include "io.h"

#define KB_DATA 0x60
#define KB_CMD  0x64
#define KB_BUF  256

static const char map_normal[58] =
    "\0\033" "1234567890-=" "\b\t" "qwertyuiop[]" "\n\0" "asdfghjkl;'`" "\0\\"
    "zxcvbnm,./" "\0*\0 ";
static const char map_shift[58] =
    "\0\033" "!@#$%^&*()_+" "\b\t" "QWERTYUIOP{}" "\n\0" "ASDFGHJKL:\"~" "\0|"
    "ZXCVBNM<>?" "\0*\0 ";

static volatile uint16_t buf[KB_BUF];
static volatile uint32_t head, tail;
static bool shift, ctrl, alt, caps, e0;

static void push(int key) {
    uint32_t next = (head + 1) % KB_BUF;
    if (next == tail) return;               /* full: drop */
    buf[head] = (uint16_t)key;
    head = next;
}

static void kb_irq(struct regs *r) {
    (void)r;
    uint8_t sc = inb(KB_DATA);

    if (sc == 0xE0) { e0 = true; return; }
    if (sc == 0xFA || sc == 0xFE || sc == 0x00 || sc == 0xFF) { e0 = false; return; }  /* ACK/resend/overrun */

    bool release = sc & 0x80;
    uint8_t code = sc & 0x7F;
    bool ext = e0;
    e0 = false;

    if (ext) {
        switch (code) {
        case 0x1D: ctrl = !release; return;
        case 0x38: alt  = !release; return;
        }
        if (release) return;
        switch (code) {
        case 0x48: push(KEY_UP);    break;
        case 0x50: push(KEY_DOWN);  break;
        case 0x4B: push(KEY_LEFT);  break;
        case 0x4D: push(KEY_RIGHT); break;
        case 0x47: push(KEY_HOME);  break;
        case 0x4F: push(KEY_END);   break;
        case 0x53: push(KEY_DEL);   break;
        case 0x49: push(KEY_PGUP);  break;
        case 0x51: push(KEY_PGDN);  break;
        case 0x1C: push('\n');      break;   /* keypad enter */
        case 0x35: push('/');       break;   /* keypad slash */
        }
        return;
    }

    switch (code) {
    case 0x2A: case 0x36: shift = !release; return;
    case 0x1D:            ctrl  = !release; return;
    case 0x38:            alt   = !release; return;
    case 0x3A:            if (!release) caps = !caps; return;
    }
    if (release || code >= sizeof map_normal) return;

    char c = map_normal[code];
    if (c >= 'a' && c <= 'z') {
        if (ctrl) c = (char)(c - 'a' + 1);          /* Ctrl+A..Z -> 1..26 */
        else if (shift ^ caps) c = (char)(c - 32);
    } else if (shift) {
        c = map_shift[code];
    }
    if (c) push((unsigned char)c);
}

static bool wait_write(void) { for (int i = 0; i < 100000; i++) if (!(inb(KB_CMD) & 0x02)) return true; return false; }
static bool wait_read(void)  { for (int i = 0; i < 100000; i++) if (inb(KB_CMD) & 0x01)   return true; return false; }

void keyboard_init(void) {
    for (int i = 0; i < 32 && (inb(KB_CMD) & 0x01); i++) inb(KB_DATA);   /* flush stale bytes */

    if (wait_write()) outb(KB_CMD, 0xAE);                   /* enable first PS/2 port */
    if (wait_write()) {
        outb(KB_CMD, 0x20);                                 /* read config byte */
        if (wait_read()) {
            uint8_t cfg = inb(KB_DATA);
            cfg |= 0x01;                                    /* IRQ1 enabled */
            cfg |= 0x40;                                    /* translate to scancode set 1 */
            cfg &= (uint8_t)~0x10;                          /* port 1 clock enabled */
            if (wait_write()) outb(KB_CMD, 0x60);
            if (wait_write()) outb(KB_DATA, cfg);
        }
    }
    idt_set_handler(IRQ_BASE + 1, kb_irq);
    pic_unmask(1);
}

int keyboard_poll(void) {
    if (tail == head) return -1;
    int k = buf[tail];
    tail = (tail + 1) % KB_BUF;
    return k;
}
