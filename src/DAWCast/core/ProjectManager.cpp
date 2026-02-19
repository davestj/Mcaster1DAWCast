// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ProjectManager.h"

#include "../timeline/Timeline.h"
#include "../timeline/AudioTrack.h"
#include "../timeline/VideoTrack.h"
#include "../timeline/Clip.h"
#include "../timeline/Automation.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>

namespace dawcast {

static constexpr const char* kFileVersion = "1.0.0-alpha";

ProjectManager::ProjectManager(QObject* parent)
    : QObject(parent)
{
}

ProjectManager::~ProjectManager() = default;

// ---------------------------------------------------------------------------
// newProject — reset to defaults (optionally from default_project.json)
// ---------------------------------------------------------------------------
void ProjectManager::newProject()
{
    m_projectPath.clear();
    m_projectName = QStringLiteral("Untitled Project");
    m_author.clear();
    m_sampleRate = 48000;
    m_modified = false;

    // Clear the timeline if we have one
    if (m_timeline) {
        while (m_timeline->trackCount() > 0) {
            m_timeline->removeTrack(0);
        }
        m_timeline->setPlayhead(0);
        m_timeline->setSampleRate(m_sampleRate);
    }

    // Try loading defaults from configs/default_project.json
    // Look relative to the application directory, then fallback
    QString defaultPath = QCoreApplication::applicationDirPath()
                          + QStringLiteral("/../configs/default_project.json");
    QFile defaultFile(defaultPath);
    if (defaultFile.exists() && defaultFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(defaultFile.readAll());
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject root = doc.object();
            QJsonObject proj = root.value(QStringLiteral("project")).toObject();
            m_projectName = proj.value(QStringLiteral("name")).toString(m_projectName);
            m_sampleRate  = proj.value(QStringLiteral("sample_rate")).toInt(m_sampleRate);
            if (m_timeline) {
                m_timeline->setSampleRate(m_sampleRate);
            }
        }
    }

    emit projectChanged();
}

// ---------------------------------------------------------------------------
// openProject — read JSON, reconstruct timeline state
// ---------------------------------------------------------------------------
bool ProjectManager::openProject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();

    // Read project metadata
    QJsonObject proj = root.value(QStringLiteral("project")).toObject();
    m_projectName = proj.value(QStringLiteral("name")).toString(QStringLiteral("Untitled Project"));
    m_sampleRate  = proj.value(QStringLiteral("sample_rate")).toInt(48000);
    m_author      = proj.value(QStringLiteral("author")).toString();

    // Reconstruct timeline
    if (m_timeline) {
        QJsonObject tl = root.value(QStringLiteral("timeline")).toObject();
        deserializeTimeline(tl);
    }

    m_projectPath = path;
    m_modified = false;
    emit projectChanged();
    return true;
}

// ---------------------------------------------------------------------------
// saveProject / saveProjectAs — serialize all state to JSON
// ---------------------------------------------------------------------------
bool ProjectManager::saveProject()
{
    if (m_projectPath.isEmpty()) {
        return false;
    }
    return saveProjectAs(m_projectPath);
}

bool ProjectManager::saveProjectAs(const QString& path)
{
    QJsonObject root;
    root[QStringLiteral("dawcast_version")] = QString::fromUtf8(kFileVersion);
    root[QStringLiteral("app")] = QStringLiteral("Mcaster1DAWCast");

    // Project metadata
    QJsonObject proj;
    proj[QStringLiteral("name")]        = m_projectName;
    proj[QStringLiteral("author")]      = m_author;
    proj[QStringLiteral("sample_rate")] = m_sampleRate;
    proj[QStringLiteral("bit_depth")]   = 32;
    proj[QStringLiteral("channels")]    = 2;
    proj[QStringLiteral("created")]     = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    proj[QStringLiteral("modified")]    = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    root[QStringLiteral("project")] = proj;

    // Timeline
    if (m_timeline) {
        root[QStringLiteral("timeline")] = serializeTimeline();
    }

    // Master bus defaults
    QJsonObject master;
    master[QStringLiteral("volume_db")] = 0.0;
    master[QStringLiteral("effects")]   = QJsonArray();
    root[QStringLiteral("master")] = master;

    QJsonDocument doc(root);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    m_projectPath = path;
    m_modified = false;
    emit projectSaved();
    return true;
}

