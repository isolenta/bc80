#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <limits.h>
#include <unistd.h>

#include "dynarray.h"
#include "commands.h"

#include "dbgproto.h"

static command_t *commands = NULL;
static char *g_ttyfile = NULL;
int g_bus_state = 0;

static void hex_dump(void* data, ssize_t size, uint16_t baseaddr) {
  int i, c;
  unsigned char *p = (unsigned char *)data;

  while (size > 0)
  {
    printf("%04X: ", baseaddr);

    for (i = 0; i < 16; i++) {
      if (i < size)
        printf("%02X ", p[i] & 0xFF);
      else
        printf("   ");

      if (i == 7)
        printf(" ");
    }

    printf(" |");

    for (i = 0; i < 16; i++) {
      if (i < size) {
        c = p[i] & 0xFF;
        if ((c < 0x20) || (c >= 0x7F))
          c = '.';
      } else {
        c = ' ';
      }

      printf("%c", c);
    }

    printf("|\n");

    size -= 16;
    p += 16;
    baseaddr += 16;
  }
}

static size_t fs_file_size(char *path) {
  struct stat st;
  int ret = stat(path, &st);

  if (ret != 0)
    return 0;

  return st.st_size;
}

// TODO: timeout
static ssize_t read_all(int fd, void *buf, size_t n) {
  ssize_t total_read = 0;
  char *p = (char *)buf;
  while (n) {
    ssize_t nread = read(fd, p, n);
    if (nread > 0) {
      p += nread;
      n -= nread;
      total_read += nread;
    } else if (nread < 0) {
      return nread;
    } else {
      continue;
    }
  }
  return total_read;
}

static int cmd_help(dynarray *args) {
  dynarray_cell *dc;

  printf("bc80 interactive debugging tool\n"
         "===============================\n\n"
         "usage: debug [usb.serial.file]  It will try to autodetect usb.serial.file name if omitted\n"
         ""
         "wr <addr> <value>...  Write byte value(s) to the memory.\n"
         "                      It's possible to pass several bytes in the single command,\n"
         "                      in this case they will written on consequent addresses.\n"
         "rd <addr> [n]         Read value(s) from memory. <n> means number of consequent\n"
         "                      bytes to read (default: 1)\n"
         "<PIN> [1|0]           Raw pin access. If argument omitted returns current pin state.\n"
         "                      If argument specified changes pin state to value specified.\n"
         "pins                  Show suggested pins mapping\n"
         "memtest [start [end]] Test memory (write pseudo random data pattern and read back)\n"
         "acquire               Acquire CPU bus\n"
         "release               Release CPU bus\n"
         "load <file> [<addr>]  Load <file> contents to RAM. Default destination address is 0.\n"
         "help                  This message\n"
         "quit                  Quit\n"
         "\nPossible PIN values are: pa0, pa1, pa2, pa3, pa4, pa5, pa6, pa7, pa8, pa9, pa10,\n"
         "pb0, pb1, pb3, pb4, pb5, pb6, pb7, pb8, pb9, pb10, pb11, pb12, pb13, pb14, pb15,\n"
         "pc13, pc14, pc15\n\n"
         "All argument values must be passed as hexademical integer\n(without prefix/suffix, i.e. 20 means 0x20\n"
         "Command names are case-insensitive\n\n");
  return 0;
}

static bool parse_integer(char *str, int *ival) {
  char *endptr;
  char *last = str + strlen(str);
  int tmp = strtol(str, &endptr, 16);

  if (endptr != last)
    return false;

  *ival = tmp;
  return true;
}

static int cmd_quit(dynarray *args) {
  printf("quit\n");
  return 1;
}

