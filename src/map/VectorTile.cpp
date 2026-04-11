#include "map/VectorTile.h"
#include <cstring>
#include <fstream>
#include <sstream>
#include <miniz.h>
#include "core/Log.h"
#include <protozero/pbf_reader.hpp>

namespace WebEngine
{
  // --- Zigzag and MVT geometry command decoder (MVT-level, not protobuf-level) ---

  static int32_t ZigzagDecode(uint32_t n)
  {
    return (int32_t)((n >> 1) ^ (-(int32_t)(n & 1)));
  }

  static void DecodeGeometry(const std::vector<uint32_t>& geom, MVTFeature& feature)
  {
    int32_t cursorX = 0, cursorY = 0;
    size_t i = 0;

    while (i < geom.size())
    {
      uint32_t cmdInt = geom[i++];
      uint32_t cmdId = cmdInt & 0x7;
      uint32_t cmdCount = cmdInt >> 3;

      if (cmdId == 1)  // MoveTo
      {
        for (uint32_t j = 0; j < cmdCount && i + 1 < geom.size(); j++)
        {
          int32_t dx = ZigzagDecode(geom[i++]);
          int32_t dy = ZigzagDecode(geom[i++]);
          cursorX += dx;
          cursorY += dy;
          feature.rings.push_back({});
          feature.rings.back().push_back(glm::vec2((float)cursorX, (float)cursorY));
        }
      }
      else if (cmdId == 2)  // LineTo
      {
        for (uint32_t j = 0; j < cmdCount && i + 1 < geom.size(); j++)
        {
          int32_t dx = ZigzagDecode(geom[i++]);
          int32_t dy = ZigzagDecode(geom[i++]);
          cursorX += dx;
          cursorY += dy;
          if (!feature.rings.empty())
          {
            feature.rings.back().push_back(glm::vec2((float)cursorX, (float)cursorY));
          }
        }
      }
      else if (cmdId == 7)  // ClosePath
      {
        if (!feature.rings.empty() && !feature.rings.back().empty())
        {
          feature.rings.back().push_back(feature.rings.back().front());
        }
      }
    }
  }

  // --- Protobuf parsing via protozero ---

  static MVTValue ParseValue(protozero::pbf_reader msg)
  {
    MVTValue val;
    val.type = MVTValue::String;

    while (msg.next())
    {
      switch (msg.tag())
      {
        case 1:  // string_value
          val.type = MVTValue::String;
          val.stringVal = msg.get_string();
          break;
        case 2:  // float_value
          val.type = MVTValue::Float;
          val.numVal = msg.get_float();
          break;
        case 3:  // double_value
          val.type = MVTValue::Double;
          val.numVal = msg.get_double();
          break;
        case 4:  // int_value
          val.type = MVTValue::Int;
          val.numVal = (double)msg.get_int64();
          break;
        case 5:  // uint_value
          val.type = MVTValue::UInt;
          val.numVal = (double)msg.get_uint64();
          break;
        case 6:  // sint_value (protozero handles zigzag decoding)
          val.type = MVTValue::SInt;
          val.numVal = (double)msg.get_sint64();
          break;
        case 7:  // bool_value
          val.type = MVTValue::Bool;
          val.boolVal = msg.get_bool();
          break;
        default:
          msg.skip();
      }
    }
    return val;
  }

