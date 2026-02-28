
#import <CxxTagLibBridge/AudioMetadata.hpp>
#import <unordered_map>
#import <cstring>
#import <string>
#import <string_view>

// MARK: - Keys
namespace MetadataKey {
    struct APE final {
        static constexpr const char* title = "TITLE";
        static constexpr const char* album = "ALBUM";
        static constexpr const char* artist = "ARTIST";
        static constexpr const char* genre = "GENRE";
        static constexpr const char* releaseDate = "DATE";
        static constexpr const char* comment = "DESCRIPTION";
        static constexpr const char* trackNumber = "TRACKNUMBER";
        static constexpr const char* trackTotal = "TRACKTOTAL";
        static constexpr const char* discNumber = "DISCNUMBER";
        static constexpr const char* discTotal = "DISCTOTAL";
        static constexpr const char* composer = "COMPOSER";
        static constexpr const char* albumArtist = "ALBUMARTIST";
        static constexpr const char* bpm = "BPM";
        static constexpr const char* rating = "RATING";
        static constexpr const char* lyrics = "LYRICS";
        static constexpr const char* compilation = "COMPILATION";
        static constexpr const char* isrc = "ISRC";
        static constexpr const char* mcn = "MCN";
        static constexpr const char* musicBrainzReleaseID = "MUSICBRAINZ_ALBUMID";
        static constexpr const char* musicBrainzRecordingID = "MUSICBRAINZ_TRACKID";
        static constexpr const char* coverArtFront = "COVER ART (FRONT)";
        static constexpr const char* coverArtBack = "COVER ART (BACK)";

        using StringProperty = std::optional<std::string> AudioMetadata::*;
        static std::unordered_map<std::string, StringProperty, std::hash<std::string>> stringPropertiesByKeys() {
            return {
                {APE::title, &AudioMetadata::title},
                {APE::album, &AudioMetadata::albumTitle},
                {APE::artist, &AudioMetadata::artist},
                {APE::genre, &AudioMetadata::genre},
                {APE::comment, &AudioMetadata::comment},
                {APE::releaseDate, &AudioMetadata::releaseDate},
                {APE::composer, &AudioMetadata::composer},
                {APE::albumArtist, &AudioMetadata::albumArtist},
                {APE::lyrics, &AudioMetadata::lyrics},
                {APE::isrc, &AudioMetadata::internationalStandardRecordingCode},
                {APE::mcn, &AudioMetadata::mediaCatalogNumber},
                {APE::musicBrainzReleaseID, &AudioMetadata::musicBrainzReleaseID},
                {APE::musicBrainzRecordingID, &AudioMetadata::musicBrainzRecordingID},
            };
        }

        using IntProperty = std::optional<int> AudioMetadata::*;
        static std::unordered_map<std::string, IntProperty, std::hash<std::string>> intPropertiesByKeys() {
            return {
                {APE::trackNumber, &AudioMetadata::trackNumber},
                {APE::trackTotal, &AudioMetadata::trackTotal},
                {APE::discNumber, &AudioMetadata::discNumber},
                {APE::discTotal, &AudioMetadata::discTotal},
                {APE::bpm, &AudioMetadata::beatPerMinute},
                {APE::rating, &AudioMetadata::rating},
            };
        }
    };
}

