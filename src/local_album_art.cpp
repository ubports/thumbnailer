/*
 * Copyright (C) 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#include <internal/local_album_art.h>

#include <taglib/taglib.h>

#if TAGLIB_MAJOR_VERSION == 1 && TAGLIB_MINOR_VERSION <= 9
#define TAGLIB_1_9 1
#include <QByteArray>
#endif

#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4file.h>
#include <taglib/mpegfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/opusfile.h>
#include <taglib/speexfile.h>
#include <taglib/vorbisfile.h>

#include <cassert>
#include <memory>

using namespace std;

namespace unity
{

namespace thumbnailer
{

namespace internal
{

namespace
{

class ArtExtractor
{
public:
    ArtExtractor(const string& filename, TagLib::FileRef const& fileref) noexcept
        : filename_(filename)
        , fileref_(fileref)
    {
        assert(!fileref.isNull());
    }
    virtual ~ArtExtractor() = default;

    ArtExtractor(ArtExtractor const&) = delete;
    ArtExtractor& operator=(ArtExtractor const&) = delete;

    virtual string get_album_art() const = 0;

protected:
    string const filename_;
    TagLib::FileRef const fileref_;
};

class MpegExtractor : public ArtExtractor
{
public:
    MpegExtractor(string const& filename, TagLib::FileRef const& fileref)
        : ArtExtractor(filename, fileref)
    {
    }

    virtual string get_album_art() const override;
};

class OggExtractor : public ArtExtractor
{
public:
    OggExtractor(string const& filename, TagLib::FileRef const& fileref)
        : ArtExtractor(filename, fileref)
    {
    }

    virtual string get_album_art() const override;

private:
    TagLib::Ogg::XiphComment* get_xiph_comment() const;
};

class FlacExtractor : public ArtExtractor
{
public:
    FlacExtractor(string const& filename, TagLib::FileRef const& fileref)
        : ArtExtractor(filename, fileref)
    {
    }

    virtual string get_album_art() const override;
};

class MP4Extractor : public ArtExtractor
{
public:
    MP4Extractor(string const& filename, TagLib::FileRef const& fileref)
        : ArtExtractor(filename, fileref)
    {
    }

    virtual string get_album_art() const override;
};

string MpegExtractor::get_album_art() const
{
    using namespace TagLib::ID3v2;

    auto file = dynamic_cast<TagLib::MPEG::File const*>(fileref_.file());
    assert(file);

    string art;

    TagLib::ID3v2::Tag const* tag = const_cast<TagLib::MPEG::File*>(file)->ID3v2Tag();
    if (tag)
    {
        TagLib::ByteVector byte_vector;
        auto picture_frames = tag->frameList("APIC");
        for (auto const& f : picture_frames)
        {
            bool found_front_cover = false;
            auto frame = dynamic_cast<AttachedPictureFrame const*>(f);
            assert(frame);
            switch (frame->type())
            {
                case AttachedPictureFrame::FrontCover:
                {
                    // If we have a front cover, we use it and break
                    // out of loop, overwriting any "Other" picture
                    // we may have found earlier.
                    byte_vector = frame->picture();
                    art = string(byte_vector.data(), byte_vector.size());
                    found_front_cover = true;
                    break;
                }
                case AttachedPictureFrame::Other:
                {
                    // Use the first "Other" picture and ignore any "Other"
                    // pictures that might follow in the list.
                    if (byte_vector.size() == 0)
                    {
                        byte_vector = frame->picture();
                        art = string(byte_vector.data(), byte_vector.size());
                    }
                    break;
                }
                default:
                {
                    break;  // Ignore all the other image types.
                }
            }
            if (found_front_cover)
            {
                break;
            }
        }
    }

    return art;
}

TagLib::Ogg::XiphComment* OggExtractor::get_xiph_comment() const
{
    return dynamic_cast<TagLib::Ogg::XiphComment*>(fileref_.tag());
}

namespace
{

#if TAGLIB_1_9

// Decode base_64 string into a QByteArray

QByteArray decode_b64(TagLib::String const& base_64)
{
    // Use QByteArray to decode from base-64.
    // TODO: taglib 1.10 has helper methods for base_64. Once we get that
    //       version into the archives, we might be able to avoid the
    //       extra copy.
    TagLib::ByteVector b64_bytes = base_64.data(TagLib::String::Latin1);
    QByteArray bytes;
    bytes.setRawData(b64_bytes.data(), b64_bytes.size());
    return QByteArray::fromBase64(bytes);
}

// Convert base-64 encoded image data from a Xiph comment a picture.
// We return a unique_ptr because TagLib::Picture has a private
// copy constructor and no move constructor, so it is impossible
// to return it from a function by value :-(

unique_ptr<TagLib::FLAC::Picture> to_picture(TagLib::String const& base_64)
{
    // TODO: taglib 1.10 has helper methods for base_64. Once we get that
    //       version into the archives, we might be able to avoid the
    //       extra copy.
    QByteArray bytes = decode_b64(base_64);
    // Now we need a byte vector to create the Picture :-(
    TagLib::ByteVector byte_vector(bytes.data(), bytes.size());
    QByteArray().swap(bytes);  // Release memory
    return unique_ptr<TagLib::FLAC::Picture>(new TagLib::FLAC::Picture(byte_vector));
}

#endif

// Helper function to pull cover (or other) art from a TagLib::FLAC::Picture.
// Returns true if an image was extracted and that image is the front cover.
// Otherwise, returns false and the art string is either set to some other
// image that was found, or remains untouched.

bool extract_flac_art(TagLib::FLAC::Picture const* pic, string& art)
{
    switch (pic->type())
    {
        case TagLib::FLAC::Picture::FrontCover:
        {
            // If we have a front cover, we use it, overwriting
            // any "Other" picture we may have found earlier.
            TagLib::ByteVector raw(pic->data());
            art = string(raw.data(), raw.size());
            return true;
        }
        case TagLib::FLAC::Picture::Other:
        {
            if (art.empty())
            {
                // Use the first "Other" picture and ignore any "Other"
                // pictures that might follow in the list.
                TagLib::ByteVector raw(pic->data());
                art = string(raw.data(), raw.size());
            }
            break;
        }
        default:
        {
            break;  // Ignore all the other image types.
        }
    }
    return false;
}

}  // namespace

string OggExtractor::get_album_art() const
{
    string art;

    auto const xc = get_xiph_comment();
    if (xc)
    {
#if TAGLIB_1_9
        // taglib 1.11 added a new API, and broke the behavior of the existing one.
        auto const& map = xc->fieldListMap();
        auto it = map.find("METADATA_BLOCK_PICTURE");
        if (it != map.end())
        {
            TagLib::ByteVector raw;
            for (auto const& base_64 : it->second)
            {
                auto pic = to_picture(base_64);
                if (extract_flac_art(pic.get(), art))
                {
                    break;
                }
            }
        }
        if (art.empty() && (it = map.find("COVERART")) != map.end())
        {
            // Support decprecated way of storing cover art.
            // https://wiki.xiph.org/VorbisComment#Unofficial_COVERART_field_.28deprecated.29
            if (it->second.size() > 0)
            {
                art = decode_b64(it->second.front()).toStdString();
            }
        }
#else
        auto piclist = xc->pictureList();
        for (auto const& pic : piclist)
        {
            if (extract_flac_art(pic, art))
            {
                break;
            }
        }
#endif
    }

    return art;
}

string FlacExtractor::get_album_art() const
{
    TagLib::FLAC::File const* file = dynamic_cast<TagLib::FLAC::File const*>(fileref_.file());
    assert(file);

    string art;

    for (auto const& pic : const_cast<TagLib::FLAC::File*>(file)->pictureList())
    {
        if (extract_flac_art(pic, art))
        {
            break;
        }
    }

    return art;
}

string MP4Extractor::get_album_art() const
{
    TagLib::MP4::File const* file = dynamic_cast<TagLib::MP4::File const*>(fileref_.file());
    assert(file);

    string art;

    TagLib::MP4::Tag const* tag = const_cast<TagLib::MP4::File const*>(file)->tag();
    if (tag)
    {
        // Despite the name, this returns a map<String, Item>, not map<String, ItemList>.
        auto const& map = const_cast<TagLib::MP4::Tag*>(tag)->itemListMap();
        auto const it = map.find("covr");
        if (it != map.end())
        {
            auto const& art_list = it->second.toCoverArtList();
            for (auto const& img : art_list)
            {
                TagLib::ByteVector const raw = img.data();
                art = string(raw.data(), raw.size());
                break;  // We use the first image we find.
            }
        }
    }

    return art;
}

unique_ptr<ArtExtractor> make_extractor(string const& filename, TagLib::FileRef const& fileref)
{
    if (dynamic_cast<TagLib::MPEG::File const*>(fileref.file()))
    {
        return unique_ptr<ArtExtractor>(new MpegExtractor(filename, fileref));
    }
    if (dynamic_cast<TagLib::Ogg::File const*>(fileref.file()))
    {
        return unique_ptr<ArtExtractor>(new OggExtractor(filename, fileref));
    }
    if (dynamic_cast<TagLib::FLAC::File const*>(fileref.file()))
    {
        return unique_ptr<ArtExtractor>(new FlacExtractor(filename, fileref));
    }
    if (dynamic_cast<TagLib::MP4::File const*>(fileref.file()))
    {
        return unique_ptr<ArtExtractor>(new MP4Extractor(filename, fileref));
    }
    throw runtime_error(filename + ": unknown container format");
}

}  // namespace

string extract_local_album_art(string const& filename)
{
    TagLib::FileRef fileref(filename.c_str(), false, TagLib::AudioProperties::Fast);
    if (fileref.isNull())
    {
        // Unfortunately, this happens for all files without a file extension.
        throw runtime_error(filename + ": cannot create TagLib::FileRef");
    }

    return make_extractor(filename, fileref)->get_album_art();
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
