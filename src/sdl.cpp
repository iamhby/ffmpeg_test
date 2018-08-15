
#include "main.h"

#if (HBY_RUN==3)

#define __run__ 0
#if (__run__ == 0)

#define SDL_AUDIO_BUFFER_SIZE 1024
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;


// 分配解码过程中的使用缓存
AVFrame* audioFrame = av_frame_alloc();
PacketQueue *audioq;

void packet_queue_init(PacketQueue *q) {
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt) {

    AVPacketList *pkt1;
    if (av_dup_packet(pkt) < 0) {
        return -1;
    }
    pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    SDL_LockMutex(q->mutex);

    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size;
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
    return 0;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
    AVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for (;;) {

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}


int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf, int buf_size)
{
    static AVPacket pkt;
    static uint8_t *audio_pkt_data = NULL;
    static int audio_pkt_size = 0;
    int len1, data_size;

    for(;;)
    {
        if(packet_queue_get(audioq, &pkt, 1) < 0)
        {
            return -1;
        }
        audio_pkt_data = pkt.data;
        audio_pkt_size = pkt.size;
        while(audio_pkt_size > 0)
        {
            int got_picture;

            int ret = avcodec_decode_audio4( aCodecCtx, audioFrame, &got_picture, &pkt);
            if( ret < 0 ) {
                printf("Error in decoding audio frame.\n");
                exit(0);
            }

            if( got_picture ) {
                int in_samples = audioFrame->nb_samples;
                short *sample_buffer = (short*)malloc(audioFrame->nb_samples * 2 * 2);
                memset(sample_buffer, 0, audioFrame->nb_samples * 4);

                int i=0;
                float *inputChannel0 = (float*)(audioFrame->extended_data[0]);

                // Mono
                if( audioFrame->channels == 1 ) {
                    for( i=0; i<in_samples; i++ ) {
                        float sample = *inputChannel0++;
                        if( sample < -1.0f ) {
                            sample = -1.0f;
                        } else if( sample > 1.0f ) {
                            sample = 1.0f;
                        }

                        sample_buffer[i] = (int16_t)(sample * 32767.0f);
                    }
                } else { // Stereo
                    float* inputChannel1 = (float*)(audioFrame->extended_data[1]);
                    for( i=0; i<in_samples; i++) {
                        sample_buffer[i*2] = (int16_t)((*inputChannel0++) * 32767.0f);
                        sample_buffer[i*2+1] = (int16_t)((*inputChannel1++) * 32767.0f);
                    }
                }
//                fwrite(sample_buffer, 2, in_samples*2, pcmOutFp);
                memcpy(audio_buf,sample_buffer,in_samples*4);
                free(sample_buffer);
            }

            audio_pkt_size -= ret;

            if (audioFrame->nb_samples <= 0)
            {
                continue;
            }

            data_size = audioFrame->nb_samples * 4;

            return data_size;
        }
        if(pkt.data)
            av_free_packet(&pkt);
   }
}

void audio_callback(void *userdata, Uint8 *stream, int len)
{

    AVCodecContext *aCodecCtx = (AVCodecContext *) userdata;
    int len1, audio_data_size;

    static uint8_t audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index = 0;

    /*   len是由SDL传入的SDL缓冲区的大小，如果这个缓冲未满，我们就一直往里填充数据 */
    while (len > 0) {
        /*  audio_buf_index 和 audio_buf_size 标示我们自己用来放置解码出来的数据的缓冲区，*/
        /*   这些数据待copy到SDL缓冲区， 当audio_buf_index >= audio_buf_size的时候意味着我*/
        /*   们的缓冲为空，没有数据可供copy，这时候需要调用audio_decode_frame来解码出更
         /*   多的桢数据 */

        if (audio_buf_index >= audio_buf_size) {
            audio_data_size = audio_decode_frame(aCodecCtx, audio_buf,sizeof(audio_buf));
            /* audio_data_size < 0 标示没能解码出数据，我们默认播放静音 */
            if (audio_data_size < 0) {
                /* silence */
                audio_buf_size = 1024;
                /* 清零，静音 */
                memset(audio_buf, 0, audio_buf_size);
            } else {
                audio_buf_size = audio_data_size;
            }
            audio_buf_index = 0;
        }
        /*  查看stream可用空间，决定一次copy多少数据，剩下的下次继续copy */
        len1 = audio_buf_size - audio_buf_index;
        if (len1 > len) {
            len1 = len;
        }

        memcpy(stream, (uint8_t *) audio_buf + audio_buf_index, len1);
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
    }
}

int main(int argc, char* argv[])
{
    //目前这里只能用aac的文件 试过mp3播放不正常 其他没有测试
    //可能是ffmpeg自带的解码器解码mp3有问题  以后再研究了
    //这个例子的重点是讲SDL的使用 因此不管他了
    //（记住路径不要有中文）
    char *filename = "E:\\workerspace\\Demo.mp3";//in.aac";

    //初始化FFMPEG  调用了这个才能正常使用编码器和解码器
    av_register_all();

    AVFormatContext* pFormatCtx = avformat_alloc_context();
    if( avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0 ) {
        printf("Couldn't open file.\n");
        return -1;
    }

    // Retrieve stream information
    if( avformat_find_stream_info(pFormatCtx, NULL) < 0 ) {
        printf("Couldn't find stream information.\n");
        return -1;
    }

    // Dump valid information onto standard error
    av_dump_format(pFormatCtx, 0, filename, false);

    ///循环查找包含的音频流信息，直到找到音频类型的流
    ///便将其记录下来 保存到audioStream变量中
    int audioStream = -1;
    for(int i=0; i < pFormatCtx->nb_streams; i++) {
        if( pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO ) {
            audioStream = i;
            break;
        }
    }

    ///如果audioStream为-1 说明没有找到音频流
    if( audioStream == -1 ) {
        printf("Didn't find a audio stream.\n");
        return -1;
    }

    // Get a pointer to the codec context for the audio stream
    AVCodecContext* audioCodecCtx = pFormatCtx->streams[audioStream]->codec;

    ///查找解码器
    AVCodec* pCodec = avcodec_find_decoder(audioCodecCtx->codec_id);
    if( pCodec == NULL ) {
        printf("Codec not found.\n");
        return -1;
    }

    ///打开解码器
    AVDictionary* options = NULL;
    if( avcodec_open2(audioCodecCtx, pCodec, &options) < 0 ) {
        printf("Could not open codec.\n");
        return -1;
    }




    ///  打开SDL播放设备 - 开始
   SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER);
