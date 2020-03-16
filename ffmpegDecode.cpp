#pragma once

#include <stdio.h>
#include <ctime>
#include "string"

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
};
#endif
#endif
using namespace std;

#include <pybind11/pybind11.h>
#include <stdio.h>
#include <torch/torch.h>
#include <vector>
#include <thread>

namespace py = pybind11;

class ncap3d {
    // constructor.
public:
    ncap3d() {
        av_register_all();
        avformat_network_init();
    }

    int decode(char *filepath, int length, int width_dst, int height_dst, long ptr) {
        long new_ptr = ptr;
        for (int j = 0; j < length; ++j) {
            // open independent threads for every frames
            t[j] = thread(decode_, filepath, length, j, width_dst, height_dst, new_ptr);
            new_ptr = new_ptr + width_dst * height_dst * 3;
        }

        for (int k = 0; k < length; ++k) {
            t[k].join();
        }

        return 1;
    }

    // l h w 3
    static int decode_(char *filepath, double den, double num, int width_dst, int height_dst, long ptr) {
        AVPacket *packet = NULL;
        AVFrame *pFrame = NULL, *pFrameRGB = NULL;
        AVFormatContext *pFormatCtx = NULL;
        AVCodecContext *pCodecCtx = NULL;
        AVCodec *pCodec = NULL;
        struct SwsContext *pImgConvertCtx = NULL;
        int i = 0;
        int ret = -1;
        int videoindex = -1;
        int got_picture = 0;

        pFormatCtx = avformat_alloc_context();
        packet = (AVPacket *) av_malloc(sizeof(AVPacket));
        pFrame = av_frame_alloc();
        pFrameRGB = av_frame_alloc();

        if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
            printf("Couldn't open time_ratio stream.\n");
            return 0;
        }
        if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
            printf("Couldn't find stream information.\n");
            return 0;
        }

        videoindex = -1;
        for (i = 0; i < pFormatCtx->nb_streams; i++)
            if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoindex = i;
                break;
            }

        if (videoindex == -1) {
            printf("Didn't find a video stream.\n");
            return 0;
        }

        pCodecCtx = pFormatCtx->streams[videoindex]->codec;
        pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
        if (pCodec == NULL) {
            printf("Codec not found.\n");
            return 0;
        }
        if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
            printf("Could not open codec.\n");
            return 0;
        }

        av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, (uint8_t *) ptr,
                             AV_PIX_FMT_RGB24, width_dst, height_dst, 1);


        pImgConvertCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                        width_dst, height_dst, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL,
                                        NULL);

        AVStream* stream = pFormatCtx->streams[videoindex];
        // find the position of the frame and jump to it
        double seek_time = double(pFormatCtx->duration) / double(AV_TIME_BASE)/ den * num;
        int seek_ts = av_rescale(seek_time, stream->time_base.den, stream->time_base.num);
        if (av_seek_frame(pFormatCtx, videoindex, seek_ts,
                          AVSEEK_FLAG_BACKWARD) < 0)
            printf("Decode seek error.\n");


        while (av_read_frame(pFormatCtx, packet) >= 0) {
            if (packet->stream_index == videoindex) {
                // decode frame
                ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
                if (ret < 0) {
                    printf("Decode Error.\n");
                    return 0;
                }

                if (pFrame->pict_type == AV_PICTURE_TYPE_I) {
                    // transform the pframe into the RGB format, mean while resize the image
                    sws_scale(pImgConvertCtx, (const unsigned char *const *) pFrame->data, pFrame->linesize, 0,
                              pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

                    if (ret < 0) {
                        printf("Crop Error.\n");
                        return 0;
                    }

//                    printf("Succeed to decode 1 frame!\n");

                    break;
                } else { ;
//                    printf("wrong seek. \n");
                }

            }
        }
        avcodec_close(pCodecCtx);
        avformat_close_input(&pFormatCtx);
        sws_freeContext(pImgConvertCtx);
        av_free_packet(packet);
        av_frame_free(&pFrameRGB);
        av_frame_free(&pFrame);
        return 1;
    }

    // destructor.
public:
    virtual ~ncap3d() {
    }

    thread t[8];
};


PYBIND11_MODULE(ncap3d, m) {
    py::class_<ncap3d>(m, "ncap3d")
            .def(py::init<>())
            .def("decode", &ncap3d::decode, "decode");
}
