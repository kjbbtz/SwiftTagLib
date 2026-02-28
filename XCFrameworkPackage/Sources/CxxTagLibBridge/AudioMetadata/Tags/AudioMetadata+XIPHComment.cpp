
#import <CxxTagLibBridge/AudioMetadata.hpp>
#import <unordered_map>
#import <cstring>
#import <string>
#import <string_view>

// MARK: - Keys
namespace MetadataKey {
    struct XIPHComment final {
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
        // no key for picture needed
        static constexpr const char* blockPicture = "METADATA_BLOCK_PICTURE";
        static constexpr const char* coverArt = "COVERART";

        using StringProperty = std::optional<std::string> AudioMetadata::*;
        static std::unordered_map<std::string, StringProperty, std::hash<std::string>> stringPropertiesByKeys() {
            return {
                {XIPHComment::title, &AudioMetadata::title},
                {XIPHComment::album, &AudioMetadata::albumTitle},
                {XIPHComment::artist, &AudioMetadata::artist},
                {XIPHComment::genre, &AudioMetadata::genre},
                {XIPHComment::comment, &AudioMetadata::comment},
                {XIPHComment::releaseDate, &AudioMetadata::releaseDate},
                {XIPHComment::composer, &AudioMetadata::composer},
                {XIPHComment::albumArtist, &AudioMetadata::albumArtist},
                {XIPHComment::lyrics, &AudioMetadata::lyrics},
                {XIPHComment::isrc, &AudioMetadata::internationalStandardRecordingCode},
                {XIPHComment::mcn, &AudioMetadata::mediaCatalogNumber},
                {XIPHComment::musicBrainzReleaseID, &AudioMetadata::musicBrainzReleaseID},
                {XIPHComment::musicBrainzRecordingID, &AudioMetadata::musicBrainzRecordingID},
            };
        }

        using IntProperty = std::optional<int> AudioMetadata::*;
        static std::unordered_map<std::string, IntProperty, std::hash<std::string>> intPropertiesByKeys() {
            return {
                {XIPHComment::trackNumber, &AudioMetadata::trackNumber},
                {XIPHComment::trackTotal, &AudioMetadata::trackTotal},
                {XIPHComment::discNumber, &AudioMetadata::discNumber},
                {XIPHComment::discTotal, &AudioMetadata::discTotal},
                {XIPHComment::bpm, &AudioMetadata::beatPerMinute},
                {XIPHComment::rating, &AudioMetadata::rating},
            };
        }
    };
}

// MARK: - Read
/// constructor for `AudioMetadata` from `TagLib::Ogg::XiphComment`.
AudioMetadata AudioMetadata::read_from_XiphComment(const TagLib::Ogg::XiphComment *tag, const MetadataReadingOptions options) {
    auto metadata = AudioMetadata();
    if (tag->isEmpty()) {
        return metadata;
    }
    metadata.tagSource |= TagSource::Xiph;
    auto additionalMetadata = AudioMetadata::AdditionalMetadata();
    using Key = MetadataKey::XIPHComment;

    auto none_if_empty = [] (const char* string) -> std::optional<std::string> {
        return (!std::string_view(string).empty()) ? std::optional<std::string>(string) : std::nullopt;
    };

    auto none_if_zero = [] (int number) -> std::optional<int> {
        return number != 0 ? std::optional<int>(number) : std::nullopt;
    };

    auto string_to_optional_bool = [] (char const* value) -> std::optional<bool> {
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

    auto stringPropertiesByKeys = MetadataKey::XIPHComment::stringPropertiesByKeys();
    auto intPropertiesByKeys = MetadataKey::XIPHComment::intPropertiesByKeys();

    for (auto iterator: tag->fieldListMap()) {
        auto key = iterator.first.toCString(true);
        auto value = iterator.second.front().toCString(true);
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

        /// TagLib parses `METADATA_BLOCK_PICTURE` and `COVERART` Xiph comments as pictures, so ignore them here
        if (lookupKey == Key::blockPicture || lookupKey == Key::coverArt) {
            // specifically do nothing here
            continue;
        } else {
            additionalMetadata.push_back(std::pair<std::string, std:: string>(key, value));
            continue;
        }
    }

    if (!additionalMetadata.empty()) {
        metadata.additional = additionalMetadata;
    }

    for (auto iterator: const_cast<TagLib::Ogg::XiphComment *>(tag)->pictureList()) {
        ++metadata.attachedPicturesCount;
        /// if theres no need to read pictures skip this step.
        if (options & MetadataReadingOptions::skipPictures) {
            continue;
        }
        auto picture = AudioMetadata::Picture::create_from_FLACPicture(iterator);
        if (picture.has_value()) {
            metadata.attachedPictures.push_back(picture.value());
        } else {
            --metadata.attachedPicturesCount;
        }
    }

    return metadata;
}

// MARK: - Write
/// fills`TagLib::Ogg::XiphComment` from `AudioMetadata`.
void AudioMetadata::write_to_XiphComment(TagLib::Ogg::XiphComment *tag, bool shouldWritePictures) const {
    using Key = MetadataKey::XIPHComment;
    tag->removeAllFields();

    auto write_string = [&tag] (const std::string key, std::optional<std::string> optional) {
        tag->removeFields(key.c_str());
        if (optional.has_value()) {
            auto string = optional.value();
            tag->addField(key.c_str(), TagLib::String(string, TagLib::String::UTF8));
        }
    };

    auto write_int = [&tag] (const std::string key, std::optional<int> optional) {
        tag->removeFields(key.c_str());
        if (optional.has_value()) {
            auto number = std::to_string(optional.value());
            tag->addField(key.c_str(), TagLib::String(number, TagLib::String::UTF8));
        }
    };

    auto write_bool = [&tag] (const std::string key, std::optional<bool> optional) {
        tag->removeFields(key.c_str());
        if (optional.has_value()) {
            auto flag = std::to_string(optional.value() ? 1 : 0);
            tag->addField(key.c_str(), TagLib::String(flag, TagLib::String::UTF8));
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
            auto value = item.second;
            tag->removeFields(key);
            tag->addField(key, TagLib::String(value, TagLib::String::UTF8));
        }
    }

    // Album art
    tag->removeAllPictures();

    if (shouldWritePictures && !attachedPictures.empty()) {
        for (auto picture: attachedPictures) {
            tag->addPicture(picture.convert_to_FLACPicture());
        }
    }
}
