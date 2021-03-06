
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <linux/types.h>
#include <linux/netfilter.h>		/* for NF_ACCEPT */
#include <errno.h>
#include <iostream>
#include <netinet/tcp.h>

#include <libnetfilter_queue/libnetfilter_queue.h>
#include <algorithm>
#include <regex>

using namespace std;

#define IP_PROTO_TCP 0x06
bool opt_verbose;

typedef pair<u_int32_t, bool> ret_data;

/* returns packet id */
// static u_int32_t print_pkt (struct nfq_data *tb)
static ret_data print_pkt (struct nfq_data *tb)
{
	int id = 0;
	struct nfqnl_msg_packet_hdr *ph;
	struct nfqnl_msg_packet_hw *hwph;
	u_int32_t mark,ifi; 
	int ret;
	unsigned char *data;
	struct ip* ip_hdr;
	struct tcphdr *tcp_hdr;
	bool allow = true;
	ret_data ret_val;

	ph = nfq_get_msg_packet_hdr(tb);
	if (ph) {
		id = ntohl(ph->packet_id);
		if (opt_verbose) {
			printf("hw_protocol=0x%04x hook=%u id=%u ",
				ntohs(ph->hw_protocol), ph->hook, id);
		}
	}

	hwph = nfq_get_packet_hw(tb);
	if (hwph) {
		int i, hlen = ntohs(hwph->hw_addrlen);

		if (opt_verbose) {
			printf("hw_src_addr=");
		}
		for (i = 0; i < hlen-1; i++)
			printf("%02x:", hwph->hw_addr[i]);
		printf("%02x ", hwph->hw_addr[hlen-1]);
	}

	mark = nfq_get_nfmark(tb);
	if (mark) {
		if (opt_verbose) {
			printf("mark=%u ", mark);
		}
	}

	ifi = nfq_get_indev(tb);
	if (ifi) {
		if (opt_verbose) {
			printf("indev=%u ", ifi);
		}
	}

	ifi = nfq_get_outdev(tb);
	if (ifi) {
		if (opt_verbose) {
			printf("outdev=%u ", ifi);
		}
	}
	ifi = nfq_get_physindev(tb);
	if (ifi) {
		if (opt_verbose) {
			printf("physindev=%u ", ifi);
		}
	}

	ifi = nfq_get_physoutdev(tb);
	if (ifi) {
		if (opt_verbose) {
			printf("physoutdev=%u ", ifi);
		}
	}

	ret = nfq_get_payload(tb, &data);
	if (ret >= 0) {
		if (opt_verbose) {
			printf("payload_len=%d ", ret);
		}
		ip_hdr = (struct ip*)data;
		if (ip_hdr-> ip_v == 4) {
			uint32_t ip_hdr_len = (ip_hdr->ip_hl << 2);
			uint32_t ip_total_len = ntohs(ip_hdr->ip_len);
			if (ip_hdr->ip_p == IP_PROTO_TCP) {
				printf("this is tcp packet\n");
				tcp_hdr = (struct tcphdr*)(data + ip_hdr_len);
				int tcp_hdr_len = tcp_hdr->th_off * 4;
				int start_offset = ip_hdr_len + tcp_hdr_len;
				uint8_t *payload_ptr = (uint8_t*)data;
				payload_ptr += start_offset;
				int payload_len = ip_total_len - ip_hdr_len - tcp_hdr_len;
				if (payload_len > 0) {
					string tcp_payload(payload_ptr, payload_ptr + payload_len);
					cout << "tcp_payload: " << endl << tcp_payload << endl;
					std::regex base_regex("Host: ([^\n]+)");
				    std::smatch base_match;
			        if (std::regex_search(tcp_payload, base_match, base_regex)) {
			            // cout << "base_match.size() : " << base_match.size() << endl;

			        	string host_name;
			            bool found = false;
			            if (base_match.size() > 0) {
				            string found_str = base_match[0].str();
				            cout << found_str << endl; // Host: test.gilgil.net
				            int str_len = found_str.length();
				            cout << str_len << endl;
				            for (int i=0; i < str_len; i++) {
				            	char ch = found_str[i];
				            	if (ch == ' ') {
				            		found = true;
				            		host_name = found_str.substr(i+1, str_len - i - 2);// test.gilgil.net
				            		break;
				            	}
				            }
	
				            if (found) {
				            	cout << "host_name in http packet: " << host_name << endl;
				            	if (host_name.compare("test.gilgil.net") == 0) {
				            		allow = false;
				            		cout << "DROP THIS PACKET!!" << endl;
				            	}
				            }
			            }
		

			        }
			        
				}
			
			}
		}
		
	}

	fputc('\n', stdout);
	ret_val.first = id;
	ret_val.second = allow;
	return ret_val;
	// return id;
}
	