static int cmd_wr(dynarray *args) {
  uint8_t *req, *resp, *preq;
  int ival;
  uint16_t addr;
  bool parse_ok;

  if ((dynarray_length(args) < 3) || (dynarray_length(args) > (MAX_MULTIBYTE_BYTES + 2))) {
    printf("invalid number of arguments %d\n", dynarray_length(args));
    return 0;
  }

  int fd = open(g_ttyfile, O_RDWR);
  if (fd < 0) {
    perror(g_ttyfile);
    return 1;
  }

  if (g_bus_state != BUS_ACQUIRED) {
    printf("use 'acquire' before accessing the bus from debugger\n");
    return 0;
  }

  req = malloc(dynarray_length(args) + 1);
  resp = malloc(dynarray_length(args) + 1);
  preq = req;

  // we can send as single byte as multibyte write command
  if (dynarray_length(args) == 3)
    *preq++ = CMD_MEM_WR;
  else
    *preq++ = CMD_MEM_WRN;

  parse_ok = parse_integer((char *)dfirst(dynarray_nth_cell(args, 1)), &ival);
  if (!parse_ok) {
    printf("invalid argument '%s': hex integer required\n", (char *)dfirst(dynarray_nth_cell(args, 1)));
    goto cmd_wr_exit;
  }

  // address is packed high byte first
  addr = (uint16_t)ival;
  *preq++ = (addr >> 8) & 0xff;
  *preq++ = addr & 0xff;

  // for multibyte command pack number of bytes (it's limited to 128 so one byte is enough)
  if (dynarray_length(args) > 3)
    *preq++ = dynarray_length(args) - 2;

  for (int i = 0; i < (dynarray_length(args) - 2); i++) {
    parse_ok = parse_integer((char *)dfirst(dynarray_nth_cell(args, 2 + i)), &ival);
    if (!parse_ok) {
      printf("invalid argument '%s': hex integer required\n", (char *)dfirst(dynarray_nth_cell(args, 2 + i)));
      goto cmd_wr_exit;
    }

    *preq++ = ival & 0xff;
  }

  // send it
  size_t req_size = (size_t)(preq - req);
  ssize_t ret = write(fd, req, req_size);
  if (ret != req_size) {
    perror("CMD_MEM_WR(N) write");
    goto cmd_wr_exit;
  }

  ret = read_all(fd, resp, req_size);
  if (ret != req_size) {
    perror("CMD_MEM_WR(N) read");
    goto cmd_wr_exit;
  }

  if (memcmp(req, resp, req_size) != 0) {
    printf("invalid response from device while CMD_MEM_WR(N), req_size=%ld\n", req_size);
    goto cmd_wr_exit;
  }

  if (req[0] == CMD_MEM_WR)
    printf("data is written to address %X\n", addr);
  else
    printf("%zd bytes are written to address %X\n", req_size - 4, addr);

cmd_wr_exit:
  free(req);
  free(resp);
  close(fd);
  return 0;
}

static int cmd_rd(dynarray *args) {
  int ival;
  bool parse_ok;
  uint16_t addr;
  uint8_t req[] = {CMD_MEM_RD, 0, 0, 1};
  uint8_t *resp;

  if ((dynarray_length(args) != 2) && (dynarray_length(args) != 3)) {
    printf("invalid number of arguments: 2 or 3 required\n");
    return 0;
  }

  int fd = open(g_ttyfile, O_RDWR);
  if (fd < 0) {
    perror(g_ttyfile);
    return 1;
  }

  if (g_bus_state != BUS_ACQUIRED) {
    printf("use 'acquire' before accessing the bus from debugger\n");
    return 0;
  }

  parse_ok = parse_integer((char *)dfirst(dynarray_nth_cell(args, 1)), &ival);
  if (!parse_ok) {
    printf("invalid argument '%s': hex integer required\n", (char *)dfirst(dynarray_nth_cell(args, 1)));
    goto cmd_rd_exit;
  }

  // address is packed high byte first
  addr = (uint16_t)ival;
  req[1] = (addr >> 8) & 0xff;
  req[2] = addr & 0xff;

  // multibyte read
  if (dynarray_length(args) == 3) {
    req[0] = CMD_MEM_RDN;

    parse_ok = parse_integer((char *)dfirst(dynarray_nth_cell(args, 2)), &ival);
    if (!parse_ok) {
      printf("invalid argument '%s': hex integer required\n", (char *)dfirst(dynarray_nth_cell(args, 1)));
      goto cmd_rd_exit;
    }

    if (ival > MAX_MULTIBYTE_BYTES) {
      printf("can't read %d bytes at once\n", ival);
      goto cmd_rd_exit;
    }

    req[3] = (uint8_t)ival;
  }

  resp = malloc(4 + req[3]);

  // send request
  size_t req_size = (req[0] == CMD_MEM_RD) ? 3 : 4;
  ssize_t ret = write(fd, req, req_size);
  if (ret != req_size) {
    perror("CMD_MEM_RD(N) write");
    goto cmd_rd_exit;
  }

  size_t resp_size = 4 + req[3];
  ret = read_all(fd, resp, req_size + req[3]);
  if (ret != req_size + req[3]) {
    perror("CMD_MEM_RD(N) read");
    goto cmd_rd_exit;
  }

  // compare only equal parts (same as request one)
  if (memcmp(req, resp, req_size) != 0) {
    printf("invalid response from device while CMD_MEM_RD(N)");
    goto cmd_rd_exit;
  }

  if (req[3] == 1)
    printf("%04X: %02X\n", addr, resp[3]);
  else
    hex_dump(resp + 4, resp[3], addr);

cmd_rd_exit:
  free(resp);
  close(fd);
  return 0;
}


