#include "map/VectorTile.h"
#include <cstring>
#include <fstream>
#include <sstream>
#include <miniz.h>
#include "core/Log.h"

namespace WebEngine
{
  // --- Protobuf wire format decoder ---

  static uint64_t DecodeVarint(const uint8_t* data, size_t size, size_t& offset)
  {
    uint64_t result = 0;
    int shift = 0;
    while (offset < size)
    {
      uint8_t byte = data[offset++];
      result |= (uint64_t)(byte & 0x7F) << shift;
      if ((byte & 0x80) == 0)
        return result;
      shift += 7;
    }
    return result;
  }

  static int32_t ZigzagDecode(uint32_t n)
  {
    return (int32_t)((n >> 1) ^ (-(int32_t)(n & 1)));
  }

  struct PBField
  {
    uint32_t fieldNumber;
    uint32_t wireType;
    // varint value (wire type 0)
    uint64_t varintVal;
    // length-delimited data (wire type 2)
    const uint8_t* data;
    size_t length;
    // fixed32 (wire type 5)
    uint32_t fixed32Val;
    // fixed64 (wire type 1)
    uint64_t fixed64Val;
  };

  static bool ReadField(const uint8_t* data, size_t size, size_t& offset, PBField& field)
  {
    if (offset >= size)
      return false;

    uint64_t tag = DecodeVarint(data, size, offset);
    field.fieldNumber = (uint32_t)(tag >> 3);
    field.wireType = (uint32_t)(tag & 0x7);
    field.data = nullptr;
    field.length = 0;
    field.varintVal = 0;
    field.fixed32Val = 0;
    field.fixed64Val = 0;

    switch (field.wireType)
    {
      case 0:  // varint
        field.varintVal = DecodeVarint(data, size, offset);
        break;
      case 1:  // fixed64
        if (offset + 8 > size) return false;
        memcpy(&field.fixed64Val, data + offset, 8);
        offset += 8;
        break;
      case 2:  // length-delimited
      {
        uint64_t len = DecodeVarint(data, size, offset);
        if (offset + len > size) return false;
        field.data = data + offset;
        field.length = (size_t)len;
        offset += (size_t)len;
        break;
      }
      case 5:  // fixed32
        if (offset + 4 > size) return false;
        memcpy(&field.fixed32Val, data + offset, 4);
        offset += 4;
        break;
      default:
        return false;
    }
    return true;
  }

  static std::vector<uint32_t> DecodePackedUint32(const uint8_t* data, size_t length)
  {
    std::vector<uint32_t> result;
    size_t offset = 0;
    while (offset < length)
    {
      result.push_back((uint32_t)DecodeVarint(data, length, offset));
    }
    return result;
  }

  // --- Geometry command decoder ---

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
          // Start a new ring
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

  // --- MVT Value parser ---

  static MVTValue ParseValue(const uint8_t* data, size_t length)
  {
    MVTValue val;
    val.type = MVTValue::String;
    size_t offset = 0;
    PBField field;

    while (ReadField(data, length, offset, field))
    {
      switch (field.fieldNumber)
      {
        case 1:  // string_value
          val.type = MVTValue::String;
          val.stringVal = std::string((const char*)field.data, field.length);
          break;
        case 2:  // float_value
        {
          val.type = MVTValue::Float;
          float f;
          memcpy(&f, &field.fixed32Val, 4);
          val.numVal = f;
          break;
        }
        case 3:  // double_value
        {
          val.type = MVTValue::Double;
          double d;
          memcpy(&d, &field.fixed64Val, 8);
          val.numVal = d;
          break;
        }
        case 4:  // int_value
          val.type = MVTValue::Int;
          val.numVal = (double)(int64_t)field.varintVal;
          break;
        case 5:  // uint_value
          val.type = MVTValue::UInt;
          val.numVal = (double)field.varintVal;
          break;
        case 6:  // sint_value
          val.type = MVTValue::SInt;
          val.numVal = (double)ZigzagDecode((uint32_t)field.varintVal);
          break;
        case 7:  // bool_value
          val.type = MVTValue::Bool;
          val.boolVal = field.varintVal != 0;
          break;
      }
    }
    return val;
  }

