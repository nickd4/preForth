#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "z80.h"

// I/O ports for communication with unix host
#define STDIN_PORT 0
#define STDOUT_PORT 1
#define STDERR_PORT 2
#define SYSTEM_PORT 3

// readable status bits
#define SYSTEM_STDIN_EOF 1
#define SYSTEM_STDIN_READY 2
#define SYSTEM_STDOUT_READY 4
#define SYSTEM_STDERR_READY 8

// writeable command bits
#define SYSTEM_EXITCODE 0x3f
#define SYSTEM_EXIT 0x40
#define SYSTEM_YIELD 0x80

uint8_t mem[0x10000];

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define N_STDIO_FILENO 3

#define STDIO_BUFFER_SIZE 0x100
struct stdio_buffer {
  uint8_t data[STDIO_BUFFER_SIZE];
  int head;
  int count;
} stdin_buffer, stdout_buffer, stderr_buffer;
bool stdin_eof;

byte memRead(int param, ushort address) {
  return mem[address];
}

void memWrite(int param, ushort address, byte data) {
  mem[address] = data;
}

// single-character reads from buffer
bool stdio_buffer_read(struct stdio_buffer *self, uint8_t *c) {
  if (self->count == 0)
    return false;
  *c = self->data[self->head];
  --self->count;
  self->head =
    self->count ? (self->head + 1) & (STDIO_BUFFER_SIZE - 1) : 0;
  return true;
}

// bulk reads from buffer, done in two calls
// first call gets pointer to contiguous data and how much available
// second call says how much available data was taken
uint8_t *stdio_buffer_read0(struct stdio_buffer *self, int *count) {
  *count = self->count;
  return self->data + self->head;
}

void stdio_buffer_read1(struct stdio_buffer *self, int count) {
  self->count -= count;
  self->head =
    self->count ? (self->head + count) & (STDIO_BUFFER_SIZE - 1) : 0;
}

// single-character writes to buffer
bool stdio_buffer_write(struct stdio_buffer *self, uint8_t c) {
  if (self->count >= STDIO_BUFFER_SIZE)
    return false;
  self->data[(self->head + self->count++) & (STDIO_BUFFER_SIZE - 1)] = c;
  return true;
}

// bulk writes to buffer, done in two calls
// first call gets pointer to contiguous space and how much available
// second call says how much available space was filled
uint8_t *stdio_buffer_write0(struct stdio_buffer *self, int *count) {
  *count = STDIO_BUFFER_SIZE - (
    self->head > self->count ? self->head : self->count
  );
  return self->data + ((self->head + self->count) & (STDIO_BUFFER_SIZE - 1));
}

void stdio_buffer_write1(struct stdio_buffer *self, int count) {
  self->count += count;
}

void stdio_service(int in_thres, int out_thres) {
  if (
    stdin_buffer.count < in_thres ||
    stdout_buffer.count >= out_thres ||
    stderr_buffer.count >= out_thres
  ) {
    fd_set readfds, writefds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    FD_ZERO(&writefds);
    FD_SET(STDOUT_FILENO, &writefds);
    FD_SET(STDERR_FILENO, &writefds);
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    if (select(N_STDIO_FILENO, &readfds, &writefds, NULL, &timeout) == -1) {
      perror("select()");
      exit(EXIT_FAILURE);
    }

    if (
      stdin_buffer.count < in_thres &&
        FD_ISSET(STDIN_FILENO, &readfds)
    ) {
      int count;
      uint8_t *data = stdio_buffer_write0(&stdin_buffer, &count);
      ssize_t result = read(STDIN_FILENO, data, count);
      if (result == -1) {
        perror("read()");
        exit(EXIT_FAILURE);
      }
      if (result == 0)
        stdin_eof = true;
      stdio_buffer_write1(&stdin_buffer, (int)result);
    }

    if (
      stdout_buffer.count >= out_thres &&
        FD_ISSET(STDOUT_FILENO, &writefds)
    ) {
      int count;
      uint8_t *data = stdio_buffer_read0(&stdout_buffer, &count);
      ssize_t result = write(STDOUT_FILENO, data, count);
      if (result == -1) {
        perror("write()");
        exit(EXIT_FAILURE);
      }
      stdio_buffer_read1(&stdout_buffer, (int)result);
    }

    if (
      stderr_buffer.count >= out_thres &&
        FD_ISSET(STDERR_FILENO, &writefds)
    ) {
      int count;
      uint8_t *data = stdio_buffer_read0(&stdout_buffer, &count);
      ssize_t result = write(STDERR_FILENO, data, count);
      if (result == -1) {
        perror("write()");
        exit(EXIT_FAILURE);
      }
      stdio_buffer_read1(&stderr_buffer, (int)result);
    }
  }
}

