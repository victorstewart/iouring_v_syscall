#include "liburing.h"
#include <chrono>
#include <netinet/in.h>
#include <string>
#include <cstring>

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

int main(int argc, char *argv[])
{
    int rFd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

    struct sockaddr_in6 *address = (struct sockaddr_in6 *)calloc(1, sizeof(struct sockaddr_in6));
    address->sin6_family = AF_INET6;
    address->sin6_flowinfo = 0;
    address->sin6_port = htons(111);
    address->sin6_addr = in6addr_loopback;

    bind(rFd, (struct sockaddr *)address, sizeof(struct sockaddr_in6));
    listen(rFd, SOMAXCONN);

    int sFd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

    msghdr smsg = {};
    {
        smsg.msg_name = (struct sockaddr *)address;
        smsg.msg_namelen = sizeof(struct sockaddr_in6);

        smsg.msg_iov = (struct iovec *)malloc(sizeof(struct iovec));

        struct iovec& buffer = smsg.msg_iov[0];
        buffer.iov_len = 1400;
        buffer.iov_base = malloc(1400);
        memset(buffer.iov_base, 7, 1400);

        smsg.msg_iovlen = 1;
    }

    msghdr rmsg = {};
    {
        rmsg.msg_name = malloc(sizeof(struct sockaddr_in6));
        rmsg.msg_namelen = sizeof(struct sockaddr_in6);

        rmsg.msg_iov = (struct iovec *)malloc(sizeof(struct iovec));
        rmsg.msg_iov[0].iov_len = 1400;
        rmsg.msg_iov[0].iov_base = malloc(1400);
        rmsg.msg_iovlen = 1;
    }
    
    // io_uring test
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

        for (int i = 0; i < 10000; i++) 
        {
            sqe = io_uring_get_sqe(&ring);
            io_uring_prep_sendmsg(sqe, 1, &smsg, 0);
            sqe->flags |= IOSQE_FIXED_FILE;

            sqe = io_uring_get_sqe(&ring);
            io_uring_prep_recvmsg(sqe, 0, &rmsg, 0);
            sqe->flags |= IOSQE_FIXED_FILE;

            io_uring_submit_and_wait(&ring, 2);

            io_uring_for_each_cqe(&ring, head, cqe) 
            {
                if (unlikely(cqe->res != 1400))
                {
                    printf("iouring failed with result = %d\n", cqe->res);
                    return 0;
                }
            }   

            io_uring_cq_advance(&ring, 2);
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
