/***************************************************
 Cpoyright (C) 2018 Iti Shree

This tool intends to calculate luminance of a raw12
image and write the data so obtained into a text
file. The text file could be used to generate graph
and observe the variation in value of luminance in
the image.

***************************************************/
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

static char *cmd_name = NULL;

#define NUM_COLS 4096
#define NUM_ROWS 3072
#define NUM_POINTS (NUM_ROWS / 2) * NUM_ROWS

#define u_max 0.436
#define v_max 0.615
#define gamma 0.5

#define ERR_WRITE 1

static uint32_t map_size = 0x08000000;
static uint32_t map_base = 0x18000000;
static uint32_t map_addr = 0x00000000;

static char *dev_mem = "/dev/mem";

static double luminance[NUM_POINTS];
static double u_val[NUM_POINTS];
static double v_val[NUM_POINTS];

void gamma_correction(double *buf, uint64_t *ptr) {

  for (unsigned i = 0; i < NUM_COLS / 2; i++) {
    unsigned ce = i * 3;
    unsigned co = (i + NUM_COLS / 2) * 3;

    buf[ce + 0] = 255 * pow(buf[ce + 0] / 255, gamma);
    buf[ce + 1] = 255 * pow(buf[ce + 1] / 255, gamma);
    buf[ce + 2] = 255 * pow(buf[ce + 2] / 255, gamma);

    buf[co + 0] = 255 * pow(buf[co + 0] / 255, gamma);
    buf[co + 1] = 255 * pow(buf[co + 1] / 255, gamma);
    buf[co + 2] = 255 * pow(buf[co + 2] / 255, gamma);
  }
}

void copy_data(uint8_t *buf, double *norm_buf, uint64_t *ptr) {
  for (unsigned i = 0; i < NUM_COLS / 2; i++) {
    unsigned ce = i * 3;
    unsigned co = (i + NUM_COLS / 2) * 3;

    norm_buf[ce + 0] = buf[ce + 0];
    norm_buf[ce + 1] = buf[ce + 1];
    norm_buf[ce + 2] = buf[ce + 2];

    norm_buf[co + 0] = buf[co + 0];
    norm_buf[co + 1] = buf[co + 1];
    norm_buf[co + 2] = buf[co + 2];
  }
}

void load_data(uint8_t *buf, uint64_t *ptr) {

  for (unsigned i = 0; i < NUM_COLS / 2; i++) {
    unsigned ce = i * 3;
    unsigned co = (i + NUM_COLS / 2) * 3;
    uint64_t val = ptr[i];

    buf[ce + 0] = (val >> 56) & 0xFF;
    buf[ce + 1] = (val >> 48) & 0xFF;
    buf[ce + 2] = (val >> 40) & 0xFF;

    buf[co + 0] = (val >> 32) & 0xFF;
    buf[co + 1] = (val >> 24) & 0xFF;
    buf[co + 2] = (val >> 16) & 0xFF;
  }
}

int main(int argc, char **argv) {
  extern int optind;
  extern char *optarg;
  int c, err_flag = 0;

#define OPTIONS "hB:S:"
#define VERSION "1.0"

  cmd_name = argv[0];
  while ((c = getopt(argc, argv, OPTIONS)) != EOF) {
    switch (c) {
    case 'h':
      fprintf(stderr, "This is %s " VERSION "\n"
                      "options are:\n"
                      "-h        print this help message\n"
                      "-B <val>  memory mapping base\n"
                      "-S <val>  memory mapping size\n",
              cmd_name);
      exit(0);
      break;

    case 'B':
      map_base = strtoll(optarg, NULL, 0);
      break;

    case 'S':
      map_size = strtoll(optarg, NULL, 0);
      break;

    default:
      err_flag++;
      break;
    }
  }

  /* If no option is matched print this message */
  if (err_flag) {
    fprintf(stderr, "Usage: %s -[" OPTIONS "] [file]\n"
                    "%s -h for help.\n",
            cmd_name, cmd_name);
    exit(1);
  }

  /* Opening dev/mem with read/write permission */
  int fd = open(dev_mem, O_RDWR | O_SYNC);
  if (fd == -1) {
    fprintf(stderr, "error opening >%s<.\n%s\n", dev_mem, strerror(errno));
    exit(2);
  }

  if (map_addr == 0)
    map_addr = map_base;

  /* Mapping base */
  void *base = mmap((void *)map_addr, map_size, PROT_READ | PROT_WRITE,
                    MAP_SHARED, fd, map_base);
  if (base == (void *)-1) {
    fprintf(stderr, "error mapping 0x%08X+0x%08X @0x%08X.\n%s\n", map_base,
            map_size, map_addr, strerror(errno));
    exit(2);
  }

  fprintf(stderr, "mapped 0x%08lX+0x%08lX to 0x%08lX.\n",
          (long unsigned)map_base, (long unsigned)map_size,
          (long unsigned)base);
  if (argc > optind) {
    close(1);
    int fd = open(argv[optind], O_CREAT | O_WRONLY, S_IRUSR);

    if (fd == -1) {
      fprintf(stderr, "error opening >%s< for writing.\n%s\n", argv[optind],
              strerror(errno));
      exit(2);
    }
  }
  uint8_t buf[NUM_COLS * 3];
  double norm_buf[NUM_COLS * 3];
  uint64_t *ptr = base;
  unsigned count = 0, count2;

  for (unsigned j = 0; j < NUM_ROWS / 2; j++) {
    load_data(buf, ptr);
    copy_data(buf, norm_buf, ptr);
    gamma_correction(norm_buf, ptr);
    ptr += NUM_COLS / 2;
  }

  for (unsigned j = 0; j < NUM_ROWS / 2; j++) {
    for (unsigned i = 0; i < NUM_ROWS / 2; i++) {
      unsigned ce = i * 4;
      unsigned co = (i + NUM_COLS / 2) * 4;
      count2 = (count + NUM_ROWS / 2);
      luminance[count] =
          0.299 * norm_buf[ce] +
          (0.587 * norm_buf[ce + 1] + 0.587 * norm_buf[ce + 2]) / 2 +
          0.114 * norm_buf[ce + 3];
      luminance[count2] =
          0.299 * norm_buf[co] +
          (0.587 * norm_buf[co + 1] + 0.587 * norm_buf[co + 2]) / 2 +
          0.114 * norm_buf[co + 3];
      u_val[count] = u_max * (norm_buf[ce + 3] - luminance[i]);
      u_val[count2] = u_max * (norm_buf[co + 3] - luminance[i]);
      v_val[count] = v_max * (norm_buf[ce] - luminance[i]);
      v_val[count2] = v_max * (norm_buf[co] - luminance[i]);
      count++;
    }
  }

  for (unsigned j = 0; j < count + count2; j++) {
  }
  char *output = "test.txt";

  FILE *fp;
  fp = fopen(output, "w");
  if (fp == NULL) {
    printf("Error while opening the file.\n");
    return 0;
  }

  for (unsigned i = 0; i < NUM_ROWS / 2; i++) {
    fprintf(fp, "%lf \t %f \t %f \n", luminance[i], u_val[i], v_val[i]);
  }

  fclose(fp);
  exit(0);
}