// ---------------------------------------------------------------------------
// serializeTimeline — convert timeline to JSON
// ---------------------------------------------------------------------------
QJsonObject ProjectManager::serializeTimeline() const
{
    QJsonObject tl;

    if (!m_timeline) return tl;

    tl[QStringLiteral("playhead_samples")]  = static_cast<qint64>(m_timeline->playhead());
    tl[QStringLiteral("duration_samples")]  = static_cast<qint64>(m_timeline->duration());
    tl[QStringLiteral("sample_rate")]       = m_timeline->sampleRate();

    // Serialize audio tracks
    QJsonArray audioTracks;
    QJsonArray videoTracks;

    for (int i = 0; i < m_timeline->trackCount(); ++i) {
        QObject* trackObj = m_timeline->track(i);
        if (!trackObj) continue;

        // Try AudioTrack
        auto* at = qobject_cast<AudioTrack*>(trackObj);
        if (at) {
            QJsonObject trackJson;
            trackJson[QStringLiteral("name")]       = at->name();
            trackJson[QStringLiteral("volume_db")]  = static_cast<double>(at->volumeDb());
            trackJson[QStringLiteral("pan")]        = static_cast<double>(at->pan());
            trackJson[QStringLiteral("muted")]      = at->isMuted();
            trackJson[QStringLiteral("solo")]       = at->isSolo();
            trackJson[QStringLiteral("record_armed")] = at->isRecordArmed();

            // Serialize clips
            QJsonArray clips;
            for (int c = 0; c < at->clipCount(); ++c) {
                const Clip* clip = at->clip(c);
                if (!clip) continue;

                QJsonObject clipJson;
                clipJson[QStringLiteral("source_path")]       = clip->sourcePath();
                clipJson[QStringLiteral("source_in")]         = static_cast<qint64>(clip->sourceIn());
                clipJson[QStringLiteral("source_out")]        = static_cast<qint64>(clip->sourceOut());
                clipJson[QStringLiteral("timeline_position")] = static_cast<qint64>(clip->timelinePosition());
                clipJson[QStringLiteral("gain")]              = static_cast<double>(clip->gain());
                clipJson[QStringLiteral("fade_in")]           = static_cast<qint64>(clip->fadeIn());
                clipJson[QStringLiteral("fade_out")]          = static_cast<qint64>(clip->fadeOut());

                // Serialize clip gain envelope
                const auto& envelope = clip->gainEnvelope();
                if (!envelope.isEmpty()) {
                    QJsonArray envArray;
                    for (const auto& pt : envelope) {
                        QJsonObject ptJson;
                        ptJson[QStringLiteral("offset")]  = static_cast<qint64>(pt.offsetSamples);
                        ptJson[QStringLiteral("gain_db")] = static_cast<double>(pt.gainDb);
                        envArray.append(ptJson);
                    }
                    clipJson[QStringLiteral("gain_envelope")] = envArray;
                }

                clips.append(clipJson);
            }
            trackJson[QStringLiteral("clips")] = clips;
            audioTracks.append(trackJson);
            continue;
        }

        // Try VideoTrack
        auto* vt = qobject_cast<VideoTrack*>(trackObj);
        if (vt) {
            QJsonObject trackJson;
            trackJson[QStringLiteral("name")]    = vt->name();
            trackJson[QStringLiteral("visible")] = vt->isVisible();
            trackJson[QStringLiteral("opacity")] = static_cast<double>(vt->opacity());
            trackJson[QStringLiteral("muted")]   = vt->isMuted();
            trackJson[QStringLiteral("solo")]    = vt->isSolo();

            QJsonArray clips;
            for (int c = 0; c < vt->clipCount(); ++c) {
                const Clip* clip = vt->clip(c);
                if (!clip) continue;

                QJsonObject clipJson;
                clipJson[QStringLiteral("source_path")]       = clip->sourcePath();
                clipJson[QStringLiteral("source_in")]         = static_cast<qint64>(clip->sourceIn());
                clipJson[QStringLiteral("source_out")]        = static_cast<qint64>(clip->sourceOut());
                clipJson[QStringLiteral("timeline_position")] = static_cast<qint64>(clip->timelinePosition());
                clipJson[QStringLiteral("gain")]              = static_cast<double>(clip->gain());
                clipJson[QStringLiteral("fade_in")]           = static_cast<qint64>(clip->fadeIn());
                clipJson[QStringLiteral("fade_out")]          = static_cast<qint64>(clip->fadeOut());

                // Serialize clip gain envelope
                const auto& envelope = clip->gainEnvelope();
                if (!envelope.isEmpty()) {
                    QJsonArray envArray;
                    for (const auto& pt : envelope) {
                        QJsonObject ptJson;
                        ptJson[QStringLiteral("offset")]  = static_cast<qint64>(pt.offsetSamples);
                        ptJson[QStringLiteral("gain_db")] = static_cast<double>(pt.gainDb);
                        envArray.append(ptJson);
                    }
                    clipJson[QStringLiteral("gain_envelope")] = envArray;
                }

                clips.append(clipJson);
            }
            trackJson[QStringLiteral("clips")] = clips;
            videoTracks.append(trackJson);
        }
    }

    tl[QStringLiteral("audio_tracks")] = audioTracks;
    tl[QStringLiteral("video_tracks")] = videoTracks;
    tl[QStringLiteral("markers")]      = QJsonArray(); // Markers serialized separately if needed

    return tl;
}

