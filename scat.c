#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termio.h>
#include <string.h>
#include <errno.h>

#define PROGNAME "scat"

#define PARITY_NONE 0
#define PARITY_ODD  1
#define PARITY_EVEN 2

#define FLOW_NONE 0
#define FLOW_HARD 1
#define FLOW_SOFT 2

void usage(FILE *fp)
{
	fprintf(fp, "Usage: %s [options] [line]\n", PROGNAME);
	fprintf(fp, "  -l <line>\n");
	fprintf(fp, "  -s <speed>\n");
	fprintf(fp, "  -[45678]: datasize\n");
	fprintf(fp, "  -[NEO]: parity\n");
	fprintf(fp, "  -[12]: stopbit\n");
	fprintf(fp, "  -[HX]: flow control\n");
	fprintf(fp, "  -x: hex dump mode\n");
	fprintf(fp, "  -r: raw mode (displays characters AS IS)\n");
	exit(0);
}

int open_serial(const char *fname, mode_t mode, int com_speed, int com_datasize, int com_parity, int com_stopbit, int com_flow)
{
	struct termios newtio;
	int fd;

	fd = open(fname, mode);
	if (fd < 0) {
		char fname_dev[1024];
		snprintf(fname_dev, sizeof(fname_dev), "/dev/%s", fname);
		fd = open(fname, mode);
	}
	if (fd < 0) {
		return -1;
	}

	fprintf(stderr, "* Setting COM speed to %d\n", com_speed);

	memset(&newtio, 0, sizeof(newtio)); // clear struct for new port settings

	newtio.c_ospeed = com_speed;
	newtio.c_ispeed = com_speed;
	newtio.c_cflag = CREAD;
	newtio.c_iflag = 0;//IGNPAR;
	newtio.c_oflag = 0;

	newtio.c_lflag = 0;
	newtio.c_cc[VTIME] = 0; // inter-character timer unused
	newtio.c_cc[VMIN] = 1;  // blocking read until 1 character arrives

	switch (com_datasize) {
	case 5: newtio.c_cflag |= CS5; break;
	case 6: newtio.c_cflag |= CS6; break;
	case 7: newtio.c_cflag |= CS7; break;
	case 8: newtio.c_cflag |= CS8; break;
	default:
		fprintf(stderr, "Unsupported character size.\n");
		goto err;
	}

	switch (com_parity) {
	case PARITY_NONE: break; // none
	case PARITY_ODD : newtio.c_cflag |= PARENB | PARODD;	break; // odd
	case PARITY_EVEN: newtio.c_cflag |= PARENB;			break; // even
	default:
		fprintf(stderr, "Unsupported parity.\n");
		goto err;
	}

	switch (com_stopbit) {
	default:
		fprintf(stderr, "Unsupported stopbits.\n");
		goto err;
	case 1:
		break;
	case 2:
		newtio.c_cflag |= CSTOPB;
		break;
	}

	switch (com_flow) {
	case FLOW_HARD: // hard
		newtio.c_cflag |= CRTSCTS;
		break;

	case FLOW_SOFT: // soft
		newtio.c_iflag |= IXON | IXOFF;
		break;

	case FLOW_NONE:
	default:
		break;
	}

	tcflush(fd, TCIFLUSH);

	tcsetattr(fd, TCSANOW, &newtio);

	return fd;

err:
	return -1;
}

void hexdump(void *buf_, size_t len, unsigned long start_addr)
{
	const int fold_size = 16;
	const int flag_print_chars = 1;
	const unsigned char *buf = (const unsigned char *)buf_;

	unsigned long i, j;
	unsigned long offset;
	unsigned long count;

	offset = start_addr % fold_size;
	count = (offset + len + fold_size - 1) / fold_size;

	for (i = 0; i < count; i++) {
		printf("%08lx: ", start_addr);
		start_addr = (start_addr / fold_size) * fold_size;
		for (j = 0; j < fold_size; j++) {
			unsigned long idx = i * fold_size + j - offset;
			if (j % 8 == 0) printf(" ");
			if (idx < len) {
				printf("%02x ", buf[idx]);
			} else {
				printf("   ");
			}
		}
		if (flag_print_chars) {
			printf(" |");
			for (j = 0; j < fold_size; j++) {
				unsigned long idx = i * fold_size + j - offset;
				//if (j % 8 == 0) printf(" ");
				if (idx < len) {
					printf("%c", buf[idx] >= 0x20 && buf[idx] < 0x7fUL ? buf[idx] : '.');
				} else {
					printf(" ");
				}
			}
			printf("|");
		}
		printf("\n");
		start_addr += fold_size;
	}
}