static int cb(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg,
	      struct nfq_data *nfa, void *data)
{
	ret_data ret = print_pkt(nfa);
	u_int32_t id = ret.first;
	bool allow = ret.second;
	// u_int32_t id = print_pkt(nfa);
	if (opt_verbose) {
		printf("entering callback\n");
	}
	// return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
	return nfq_set_verdict(qh, id, allow ? NF_ACCEPT : NF_DROP, 0, NULL);
}

int main(int argc, char **argv)
{
	struct nfq_handle *h;
	struct nfq_q_handle *qh;
	// struct nfnl_handle *nh;
	int fd;
	int rv;
	char buf[4096] __attribute__ ((aligned));
	

	for (int i=1; i < argc; i++) {
		char *cur_arg = argv[i];
		if (cur_arg[0] == '-') {
			switch(cur_arg[1]) {
				case 'v':
				opt_verbose = true;
				break;
			}
		}
	}

	if (opt_verbose) {
		printf("opening library handle\n");
	}
	h = nfq_open();
	if (!h) {
		fprintf(stderr, "error during nfq_open()\n");
		exit(1);
	}

	if (opt_verbose) {
		printf("unbinding existing nf_queue handler for AF_INET (if any)\n");
	}
	if (nfq_unbind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "error during nfq_unbind_pf()\n");
		exit(1);
	}

	if (opt_verbose) {
		printf("binding nfnetlink_queue as nf_queue handler for AF_INET\n");
	}
	if (nfq_bind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "error during nfq_bind_pf()\n");
		exit(1);
	}

	if (opt_verbose) {
		printf("binding this socket to queue '0'\n");
	}
	qh = nfq_create_queue(h,  0, &cb, NULL);
	if (!qh) {
		fprintf(stderr, "error during nfq_create_queue()\n");
		exit(1);
	}

	if (opt_verbose) {
		printf("setting copy_packet mode\n");
	}

	if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) {
		fprintf(stderr, "can't set packet_copy mode\n");
		exit(1);
	}

	fd = nfq_fd(h);

	for (;;) {
		if ((rv = recv(fd, buf, sizeof(buf), 0)) >= 0) {
			printf("pkt received\n");
			nfq_handle_packet(h, buf, rv);
			continue;
		}
		/* if your application is too slow to digest the packets that
		 * are sent from kernel-space, the socket buffer that we use
		 * to enqueue packets may fill up returning ENOBUFS. Depending
		 * on your application, this error may be ignored. Please, see
		 * the doxygen documentation of this library on how to improve
		 * this situation.
		 */
		if (rv < 0 && errno == ENOBUFS) {
			printf("losing packets!\n");
			continue;
		}
		perror("recv failed");
		break;
	}

	if (opt_verbose) {
		printf("unbinding from queue 0\n");
	}
	nfq_destroy_queue(qh);

#ifdef INSANE
	/* normally, applications SHOULD NOT issue this command, since
	 * it detaches other programs/sockets from AF_INET, too ! */

	if (opt_verbose) {
		printf("unbinding from AF_INET\n");
	}
	nfq_unbind_pf(h, AF_INET);
#endif

	if (opt_verbose) {
		printf("closing library handle\n");
	}
	nfq_close(h);

	exit(0);
}
