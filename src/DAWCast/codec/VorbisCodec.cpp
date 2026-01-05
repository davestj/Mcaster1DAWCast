// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VorbisCodec.h"
#include "FFmpegCodec.h"

#include <QDebug>

#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <vector>

#ifdef HAVE_VORBIS
#include <vorbis/vorbisfile.h>
#include <vorbis/vorbisenc.h>
#include <ogg/ogg.h>
#endif

namespace dawcast {

// ============================================================
// Vorbis decode
// ============================================================

AudioBuffer VorbisCodec::decode(const QString &path)
{
#ifdef HAVE_VORBIS
    OggVorbis_File vf;
    int err = ov_fopen(path.toUtf8().constData(), &vf);
    if (err != 0) {
        qWarning() << "VorbisCodec::decode: ov_fopen() failed, error" << err;
        // Fall through to FFmpeg
        goto ffmpeg_fallback;
    }

    {
        vorbis_info *vi = ov_info(&vf, -1);
        if (!vi) {
            qWarning() << "VorbisCodec::decode: ov_info() returned null";
            ov_clear(&vf);
            goto ffmpeg_fallback;
        }

        int channels = vi->channels;
        int sampleRate = static_cast<int>(vi->rate);
        ogg_int64_t totalPcm = ov_pcm_total(&vf, -1);

        if (channels <= 0 || sampleRate <= 0 || totalPcm <= 0) {
            qWarning() << "VorbisCodec::decode: invalid stream info";
            ov_clear(&vf);
            goto ffmpeg_fallback;
        }

        int totalFrames = static_cast<int>(totalPcm);
        int totalSamples = totalFrames * channels;
        std::vector<float> pcmData;
        pcmData.reserve(static_cast<size_t>(totalSamples));

        float **pcmChannels = nullptr;
        int bitstream = 0;

        while (true) {
            long ret = ov_read_float(&vf, &pcmChannels, 4096, &bitstream);
            if (ret <= 0) break;

            // ov_read_float gives non-interleaved per-channel float buffers.
            // Interleave into our output.
            for (long f = 0; f < ret; ++f) {
                for (int c = 0; c < channels; ++c) {
                    pcmData.push_back(pcmChannels[c][f]);
                }
            }
        }

        ov_clear(&vf);

        int actualSamples = static_cast<int>(pcmData.size());
        int actualFrames = actualSamples / channels;

        AudioBuffer buf;
        buf.data = new float[static_cast<size_t>(actualSamples)];
        std::memcpy(buf.data, pcmData.data(),
                    static_cast<size_t>(actualSamples) * sizeof(float));
        buf.frames = actualFrames;
        buf.channels = channels;
        buf.sampleRate = sampleRate;
        return buf;
    }

ffmpeg_fallback:
#endif
    // Fallback to FFmpeg
    FFmpegCodec ffmpeg;
    return ffmpeg.decode(path);
}

// ============================================================
// Vorbis encode
// ============================================================

bool VorbisCodec::encode(const AudioBuffer &buffer, const QString &path, float quality)
{
#ifdef HAVE_VORBIS
    if (!buffer.data || buffer.frames <= 0 || buffer.channels <= 0) {
        qWarning() << "VorbisCodec::encode: invalid AudioBuffer";
        return false;
    }

    // Clamp quality to valid range [-0.1, 1.0]
    quality = std::fmax(-0.1f, std::fmin(1.0f, quality));

    vorbis_info vi;
    vorbis_info_init(&vi);

    int ret = vorbis_encode_init_vbr(&vi,
                                     buffer.channels,
                                     buffer.sampleRate,
                                     quality);
    if (ret != 0) {
        qWarning() << "VorbisCodec::encode: vorbis_encode_init_vbr() failed";
        vorbis_info_clear(&vi);
        return false;
    }

    // Set up analysis state and auxiliary encoding storage
    vorbis_dsp_state vd;
    vorbis_block vb;
    vorbis_comment vc;

    vorbis_comment_init(&vc);
    vorbis_comment_add_tag(&vc, "ENCODER", "Mcaster1DAWCast");

    vorbis_analysis_init(&vd, &vi);
    vorbis_block_init(&vd, &vb);

    // Set up Ogg stream with a random serial number
    ogg_stream_state os;
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    ogg_stream_init(&os, std::rand());

    // Write Vorbis headers into the Ogg stream
    ogg_packet header, headerComm, headerCode;
    vorbis_analysis_headerout(&vd, &vc, &header, &headerComm, &headerCode);
    ogg_stream_packetin(&os, &header);
    ogg_stream_packetin(&os, &headerComm);
    ogg_stream_packetin(&os, &headerCode);

    // Open output file
    FILE *outFile = std::fopen(path.toUtf8().constData(), "wb");
    if (!outFile) {
        qWarning() << "VorbisCodec::encode: cannot open" << path;
        ogg_stream_clear(&os);
        vorbis_block_clear(&vb);
        vorbis_dsp_clear(&vd);
        vorbis_comment_clear(&vc);
        vorbis_info_clear(&vi);
        return false;
    }

    // Flush header pages
    ogg_page og;
    while (ogg_stream_flush(&os, &og)) {
        std::fwrite(og.header, 1, static_cast<size_t>(og.header_len), outFile);
        std::fwrite(og.body, 1, static_cast<size_t>(og.body_len), outFile);
    }

    // Encode audio data in blocks
    int channels = buffer.channels;
    int totalFrames = buffer.frames;
    int offset = 0;
    bool eos = false;
    constexpr int kBlockSize = 4096;

    while (!eos) {
        int framesToWrite = std::min(kBlockSize, totalFrames - offset);

        if (framesToWrite == 0) {
            // Signal end of data
            vorbis_analysis_wrote(&vd, 0);
        } else {
            // Expose the vorbis analysis buffer and fill it with de-interleaved data
            float **analysisBuffer = vorbis_analysis_buffer(&vd, framesToWrite);
            for (int f = 0; f < framesToWrite; ++f) {
                for (int c = 0; c < channels; ++c) {
                    analysisBuffer[c][f] = buffer.data[(offset + f) * channels + c];
                }
            }
            vorbis_analysis_wrote(&vd, framesToWrite);
            offset += framesToWrite;
        }

        // Extract and write Ogg pages
        while (vorbis_analysis_blockout(&vd, &vb) == 1) {
            vorbis_analysis(&vb, nullptr);
            vorbis_bitrate_addblock(&vb);

            ogg_packet op;
            while (vorbis_bitrate_flushpacket(&vd, &op)) {
                ogg_stream_packetin(&os, &op);

                while (!eos) {
                    int result = ogg_stream_pageout(&os, &og);
                    if (result == 0) break;

                    std::fwrite(og.header, 1,
                                static_cast<size_t>(og.header_len), outFile);
                    std::fwrite(og.body, 1,
                                static_cast<size_t>(og.body_len), outFile);

                    if (ogg_page_eos(&og)) eos = true;
                }
            }
        }
    }

    std::fclose(outFile);

    // Cleanup
    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);

    return true;

#else
    // Fallback to FFmpeg
    FFmpegCodec ffmpeg;
    return ffmpeg.encode(buffer, path, QStringLiteral("libvorbis"),
                         static_cast<int>(quality * 320)); // rough quality-to-bitrate
#endif
}

} // namespace dawcast
