#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "queue.h"

#define TRANSACTION_SIZE             1024UL
#define TRANSACTION_SIZE_HEADER      4UL
#define TRANSACTION_SIZE_PAYLOAD_MAX (TRANSACTION_SIZE - TRANSACTION_SIZE_HEADER)

#define PTY            "/dev/ttySPI0"
#define SPI            "/dev/spidev0.0"
#define SPI_CLOCK_HZ   8000000UL
#define SPI_GAP_NS     10000000UL
#define SPI_QUEUE_SIZE (32UL * 1024UL)

static int keepalive;
static int pty;
static queue_t *spi_tx;
static queue_t *spi_rx;
static atomic_int running = 1;

static void *spi_thread(void *arg) {
    (void)arg;

    int spi = open(SPI, O_RDWR);
    if(spi < 0) {
        perror(SPI);
        running = 0;
        return NULL;
    }
    uint8_t mode = SPI_MODE_0;
    if(ioctl(spi, SPI_IOC_WR_MODE, &mode) < 0) {
        perror("SPI_IOC_WR_MODE");
        close(spi);
        running = 0;
        return NULL;
    }
    uint8_t bpw = 8;
    if(ioctl(spi, SPI_IOC_WR_BITS_PER_WORD, &bpw) < 0) {
        perror("SPI_IOC_WR_BITS_PER_WORD");
        close(spi);
        running = 0;
        return NULL;
    }
    uint32_t speed = SPI_CLOCK_HZ;
    if(ioctl(spi, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        perror("SPI_IOC_WR_MAX_SPEED_HZ");
        close(spi);
        running = 0;
        return NULL;
    }

    uint8_t transaction_tx[TRANSACTION_SIZE];
    uint8_t transaction_rx[TRANSACTION_SIZE];
    uint32_t len;

    const struct spi_ioc_transfer transaction = {
        .tx_buf = (uintptr_t)transaction_tx,
        .rx_buf = (uintptr_t)transaction_rx,
        .len = TRANSACTION_SIZE,
        .speed_hz = speed,
        .bits_per_word = 8,
        .cs_change = 0,
    };

    struct timespec next;
    int res = 0;

    while(running) {
        if(res >= 0) {
            memset(transaction_tx, 0, sizeof(transaction_tx));
            memset(transaction_rx, 0, sizeof(transaction_rx));

            len = queue_read(spi_tx, &transaction_tx[TRANSACTION_SIZE_HEADER],
                             TRANSACTION_SIZE_PAYLOAD_MAX, QUEUE_NO_WAIT);

            transaction_tx[0] = (len & 0x000000FF) >> 0;
            transaction_tx[1] = (len & 0x0000FF00) >> 8;
            transaction_tx[2] = (len & 0x00FF0000) >> 16;
            transaction_tx[3] = (len & 0xFF000000) >> 24;
        }

        res = ioctl(spi, SPI_IOC_MESSAGE(1), &transaction);

        clock_gettime(CLOCK_MONOTONIC, &next);
        next.tv_nsec += SPI_GAP_NS;
        while(next.tv_nsec >= 1000000000L) {
            next.tv_sec++;
            next.tv_nsec -= 1000000000L;
        }

        if(res >= 0) {
            len = (((uint32_t)transaction_rx[0]) << 0) | (((uint32_t)transaction_rx[1]) << 8) |
                  (((uint32_t)transaction_rx[2]) << 16) | (((uint32_t)transaction_rx[3]) << 24);

            if(len <= TRANSACTION_SIZE_PAYLOAD_MAX) {
                queue_write(spi_rx, &transaction_rx[TRANSACTION_SIZE_HEADER], len, QUEUE_NO_WAIT);
            }
        } else if(errno != EINTR) {
            perror("SPI transfer");
        }

        while(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL) == EINTR) {
        }
    }

    close(spi);

    return NULL;
}