// MARK: - Read
/// constructor for `AudioMetadata` from `TagLib::APE::Tag`.
AudioMetadata AudioMetadata::read_from_APE_tag(const TagLib::APE::Tag *tag, const MetadataReadingOptions options) {
    auto metadata = AudioMetadata();
    if (tag->isEmpty()) {
        return metadata;
    }
    metadata.tagSource |= TagSource::APE;
    auto additionalMetadata = AudioMetadata::AdditionalMetadata();
    using Key = MetadataKey::APE;

    auto none_if_empty = [](const char* string) -> std::optional<std::string> {
        return (!std::string_view(string).empty()) ? std::optional<std::string>(string) : std::nullopt;
    };

    auto none_if_zero = [](int number) -> std::optional<int> {
        return number != 0 ? std::optional<int>(number) : std::nullopt;
    };

    auto string_to_optional_bool = [](char const * value) -> std::optional<bool> {
        auto lookupValue = std::string(value);
        std::transform(lookupValue.begin(), lookupValue.end(), lookupValue.begin(), ::tolower);
        if (lookupValue == "true") {
            return true;
        } else if (lookupValue == "false") {
            return false;
        } else {
            int number = std::atoi(value);
            if (number == 0) {
                return false;
            } else if (number == 1) {
                return true;
            } else {
                return std::nullopt;
            }
        }
    };

    auto stringPropertiesByKeys = MetadataKey::APE::stringPropertiesByKeys();
    auto intPropertiesByKeys = MetadataKey::APE::intPropertiesByKeys();

    for (auto iterator: tag->itemListMap()) {
        auto item = iterator.second;
        if (item.isEmpty()) { continue; }
        auto key = item.key().toCString(true);
        auto value = item.toString().toCString(true);

        if (TagLib::APE::Item::Text == item.type()) {
            bool hasHandledValue = false;
            auto lookupKey = std::string(key);
            std::transform(lookupKey.begin(), lookupKey.end(), lookupKey.begin(), ::toupper);

            auto stringPropertyIterator = stringPropertiesByKeys.find(lookupKey);
            if (stringPropertyIterator != stringPropertiesByKeys.end()) {
                auto memberPointer = stringPropertyIterator->second; /// maybe `auto&`
                metadata.*memberPointer = none_if_empty(value);
                hasHandledValue = true;
            }
            if (hasHandledValue) { continue; }

            auto intPropertyIterator = intPropertiesByKeys.find(lookupKey);
            if (intPropertyIterator != intPropertiesByKeys.end()) {
                auto memberPointer = intPropertyIterator->second; /// maybe `auto&`
                try {
                    auto number = std::stoi(value);
                    metadata.*memberPointer = none_if_zero(number);
                    hasHandledValue = true;
                } catch (const std::invalid_argument &exception) {}
            }
            if (hasHandledValue) { continue; }

            if (lookupKey == Key::compilation) {
                metadata.compilation = string_to_optional_bool(value);
                if (metadata.compilation.has_value()) {
                    hasHandledValue = true;
                }
            }
            if (hasHandledValue) { continue; }

            if (!hasHandledValue) {
                additionalMetadata.push_back(std::pair<std::string, std:: string>(key, value));
                continue;;
            }
        } else if(TagLib::APE::Item::Binary == item.type()) {
            if (std::strcmp(key, Key::coverArtFront) == 0 || std::strcmp(key, Key::coverArtBack) == 0) {
                ++metadata.attachedPicturesCount;
                /// if theres no need to read pictures skip this step.
                if (options & MetadataReadingOptions::skipPictures) {
                    continue;
                }
                auto picture = AudioMetadata::Picture::create_from_APEPicture(&item, key);
                if (picture.has_value()) {
                    metadata.attachedPictures.push_back(picture.value());
                } else {
                    --metadata.attachedPicturesCount;
                }
            } else {
                continue;
            }
        }
    }

    if (!additionalMetadata.empty()) {
        metadata.additional = additionalMetadata;
    }

    return metadata;
}

// MARK: - Write
/// fills`TagLib::APE::Tag` from `AudioMetadata`.
void AudioMetadata::write_to_APE_tag(TagLib::APE::Tag * tag, bool shouldWritePictures) const {
    using Key = MetadataKey::APE;

    auto write_string = [&tag] (const std::string key, std::optional<std::string> optional) {
        tag->removeItem(key);
        if (optional.has_value()) {
            auto string = optional.value();
            tag->addValue(key, TagLib::String(string, TagLib::String::UTF8));
        }
    };

    auto write_int = [&tag] (const std::string key, std::optional<int> optional) {
        tag->removeItem(key.c_str());
        if (optional.has_value()) {
            auto number = std::to_string(optional.value());
            tag->addValue(key.c_str(), TagLib::String(number, TagLib::String::UTF8));
        }
    };

    auto write_bool = [&tag] (const std::string key, std::optional<bool> optional) {
        tag->removeItem(key.c_str());
        if (optional.has_value()) {
            auto flag = std::to_string(optional.value() ? 1 : 0);
            tag->addValue(key.c_str(), TagLib::String(flag, TagLib::String::UTF8));
        }
    };

    write_string(Key::title, title);
    write_string(Key::album, albumTitle);
    write_string(Key::artist, artist);
    write_string(Key::genre, genre);
    write_string(Key::comment, comment);
    write_string(Key::releaseDate, releaseDate);
    write_int(Key::trackNumber, trackNumber);
    write_int(Key::trackTotal, trackTotal);
    write_int(Key::discNumber, discNumber);
    write_int(Key::discTotal, discTotal);
    write_string(Key::composer, composer);
    write_string(Key::albumArtist, albumArtist);
    write_int(Key::bpm, beatPerMinute);
    write_int(Key::rating, rating);
    write_string(Key::lyrics, lyrics);
    write_bool(Key::compilation, compilation);
    write_string(Key::isrc, internationalStandardRecordingCode);
    write_string(Key::mcn, mediaCatalogNumber);
    write_string(Key::musicBrainzReleaseID, musicBrainzReleaseID);
    write_string(Key::musicBrainzRecordingID, musicBrainzRecordingID);

    // Additional metadata
    if (!additional.empty()) {
        for (auto item: additional) {
            auto key = item.first.c_str();
            auto value = item.second.c_str();
            tag->removeItem(key);
            tag->addValue(key, TagLib::String(value, TagLib::String::UTF8));
        }
    }

    // Album art
    tag->removeItem(Key::coverArtFront);
    tag->removeItem(Key::coverArtBack);

    if (shouldWritePictures && !attachedPictures.empty()) {
        for (auto picture: attachedPictures) {
            auto coverData = picture.convert_to_APEPicture();
            if (coverData) {
                auto& [key, data] = *coverData;
                tag->setData(key, data);
            }
        }
    }
}
