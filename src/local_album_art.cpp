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

#include <QByteArray>

#include <taglib/aifffile.h>
#include <taglib/apefile.h>
#include <taglib/apetag.h>
#include <taglib/asffile.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4file.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/opusfile.h>
#include <taglib/speexfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/wavpackfile.h>

#include <cassert>
#include <memory>

#include <fcntl.h>
#include <unistd.h>

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

class ID3v2Extractor : public ArtExtractor
{
public:
    ID3v2Extractor(string const& filename, TagLib::FileRef const& fileref)
        : ArtExtractor(filename, fileref)
        , tag_(nullptr)
    {
        if (dynamic_cast<TagLib::MPEG::File const*>(fileref_.file()))
        {
            tag_ = dynamic_cast<TagLib::MPEG::File*>(fileref_.file())->ID3v2Tag();
        }
        else if (dynamic_cast<TagLib::FLAC::File const*>(fileref_.file()))
        {
            tag_ = dynamic_cast<TagLib::FLAC::File*>(fileref_.file())->ID3v2Tag();
        }
    }

    virtual string get_album_art() const override;

private:
    TagLib::ID3v2::Tag const* tag_;
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

class ASFExtractor : public ArtExtractor
{
public:
    ASFExtractor(string const& filename, TagLib::FileRef const& fileref)
        : ArtExtractor(filename, fileref)
    {
    }

    virtual string get_album_art() const override;
};

class APEExtractor : public ArtExtractor
{
public:
    APEExtractor(string const& filename, TagLib::FileRef const& fileref)
        : ArtExtractor(filename, fileref)
    {
    }

    virtual string get_album_art() const override;
};

class MPCExtractor : public ArtExtractor
{
public:
    MPCExtractor(string const& filename, TagLib::FileRef const& fileref)
        : ArtExtractor(filename, fileref)
    {
    }

    virtual string get_album_art() const override;
};

class WavPackExtractor : public ArtExtractor
{
public:
    WavPackExtractor(string const& filename, TagLib::FileRef const& fileref)
        : ArtExtractor(filename, fileref)
    {
    }

    virtual string get_album_art() const override;
};

class AIFFExtractor : public ArtExtractor
{
public:
    AIFFExtractor(string const& filename, TagLib::FileRef const& fileref)
        : ArtExtractor(filename, fileref)
    {
    }

    virtual string get_album_art() const override;
};

class WAVExtractor : public ArtExtractor
{
public:
    WAVExtractor(string const& filename, TagLib::FileRef const& fileref)
        : ArtExtractor(filename, fileref)
    {
    }

    virtual string get_album_art() const override;
};

namespace
{

string extract_id3v2_art(TagLib::ID3v2::Tag const* tag)
{
    using namespace TagLib::ID3v2;

    if (!tag)
    {
        return "";
    }

    string art;

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
                auto byte_vector = frame->picture();
                art = string(byte_vector.data(), byte_vector.size());
                found_front_cover = true;
                break;
            }
            case AttachedPictureFrame::Other:
            {
                auto byte_vector = frame->picture();
                art = string(byte_vector.data(), byte_vector.size());
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

    return art;
}

}  // namespace

string ID3v2Extractor::get_album_art() const
{
    return extract_id3v2_art(tag_);
}

TagLib::Ogg::XiphComment* OggExtractor::get_xiph_comment() const
{
    using namespace TagLib::Ogg;

    {
        FLAC::File const* file = dynamic_cast<FLAC::File const*>(fileref_.file());
        if (file)
        {
            return file->tag();
        }
    }
    {
        Opus::File const* file = dynamic_cast<Opus::File const*>(fileref_.file());
        if (file)
        {
            return file->tag();
        }
    }
    {
        Speex::File const* file = dynamic_cast<Speex::File const*>(fileref_.file());
        if (file)
        {
            return file->tag();
        }
    }
    {
        Vorbis::File const* file = dynamic_cast<Vorbis::File const*>(fileref_.file());
        if (file)
        {
            return file->tag();
        }
    }
    throw runtime_error(filename_ + ": unknown Ogg file type");
}

namespace
{

// Convert base-64 encoded image data from a Xiph comment a picture.
// We return a unique_ptr because TagLib::Picture has a private
// copy constructor and no move constructor, so it is impossible
// to return it from a function by value :-(

unique_ptr<TagLib::FLAC::Picture> to_picture(TagLib::String const& base_64)
{
    // Use QByteArray to decode from base-64.
    TagLib::ByteVector b64_bytes = base_64.data(TagLib::String::Latin1);
    QByteArray bytes;
    bytes.setRawData(b64_bytes.data(), b64_bytes.size());
    bytes = QByteArray::fromBase64(bytes);

    // Now we need a byte vector to create the Picture :-(
    TagLib::ByteVector byte_vector(bytes.data(), bytes.size());
    QByteArray().swap(bytes);  // Release memory
    return unique_ptr<TagLib::FLAC::Picture>(new TagLib::FLAC::Picture(byte_vector));
}

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
            TagLib::ByteVector raw(pic->data());
            art = string(raw.data(), raw.size());
            return true;
        }
        case TagLib::FLAC::Picture::Other:
        {
            TagLib::ByteVector raw(pic->data());
            art = string(raw.data(), raw.size());
            break;
        }
        default:
        {
            break;  // Ignore all the other image types.
        }
    }
    return false;
}

} // namespace