byte ioRead(int param, ushort address) {
  switch (address & 0xff) {
  case STDIN_PORT:
    {
      stdio_service(1, STDIO_BUFFER_SIZE);
      uint8_t c = 0;
      stdio_buffer_read(&stdin_buffer, &c);
      return c;
    }
  case SYSTEM_PORT:
    {
      stdio_service(1, STDIO_BUFFER_SIZE);
      uint8_t status = 0;
      if (stdin_eof)
        status |= SYSTEM_STDIN_EOF;
      if (stdin_buffer.count)
        status |= SYSTEM_STDIN_READY;
      if (stdout_buffer.count < STDIO_BUFFER_SIZE)
        status |= SYSTEM_STDOUT_READY;
      if (stderr_buffer.count < STDIO_BUFFER_SIZE)
        status |= SYSTEM_STDERR_READY;
      return status;
    } 
  }
  return 0;
}

void ioWrite(int param, ushort address, byte data) {
  switch (address & 0xff) {
  case STDOUT_PORT:
    stdio_service(1, STDIO_BUFFER_SIZE);
    stdio_buffer_write(&stdout_buffer, data);
 stdio_service(1, 1);
    break;
  case STDERR_PORT:
    stdio_service(1, STDIO_BUFFER_SIZE);
    stdio_buffer_write(&stderr_buffer, data);
    break;
  case SYSTEM_PORT:
    stdio_service(1, 1); // flush everything first
    if (data & SYSTEM_EXIT) {
      while (stdout_buffer.count || stderr_buffer.count) {
        usleep(1000);
        stdio_service(1, 1);
      }
      exit(data & SYSTEM_EXITCODE);
    }
    if (data & SYSTEM_YIELD)
      usleep(1000);
    break;
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("usage: %s filename.com\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int fd = open(argv[1], O_RDONLY);
  if (fd == -1) {
    perror(argv[1]);
    exit(EXIT_FAILURE);
  }

  ssize_t result = read(fd, mem, 0x10000);
  if (result == -1) {
    perror("read()");
    exit(EXIT_FAILURE);
  }
#if 0
  ushort len = result;
  printf("len=%04x\n", len);
#endif

  close(fd);

  Z80Context ctx;
  memset(&ctx, 0, sizeof(Z80Context));
  ctx.memRead = memRead;
  ctx.memWrite = memWrite;
  ctx.ioRead = ioRead;
  ctx.ioWrite = ioWrite;

  Z80RESET(&ctx);
#if 0
  while (true) {
    printf(
      "pc=%04x af=%04x bc=%04x de=%04x hl=%04x ix=%04x iy=%04x sp=%04x\n",
      ctx.PC,
      ctx.R1.wr.AF,
      ctx.R1.wr.BC,
      ctx.R1.wr.DE,
      ctx.R1.wr.HL,
      ctx.R1.wr.IX,
      ctx.R1.wr.IY,
      ctx.R1.wr.SP
    );
    fflush(stdout);
    if (ctx.PC >= len) {
      fprintf(stderr, "pc %04x out of bounds\n", ctx.PC);
      exit(EXIT_FAILURE);
    }
    Z80Execute(&ctx);
  }
#else
  while (true)
    Z80ExecuteTStates(&ctx, 1000);
#endif
}
