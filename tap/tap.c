#include <stdio.h>
#include <linux/if_tun.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <unistd.h>

#define MAXEVENTS 64
#define ngx_nonblocking(s)	fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK)

int tap_create(char *dev, int flags)
{
    struct ifreq ifr;
    int fd, err;


    assert(dev != NULL);

    if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
        return fd;

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags |= flags;
    if (*dev != '\0')
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);

    if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) 
    {
        close(fd);
        return err;
    }
    
    strcpy(dev, ifr.ifr_name);

    return fd;
}

int main(int argc, char *argv[])
{
    int tap0, tap1, ret;
    char tap0_name[IFNAMSIZ] = "tap0";
    char tap1_name[IFNAMSIZ] = "tap1";

	int efd;
	int s;
	struct epoll_event event;
	struct epoll_event *events;


	/*~~~~~~~~~~~~  create tap device ~~~~~~~~~~~~*/
    tap0 = tap_create(tap0_name, IFF_TAP | IFF_NO_PI);

    if (tap0 < 0) 
    {
        perror("tap0_create");
        return 1;
    }
    printf("TAP name is %s\n", tap0_name);

    tap1 = tap_create(tap1_name, IFF_TAP | IFF_NO_PI);

    if (tap1 < 0) 
    {
        perror("tap1_create");
        return 1;
    }
    printf("TAP name is %s\n", tap1_name);

	ngx_nonblocking(tap0);
	ngx_nonblocking(tap1);

	/*~~~~~~~~~~~~  epoll create ~~~~~~~~~~~~*/
    efd = epoll_create(MAXEVENTS);
    if (efd == -1)
    {
		perror ("epoll_create");
		abort ();
    }


	/*~~~~~~~~~~~~~ epoll_ctl ~~~~~~~~~~~~~*/
	event.data.fd = tap0;
	event.events = EPOLLIN;// 读入，水平触发方式（默认）
	s = epoll_ctl (efd, EPOLL_CTL_ADD, tap0, &event);
	if (s == -1)
	{
		perror ("epoll_ctl");
		abort ();
	}
	event.data.fd = tap1;
	event.events = EPOLLIN;// 读入，水平触发方式（默认）
	s = epoll_ctl (efd, EPOLL_CTL_ADD, tap1, &event);	
	if (s == -1)
	{
		perror ("epoll_ctl");
		abort ();
	}



	/* Buffer where events are returned */
	events = calloc (MAXEVENTS, sizeof event);

	while(1)
	{
		int n, i;
 		n = epoll_wait (efd, events, MAXEVENTS, -1);
		
 		for (i = 0; i < n; i++)
 		{
 			if ((events[i].events & EPOLLERR) ||
              (events[i].events & EPOLLHUP) ||
              (!(events[i].events & EPOLLIN)))
 			{
 				/* An error has occured on this fd, or the socket is not
                 ready for reading (why were we notified then?) 
                 这个fd上发生了一个错误，或者套接字还没有准备好读取(为什么当时通知了我们?)*/
				fprintf (stderr, "epoll error\n");
				close (events[i].data.fd);
				continue;
 			}
 			else if (tap0 == events[i].data.fd)
 			{
 				printf("Hello ");
			    int ret;
			    unsigned char buf[4096];
		        ret = read(tap0, buf, sizeof(buf));
		        if(ret < 0)
		        {
				    continue;
			    }
			    int i = 0;
			    printf("Receive from tap0!\n");
/*
			    for(i=0; i<ret; i++)
			    {
				    printf("%x", buf[i]);
			    }
*/
			    if(ret > 0)
		        {
		        	ret = write(tap1, buf, ret);
			    }
			    
			}
			else if (tap1 == events[i].data.fd)
 			{
 				printf("Hello ");
			    int ret;
			    unsigned char buf[4096];
		        ret = read(tap1, buf, sizeof(buf));
		        if(ret < 0)
		        {
				    continue;
			    }
			    int i = 0;
			    printf("Receive from tap1!\n");
/*
			    for(i=0; i<ret; i++)
			    {
				    printf("%x", buf[i]);
			    }
*/
			    if(ret > 0)
		        {
		        	ret = write(tap0, buf, ret);
			    }
			    
			}
			else
			{
				continue;
			}
 			
 		}
	}
	free (events);

	close (tap0);
	close (tap1);

	return EXIT_SUCCESS;
}