  static MVTFeature ParseFeature(protozero::pbf_reader msg,
                                 const std::vector<std::string>& keys,
                                 const std::vector<MVTValue>& values)
  {
    MVTFeature feature;
    std::vector<uint32_t> tags;
    std::vector<uint32_t> geometry;

    while (msg.next())
    {
      switch (msg.tag())
      {
        case 1:  // id
          feature.id = msg.get_uint64();
          break;
        case 2:  // tags (packed uint32)
          for (auto v : msg.get_packed_uint32())
          {
            tags.push_back(v);
          }
          break;
        case 3:  // type
          feature.type = (MVTGeomType)msg.get_uint32();
          break;
        case 4:  // geometry (packed uint32)
          for (auto v : msg.get_packed_uint32())
          {
            geometry.push_back(v);
          }
          break;
        default:
          msg.skip();
      }
    }

    for (size_t i = 0; i + 1 < tags.size(); i += 2)
    {
      uint32_t keyIdx = tags[i];
      uint32_t valIdx = tags[i + 1];
      if (keyIdx < keys.size() && valIdx < values.size())
      {
        feature.properties.emplace_back(keys[keyIdx], values[valIdx].ToString());
      }
    }

    DecodeGeometry(geometry, feature);
    return feature;
  }

  static MVTLayer ParseLayer(protozero::pbf_reader msg)
  {
    MVTLayer layer;
    std::vector<std::string> keys;
    std::vector<MVTValue> values;

    // Collect raw feature views for deferred parsing (keys/values must be
    // gathered from the whole layer message before features can be resolved).
    std::vector<protozero::data_view> rawFeatureViews;

    while (msg.next())
    {
      switch (msg.tag())
      {
        case 1:  // name
          layer.name = msg.get_string();
          break;
        case 2:  // feature (deferred — store view into original buffer)
          rawFeatureViews.push_back(msg.get_view());
          break;
        case 3:  // keys
          keys.push_back(msg.get_string());
          break;
        case 4:  // values
          values.push_back(ParseValue(msg.get_message()));
          break;
        case 5:  // extent
          layer.extent = msg.get_uint32();
          break;
        case 15:  // version
          layer.version = msg.get_uint32();
          break;
        default:
          msg.skip();
      }
    }

    layer.features.reserve(rawFeatureViews.size());
    for (const auto& view : rawFeatureViews)
    {
      protozero::pbf_reader feature_reader{view};
      layer.features.push_back(ParseFeature(feature_reader, keys, values));
    }

    return layer;
  }

  // --- MVT Value helpers ---

  std::string MVTValue::ToString() const
  {
    switch (type)
    {
      case String:
        return stringVal;
      case Bool:
        return boolVal ? "true" : "false";
      default:
      {
        std::ostringstream oss;
        oss << numVal;
        return oss.str();
      }
    }
  }

  // --- Top-level tile parser ---

  MVTTile ParseMVTFromMemory(const uint8_t* data, size_t size)
  {
    MVTTile tile;
    try
    {
      protozero::pbf_reader tile_reader(reinterpret_cast<const char*>(data), size);
      while (tile_reader.next())
      {
        if (tile_reader.tag() == 3)  // layer
        {
          tile.layers.push_back(ParseLayer(tile_reader.get_message()));
        }
        else
        {
          tile_reader.skip();
        }
      }
    }
    catch (const protozero::exception& e)
    {
      RN_LOG_ERR("ParseMVTFromMemory: protobuf parse error: {}", e.what());
    }
    return tile;
  }

  // --- Gzip decompression + file loading ---