static int cmd_pin(dynarray *args) {
  ssize_t ret;
  char *cmd = (char *)dinitial(args);
  int pincode;
  uint8_t req;

  int fd = open(g_ttyfile, O_RDWR);
  if (fd < 0) {
    perror(g_ttyfile);
    return 1;
  }

  if (strcasecmp(cmd, "pa0") == 0)
    pincode = PIN_PA0;
  else if (strcasecmp(cmd, "pa1") == 0)
    pincode = PIN_PA1;
  else if (strcasecmp(cmd, "pa2") == 0)
    pincode = PIN_PA2;
  else if (strcasecmp(cmd, "pa3") == 0)
    pincode = PIN_PA3;
  else if (strcasecmp(cmd, "pa4") == 0)
    pincode = PIN_PA4;
  else if (strcasecmp(cmd, "pa5") == 0)
    pincode = PIN_PA5;
  else if (strcasecmp(cmd, "pa6") == 0)
    pincode = PIN_PA6;
  else if (strcasecmp(cmd, "pa7") == 0)
    pincode = PIN_PA7;
  else if (strcasecmp(cmd, "pa8") == 0)
    pincode = PIN_PA8;
  else if (strcasecmp(cmd, "pa9") == 0)
    pincode = PIN_PA9;
  else if (strcasecmp(cmd, "pa10") == 0)
    pincode = PIN_PA10;
  else if (strcasecmp(cmd, "pa15") == 0)
    pincode = PIN_PA15;
  else if (strcasecmp(cmd, "pb0") == 0)
    pincode = PIN_PB0;
  else if (strcasecmp(cmd, "pb1") == 0)
    pincode = PIN_PB1;
  else if (strcasecmp(cmd, "pb3") == 0)
    pincode = PIN_PB3;
  else if (strcasecmp(cmd, "pb4") == 0)
    pincode = PIN_PB4;
  else if (strcasecmp(cmd, "pb5") == 0)
    pincode = PIN_PB5;
  else if (strcasecmp(cmd, "pb6") == 0)
    pincode = PIN_PB6;
  else if (strcasecmp(cmd, "pb7") == 0)
    pincode = PIN_PB7;
  else if (strcasecmp(cmd, "pb8") == 0)
    pincode = PIN_PB8;
  else if (strcasecmp(cmd, "pb9") == 0)
    pincode = PIN_PB9;
  else if (strcasecmp(cmd, "pb10") == 0)
    pincode = PIN_PB10;
  else if (strcasecmp(cmd, "pb11") == 0)
    pincode = PIN_PB11;
  else if (strcasecmp(cmd, "pb12") == 0)
    pincode = PIN_PB12;
  else if (strcasecmp(cmd, "pb13") == 0)
    pincode = PIN_PB13;
  else if (strcasecmp(cmd, "pb14") == 0)
    pincode = PIN_PB14;
  else if (strcasecmp(cmd, "pb15") == 0)
    pincode = PIN_PB15;
  else if (strcasecmp(cmd, "pc13") == 0)
    pincode = PIN_PC13;
  else if (strcasecmp(cmd, "pc14") == 0)
    pincode = PIN_PC14;
  else if (strcasecmp(cmd, "pc15") == 0)
    pincode = PIN_PC15;
  else {
    printf("wrong pin name\n");
    goto cmd_pin_exit;
  }

  if (dynarray_length(args) == 1) {
    // read pin state
    uint8_t resp[2];

    req = VAR_RAW | SUBCMD_GET | pincode;

    ret = write(fd, &req, 1);
    if (ret != 1) {
      perror("SUBCMD_GET write");
      goto cmd_pin_exit;
    }

    ret = read_all(fd, resp, 2);
    if (ret != 2) {
      perror("SUBCMD_GET read");
      printf("%zd\n", ret);
      goto cmd_pin_exit;
    }

    if (resp[0] != req)
      printf("invalid response from device 0x%x (request was 0x%x)\n", resp[0], req);
    else
      printf("%s: %d\n", cmd, resp[1]);
  } else {
    // write pin state
    uint8_t resp, subcmd;
    bool quiet = false;

    if ((dynarray_length(args) == 3) && ((strcmp(dfirst(dynarray_nth_cell(args, 2)), "q") == 0)))
      quiet = true;

    if (strcmp(dsecond(args), "1") == 0)
      subcmd = SUBCMD_SET_HI;
    else if (strcmp(dsecond(args), "0") == 0)
      subcmd = SUBCMD_SET_LO;
    else {
      printf("invalid argument %s (only 0, 1 is valid)\n", (char *)dsecond(args));
      goto cmd_pin_exit;
    }

    req = VAR_RAW | subcmd | pincode;

    ret = write(fd, &req, 1);
    if (ret != 1) {
      perror("SET_PIN write");
      goto cmd_pin_exit;
    }

    ret = read(fd, &resp, 1);
    if (ret != 1) {
      perror("SET_PIN read");
      goto cmd_pin_exit;
    }

    if (resp != req) {
      printf("invalid response from device 0x%x (request was 0x%x)\n", resp, req);
      goto cmd_pin_exit;
    }

    if (!quiet)
      printf("%s := %s\n", (char *)dinitial(args), (char *)dsecond(args));
  }

cmd_pin_exit:
  close(fd);
  return 0;
}