string OggExtractor::get_album_art() const
{
    auto const xc = get_xiph_comment();
    if (!xc)
    {
        return "";
    }

    string art;

    auto const& map = xc->fieldListMap();
    auto const it = map.find("METADATA_BLOCK_PICTURE");
    if (it != map.end())
    {
        for (auto const& base_64 : it->second)
        {
            auto pic = to_picture(base_64);
            if (extract_flac_art(pic.get(), art))
            {
                break;
            }
        }
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

    TagLib::MP4::Tag const* tag = const_cast<TagLib::MP4::File const*>(file)->tag();
    if (!tag)
    {
        return "";
    }

    string art;

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

    return art;
}

namespace
{

// Helper function to pull cover (or other) art from a TagLib::ASF::Picture.
// Returns true if an image was extracted and that image is the front cover.
// Otherwise, returns false and the art string is either set to some other
// image that was found, or remains untouched.

bool extract_asf_art(TagLib::ASF::Picture const& pic, string& art)
{
    switch (pic.type())
    {
        case TagLib::FLAC::Picture::FrontCover:
        {
            TagLib::ByteVector raw(pic.picture());
            art = string(raw.data(), raw.size());
            return true;
        }
        case TagLib::FLAC::Picture::Other:
        {
            TagLib::ByteVector raw(pic.picture());
            art = string(raw.data(), raw.size());
            break;
        }
        default:
        {
            break;  // Ignore all the other image types.
        }
    }
    return false;
}

} // namespace

string ASFExtractor::get_album_art() const
{
    TagLib::ASF::File const* file = dynamic_cast<TagLib::ASF::File const*>(fileref_.file());
    assert(file);

    TagLib::ASF::Tag const* tag = const_cast<TagLib::ASF::File const*>(file)->tag();
    if (!tag)
    {
        return "";
    }

    string art;

    auto const& map = const_cast<TagLib::ASF::Tag*>(tag)->attributeListMap();
    auto const it = map.find("WM/Picture");
    if (it != map.end())
    {
        for (auto const& attr : it->second)
        {
            auto const& pic = attr.toPicture();
            if (extract_asf_art(pic, art))
            {
                break;
            }
        }
    }

    return art;
}

namespace
{

string extract_ape_art(TagLib::APE::Tag const* tag)
{
    if (!tag)
    {
        return "";
    }

    string art;

    auto const& map = const_cast<TagLib::APE::Tag*>(tag)->itemListMap();
    auto const it = map.find("COVER ART (FRONT)");
    if (it != map.end())
    {
        auto filename = it->second.toString();
        auto raw_bytes = it->second.value();
        // Image is stored as <filename>\0<image_bytes>.
        auto skip_count = filename.size() + 1;
        art = string(raw_bytes.data() + skip_count, raw_bytes.size() - skip_count);
    }

    return art;
}

}  // namespace

string APEExtractor::get_album_art() const
{
    TagLib::APE::File const* file = dynamic_cast<TagLib::APE::File const*>(fileref_.file());
    assert(file);

    TagLib::APE::Tag const* tag = const_cast<TagLib::APE::File*>(file)->APETag();
    return extract_ape_art(tag);
}

string MPCExtractor::get_album_art() const
{
    TagLib::MPC::File const* file = dynamic_cast<TagLib::MPC::File const*>(fileref_.file());
    assert(file);

    TagLib::APE::Tag const* tag = const_cast<TagLib::MPC::File*>(file)->APETag();
    return extract_ape_art(tag);
}

string WavPackExtractor::get_album_art() const
{
    TagLib::WavPack::File const* file = dynamic_cast<TagLib::WavPack::File const*>(fileref_.file());
    assert(file);

    TagLib::APE::Tag const* tag = const_cast<TagLib::WavPack::File*>(file)->APETag();
    return extract_ape_art(tag);
}

string AIFFExtractor::get_album_art() const
{
    TagLib::RIFF::AIFF::File const* file = dynamic_cast<TagLib::RIFF::AIFF::File const*>(fileref_.file());
    assert(file);

    return extract_id3v2_art(file->tag());
}

string WAVExtractor::get_album_art() const
{
    TagLib::RIFF::WAV::File const* file = dynamic_cast<TagLib::RIFF::WAV::File const*>(fileref_.file());
    assert(file);

    return extract_id3v2_art(file->tag());
}

unique_ptr<ArtExtractor> make_extractor(string const& filename, TagLib::FileRef const& fileref)
{
    if (dynamic_cast<TagLib::MPEG::File const*>(fileref.file()))
    {
        return unique_ptr<ArtExtractor>(new ID3v2Extractor(filename, fileref));
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
    if (dynamic_cast<TagLib::ASF::File const*>(fileref.file()))
    {
        return unique_ptr<ArtExtractor>(new ASFExtractor(filename, fileref));
    }
    if (dynamic_cast<TagLib::APE::File const*>(fileref.file()))
    {
        return unique_ptr<ArtExtractor>(new APEExtractor(filename, fileref));
    }
    if (dynamic_cast<TagLib::MPC::File const*>(fileref.file()))
    {
        return unique_ptr<ArtExtractor>(new MPCExtractor(filename, fileref));
    }
    if (dynamic_cast<TagLib::WavPack::File const*>(fileref.file()))
    {
        return unique_ptr<ArtExtractor>(new WavPackExtractor(filename, fileref));
    }
    if (dynamic_cast<TagLib::RIFF::AIFF::File const*>(fileref.file()))
    {
        return unique_ptr<ArtExtractor>(new AIFFExtractor(filename, fileref));
    }
    if (dynamic_cast<TagLib::RIFF::WAV::File const*>(fileref.file()))
    {
        return unique_ptr<ArtExtractor>(new WAVExtractor(filename, fileref));
    }
    throw runtime_error(filename + ": unknown container format");
}

}  // namespace

string get_album_art(string const& filename)
{
    {
        // taglib has no error reporting, so we try to open the file for reading
        // in order to get a decent error message when something is likely to go wrong.
        int fd = open(filename.c_str(), O_RDONLY);
        if (fd == -1)
        {
            throw runtime_error(filename + ": cannot open for reading: " + strerror(errno));
        }
        close(fd);
    }

    TagLib::FileRef fileref(filename.c_str(), true, TagLib::AudioProperties::Fast);
    if (fileref.isNull())
    {
        throw runtime_error(filename + ": cannot create TagLib::FileRef");
    }

    return make_extractor(filename, fileref)->get_album_art();
}

}  // namespace internal

}  // namespace thumbnailer

}  // namespace unity
