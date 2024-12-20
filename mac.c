#include <stdio.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

int main(int argc, char *argv[]) {
	struct ifreq ifr;
	struct ifconf ifc;
	char buf[2048];

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock == -1) {
		printf("socket error\n");
		return -1;
	}

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
		printf("ioctl error\n");
		return -1;
	}

	struct ifreq* it = ifc.ifc_req;
	const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));
	int count = 0;
	unsigned char *ptr;
	for (; it != end; ++it) {
		strcpy(ifr.ifr_name, it->ifr_name);
		if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
			// if (! (ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
				if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
					count++;

					ptr = (unsigned char *) &ifr.ifr_ifru.ifru_hwaddr.sa_data[0];
					printf("%2d %20s %02X:%02X:%02X:%02X:%02X:%02X\n", count, ifr.ifr_name, *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5));
				}
			// }
		} else {
			printf("get mac info error\n");
			return -1;
		}
	}
}