static int cmd_pins(dynarray *args) {
  printf("Bluepill pins to bc80 bus mapping\n"
         "=================================\n"
         "pa0   A0                pb5   D0\n"
         "pa1   A1                pb6   D1\n"
         "pa2   A2                pb7   D2\n"
         "pa3   A3                pb8   D3\n"
         "pa4   A4                pb9   D4\n"
         "pa5   A5                pb10  D5\n"
         "pa6   A6                pb11  D6\n"
         "pa7   A7                pb12  D7\n"
         "pa8   A8\n"
         "pa9   A9                pb13  MREQ\n"
         "pa10  A10               pb14  BUSRQ\n"
         "pa15  A11               pb15  RD\n"
         "pb0   A12               pc13  WR\n"
         "pb1   A13               pc14  RESET\n"
         "pb3   A14               pc15  INT\n"
         "pb4   A15\n\n"
    );
  return 0;
}

static void mem_write(int fd, uint16_t addr, uint8_t val) {
  uint8_t req[] = {CMD_MEM_WR, 0, 0, 0};
  uint8_t resp[4];

  req[1] = (addr >> 8) & 0xff;
  req[2] = addr & 0xff;
  req[3] = val;

  ssize_t ret = write(fd, req, 4);
  if (ret != 4) {
    perror("CMD_MEM_WR write");
    return;
  }

  ret = read_all(fd, resp, 4);
  if (ret != 4) {
    perror("CMD_MEM_WR read");
    return;
  }

  if (memcmp(req, resp, 4) != 0)
    printf("invalid response from device while CMD_MEM_WR");
}

