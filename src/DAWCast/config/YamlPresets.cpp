// Mcaster1DAWCast — Multi-Channel DAW for Broadcasting
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "YamlPresets.h"

#include <QDir>
#include <QFile>

// libyaml
#include <yaml.h>

namespace dawcast::config {

QVariantMap YamlPresets::loadPreset(const QString& yamlPath)
{
    QVariantMap result;

    QFile file(yamlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }

    QByteArray data = file.readAll();

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser)) {
        return result;
    }

    yaml_parser_set_input_string(&parser,
        reinterpret_cast<const unsigned char*>(data.constData()),
        static_cast<size_t>(data.size()));

    // Walk YAML events to build a flat or one-level-nested QVariantMap.
    // Supports top-level scalars and one level of nested mapping (e.g. "colors:" section).
    yaml_event_t event;
    bool done = false;

    // State machine for parsing
    enum class State { ExpectKey, ExpectValue, ExpectNestedKey, ExpectNestedValue };
    State state = State::ExpectKey;
    QString currentKey;
    QString nestedMapKey;
    QVariantMap nestedMap;
    int mappingDepth = 0;
    bool inNestedMap = false;

    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            break;
        }

        switch (event.type) {
        case YAML_STREAM_END_EVENT:
            done = true;
            break;

        case YAML_MAPPING_START_EVENT:
            if (mappingDepth == 0) {
                // Root mapping
                mappingDepth = 1;
                state = State::ExpectKey;
            } else if (mappingDepth == 1 && state == State::ExpectValue) {
                // Nested mapping (e.g. "colors:")
                mappingDepth = 2;
                inNestedMap = true;
                nestedMapKey = currentKey;
                nestedMap.clear();
                state = State::ExpectNestedKey;
            }
            break;

        case YAML_MAPPING_END_EVENT:
            if (mappingDepth == 2 && inNestedMap) {
                // End of nested mapping — store as sub-map in result
                result.insert(nestedMapKey, nestedMap);
                inNestedMap = false;
                mappingDepth = 1;
                state = State::ExpectKey;
            } else if (mappingDepth == 1) {
                mappingDepth = 0;
            }
            break;

        case YAML_SCALAR_EVENT: {
            QString value = QString::fromUtf8(
                reinterpret_cast<const char*>(event.data.scalar.value),
                static_cast<int>(event.data.scalar.length));

            if (inNestedMap) {
                if (state == State::ExpectNestedKey) {
                    currentKey = value;
                    state = State::ExpectNestedValue;
                } else if (state == State::ExpectNestedValue) {
                    nestedMap.insert(currentKey, value);
                    state = State::ExpectNestedKey;
                }
            } else if (mappingDepth == 1) {
                if (state == State::ExpectKey) {
                    currentKey = value;
                    state = State::ExpectValue;
                } else if (state == State::ExpectValue) {
                    result.insert(currentKey, value);
                    state = State::ExpectKey;
                }
            }
            break;
        }

        default:
            break;
        }

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);

    return result;
}

// Helper to emit a scalar event via libyaml
static bool emitScalar(yaml_emitter_t* emitter, const QByteArray& value)
{
    yaml_event_t event;
    yaml_scalar_event_initialize(&event,
        nullptr,
        nullptr,
        reinterpret_cast<yaml_char_t*>(const_cast<char*>(value.constData())),
        static_cast<int>(value.size()),
        1, 1, YAML_ANY_SCALAR_STYLE);
    return yaml_emitter_emit(emitter, &event) != 0;
}

bool YamlPresets::savePreset(const QString& yamlPath, const QVariantMap& params)
{
    QFile file(yamlPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    yaml_emitter_t emitter;
    if (!yaml_emitter_initialize(&emitter)) {
        return false;
    }

    // Write callback that appends to a QByteArray
    QByteArray output;
    yaml_emitter_set_output(&emitter,
        [](void* data, unsigned char* buffer, size_t size) -> int {
            auto* out = static_cast<QByteArray*>(data);
            out->append(reinterpret_cast<const char*>(buffer), static_cast<int>(size));
            return 1;
        },
        &output);

    yaml_emitter_set_unicode(&emitter, 1);

    yaml_event_t event;

    // Stream start
    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    // Document start
    yaml_document_start_event_initialize(&event, nullptr, nullptr, nullptr, 0);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    // Root mapping start
    yaml_mapping_start_event_initialize(&event, nullptr, nullptr, 1, YAML_BLOCK_MAPPING_STYLE);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    // Emit each key-value pair
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        QByteArray keyBytes = it.key().toUtf8();
        if (!emitScalar(&emitter, keyBytes)) goto error;

        if (it.value().typeId() == QMetaType::QVariantMap) {
            // Nested mapping (e.g. "colors" section)
            QVariantMap nested = it.value().toMap();
            yaml_mapping_start_event_initialize(&event, nullptr, nullptr, 1, YAML_BLOCK_MAPPING_STYLE);
            if (!yaml_emitter_emit(&emitter, &event)) goto error;

            for (auto nit = nested.constBegin(); nit != nested.constEnd(); ++nit) {
                QByteArray nKey = nit.key().toUtf8();
                QByteArray nVal = nit.value().toString().toUtf8();
                if (!emitScalar(&emitter, nKey)) goto error;
                if (!emitScalar(&emitter, nVal)) goto error;
            }

            yaml_mapping_end_event_initialize(&event);
            if (!yaml_emitter_emit(&emitter, &event)) goto error;
        } else {
            QByteArray valBytes = it.value().toString().toUtf8();
            if (!emitScalar(&emitter, valBytes)) goto error;
        }
    }

    // Root mapping end
    yaml_mapping_end_event_initialize(&event);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    // Document end
    yaml_document_end_event_initialize(&event, 0);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    // Stream end
    yaml_stream_end_event_initialize(&event);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    yaml_emitter_delete(&emitter);

    file.write(output);
    file.close();
    return true;

error:
    yaml_emitter_delete(&emitter);
    file.close();
    return false;
}

QStringList YamlPresets::listPresets(const QString& directory)
{
    QStringList presets;
    QDir dir(directory);
    if (!dir.exists()) return presets;

    const auto entries = dir.entryList(
        {QStringLiteral("*.yaml"), QStringLiteral("*.yml")},
        QDir::Files, QDir::Name);

    for (const auto& entry : entries) {
        presets.append(dir.filePath(entry));
    }

    return presets;
}

} // namespace dawcast::config
