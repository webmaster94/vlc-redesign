/* Copyright 2026 webmaster94. GPL-3.0-or-later.
 * Shared profiles are data, never interface, scripting, network or filesystem configuration. */
#include "redesign_profile.hpp"
#include <QSet>
#include <QColor>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
namespace Reframe
{
static bool number(const QVariant &v, double low, double high)
{
    if (v.type() != QVariant::Double && v.type() != QVariant::Int && v.type() != QVariant::UInt &&
        v.type() != QVariant::LongLong && v.type() != QVariant::ULongLong)
        return false;
    const double x = v.toDouble();
    return std::isfinite(x) && x >= low && x <= high;
}
static bool boolean(const QVariant &v)
{
    return v.type() == QVariant::Bool || (number(v, 0, 1) && std::floor(v.toDouble()) == v.toDouble());
}
static bool text(const QVariant &v, int maximum = 128)
{
    if (v.type() != QVariant::String || v.toString().size() > maximum)
        return false;
    for (const auto c : v.toString())
        if (c.unicode() < 32 || c.unicode() == 127)
            return false;
    return true;
}
bool profileValueSafe(const QString &key, const QVariant &v)
{
    static const QSet<QString> booleans = {"ui:compactOnPlayback",
                                           "ui:seekThumbnails",
                                           "ui:waveform",
                                           "ui:subtitleTimeline",
                                           "ui:pauseOtherAudio",
                                           "ui:reverseScroll",
                                           "ui:nightMode",
                                           "ui:rememberWindow",
                                           "ui:checkUpdates",
                                           "audio",
                                           "video",
                                           "spu",
                                           "equalizer-2pass",
                                           "audio-time-stretch",
                                           "volume-save",
                                           "freetype-bold",
                                           "ass-use-embedded-style",
                                           "video-title-show",
                                           "osd",
                                           "spatialaudio-headphones"};
    if (booleans.contains(key))
        return boolean(v);
    if (key == "ui:theme")
        return text(v) && QStringList({"Dark", "Light", "System"}).contains(v.toString());
    if (key == "ui:accent")
        return text(v, 7) && QRegularExpression("^#[0-9a-fA-F]{6}$").match(v.toString()).hasMatch();
    if (key == "ui:autoHideControls")
        return text(v) && QStringList({"Never", "Always", "In fullscreen only"}).contains(v.toString());
    if (key == "ui:singleClick" || key == "ui:doubleClick")
        return text(v) && QStringList({"None", "Play / Pause", "Toggle fullscreen"}).contains(v.toString());
    if (key == "ui:startupVolume")
        return number(v, 0, 200);
    if (key == "ui:notificationDuration")
        return number(v, .5, 5);
    if (QStringList({"ui:toolbar", "ui:toolbar2", "ui:leftControls", "ui:topControls"}).contains(key))
    {
        if (v.type() != QVariant::StringList && v.type() != QVariant::List)
            return false;
        const QVariantList list =
            v.type() == QVariant::StringList ? QVariant(v.toStringList()).toList() : v.toList();
        const auto entries = v.toStringList();
        if (entries.size() > 64 || (v.type() == QVariant::List && list.size() != entries.size()))
            return false;
        static const QSet<QString> actions = {
            "previous", "play",       "next",         "stop",       "elapsed",    "seek",
            "duration", "volume",     "volumeSlider", "audio",      "subtitles",  "settings",
            "playlist", "fullscreen", "record",       "snapshot",   "repeat",     "shuffle",
            "frame",    "reverse",    "mini",         "hdr",        "zoom",       "back10",
            "back15",   "forward10",  "forward15",    "home",       "playlists",  "quit",
            "chapters", "bookmarks",  "preferences",  "properties", "favorite",   "spacer",
            "divider",  "thumbnail",  "fileName",     "artistName", "folderName", "playlistName"};
        for (const auto &entry : entries)
            if (!actions.contains(entry))
                return false;
        if (v.type() == QVariant::List)
            for (const auto &entry : v.toList())
                if (entry.type() != QVariant::String)
                    return false;
        return true;
    }
    if (key == "ordinal:audio-track" || key == "ordinal:sub-track")
        return number(v, 0, 512) && std::floor(v.toDouble()) == v.toDouble();
    if (key == "equalizer-bands")
    {
        if (!text(v, 256))
            return false;
        const auto bands = v.toString().split(' ', QString::SkipEmptyParts);
        if (bands.size() != 10)
            return false;
        for (const auto &band : bands)
        {
            bool ok;
            const double x = band.toDouble(&ok);
            if (!ok || !std::isfinite(x) || x < -20 || x > 20)
                return false;
        }
        return true;
    }
    if (key == "audio-filter" || key == "video-filter")
    {
        if (!text(v, 256))
            return false;
        const QSet<QString> audio = {"equalizer",    "normvol",        "spatializer",
                                     "stereo_widen", "compressor",     "scaletempo_pitch",
                                     "scaletempo",   "chorus_flanger", "karaoke"};
        const QSet<QString> video = {
            "transform",  "adjust",       "crop",      "croppadd", "sepia",  "sharpen", "grain",  "gradient",
            "motionblur", "motiondetect", "posterize", "mirror",   "invert", "wave",    "ripple", "magnify"};
        const auto parts = v.toString().split(':', QString::SkipEmptyParts);
        if (parts.size() > 16)
            return false;
        for (const auto &part : parts)
            if (!(key == "audio-filter" ? audio : video).contains(part))
                return false;
        return true;
    }
    if (key == "freetype-font" || key == "freetype-monofont")
        return text(v) && !v.toString().contains(QRegularExpression("[\\\\/:]"));
    if (key == "audio-language" || key == "sub-language")
        return text(v, 256) && QRegularExpression("^[A-Za-z0-9,; _-]*$").match(v.toString()).hasMatch();
    if (key == "equalizer-preset")
        return text(v, 64) && QRegularExpression("^[A-Za-z0-9 _-]*$").match(v.toString()).hasMatch();
    if (key == "transform-type")
        return text(v) && QStringList({"90", "180", "270", "hflip", "vflip", "transpose", "antitranspose"})
                              .contains(v.toString());
    if (key == "audio-replay-gain-mode")
        return text(v) && QStringList({"none", "track", "album"}).contains(v.toString());
    static const QSet<QString> numeric = {"equalizer-preamp",
                                          "rate",
                                          "audio-desync",
                                          "audio-track",
                                          "sub-track",
                                          "sub-delay",
                                          "sub-fps",
                                          "sub-text-scale",
                                          "subsdelay-factor",
                                          "freetype-color",
                                          "freetype-opacity",
                                          "freetype-rel-fontsize",
                                          "freetype-fontsize",
                                          "freetype-outline-thickness",
                                          "freetype-outline-color",
                                          "freetype-shadow-color",
                                          "freetype-shadow-opacity",
                                          "freetype-background-color",
                                          "freetype-background-opacity",
                                          "sub-margin",
                                          "video-title-timeout",
                                          "video-title-position",
                                          "osd-timeout",
                                          "stereo-mode",
                                          "pitch-shift",
                                          "spatializer-roomsize",
                                          "spatializer-width",
                                          "spatializer-wet",
                                          "spatializer-dry",
                                          "spatializer-damp",
                                          "stereowiden-delay",
                                          "stereowiden-feedback",
                                          "stereowiden-crossfeed",
                                          "stereowiden-dry-mix",
                                          "compressor-rms-peak",
                                          "compressor-attack",
                                          "compressor-release",
                                          "compressor-threshold",
                                          "compressor-ratio",
                                          "compressor-knee",
                                          "compressor-makeup-gain",
                                          "norm-max-level",
                                          "norm-buff-size",
                                          "delay-time",
                                          "sweep-depth",
                                          "sweep-rate",
                                          "feedback-gain",
                                          "wet-mix",
                                          "dry-mix",
                                          "contrast",
                                          "brightness",
                                          "hue",
                                          "saturation",
                                          "gamma",
                                          "sharpen-sigma",
                                          "grain-variance",
                                          "sepia-intensity",
                                          "crop-top",
                                          "crop-bottom",
                                          "crop-left",
                                          "crop-right"};
    if (!numeric.contains(key))
        return false;
    if (key.endsWith("color"))
        return number(v, 0, 0xffffff);
    if (key == "sub-delay")
        return number(v, -1000000000, 1000000000);
    if (key == "equalizer-preamp")
        return number(v, -20, 20);
    if (key == "rate")
        return number(v, .25, 4);
    if (key == "sub-fps")
        return number(v, 0, 240);
    if (key.contains("fontsize"))
        return number(v, 0, 512);
    if (key == "norm-buff-size")
        return number(v, 1, 4096);
    return number(v, -10000, 10000);
}
QVariantMap portableProfile(const QVariantMap &values)
{
    QVariantMap result;
    if (values.size() > 512)
        return result;
    for (auto it = values.begin(); it != values.end(); ++it)
        if (profileValueSafe(it.key(), it.value()))
            result[it.key()] = it.value();
    return result;
}
bool parseProfile(const QByteArray &bytes, QVariantMap *result)
{
    if (bytes.size() > 256 * 1024)
        return false;
    int depth = 0;
    bool quoted = false, escaped = false;
    for (char c : bytes)
    {
        if (quoted)
        {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                quoted = false;
        }
        else if (c == '"')
            quoted = true;
        else if (c == '{' || c == '[')
        {
            if (++depth > 8)
                return false;
        }
        else if (c == '}' || c == ']')
        {
            if (--depth < 0)
                return false;
        }
    }
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    const auto values = doc.object().toVariantMap();
    if (values.size() > 512 || portableProfile(values).size() != values.size())
        return false;
    *result = values;
    return true;
}
} // namespace Reframe
