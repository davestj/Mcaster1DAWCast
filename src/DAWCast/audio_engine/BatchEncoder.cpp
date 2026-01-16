// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "BatchEncoder.h"
#include "AudioResampler.h"
#include "../codec/FFmpegCodec.h"
#include "../codec/TagTransfer.h"
#include "../core/AudioBuffer.h"
#include "../dsp/DspChain.h"
#include "../dsp/ParametricEQ.h"
#include "../dsp/Compressor.h"
#include "../dsp/Limiter.h"
#include "../dsp/NoiseGate.h"
#include "../dsp/DeEsser.h"
#include "../dsp/SonicEnhancer.h"
#include "../dsp/AGC.h"
#include "../dsp/Normalizer.h"
#include "../config/YamlPresets.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include <cmath>
#include <cstring>
#include <vector>
#include <memory>

namespace dawcast {

// Block size for streaming DSP and normalization (in frames)
static constexpr int kBatchBlockSize = 4096;

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

BatchEncoder::BatchEncoder(QObject* parent)
    : QObject(parent)
{
}

BatchEncoder::~BatchEncoder()
{
    m_cancelled.store(true, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Job Management
// ---------------------------------------------------------------------------

void BatchEncoder::addJob(const BatchJob& job)
{
    m_jobs.append(job);
}

void BatchEncoder::removeJob(int index)
{
    if (index >= 0 && index < m_jobs.size()) {
        m_jobs.removeAt(index);
    }
}

void BatchEncoder::clearJobs()
{
    m_jobs.clear();
}

int BatchEncoder::jobCount() const
{
    return m_jobs.size();
}

const BatchJob& BatchEncoder::job(int index) const
{
    return m_jobs.at(index);
}

BatchJob& BatchEncoder::jobRef(int index)
{
    return m_jobs[index];
}

// ---------------------------------------------------------------------------
// Encoding Control
// ---------------------------------------------------------------------------

bool BatchEncoder::isEncoding() const
{
    return m_encoding.load(std::memory_order_acquire);
}

void BatchEncoder::setParallelJobs(int count)
{
    m_parallelJobs = qBound(1, count, 4);
}

int BatchEncoder::parallelJobs() const
{
    return m_parallelJobs;
}

void BatchEncoder::setOutputDirectory(const QString& dir)
{
    m_outputDirectory = dir;
}

QString BatchEncoder::outputDirectory() const
{
    return m_outputDirectory;
}

void BatchEncoder::startEncoding()
{
    m_cancelled.store(false, std::memory_order_release);
    m_encoding.store(true, std::memory_order_release);

    int completed = 0;
    int total = 0;

    // Count pending jobs
    for (int i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs[i].status == BatchJob::Pending) {
            ++total;
        }
    }

    if (total == 0) {
        m_encoding.store(false, std::memory_order_release);
        emit allFinished();
        return;
    }

    // Process jobs sequentially (parallel support is reserved for future
    // QtConcurrent integration — sequential is safer for FFmpeg contexts)
    for (int i = 0; i < m_jobs.size(); ++i) {
        if (m_cancelled.load(std::memory_order_acquire)) {
            // Mark remaining pending jobs as cancelled
            for (int j = i; j < m_jobs.size(); ++j) {
                if (m_jobs[j].status == BatchJob::Pending) {
                    m_jobs[j].status = BatchJob::Cancelled;
                    m_jobs[j].errorMessage = tr("Cancelled by user");
                }
            }
            break;
        }

        if (m_jobs[i].status != BatchJob::Pending)
            continue;

        processJob(i);

        if (m_jobs[i].status == BatchJob::Complete)
            ++completed;

        emit batchProgress(completed, total);
        QApplication::processEvents();
    }

    m_encoding.store(false, std::memory_order_release);
    emit allFinished();
}

void BatchEncoder::cancelAll()
{
    m_cancelled.store(true, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Internal — Process One Job
// ---------------------------------------------------------------------------

void BatchEncoder::processJob(int index)
{
    BatchJob& job = m_jobs[index];
    job.status = BatchJob::Processing;
    job.progressPercent = 0;
    emit jobStarted(index);

    try {
        encodeFile(job);
    } catch (const std::exception& e) {
        job.status = BatchJob::Failed;
        job.errorMessage = QString::fromUtf8(e.what());
        emit jobFailed(index, job.errorMessage);
        return;
    } catch (...) {
        job.status = BatchJob::Failed;
        job.errorMessage = tr("Unknown error during encoding");
        emit jobFailed(index, job.errorMessage);
        return;
    }

    if (job.status == BatchJob::Processing) {
        job.status = BatchJob::Complete;
        job.progressPercent = 100;
        emit jobProgress(index, 100);
        emit jobFinished(index);
    }
}

// ---------------------------------------------------------------------------
// Codec / Format resolution
// ---------------------------------------------------------------------------

QString BatchEncoder::resolveCodecName(const QString& codec)
{
    QString c = codec.toLower();
    if (c == QStringLiteral("mp3"))    return QStringLiteral("libmp3lame");
    if (c == QStringLiteral("aac"))    return QStringLiteral("aac");
    if (c == QStringLiteral("opus"))   return QStringLiteral("libopus");
    if (c == QStringLiteral("vorbis")) return QStringLiteral("libvorbis");
    if (c == QStringLiteral("flac"))   return QStringLiteral("flac");
    if (c == QStringLiteral("wav"))    return QStringLiteral("pcm_f32le");
    if (c == QStringLiteral("ogg"))    return QStringLiteral("libvorbis");
    return c;
}

QString BatchEncoder::resolveFormat(const QString& codec)
{
    QString c = codec.toLower();
    if (c == QStringLiteral("mp3"))    return QStringLiteral("mp3");
    if (c == QStringLiteral("aac"))    return QStringLiteral("adts");
    if (c == QStringLiteral("opus"))   return QStringLiteral("ogg");
    if (c == QStringLiteral("vorbis")) return QStringLiteral("ogg");
    if (c == QStringLiteral("flac"))   return QStringLiteral("flac");
    if (c == QStringLiteral("wav"))    return QStringLiteral("wav");
    if (c == QStringLiteral("ogg"))    return QStringLiteral("ogg");
    return QString();
}

// ---------------------------------------------------------------------------
// Core Encoding Pipeline
// ---------------------------------------------------------------------------

void BatchEncoder::encodeFile(BatchJob& job)
{
    // ── 0. Read source metadata tags (before decode, so they survive) ────
    AudioTags sourceTags;
    if (job.copyTags && TagTransfer::isSupported(job.inputPath)) {
        sourceTags = TagTransfer::readTags(job.inputPath);
    }

    // ── 1. Decode input file ──────────────────────────────────────────────
    FFmpegCodec decoder;
    AudioBuffer srcBuf = decoder.decode(job.inputPath);

    if (!srcBuf.data || srcBuf.frames <= 0) {
        job.status = BatchJob::Failed;
        job.errorMessage = tr("Failed to decode: %1").arg(job.inputPath);
        emit jobFailed(m_jobs.indexOf(job), job.errorMessage);
        return;
    }

    // Store source metadata
    job.srcSampleRate = srcBuf.sampleRate;
    job.srcChannels   = srcBuf.channels;
    job.srcFrames     = srcBuf.frames;
    job.durationSec   = static_cast<double>(srcBuf.frames) / srcBuf.sampleRate;

    // Detect source format from extension
    QFileInfo srcInfo(job.inputPath);
    job.srcFormat = srcInfo.suffix().toUpper();

    int jobIndex = m_jobs.indexOf(job);

    // Working buffer — we'll operate on this, potentially replacing it
    float* workData     = srcBuf.data;
    int    workFrames   = srcBuf.frames;
    int    workChannels = srcBuf.channels;
    int    workRate     = srcBuf.sampleRate;

    // Owned buffers for resampled / channel-converted data
    std::vector<float> resampledBuf;
    std::vector<float> channelBuf;

    // Progress: decode = 10%, DSP = 30%, normalize = 20%, resample = 10%,
    //           encode = 30%
    job.progressPercent = 10;
    emit jobProgress(jobIndex, 10);
    QApplication::processEvents();

    if (m_cancelled.load(std::memory_order_acquire)) {
        job.status = BatchJob::Cancelled;
        job.errorMessage = tr("Cancelled");
        return;
    }

    // ── 2. Apply DSP chain (if requested) ─────────────────────────────────
    if (job.applyDspChain && !job.dspPresets.isEmpty()) {
        DspChain chain;

        for (const QString& presetPath : job.dspPresets) {
            QVariantMap preset = config::YamlPresets::loadPreset(presetPath);
            QVariantList effects = preset.value(QStringLiteral("effects")).toList();

            for (const QVariant& effectVar : effects) {
                QVariantMap effectMap = effectVar.toMap();
                QString effectId = effectMap.value(QStringLiteral("id")).toString();
                bool bypass = effectMap.value(QStringLiteral("bypass"), false).toBool();
                QVariantMap params = effectMap.value(QStringLiteral("params")).toMap();

                IEffectUnit* effect = nullptr;

                // Instantiate effect based on ID
                if (effectId.contains(QStringLiteral("noise_gate"))) {
                    auto* gate = new NoiseGate;
                    if (params.contains(QStringLiteral("threshold_db")))
                        gate->setParameter(NoiseGate::ThresholdDb,
                            params.value(QStringLiteral("threshold_db")).toFloat());
                    if (params.contains(QStringLiteral("attack_ms")))
                        gate->setParameter(NoiseGate::AttackMs,
                            params.value(QStringLiteral("attack_ms")).toFloat());
                    if (params.contains(QStringLiteral("hold_ms")))
                        gate->setParameter(NoiseGate::HoldMs,
                            params.value(QStringLiteral("hold_ms")).toFloat());
                    if (params.contains(QStringLiteral("release_ms")))
                        gate->setParameter(NoiseGate::ReleaseMs,
                            params.value(QStringLiteral("release_ms")).toFloat());
                    if (params.contains(QStringLiteral("range_db")))
                        gate->setParameter(NoiseGate::RangeDb,
                            params.value(QStringLiteral("range_db")).toFloat());
                    effect = gate;
                }
                else if (effectId.contains(QStringLiteral("deesser"))) {
                    auto* de = new DeEsser;
                    if (params.contains(QStringLiteral("frequency_hz")))
                        de->setParameter(DeEsser::FrequencyHz,
                            params.value(QStringLiteral("frequency_hz")).toFloat());
                    if (params.contains(QStringLiteral("bandwidth")))
                        de->setParameter(DeEsser::Bandwidth,
                            params.value(QStringLiteral("bandwidth")).toFloat());
                    if (params.contains(QStringLiteral("threshold_db")))
                        de->setParameter(DeEsser::ThresholdDb,
                            params.value(QStringLiteral("threshold_db")).toFloat());
                    if (params.contains(QStringLiteral("reduction_db")))
                        de->setParameter(DeEsser::ReductionDb,
                            params.value(QStringLiteral("reduction_db")).toFloat());
                    effect = de;
                }
                else if (effectId.contains(QStringLiteral("parametric_eq"))) {
                    auto* eq = new ParametricEQ;
                    // Parse per-band parameters
                    for (int b = 0; b < ParametricEQ::NumBands; ++b) {
                        QString key = QStringLiteral("band_%1").arg(b);
                        if (!params.contains(key)) continue;
                        QVariantMap band = params.value(key).toMap();
                        int base = b * ParametricEQ::ParamsPerBand;
                        if (band.contains(QStringLiteral("freq")))
                            eq->setParameter(base + 0, band.value(QStringLiteral("freq")).toFloat());
                        if (band.contains(QStringLiteral("q")))
                            eq->setParameter(base + 1, band.value(QStringLiteral("q")).toFloat());
                        if (band.contains(QStringLiteral("gain_db")))
                            eq->setParameter(base + 2, band.value(QStringLiteral("gain_db")).toFloat());
                        if (band.contains(QStringLiteral("type"))) {
                            QString t = band.value(QStringLiteral("type")).toString();
                            float typeVal = 1.0f; // peaking
                            if (t == QStringLiteral("low_shelf"))  typeVal = 0.0f;
                            else if (t == QStringLiteral("peaking"))   typeVal = 1.0f;
                            else if (t == QStringLiteral("high_shelf")) typeVal = 2.0f;
                            else if (t == QStringLiteral("high_pass"))  typeVal = 3.0f;
                            else if (t == QStringLiteral("low_pass"))   typeVal = 4.0f;
                            eq->setParameter(base + 3, typeVal);
                        }
                    }
                    effect = eq;
                }
                else if (effectId.contains(QStringLiteral("compressor"))) {
                    auto* comp = new Compressor;
                    if (params.contains(QStringLiteral("threshold_db")))
                        comp->setParameter(Compressor::ThresholdDb,
                            params.value(QStringLiteral("threshold_db")).toFloat());
                    if (params.contains(QStringLiteral("ratio")))
                        comp->setParameter(Compressor::Ratio,
                            params.value(QStringLiteral("ratio")).toFloat());
                    if (params.contains(QStringLiteral("attack_ms")))
                        comp->setParameter(Compressor::AttackMs,
                            params.value(QStringLiteral("attack_ms")).toFloat());
                    if (params.contains(QStringLiteral("release_ms")))
                        comp->setParameter(Compressor::ReleaseMs,
                            params.value(QStringLiteral("release_ms")).toFloat());
                    if (params.contains(QStringLiteral("makeup_db")))
                        comp->setParameter(Compressor::MakeupDb,
                            params.value(QStringLiteral("makeup_db")).toFloat());
                    if (params.contains(QStringLiteral("knee_db")))
                        comp->setParameter(Compressor::KneeDb,
                            params.value(QStringLiteral("knee_db")).toFloat());
                    effect = comp;
                }
                else if (effectId.contains(QStringLiteral("limiter"))) {
                    auto* lim = new Limiter;
                    if (params.contains(QStringLiteral("ceiling_db")))
                        lim->setParameter(Limiter::CeilingDb,
                            params.value(QStringLiteral("ceiling_db")).toFloat());
                    if (params.contains(QStringLiteral("release_ms")))
                        lim->setParameter(Limiter::ReleaseMs,
                            params.value(QStringLiteral("release_ms")).toFloat());
                    if (params.contains(QStringLiteral("lookahead_ms")))
                        lim->setParameter(Limiter::LookaheadMs,
                            params.value(QStringLiteral("lookahead_ms")).toFloat());
                    effect = lim;
                }
                else if (effectId.contains(QStringLiteral("sonic_enhancer"))) {
                    auto* enh = new SonicEnhancer;
                    if (params.contains(QStringLiteral("low_contour")))
                        enh->setParameter(SonicEnhancer::LowContour,
                            params.value(QStringLiteral("low_contour")).toFloat());
                    if (params.contains(QStringLiteral("process_amount")))
                        enh->setParameter(SonicEnhancer::ProcessAmount,
                            params.value(QStringLiteral("process_amount")).toFloat());
                    if (params.contains(QStringLiteral("presence")))
                        enh->setParameter(SonicEnhancer::Presence,
                            params.value(QStringLiteral("presence")).toFloat());
                    effect = enh;
                }
                else if (effectId.contains(QStringLiteral("agc"))) {
                    auto* agc = new AGC;
                    if (params.contains(QStringLiteral("target_db")))
                        agc->setParameter(AGC::TargetDb,
                            params.value(QStringLiteral("target_db")).toFloat());
                    if (params.contains(QStringLiteral("attack_ms")))
                        agc->setParameter(AGC::AttackMs,
                            params.value(QStringLiteral("attack_ms")).toFloat());
                    if (params.contains(QStringLiteral("release_ms")))
                        agc->setParameter(AGC::ReleaseMs,
                            params.value(QStringLiteral("release_ms")).toFloat());
                    if (params.contains(QStringLiteral("max_gain_db")))
                        agc->setParameter(AGC::MaxGainDb,
                            params.value(QStringLiteral("max_gain_db")).toFloat());
                    effect = agc;
                }
                else if (effectId.contains(QStringLiteral("normalizer"))) {
                    auto* norm = new Normalizer;
                    if (params.contains(QStringLiteral("target_lufs")))
                        norm->setParameter(Normalizer::TargetLufs,
                            params.value(QStringLiteral("target_lufs")).toFloat());
                    effect = norm;
                }

                if (effect) {
                    effect->setBypassed(bypass);
                    chain.addEffect(effect);
                }
            }
        }

        // Process the entire buffer through the DSP chain in blocks
        if (chain.effectCount() > 0) {
            int pos = 0;
            while (pos < workFrames) {
                int blockFrames = qMin(kBatchBlockSize, workFrames - pos);
                float* blockPtr = workData + (pos * workChannels);
                chain.process(blockPtr, blockFrames, workChannels);
                pos += blockFrames;

                // Update progress (DSP: 10%..40%)
                int pct = 10 + static_cast<int>(
                    30.0 * pos / workFrames);
                if (pct != job.progressPercent) {
                    job.progressPercent = pct;
                    emit jobProgress(jobIndex, pct);
                    QApplication::processEvents();
                }

                if (m_cancelled.load(std::memory_order_acquire)) {
                    job.status = BatchJob::Cancelled;
                    job.errorMessage = tr("Cancelled");
                    return;
                }
            }
        }
    }

    job.progressPercent = 40;
    emit jobProgress(jobIndex, 40);
    QApplication::processEvents();

    // ── 3. Loudness normalization (if requested) ──────────────────────────
    if (std::fabs(job.targetLUFS) > 0.001f) {
        // Two-pass loudness normalization:
        //   Pass 1: Measure integrated loudness using Normalizer
        //   Pass 2: Apply makeup gain to reach target

        // Pass 1 — measurement
        Normalizer measurer;
        measurer.setParameter(Normalizer::TargetLufs, job.targetLUFS);

        int pos = 0;
        while (pos < workFrames) {
            int blockFrames = qMin(kBatchBlockSize, workFrames - pos);
            // Copy block to scratch so measurement doesn't modify the data
            std::vector<float> scratch(static_cast<size_t>(blockFrames * workChannels));
            std::memcpy(scratch.data(),
                        workData + (pos * workChannels),
                        scratch.size() * sizeof(float));
            measurer.process(scratch.data(), blockFrames, workChannels);
            pos += blockFrames;
        }

        float measuredLufs = measurer.currentLufs();

        // Pass 2 — apply gain
        if (measuredLufs > -120.0f) {
            float gainDb = job.targetLUFS - measuredLufs;
            float gainLin = std::pow(10.0f, gainDb / 20.0f);

            int totalSamples = workFrames * workChannels;
            for (int i = 0; i < totalSamples; ++i) {
                workData[i] *= gainLin;
            }

            qDebug() << "BatchEncoder: normalized"
                     << QFileInfo(job.inputPath).fileName()
                     << "from" << measuredLufs << "to" << job.targetLUFS
                     << "LUFS (gain:" << gainDb << "dB)";
        }
    }

    job.progressPercent = 60;
    emit jobProgress(jobIndex, 60);
    QApplication::processEvents();

    if (m_cancelled.load(std::memory_order_acquire)) {
        job.status = BatchJob::Cancelled;
        job.errorMessage = tr("Cancelled");
        return;
    }

    // ── 4. Sample rate conversion (if requested) ──────────────────────────
    int targetRate = (job.sampleRate > 0) ? job.sampleRate : workRate;

    if (targetRate != workRate) {
        AudioResampler resampler;

        // Calculate output frame count
        int outFrames = static_cast<int>(
            std::ceil(static_cast<double>(workFrames) * targetRate / workRate));
        resampledBuf.resize(static_cast<size_t>(outFrames * workChannels));

        int written = resampler.resample(
            workData, workFrames, workRate,
            resampledBuf.data(), outFrames, targetRate,
            workChannels);

        if (written > 0) {
            workData   = resampledBuf.data();
            workFrames = written;
            workRate   = targetRate;
        } else {
            qWarning() << "BatchEncoder: sample rate conversion failed for"
                       << job.inputPath;
        }
    }

    job.progressPercent = 70;
    emit jobProgress(jobIndex, 70);
    QApplication::processEvents();

    // ── 5. Channel conversion (if requested) ──────────────────────────────
    int targetChannels = (job.channels > 0) ? job.channels : workChannels;

    if (targetChannels != workChannels) {
        channelBuf.resize(static_cast<size_t>(workFrames * targetChannels));

        if (workChannels == 2 && targetChannels == 1) {
            // Stereo -> Mono: average L+R
            for (int f = 0; f < workFrames; ++f) {
                channelBuf[static_cast<size_t>(f)] =
                    (workData[f * 2] + workData[f * 2 + 1]) * 0.5f;
            }
        } else if (workChannels == 1 && targetChannels == 2) {
            // Mono -> Stereo: duplicate
            for (int f = 0; f < workFrames; ++f) {
                channelBuf[static_cast<size_t>(f * 2)]     = workData[f];
                channelBuf[static_cast<size_t>(f * 2 + 1)] = workData[f];
            }
        } else if (targetChannels < workChannels) {
            // Generic downmix: average all source channels
            for (int f = 0; f < workFrames; ++f) {
                for (int c = 0; c < targetChannels; ++c) {
                    float sum = 0.0f;
                    for (int sc = 0; sc < workChannels; ++sc) {
                        sum += workData[f * workChannels + sc];
                    }
                    channelBuf[static_cast<size_t>(f * targetChannels + c)] =
                        sum / workChannels;
                }
            }
        } else {
            // Generic upmix: copy existing channels, zero-fill extra
            std::memset(channelBuf.data(), 0,
                        channelBuf.size() * sizeof(float));
            for (int f = 0; f < workFrames; ++f) {
                for (int c = 0; c < workChannels; ++c) {
                    channelBuf[static_cast<size_t>(f * targetChannels + c)] =
                        workData[f * workChannels + c];
                }
            }
        }

        workData     = channelBuf.data();
        workChannels = targetChannels;
    }

    job.progressPercent = 75;
    emit jobProgress(jobIndex, 75);
    QApplication::processEvents();

    if (m_cancelled.load(std::memory_order_acquire)) {
        job.status = BatchJob::Cancelled;
        job.errorMessage = tr("Cancelled");
        return;
    }

    // ── 6. Clamp to [-1.0, 1.0] before encoding ──────────────────────────
    {
        int totalSamples = workFrames * workChannels;
        for (int i = 0; i < totalSamples; ++i) {
            workData[i] = std::clamp(workData[i], -1.0f, 1.0f);
        }
    }

    // ── 7. Encode to output format ────────────────────────────────────────
    AudioBuffer outBuf;
    outBuf.data       = workData;
    outBuf.frames     = workFrames;
    outBuf.channels   = workChannels;
    outBuf.sampleRate = workRate;

    // Ensure output directory exists
    QFileInfo outInfo(job.outputPath);
    QDir().mkpath(outInfo.absolutePath());

    QString ffCodec = resolveCodecName(job.outputCodec);
    int bitrate = job.bitrate;

    // Lossless codecs ignore bitrate
    QString cl = job.outputCodec.toLower();
    if (cl == QStringLiteral("flac") || cl == QStringLiteral("wav")) {
        bitrate = 0;
    }

    FFmpegCodec encoder;
    bool ok = encoder.encode(outBuf, job.outputPath, ffCodec, bitrate);

    if (!ok) {
        job.status = BatchJob::Failed;
        job.errorMessage = tr("Failed to encode: %1").arg(job.outputPath);
        emit jobFailed(jobIndex, job.errorMessage);
        return;
    }

    qDebug() << "BatchEncoder: encoded" << job.inputPath
             << "->" << job.outputPath
             << "(" << job.outputCodec << bitrate << "kbps)";

    // ── 8. Copy metadata tags to output file ─────────────────────────────
    if (job.copyTags && !sourceTags.isEmpty()
        && TagTransfer::isSupported(job.outputPath)) {
        if (!TagTransfer::writeTags(job.outputPath, sourceTags)) {
            qWarning() << "BatchEncoder: failed to write metadata tags to"
                       << job.outputPath;
            // Non-fatal — the audio was encoded successfully
        } else {
            qDebug() << "BatchEncoder: copied metadata tags to"
                     << job.outputPath;
        }
    }

    job.progressPercent = 100;
    emit jobProgress(jobIndex, 100);
}

} // namespace dawcast
