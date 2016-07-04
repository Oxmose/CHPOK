#include <sysconfig.h>

const uint32_t pok_network_ip_address = 0xa000001;

unsigned sys_sampling_links_nb = 1;
sys_link_t sys_sampling_links[] = {};

unsigned sys_queuing_links_nb = 1;
sys_link_t sys_queuing_links[] = {
    {
        .port_id = 0,
        .protocol = UDP,
        .udp_data = { //destination ip
            .ip = 0xa000002,
            .port = 10001,
        }
    },
};
