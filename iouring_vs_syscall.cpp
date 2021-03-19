#include "liburing.h"
#include <chrono>
#include <netinet/in.h>
#include <string>
#include <cstring>

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

int main(int argc, char *argv[])
{
    uint32_t sendBatchSize = 1;

    int rFd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    setsockopt(rFd, SOL_SOCKET, SO_SNDBUF, (const uint32_t[]){ 10'000 * 1500 }, sizeof(uint32_t));
    setsockopt(rFd, SOL_SOCKET, SO_RCVBUF, (const uint32_t[]){ 10'000 * 1500 }, sizeof(uint32_t));

    struct sockaddr_in6 *address = (struct sockaddr_in6 *)calloc(1, sizeof(struct sockaddr_in6));
    address->sin6_family = AF_INET6;
    address->sin6_flowinfo = 0;
    address->sin6_port = htons(131);
    address->sin6_addr = in6addr_loopback;

    bind(rFd, (struct sockaddr *)address, sizeof(struct sockaddr_in6));
    listen(rFd, SOMAXCONN);

    int sFd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    setsockopt(sFd, SOL_SOCKET, SO_SNDBUF, (const uint32_t[]){ 10'000 * 1500 }, sizeof(uint32_t));
    setsockopt(sFd, SOL_SOCKET, SO_RCVBUF, (const uint32_t[]){ 10'000 * 1500 }, sizeof(uint32_t));

    struct msghdr smsg = {};
    {
        smsg.msg_name = malloc(sizeof(struct sockaddr_in6));
        memcpy(smsg.msg_name, address, sizeof(struct sockaddr_in6));
        smsg.msg_namelen = sizeof(struct sockaddr_in6);

        smsg.msg_iov = (struct iovec *)malloc(sizeof(struct iovec));

        struct iovec& buffer = smsg.msg_iov[0];
        buffer.iov_len = 1400;
        buffer.iov_base = malloc(1400);
        memset(buffer.iov_base, 7, 1400);

        smsg.msg_iovlen = 1;
    }

    struct msghdr rmsg = {};
    {
        rmsg.msg_name = malloc(sizeof(struct sockaddr_in6));
        rmsg.msg_namelen = sizeof(struct sockaddr_in6);

        rmsg.msg_iov = (struct iovec *)malloc(sizeof(struct iovec));
        rmsg.msg_iov[0].iov_len = 1400;
        rmsg.msg_iov[0].iov_base = malloc(1400);
        rmsg.msg_iovlen = 1;
    }

    // io_uring
    {
        int fds[2];
        fds[0] = rFd;
        fds[1] = sFd;

        struct io_uring ring;
        struct io_uring_params p = { };
        io_uring_queue_init_params(16384, &ring, &p);
        io_uring_register_files(&ring, fds, 2);

        struct io_uring_sqe *sqe;
        struct io_uring_cqe *cqe;
        uint32_t head;

        auto start = std::chrono::high_resolution_clock::now();

        uint32_t count = 0;

        for (int i = 0; i < 10'000; i++) 
        {
            sqe = io_uring_get_sqe(&ring);
            io_uring_prep_sendmsg(sqe, 1, &smsg, 0);
            sqe->flags |= IOSQE_FIXED_FILE;

            sqe = io_uring_get_sqe(&ring);
            io_uring_prep_recvmsg(sqe, 0, &rmsg, 0);
            sqe->flags |= IOSQE_FIXED_FILE;

            if (unlikely(++count == sendBatchSize))
            {
                io_uring_submit_and_wait(&ring, sendBatchSize * 2);
                io_uring_cq_advance(&ring, sendBatchSize * 2);
                count = 0;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();

        double durationMs = (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) / 1000.0;
        printf("io_uring -> %.1f ms\n", durationMs);
    }

    // syscalls
    {
        int result;

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 10000; i++) 
        {
            result = sendmsg(sFd, &smsg, 0);

            if (unlikely(result != 1400))
            {
                printf("sendmsg failed with result = %d\n", result);
                return 0;
            }

            result = recvmsg(rFd, &rmsg, 0);

            if (unlikely(result != 1400))
            {
                printf("recvmsg failed with result = %d\n", result);
                return 0;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();

        double durationMs = (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) / 1000.0;
        printf("syscalls -> %.1f ms\n", durationMs);
    }
}