static uint8_t mem_read(int fd, uint16_t addr) {
  uint8_t req[] = {CMD_MEM_RD, 0, 0};
  uint8_t resp[4];
  uint16_t port;

  req[1] = (addr >> 8) & 0xff;
  req[2] = addr & 0xff;

  ssize_t ret = write(fd, req, 3);
  if (ret != 3) {
    perror("CMD_MEM_RD write");
    return 0xff;
  }

  ret = read_all(fd, resp, 4);
  if (ret != 4) {
    perror("CMD_MEM_RD read");
    return 0xff;
  }

  if (memcmp(req, resp, 3) != 0)
    printf("invalid response from device while CMD_MEM_RD");

  return resp[3];
}

static int cmd_acquire(dynarray *args) {
  dynarray *fake_args = NULL;

  uint8_t req[] = {CMD_ACQUIRE};
  uint8_t resp[1];
  uint16_t port;

  int fd = open(g_ttyfile, O_RDWR);
  if (fd < 0) {
    perror(g_ttyfile);
    return 1;
  }

  ssize_t ret = write(fd, req, 1);
  if (ret != 1) {
    perror("CMD_ACQUIRE write");
    close(fd);
    return 0xff;
  }

  ret = read_all(fd, resp, 1);
  if (ret != 1) {
    perror("CMD_ACQUIRE read");
    close(fd);
    return 0xff;
  }

  if (memcmp(req, resp, 1) != 0) {
    printf("invalid response from device while CMD_ACQUIRE");
    close(fd);
    return 0xff;
  }

  g_bus_state = BUS_ACQUIRED;
  close(fd);

  return 0;
}

int cmd_release(dynarray *args) {
  dynarray *fake_args = NULL;

  uint8_t req[] = {CMD_RELEASE};
  uint8_t resp[1];
  uint16_t port;

  int fd = open(g_ttyfile, O_RDWR);
  if (fd < 0) {
    perror(g_ttyfile);
    close(fd);
    return 1;
  }

  ssize_t ret = write(fd, req, 1);
  if (ret != 1) {
    perror("CMD_RELEASE write");
    close(fd);
    return 0xff;
  }

  ret = read_all(fd, resp, 1);
  if (ret != 1) {
    perror("CMD_RELEASE read");
    close(fd);
    return 0xff;
  }

  if (memcmp(req, resp, 1) != 0) {
    printf("invalid response from device while CMD_RELEASE");
    close(fd);
    return 0xff;
  }

  g_bus_state = BUS_RELEASED;
  close(fd);

  return 0;
}

static int cmd_memtest(dynarray *args) {
  uint16_t start_addr = 0, end_addr = 0xffff;
  int ival;
  bool parse_ok;

  int fd = open(g_ttyfile, O_RDWR);
  if (fd < 0) {
    perror(g_ttyfile);
    return 1;
  }

  if (dynarray_length(args) > 1) {
    parse_ok = parse_integer((char *)dfirst(dynarray_nth_cell(args, 1)), &ival);
    if (!parse_ok) {
      printf("invalid argument '%s': hex integer required\n", (char *)dfirst(dynarray_nth_cell(args, 1)));
      goto cmd_memtest_exit;
    }
    start_addr = (uint16_t)(ival & 0xffff);
  }

  if (dynarray_length(args) > 2) {
    parse_ok = parse_integer((char *)dfirst(dynarray_nth_cell(args, 2)), &ival);
    if (!parse_ok) {
      printf("invalid argument '%s': hex integer required\n", (char *)dfirst(dynarray_nth_cell(args, 2)));
      goto cmd_memtest_exit;
    }
    end_addr = (uint16_t)(ival & 0xffff);
  }

  int num_errors = 0;
  uint8_t value = 0;

  uint32_t seed = 0;
  uint32_t a = 1664525;
  uint32_t c = 1013904223;

  cmd_acquire(NULL);

  // hide cursor
  printf("\e[?25l");
  printf("write test pattern to memory\n");
  for (int addr = start_addr; addr <= end_addr; addr++) {
    // generate pseudo random test value
    seed = a * seed + c;
    mem_write(fd, addr, (uint8_t)(seed % 256));

    int progress = (addr - start_addr) * 100 / (end_addr - start_addr);
    printf("\r%d%% completed", progress);
  }
  printf("\n");

  printf("read test pattern from memory\n");
  seed = 0;
  for (int addr = start_addr; addr <= end_addr; addr++) {
    seed = a * seed + c;
    uint8_t test_value = (uint8_t)(seed % 256);
    uint8_t mem_value = mem_read(fd, addr);

    int progress = (addr - start_addr) * 100 / (end_addr - start_addr);

    if (test_value != mem_value) {
      num_errors++;
      printf("error at %04X: written %02X, read %02X\n", addr, test_value, mem_value);
    } else {
      printf("\r%d%% completed", progress);
    }
  }
  printf("\n");

  // show cursor
  printf("\e[?25h");
  printf("memory test finished: %d bytes, %d errors\n", end_addr - start_addr + 1, num_errors);

  cmd_release(NULL);

cmd_memtest_exit:
  close(fd);
  return 0;
}