  static std::vector<uint8_t> ReadBinaryFile(const std::string& path)
  {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
      RN_LOG_ERR("Failed to open file: {}", path);
      return {};
    }
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    return buffer;
  }

  static std::vector<uint8_t> DecompressGzip(const uint8_t* compressedData, size_t compressedSize)
  {
    // Gzip header: 1f 8b
    if (compressedSize < 10 || compressedData[0] != 0x1F || compressedData[1] != 0x8B)
    {
      // Not gzip, return as-is (might be raw protobuf)
      return std::vector<uint8_t>(compressedData, compressedData + compressedSize);
    }

    // Use miniz inflate with raw deflate (skip gzip header manually)
    // Gzip header is at least 10 bytes: ID1 ID2 CM FLG MTIME(4) XFL OS
    size_t headerSize = 10;
    uint8_t flags = compressedData[3];

    // Skip optional gzip header fields
    size_t pos = headerSize;
    if (flags & 0x04)  // FEXTRA
    {
      if (pos + 2 > compressedSize)
      {
        return {};
      }
      uint16_t extraLen = compressedData[pos] | (compressedData[pos + 1] << 8);
      pos += 2 + extraLen;
    }
    if (flags & 0x08)  // FNAME
    {
      while (pos < compressedSize && compressedData[pos] != 0)
      {
        pos++;
      }
      pos++;  // skip null terminator
    }
    if (flags & 0x10)  // FCOMMENT
    {
      while (pos < compressedSize && compressedData[pos] != 0)
      {
        pos++;
      }
      pos++;
    }
    if (flags & 0x02)  // FHCRC
    {
      pos += 2;
    }

    if (pos >= compressedSize)
    {
      return {};
    }

    // The uncompressed size is stored in the last 4 bytes of the gzip file
    uint32_t uncompressedSize = 0;
    if (compressedSize >= 4)
    {
      memcpy(&uncompressedSize, compressedData + compressedSize - 4, 4);
    }

    // Safety: if uncompressed size seems unreasonable, use a generous estimate
    if (uncompressedSize == 0 || uncompressedSize > 64 * 1024 * 1024)
    {
      uncompressedSize = (uint32_t)(compressedSize * 10);
    }

    std::vector<uint8_t> decompressed(uncompressedSize);

    // Use mz_inflateInit2 with -MAX_WBITS for raw deflate (no zlib/gzip header)
    mz_stream stream = {};
    stream.next_in = compressedData + pos;
    stream.avail_in = (mz_uint32)(compressedSize - pos - 8);  // subtract gzip footer (CRC32 + size)
    stream.next_out = decompressed.data();
    stream.avail_out = (mz_uint32)decompressed.size();

    if (mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK)
    {
      RN_LOG_ERR("Failed to init inflate");
      return {};
    }

    int status = mz_inflate(&stream, MZ_FINISH);
    if (status != MZ_STREAM_END)
    {
      // Try with larger buffer
      mz_inflateEnd(&stream);
      decompressed.resize(uncompressedSize * 4);

      stream = {};
      stream.next_in = compressedData + pos;
      stream.avail_in = (mz_uint32)(compressedSize - pos - 8);
      stream.next_out = decompressed.data();
      stream.avail_out = (mz_uint32)decompressed.size();

      if (mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK)
      {
        RN_LOG_ERR("Failed to init inflate (retry)");
        return {};
      }
      status = mz_inflate(&stream, MZ_FINISH);
      if (status != MZ_STREAM_END)
      {
        RN_LOG_ERR("Failed to decompress gzip data, status: {}", status);
        mz_inflateEnd(&stream);
        return {};
      }
    }

    decompressed.resize(stream.total_out);
    mz_inflateEnd(&stream);
    return decompressed;
  }

  MVTTile ParseMVTFromBytes(const uint8_t* data, size_t size)
  {
    if (!data || size == 0)
    {
      return {};
    }

    auto decompressed = DecompressGzip(data, size);
    if (decompressed.empty())
    {
      RN_LOG_ERR("ParseMVTFromBytes: decompression produced no output ({} input bytes)", size);
      return {};
    }
    return ParseMVTFromMemory(decompressed.data(), decompressed.size());
  }

  MVTTile ParseMVTFile(const std::string& path)
  {
    auto compressed = ReadBinaryFile(path);
    if (compressed.empty())
    {
      RN_LOG_ERR("Failed to read MVT file: {}", path);
      return {};
    }

    auto tile = ParseMVTFromBytes(compressed.data(), compressed.size());
    if (tile.layers.empty())
    {
      RN_LOG_ERR("Failed to parse MVT file: {}", path);
      return {};
    }

    RN_LOG("MVT file loaded: {} ({} bytes on disk, {} layers)",
           path, compressed.size(), tile.layers.size());
    return tile;
  }
}  // namespace WebEngine
