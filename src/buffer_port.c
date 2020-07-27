#include <pjmedia/buffer_port.h>
#include <pjmedia/port.h>

#include <pj/assert.h>
#include <pjmedia/errno.h>
#include <pj/file_io.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#define THIS_FILE "buffer_port.c"

#define PJMEDIA_SIG_PORT_BUFFER PJMEDIA_SIG_CLASS_PORT_AUD('B', 'F')
#define SIGNATURE PJMEDIA_SIG_PORT_BUFFER

#define BITS_PER_SAMPLE 16

struct buffer_port
{
    pjmedia_port            base;
    unsigned                options;
    pj_bool_t               isEnd;      // 是否结束
    pj_uint32_t             bufsize;    // 实际数据长度
    pj_uint32_t             bufcap;     // buffer的最大容量
    char                    *buf;       // 存放数据的位置
    char                    *readpos;   // 读取数据的位置

    pj_status_t (*cb)(pjmedia_port*, void*);
    //pj_status_t (*getdata)(char* buf, unsigned int *len); //重新获取数据，并更新长度
    updata_buffer_data      getdata;
};

static pj_status_t buffer_get_frame(pjmedia_port *this_port,
                pjmedia_frame *frame);
static pj_status_t buffer_on_destroy(pjmedia_port *this_port);

struct buffer_port *create_buffer_port(pj_pool_t *pool,
                unsigned int clock_rate,
                unsigned int channel_count,
                unsigned int bits_per_sample,
                unsigned int samples_per_frame,
                unsigned int buffercap,
                updata_buffer_data updata)
{
    const pj_str_t name = pj_str("buffer");
    struct buffer_port *port;
    port = PJ_POOL_ZALLOC_T(pool, struct buffer_port);
    if (!port)
        return NULL;
    
    // 初始化 port数据的信息
    pjmedia_port_info_init(&port->base.info, &name, SIGNATURE,
                clock_rate,
                channel_count,
                bits_per_sample,
                samples_per_frame);
    
    port->base.get_frame = &buffer_get_frame;
    port->base.on_destroy = &buffer_on_destroy;
    port->getdata = updata;
    port->bufsize = 0;
    port->bufcap = buffercap;
    port->buf = (char*) pj_pool_alloc(pool, port->bufcap);
    if (!port->buf)
    {
        PJ_LOG(4, (THIS_FILE, "failed to init buffer_port->buf"));
        return NULL;
    }

    // 开始读数据的位置为buf的首地址
    port->readpos = port->buf;

    return port;
}

/*
 * Fill buffer.
 * 重新获取数据，把之前的数据覆盖掉
*/
static pj_status_t fill_buffer(struct buffer_port * bport)
{

    pj_status_t status;

    status = bport->getdata(bport->buf, &bport->bufsize, bport->bufcap);
    if (status != PJ_SUCCESS)
    {
        bport->bufsize = 0;
    }
    return status;
}


/*
 * Get frame from file.
 * 不考虑drift，让ec去处理
 */
static pj_status_t buffer_get_frame(pjmedia_port *this_port,
                pjmedia_frame *frame)
{
    struct buffer_port *bport = (struct buffer_port*)this_port;
    pj_size_t frame_size;
    pj_status_t status = PJ_SUCCESS;

    pj_assert(bport->base.info.signature == SIGNATURE);
    pj_assert(frame->size <= bport->bufcap);

    // pj_assert(bport->fmt_tag == PJMEDIA_WAVE_FMT_TAG_PCM);
    // 假设当前只支持 pcm

    frame_size = frame->size;

    /* Copy frame from buffer .*/
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->timestamp.u64 = 0;

    // buffer 中有足够的数据放入frame中
    if ((bport->readpos + frame_size) <= (bport->buf + bport->bufsize)) 
    {
        /* Read contiguous buffer. */
        pj_memcpy(frame->buf, bport->readpos, frame_size); // 它拷贝是按字节算的？
        bport->readpos += frame_size;

        /* Fill up the buffer if all has been read. */
        if (bport->readpos == bport->buf + bport->bufsize)
        {
            bport->readpos = bport->buf;

            status = fill_buffer(bport);
            if (status != PJ_SUCCESS) // 没有获取到数据，会不停的执行这个函数么，要不改为0信号
            {
                frame->type = PJMEDIA_FRAME_TYPE_NONE;
                frame->size = 0;
                bport->readpos = bport->buf + bport->bufsize;
                return status;
            }
        }
    }
    else // 剩余的数据不够一个frame大小
    {
        unsigned endread;

        /**
         * Split read.
         * First stage: read until end of buffer.
        */
        endread = (unsigned)( (bport->buf + bport->bufsize) - bport->readpos );
        pj_memcpy(frame->buf, bport->readpos, endread);

        // 要不要考虑再也拿不到数据的可能

        // 能拿到更多的数据
        /* Second stage: fill up buffer, and read from the start of buffer. */
        status = fill_buffer(bport);
        if (status != PJ_SUCCESS) 
        {
            frame->type = PJMEDIA_FRAME_TYPE_NONE;
            frame->size = 0;
            bport->readpos = bport->buf + bport->bufsize;
            return status;
        }

        // 再把frame需要的剩余的数据，拷贝进去
        pj_memcpy(((char*)frame->buf) + endread, bport->buf, frame_size - endread);
        bport->readpos = bport->buf + (frame_size - endread);
    } // 至此，拷贝了一个完整frame数据

    return PJ_SUCCESS;
}

static pj_status_t buffer_on_destroy(pjmedia_port *this_port)
{
    PJ_LOG(3, (THIS_FILE, "buffer_on_destroy destory"));
    return PJ_SUCCESS;
}

