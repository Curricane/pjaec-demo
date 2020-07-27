#include <pjmedia/port.h>

PJ_BEGIN_DECL

/**
 * 更新buf中的数据，会直接把buf中的数据覆盖掉
 * @param buf 存放数据的数组
 * @param len 获取到更新了多少数据
 * @param maxcap 获取数据的容量上限，一般是buf的大小
*/
typedef pj_status_t (*updata_buffer_data)(char *buf, unsigned int *len, unsigned int maxcap);

struct buffer_port *create_buffer_port(pj_pool_t *pool,
                unsigned int clock_rate,
                unsigned int channel_count,
                unsigned int bits_per_sample,
                unsigned int samples_per_frame,
                unsigned int buffercap,
                updata_buffer_data updata);

PJ_END_DECL