static int cmd_detach(dynarray *args) {
  dynarray *fake_args = NULL;

  // set all pins to Hi-Z
  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pa0"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pa1"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pa2"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pa3"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pa4"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pa5"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pa6"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pa7"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pa8"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pa9"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pa10"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pa15"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb0"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb1"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb3"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb4"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb5"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb6"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb7"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb8"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb9"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb10"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb11"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb12"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb13"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb14"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pb15"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pc13"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pc14"));
  cmd_pin(fake_args);

  fake_args = NULL;
  fake_args = dynarray_append_ptr(fake_args, strdup("pc15"));
  cmd_pin(fake_args);

  return 0;
}

static int cmd_load(dynarray *args) {
  uint16_t load_addr = 0;
  #define LOAD_BATCH_SIZE 32

  if (dynarray_length(args) < 2) {
    printf("expected at least 1 argument\n");
    return 0;
  }

  char *filename = (char *)dfirst(dynarray_nth_cell(args, 1));
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    perror(filename);
    return 0;
  }

  size_t size = fs_file_size(filename);
  char *buf = malloc(size);
  size_t ret = fread(buf, size, 1, fp);
  if (ret != 1) {
    perror("fread");
    free(buf);
    fclose(fp);
    return 0;
  }

  fclose(fp);

  int fd = open(g_ttyfile, O_RDWR);
  if (fd < 0) {
    perror(g_ttyfile);
    return 1;
  }

  if (dynarray_length(args) > 2) {
    int ival;
    bool parse_ok = parse_integer((char *)dfirst(dynarray_nth_cell(args, 2)), &ival);
    if (!parse_ok) {
      printf("invalid argument '%s': hex integer required\n", (char *)dfirst(dynarray_nth_cell(args, 2)));
      goto cmd_load_exit;
    }
    load_addr = (uint16_t)(ival & 0xffff);
  }

  cmd_acquire(NULL);

  char *pbuf = buf;
  while (size > 0) {
    mem_write(fd, load_addr, *pbuf);
    size--;
    pbuf++;
    load_addr++;
  }

  cmd_release(NULL);

cmd_load_exit:
  free(buf);
  close(fd);
  return 0;
}

dynarray *get_tokens(char *str) {
  dynarray *darray = NULL;
  char *tmpstr = strdup(str);
  char *token;

  while ((token = strsep(&tmpstr, " \t")) != NULL)
    if (token[0])
      darray = dynarray_append_ptr(darray, strdup(token));

  free(tmpstr);
  return darray;
}