//    SDL_LockAudio();
    SDL_AudioSpec spec;
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = audioCodecCtx->sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = audioCodecCtx->channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = audioCodecCtx;
//    SDL_OpenAudioDevice(NULL, 0, &wanted_spec, NULL, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if(SDL_OpenAudio(&wanted_spec,NULL /*&spec*/) < 0)
    {
        fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
        return false;
    }
//    SDL_UnlockAudio();
    SDL_PauseAudio(0);
    ///  打开SDL播放设备 - 结束


    //初始化音频队列
    audioq = new PacketQueue;
    packet_queue_init(audioq);


    AVPacket *packet = (AVPacket *)malloc(sizeof(AVPacket));
    av_init_packet(packet);

    // 分配解码过程中的使用缓存
    AVFrame* audioFrame = av_frame_alloc();

    // Debug -- Begin
    printf("比特率 %3d\n", pFormatCtx->bit_rate);
    printf("解码器名称 %s\n", audioCodecCtx->codec->long_name);
    printf("time_base  %d \n", audioCodecCtx->time_base);
    printf("声道数  %d \n", audioCodecCtx->channels);
    printf("sample per second  %d \n", audioCodecCtx->sample_rate);
    // Debug -- End

    while(1)
    {
        if (av_read_frame(pFormatCtx, packet) < 0 )
        {
            break; //这里认为音频读取完了
        }

        if( packet->stream_index == audioStream )
        {
            packet_queue_put(audioq, packet);
            //这里我们将数据存入队列 因此不调用 av_free_packet 释放
        }
        else
        {
            // Free the packet that was allocated by av_read_frame
            av_free_packet(packet);
        }

        SDL_Delay(5);
    }

    printf("read finished!\n");

    while(1);

    av_free(audioFrame);
    avcodec_close(audioCodecCtx);// Close the codec
    avformat_close_input(&pFormatCtx);// Close the video file

    return 0;
}

#elif (__run__ == 1)

#include <iostream>


#define SOUND_DATA_LEN 409600//samples*channel*2byte=4096的倍数

//音频数据结构
typedef struct SoundData
{
    Uint8   buffer[SOUND_DATA_LEN];//数据缓存
    Uint32  position;//缓存当前播放指针
    Uint32  length;//待播放的缓存长度
}SoundData;



//SDL 2.0
//播放回调
void fill_audio(void *udata, Uint8 *stream, int len)
{
    SoundData* sd = (SoundData*)udata;
    SDL_memset(stream, 0, len);

    //缓存数据已播放完毕
    if(sd->length <= 0)
    {
        return;
    }

    //缓存中能播放最大的长度
    len = len > sd->length ?sd->length : len;

    //将数据混合至声卡设备
    SDL_MixAudio(stream, sd->buffer+sd->position/*position*/, len, SDL_MIX_MAXVOLUME);
    sd->position += len;//当前播放位置更新
    sd->length -= len;//缓存剩余长度更新
}

int main()
{
    char *file_name = "E:\\workerspace\\in.wav";//只支持.wav文件

    //声音缓存结构
    SoundData sd;

    //步骤（1）设置音频信息
    //SDL 2.0 Support for multiple windows
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = 44100;//44.1KHz采样率
    wanted_spec.format = AUDIO_S16SYS;//采样数据格式
    wanted_spec.channels = 2;//2声道
    wanted_spec.silence = 0;//静音时大小
    wanted_spec.samples = 1024;//每次播放长度1024*2channels*2byte
    wanted_spec.callback = fill_audio;//设置播放回调函数
    wanted_spec.userdata = &sd;//传递数据给回调函数



    //步骤（2）打开音频设备
    if (SDL_OpenAudio(&wanted_spec, NULL)<0)
    {
        printf("can't open audio.\n");
            return 0;
    }


    FILE* file = fopen(file_name, "rb");
    if(!file)
    {
        cout<<"无法打开声音文件:"<<file_name<<endl;
            return 0;
    }

    int count = 0;//文件帧数
    int len = 0;//能读文件数据长度

    //步骤（3）开始播放
    SDL_PauseAudio(0);
    //循环读文件数据至缓存
    while(len = fread(sd.buffer, 1, SOUND_DATA_LEN, file))
    {
        sd.length = len;//Audio buffer length
        sd.position =0;// sd.buffer;
        //存存数据未播放完等待
        while(sd.length>0)
        {
            SDL_Delay(1);
        }
        printf("文件帧：%03d\n", count++);
    }

    fclose(file);

    //步骤（4）退出
    SDL_Quit();

    system("pause");
    return 0;
}

#endif
#endif