  std::string MVTValue::ToString() const
  {
    switch (type)
    {
      case String: return stringVal;
      case Bool: return boolVal ? "true" : "false";
      default:
      {
        std::ostringstream oss;
        oss << numVal;
        return oss.str();
      }
    }
  }

  // --- MVT Feature parser ---

  static MVTFeature ParseFeature(const uint8_t* data, size_t length,
                                 const std::vector<std::string>& keys,
                                 const std::vector<MVTValue>& values)
  {
    MVTFeature feature;
    size_t offset = 0;
    PBField field;
    std::vector<uint32_t> tags;
    std::vector<uint32_t> geometry;

    while (ReadField(data, length, offset, field))
    {
      switch (field.fieldNumber)
      {
        case 1:  // id
          feature.id = field.varintVal;
          break;
        case 2:  // tags (packed)
          tags = DecodePackedUint32(field.data, field.length);
          break;
        case 3:  // type
          feature.type = (MVTGeomType)field.varintVal;
          break;
        case 4:  // geometry (packed)
          geometry = DecodePackedUint32(field.data, field.length);
          break;
      }
    }

    // Decode tags into key-value properties
    for (size_t i = 0; i + 1 < tags.size(); i += 2)
    {
      uint32_t keyIdx = tags[i];
      uint32_t valIdx = tags[i + 1];
      if (keyIdx < keys.size() && valIdx < values.size())
      {
        feature.properties.emplace_back(keys[keyIdx], values[valIdx].ToString());
      }
    }

    // Decode geometry commands
    DecodeGeometry(geometry, feature);

    return feature;
  }

  // --- MVT Layer parser ---

  static MVTLayer ParseLayer(const uint8_t* data, size_t length)
  {
    MVTLayer layer;
    size_t offset = 0;
    PBField field;

    std::vector<std::string> keys;
    std::vector<MVTValue> values;

    // Raw feature data to parse after keys/values are collected
    struct RawFeature { const uint8_t* data; size_t length; };
    std::vector<RawFeature> rawFeatures;

    while (ReadField(data, length, offset, field))
    {
      switch (field.fieldNumber)
      {
        case 1:  // name
          layer.name = std::string((const char*)field.data, field.length);
          break;
        case 2:  // feature
          rawFeatures.push_back({field.data, field.length});
          break;
        case 3:  // keys
          keys.emplace_back((const char*)field.data, field.length);
          break;
        case 4:  // values
          values.push_back(ParseValue(field.data, field.length));
          break;
        case 5:  // extent
          layer.extent = (uint32_t)field.varintVal;
          break;
        case 15:  // version
          layer.version = (uint32_t)field.varintVal;
          break;
      }
    }

    // Now parse features with keys/values available
    layer.features.reserve(rawFeatures.size());
    for (auto& rf : rawFeatures)
    {
      layer.features.push_back(ParseFeature(rf.data, rf.length, keys, values));
    }

    return layer;
  }

  // --- MVT Tile parser ---

  MVTTile ParseMVTFromMemory(const uint8_t* data, size_t size)
  {
    MVTTile tile;
    size_t offset = 0;
    PBField field;

    while (ReadField(data, size, offset, field))
    {
      if (field.fieldNumber == 3 && field.wireType == 2)  // layer
      {
        tile.layers.push_back(ParseLayer(field.data, field.length));
      }
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
      if (pos + 2 > compressedSize) return {};
      uint16_t extraLen = compressedData[pos] | (compressedData[pos + 1] << 8);
      pos += 2 + extraLen;
    }
    if (flags & 0x08)  // FNAME
    {
      while (pos < compressedSize && compressedData[pos] != 0) pos++;
      pos++;  // skip null terminator
    }
    if (flags & 0x10)  // FCOMMENT
    {
      while (pos < compressedSize && compressedData[pos] != 0) pos++;
      pos++;
    }
    if (flags & 0x02)  // FHCRC
    {
      pos += 2;
    }

    if (pos >= compressedSize) return {};

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
      return {};

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
}