void init_commands() {
  char pinname[5];
  int idx = 0;
  commands = (command_t *)malloc(NUM_COMMANDS * sizeof(command_t));

  commands[idx].name = "help";
  commands[idx].callback = cmd_help;
  ++idx;

  commands[idx].name = "quit";
  commands[idx].callback = cmd_quit;
  ++idx;

  commands[idx].name = "pins";
  commands[idx].callback = cmd_pins;
  ++idx;

  commands[idx].name = "wr";
  commands[idx].callback = cmd_wr;
  ++idx;

  commands[idx].name = "rd";
  commands[idx].callback = cmd_rd;
  ++idx;

  commands[idx].name = "memtest";
  commands[idx].callback = cmd_memtest;
  ++idx;

  for (int i = 0; i <= 10; i++) {
    snprintf(pinname, sizeof(pinname), "pa%d", i);
    commands[idx].name = strdup(pinname);
    commands[idx].callback = cmd_pin;
    ++idx;
  }

  commands[idx].name = "pa15";
  commands[idx].callback = cmd_pin;
  ++idx;

  for (int i = 0; i <= 15; i++) {
    if (i == 2)
      continue;

    snprintf(pinname, sizeof(pinname), "pb%d", i);
    commands[idx].name = strdup(pinname);
    commands[idx].callback = cmd_pin;
    ++idx;
  }

  commands[idx].name = "pc13";
  commands[idx].callback = cmd_pin;
  ++idx;

  commands[idx].name = "pc14";
  commands[idx].callback = cmd_pin;
  ++idx;

  commands[idx].name = "pc15";
  commands[idx].callback = cmd_pin;
  ++idx;

  commands[idx].name = "acquire";
  commands[idx].callback = cmd_acquire;
  ++idx;

  commands[idx].name = "release";
  commands[idx].callback = cmd_release;
  ++idx;

  commands[idx].name = "load";
  commands[idx].callback = cmd_load;
  ++idx;

  commands[idx].name = "detach";
  commands[idx].callback = cmd_detach;
  ++idx;

  assert(idx == NUM_COMMANDS);
}

command_t *get_command(int idx) {
  return commands + idx;
}

ssize_t read_nb(int fd, void *buf, size_t count, int timeout_sec) {
  fd_set read_fds;
  struct timeval timeout;
  ssize_t bytes_read = 0;

  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  timeout.tv_sec = timeout_sec;
  timeout.tv_usec = 0;

  FD_ZERO(&read_fds);
  FD_SET(fd, &read_fds);

  int result = select(fd + 1, &read_fds, NULL, NULL, &timeout);
  if (result <= 0) {
    // read error or timeout
    return -1;
  }

  bytes_read = read(fd, buf, count);
  return bytes_read;
}

bool check_board(char *ttyfile) {
  uint8_t req;
  uint8_t resp[3];
  ssize_t ret;

  int fd = open(ttyfile, O_RDWR);
  if (fd < 0)
    return false;

  req = CMD_VERSION;

  ret = write(fd, &req, 1);
  if (ret < 0) {
    close(fd);
    return false;
  }

#define READDEV_TIMEOUT_SEC 1

  ret = read_nb(fd, resp, sizeof(resp), READDEV_TIMEOUT_SEC);
  if ((ret != sizeof(resp)) || resp[0] != req) {
    close(fd);
    return false;
  }

  if ((resp[1] != BOARD_VER1) || (resp[2] != BOARD_VER2)) {
    close(fd);
    return false;
  }

  close(fd);
  g_ttyfile = ttyfile;
  return true;
}

#if __APPLE__
char *try_autodetect_board_macos() {
  struct dirent *entry;
  DIR *dir;
  bool found;
  char *ttyfile = NULL;

  // try to autodetect well known usb-tty device files
  // TODO: we can autodetect for Linux as well (with another file prefix)

  found = false;
  dir = opendir("/dev");
  if (dir == NULL) {
    perror("/dev");
    return false;
  }

  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type != DT_CHR)
      continue;

    if (strstr(entry->d_name, "cu.usbserial") == entry->d_name) {
      char tmp[128];
      snprintf(tmp, sizeof(tmp), "/dev/%s", entry->d_name);

      if (check_board(tmp)) {
        ttyfile = strdup(tmp);
        found = true;
        break;
      }
    }
  }

  closedir(dir);

  if (!found)
    return NULL;

  g_ttyfile = ttyfile;
  return ttyfile;
}
#endif