static int pty_setup(void) {
    pty = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if(pty < 0) {
        perror("posix_openpt");
        return -1;
    }
    if((grantpt(pty) < 0) || (unlockpt(pty) < 0)) {
        perror("grantpt/unlockpt");
        close(pty);
        return -2;
    }
    char name[128];
    if(ptsname_r(pty, name, sizeof name)) {
        perror("ptsname_r");
        close(pty);
        return -3;
    }

    keepalive = open(name, O_RDWR | O_NOCTTY);
    if(keepalive < 0) {
        perror("open pty slave");
        close(pty);
        return -4;
    }
    struct termios t;
    if(tcgetattr(keepalive, &t) == 0) {
        cfmakeraw(&t);
        tcsetattr(keepalive, TCSANOW, &t);
    }
    cfmakeraw(&t);
    tcsetattr(keepalive, TCSANOW, &t);
    unlink(PTY);
    if(symlink(name, PTY) < 0) {
        perror("symlink " PTY);
        close(keepalive);
        close(pty);
        return -5;
    }
    chmod(name, 0666);

    return 0;
}

static void pty_cleanup(void) {
    unlink(PTY);
    close(keepalive);
    close(pty);
}

static void pty_write(const uint8_t *buffer, size_t len) {
    while(len && running) {
        ssize_t w = write(pty, buffer, len);
        if(w > 0) {
            buffer += w;
            len -= (size_t)w;
            continue;
        }
        if((w < 0) && (errno == EINTR)) {
            continue;
        }
        if((w < 0) && (errno == EAGAIN)) {
            struct pollfd pf = {pty, POLLOUT, 0};
            if(poll(&pf, 1, 100) <= 0) {
                return;
            }
            continue;
        }
        return;
    }
}

static void *pty_thread_read(void *arg) {
    (void)arg;

    uint8_t buffer[8192];

    while(running) {
        struct pollfd pf = {
            pty,
            POLLIN,
            0,
        };

        if(poll(&pf, 1, 100) <= 0) {
            continue;
        }

        ssize_t n = read(pty, buffer, sizeof(buffer));

        if(n > 0) {
            queue_write(spi_tx, buffer, n, QUEUE_WAIT_FOREVER);
        } else if((n < 0) && (errno != EINTR) && (errno != EAGAIN)) {
            usleep(10000);
        }
    }

    return NULL;
}

static void *pty_thread_write(void *arg) {
    (void)arg;

    uint8_t buffer[TRANSACTION_SIZE_PAYLOAD_MAX];
    uint32_t len;

    while(running) {
        len = queue_read(spi_rx, buffer, sizeof(buffer), QUEUE_WAIT_FOREVER);

        if(len > 0) {
            pty_write(buffer, len);
        }
    }

    return NULL;
}

int main(void) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    const int res = pty_setup();
    if(res) {
        return 1;
    }

    spi_tx = queue_create(SPI_QUEUE_SIZE);
    spi_rx = queue_create(SPI_QUEUE_SIZE);

    if(!spi_tx || !spi_rx) {
        fprintf(stderr, "queue_create failed\n\r");
        queue_free(spi_tx);
        queue_free(spi_rx);
        pty_cleanup();
        return 1;
    }

    pthread_t th[3];
    bool started[3] = {false, false, false};
    void *(*fn[3])(void *) = {
        spi_thread,
        pty_thread_read,
        pty_thread_write,
    };

    for(int i = 0; i < 3; i++) {
        const int e = pthread_create(&th[i], NULL, fn[i], NULL);
        if(e != 0) {
            fprintf(stderr, "pthread_create(%d): %s\n", i, strerror(e));
            running = 0;
            break;
        }
        started[i] = true;
    }

    // pthread_create(&th1, NULL, spi_thread, NULL);
    // pthread_create(&th2, NULL, pty_thread_read, NULL);
    // pthread_create(&th3, NULL, pty_thread_write, NULL);

    const struct timespec ts = {0, 100000000L};
    while(running) {
        if(sigtimedwait(&set, NULL, &ts) > 0) {
            running = 0;
        }
    }

    queue_close(spi_tx);
    queue_close(spi_rx);

    // pthread_join(th1, NULL);
    // pthread_join(th2, NULL);
    // pthread_join(th3, NULL);

    for(int i = 0; i < 3; i++) {
        if(started[i]) {
            pthread_join(th[i], NULL);
        }
    }

    queue_free(spi_tx);
    queue_free(spi_rx);
    pty_cleanup();

    return 0;
}