// ---------------------------------------------------------------------------
// deserializeTimeline — rebuild timeline from JSON
// ---------------------------------------------------------------------------
bool ProjectManager::deserializeTimeline(const QJsonObject& obj)
{
    if (!m_timeline) return false;

    // Clear existing tracks
    while (m_timeline->trackCount() > 0) {
        m_timeline->removeTrack(0);
    }

    int sampleRate = obj.value(QStringLiteral("sample_rate")).toInt(48000);
    m_timeline->setSampleRate(sampleRate);

    // Rebuild audio tracks
    QJsonArray audioTracks = obj.value(QStringLiteral("audio_tracks")).toArray();
    for (const auto& trackVal : audioTracks) {
        QJsonObject trackJson = trackVal.toObject();
        AudioTrack* track = m_timeline->addAudioTrack();

        track->setName(trackJson.value(QStringLiteral("name")).toString());
        track->setVolume(static_cast<float>(trackJson.value(QStringLiteral("volume_db")).toDouble(0.0)));
        track->setPan(static_cast<float>(trackJson.value(QStringLiteral("pan")).toDouble(0.0)));
        track->setMuted(trackJson.value(QStringLiteral("muted")).toBool(false));
        track->setSolo(trackJson.value(QStringLiteral("solo")).toBool(false));
        track->setRecordArmed(trackJson.value(QStringLiteral("record_armed")).toBool(false));

        QJsonArray clips = trackJson.value(QStringLiteral("clips")).toArray();
        for (const auto& clipVal : clips) {
            QJsonObject clipJson = clipVal.toObject();
            auto* clip = new Clip();

            clip->setSourcePath(clipJson.value(QStringLiteral("source_path")).toString());
            clip->setSourceIn(static_cast<int64_t>(clipJson.value(QStringLiteral("source_in")).toDouble(0)));
            clip->setSourceOut(static_cast<int64_t>(clipJson.value(QStringLiteral("source_out")).toDouble(0)));
            clip->setTimelinePosition(static_cast<int64_t>(clipJson.value(QStringLiteral("timeline_position")).toDouble(0)));
            clip->setGain(static_cast<float>(clipJson.value(QStringLiteral("gain")).toDouble(1.0)));
            clip->setFadeIn(static_cast<int64_t>(clipJson.value(QStringLiteral("fade_in")).toDouble(0)));
            clip->setFadeOut(static_cast<int64_t>(clipJson.value(QStringLiteral("fade_out")).toDouble(0)));

            // Deserialize clip gain envelope
            QJsonArray envArray = clipJson.value(QStringLiteral("gain_envelope")).toArray();
            if (!envArray.isEmpty()) {
                QList<GainPoint> envelope;
                for (const auto& ptVal : envArray) {
                    QJsonObject ptJson = ptVal.toObject();
                    GainPoint pt;
                    pt.offsetSamples = static_cast<int64_t>(ptJson.value(QStringLiteral("offset")).toDouble(0));
                    pt.gainDb = static_cast<float>(ptJson.value(QStringLiteral("gain_db")).toDouble(0.0));
                    envelope.append(pt);
                }
                clip->setGainEnvelope(envelope);
            }

            track->addClip(clip);
        }
    }

    // Rebuild video tracks
    QJsonArray videoTracks = obj.value(QStringLiteral("video_tracks")).toArray();
    for (const auto& trackVal : videoTracks) {
        QJsonObject trackJson = trackVal.toObject();
        VideoTrack* track = m_timeline->addVideoTrack();

        track->setName(trackJson.value(QStringLiteral("name")).toString());
        track->setVisible(trackJson.value(QStringLiteral("visible")).toBool(true));
        track->setOpacity(static_cast<float>(trackJson.value(QStringLiteral("opacity")).toDouble(1.0)));
        track->setMuted(trackJson.value(QStringLiteral("muted")).toBool(false));
        track->setSolo(trackJson.value(QStringLiteral("solo")).toBool(false));

        QJsonArray clips = trackJson.value(QStringLiteral("clips")).toArray();
        for (const auto& clipVal : clips) {
            QJsonObject clipJson = clipVal.toObject();
            auto* clip = new Clip();

            clip->setSourcePath(clipJson.value(QStringLiteral("source_path")).toString());
            clip->setSourceIn(static_cast<int64_t>(clipJson.value(QStringLiteral("source_in")).toDouble(0)));
            clip->setSourceOut(static_cast<int64_t>(clipJson.value(QStringLiteral("source_out")).toDouble(0)));
            clip->setTimelinePosition(static_cast<int64_t>(clipJson.value(QStringLiteral("timeline_position")).toDouble(0)));
            clip->setGain(static_cast<float>(clipJson.value(QStringLiteral("gain")).toDouble(1.0)));
            clip->setFadeIn(static_cast<int64_t>(clipJson.value(QStringLiteral("fade_in")).toDouble(0)));
            clip->setFadeOut(static_cast<int64_t>(clipJson.value(QStringLiteral("fade_out")).toDouble(0)));

            // Deserialize clip gain envelope
            QJsonArray envArray = clipJson.value(QStringLiteral("gain_envelope")).toArray();
            if (!envArray.isEmpty()) {
                QList<GainPoint> envelope;
                for (const auto& ptVal : envArray) {
                    QJsonObject ptJson = ptVal.toObject();
                    GainPoint pt;
                    pt.offsetSamples = static_cast<int64_t>(ptJson.value(QStringLiteral("offset")).toDouble(0));
                    pt.gainDb = static_cast<float>(ptJson.value(QStringLiteral("gain_db")).toDouble(0.0));
                    envelope.append(pt);
                }
                clip->setGainEnvelope(envelope);
            }

            track->addClip(clip);
        }
    }

    // Restore playhead position
    int64_t playhead = static_cast<int64_t>(
        obj.value(QStringLiteral("playhead_samples")).toDouble(0));
    m_timeline->setPlayhead(playhead);

    return true;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------
QString ProjectManager::projectPath() const
{
    return m_projectPath;
}

bool ProjectManager::isModified() const
{
    return m_modified;
}

void ProjectManager::markModified()
{
    m_modified = true;
}

} // namespace dawcast