void escape_unprintable(const char *buf, size_t len, int raw)
{
	while (len-- > 0) {
		int c = *(unsigned char *)buf++;
		if (raw || c == '\r' || c == '\n' || c == '\t' || (c >= 0x20 && c < 0x7f)) {
			putchar(c);
		} else {
			printf("<%02x>", c);
		}
	}
	fflush(stdout);
}

ssize_t write_with_crlf_conv(int fd, const char *src, size_t len)
{
	const char *last_p, *p;
	int wrote = 0;
	int r;

	for (last_p = src; len > 0 && (p = memchr(last_p, '\n', len)) != NULL; last_p = p + 1) {
		if (p - last_p > 0) {
			r = write(fd, last_p, p - last_p);
			if (r < 0) return r;
			wrote += r;

			len -= ((p + 1) - last_p);
		}

		r = write(fd, "\r\n", 2);
		if (r < 0) return r;
		wrote += r;
	}
	if (len > 0) {
		r = write(fd, last_p, len);
		if (r < 0) return r;
		wrote += r;
	}

	return wrote;
}

int main(int argc, char *argv[])
{
	int ch;
	const char *fname = NULL;
	int speed = 115200, cs = 8, parity = 0, stop = 1, flow = 0;
	int dump = 0;
	int raw = 0;

	while ((ch = getopt(argc, argv, "l:s:rNEOneoHXS56789xh")) != EOF) {
		switch (ch) {
		case 'l':
			fname = optarg;
			break;
		case 's':
			speed = strtoul(optarg, NULL, 0);
			break;
		case 'r':
			raw = 1;
			break;
		case 'N': parity = PARITY_NONE; break;
		case 'E': parity = PARITY_EVEN; break;
		case 'O': parity = PARITY_ODD; break;

		case 'H': flow = FLOW_HARD; break;
		case 'X': case 'S': flow = FLOW_SOFT; break;

		case '1': stop = 1; break;
		case '2': stop = 2; break;

		case '5': cs = 5; break;
		case '6': cs = 6; break;
		case '7': cs = 7; break;
		case '8': cs = 8; break;
		case '9': cs = 9; break;

		case 'x': dump++; break;

		case 'h': usage(stdout); break;
		}
	}
	argc -= optind;
	argv += optind;

	if (fname == NULL) fname = argv[0];
	if (fname == NULL) {
		fprintf(stderr, PROGNAME ": No device specified\n");
		return -1;
	}

	int fd = open_serial(fname, O_RDWR, speed, cs, parity, stop, flow);
	if (fd < 0) {
		perror("open");
		return -1;
	}

	int fdin = STDIN_FILENO;

	while (1) {
		char buf[4096];
		struct timeval tv;
		fd_set readfds, writefds;

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		FD_SET(fdin, &readfds);
		FD_SET(fd, &readfds);

		if (select(FD_SETSIZE, &readfds, &writefds, NULL, &tv) == -1) {
			if (errno == EAGAIN) continue;
			perror("select");
			return -1;
		}

		if (FD_ISSET(fd, &readfds)) {
			ssize_t len = read(fd, buf, sizeof(buf));
			if (len < 0) {
				perror("read");
				return -1;
			}
			if (len == 0) {
				return 0;
			}

			if (dump) {
				static unsigned long pos = 0;
				hexdump(buf, len, dump == 1 ? 0 : pos);
				pos += len;
			} else {
				escape_unprintable(buf, len, raw);
			}
		}

		if (FD_ISSET(fdin, &readfds)) {
			ssize_t len = read(fdin, buf, sizeof(buf));
			if (len <= 0) {
				return 0;
			}

			ssize_t ret = write_with_crlf_conv(fd, buf, len);
			if (ret < 0) {
				perror("write");
				return -1;
			}
		}
	}
}
