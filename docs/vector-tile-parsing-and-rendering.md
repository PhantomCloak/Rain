# Parsing and Rendering Mapbox Vector Tiles

This document explains how to parse the Mapbox Vector Tile (MVT) format from scratch
and turn the data into renderable geometry. All code references are from our
implementation in `src/map/VectorTile.cpp` and `src/map/VectorTileRenderer.cpp`.

## Table of Contents

1. [What is a Vector Tile](#1-what-is-a-vector-tile)
2. [File Format Overview](#2-file-format-overview)
3. [Step 1: Gzip Decompression](#3-step-1-gzip-decompression)
4. [Step 2: Protobuf Wire Format](#4-step-2-protobuf-wire-format)
5. [Step 3: Parsing the MVT Schema](#5-step-3-parsing-the-mvt-schema)
6. [Step 4: Decoding Geometry Commands](#6-step-4-decoding-geometry-commands)
7. [Step 5: Building Renderable Geometry](#7-step-5-building-renderable-geometry)
8. [Step 6: Coordinate Mapping](#8-step-6-coordinate-mapping)
9. [The Complete Pipeline](#9-the-complete-pipeline)

---

## 1. What is a Vector Tile

A vector tile is a compact binary representation of geographic data for a specific
rectangular region of the world at a specific zoom level. Unlike raster tiles (PNG/JPEG
images), vector tiles contain the actual geometry (points, lines, polygons) and metadata
(road names, building types, etc.) which allows the client to style and render them
however it wants.

Tiles are organized in a `z/x/y` directory structure where:
- `z` is the zoom level (0 = entire world, 14 = street-level detail)
- `x` is the column index
- `y` is the row index

For example, `14/8529/5974.pbf` is a zoom-14 tile covering part of Monaco.

Each tile contains **layers** (e.g., "water", "road", "building"), and each layer
contains **features** (individual roads, buildings, coastlines). Features carry a
geometry type, encoded geometry, and key-value properties.

## 2. File Format Overview

The on-disk format is a two-layer encoding:

```
.pbf file
  |
  +-- Gzip compression (RFC 1952)
        |
        +-- Protocol Buffers binary encoding
              |
              +-- MVT schema (Tile > Layer > Feature)
                    |
                    +-- Geometry encoded as command integers
```

The pipeline to go from a `.pbf` file to renderable vertex data is:

```
Read bytes -> Gzip decompress -> Protobuf decode -> MVT parse -> Geometry decode -> Vertices
```

## 3. Step 1: Gzip Decompression

PBF files begin with the gzip magic bytes `1F 8B`. The gzip format (RFC 1952) wraps
a DEFLATE-compressed stream with a header and footer.

### Gzip Header Structure

```
Offset  Size  Field
0       2     Magic number: 0x1F 0x8B
2       1     Compression method (8 = deflate)
3       1     Flags byte (FLG)
4       4     Modification time
8       1     Extra flags
9       1     Operating system

Then, conditional on FLG bits:
  bit 2 (FEXTRA):   2-byte length + extra data
  bit 3 (FNAME):    null-terminated original filename
  bit 4 (FCOMMENT): null-terminated comment
  bit 1 (FHCRC):    2-byte header CRC
```

After the header comes the raw DEFLATE stream. The last 8 bytes of the file are a
footer containing a CRC32 checksum and the original uncompressed size (mod 2^32).

### Implementation

We skip the gzip header manually, then use `mz_inflateInit2` with `-MZ_DEFAULT_WINDOW_BITS`
to tell miniz to expect raw DEFLATE data (no zlib or gzip wrapper):

```cpp
// VectorTile.cpp - DecompressGzip()

// Detect gzip by magic bytes
if (compressedData[0] != 0x1F || compressedData[1] != 0x8B)
  return raw_data;  // not gzip, already decompressed

// Parse the 10-byte header, skip optional fields based on FLG byte
uint8_t flags = compressedData[3];
size_t pos = 10;  // minimum header size
if (flags & 0x04) { /* skip FEXTRA */ }
if (flags & 0x08) { /* skip FNAME (null-terminated string) */ }
if (flags & 0x10) { /* skip FCOMMENT */ }
if (flags & 0x02) { pos += 2; /* skip FHCRC */ }

// Read uncompressed size from last 4 bytes of file
uint32_t uncompressedSize;
memcpy(&uncompressedSize, compressedData + compressedSize - 4, 4);

// Inflate the raw DEFLATE stream
mz_stream stream = {};
stream.next_in  = compressedData + pos;
stream.avail_in = compressedSize - pos - 8;  // exclude 8-byte footer
mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS);
mz_inflate(&stream, MZ_FINISH);
```

The key detail is `-MZ_DEFAULT_WINDOW_BITS` (the negative sign). A positive value would
expect a zlib header; negative tells the decompressor this is raw DEFLATE with no
wrapper, which is what's inside a gzip stream after we manually skip the gzip header.

## 4. Step 2: Protobuf Wire Format

The decompressed bytes are a Protocol Buffers (protobuf) binary message. Protobuf is a
schema-driven binary serialization format by Google. To avoid pulling in the heavyweight
protobuf library, we implement a minimal wire-format decoder (~80 lines of code).

### Varint Encoding

Protobuf's fundamental integer encoding is the **varint** - a variable-length integer
where each byte uses 7 bits for data and 1 bit (the MSB) as a continuation flag:

```
Byte:    [1AAAAAAA] [1BBBBBBB] [0CCCCCCC]
Value:    AAAAAAA    BBBBBBB    CCCCCCC
Result:  CCCCCCC_BBBBBBB_AAAAAAA  (little-endian order)
```

If the MSB is 1, more bytes follow. If 0, this is the last byte.

```cpp
// VectorTile.cpp:12-25
static uint64_t DecodeVarint(const uint8_t* data, size_t size, size_t& offset)
{
  uint64_t result = 0;
  int shift = 0;
  while (offset < size)
  {
    uint8_t byte = data[offset++];
    result |= (uint64_t)(byte & 0x7F) << shift;  // take lower 7 bits
    if ((byte & 0x80) == 0)                        // MSB clear = last byte
      return result;
    shift += 7;
  }
  return result;
}
```

**Example:** The value 300 is encoded as `[0xAC, 0x02]`:
- Byte 0: `0xAC` = `1_0101100` -> continuation=1, bits=0101100
- Byte 1: `0x02` = `0_0000010` -> continuation=0, bits=0000010
- Result: `0000010_0101100` = 256 + 32 + 8 + 4 = 300

### Field Tags

Every protobuf field starts with a tag varint that encodes the field number and wire type:

```
tag = (field_number << 3) | wire_type
```

The wire types are:

| Wire Type | Meaning              | Data that follows        |
|-----------|----------------------|--------------------------|
| 0         | Varint               | Another varint           |
| 1         | 64-bit fixed         | 8 bytes                  |
| 2         | Length-delimited     | Varint length, then N bytes |
| 5         | 32-bit fixed         | 4 bytes                  |

Wire type 2 is the workhorse - it carries strings, nested messages, and packed repeated
fields. The decoder reads the tag, determines the wire type, and extracts the payload:

```cpp
// VectorTile.cpp:47-89
static bool ReadField(const uint8_t* data, size_t size, size_t& offset, PBField& field)
{
  uint64_t tag = DecodeVarint(data, size, offset);
  field.fieldNumber = tag >> 3;
  field.wireType    = tag & 0x7;

  switch (field.wireType)
  {
    case 0:  // Varint
      field.varintVal = DecodeVarint(data, size, offset);
      break;
    case 1:  // Fixed 64-bit
      memcpy(&field.fixed64Val, data + offset, 8);
      offset += 8;
      break;
    case 2:  // Length-delimited
    {
      uint64_t len = DecodeVarint(data, size, offset);
      field.data   = data + offset;
      field.length = len;
      offset += len;
      break;
    }
    case 5:  // Fixed 32-bit
      memcpy(&field.fixed32Val, data + offset, 4);
      offset += 4;
      break;
  }
}
```

### Packed Repeated Fields

When a field is declared as `repeated` and `packed` in the protobuf schema, all values
are concatenated into a single length-delimited blob rather than each getting its own
tag. The geometry and tags arrays in MVT use this encoding:

```cpp
// VectorTile.cpp:91-100
static std::vector<uint32_t> DecodePackedUint32(const uint8_t* data, size_t length)
{
  std::vector<uint32_t> result;
  size_t offset = 0;
  while (offset < length)
    result.push_back((uint32_t)DecodeVarint(data, length, offset));
  return result;
}
```

### Zigzag Encoding

Protobuf uses **zigzag encoding** for signed integers (`sint32`/`sint64`). Standard
varint encoding of negative numbers would always use the maximum number of bytes (since
the MSB is set). Zigzag maps signed values to unsigned values so small magnitudes
(positive or negative) use few bytes:

```
 0 -> 0
-1 -> 1
 1 -> 2
-2 -> 3
 2 -> 4
...
```

The formula is:
```cpp
// VectorTile.cpp:27-30
static int32_t ZigzagDecode(uint32_t n)
{
  return (int32_t)((n >> 1) ^ (-(int32_t)(n & 1)));
}
```

This is critical for MVT because geometry coordinates are delta-encoded (deltas are
often small negative numbers), and zigzag keeps these compact.

## 5. Step 3: Parsing the MVT Schema

With the wire-format decoder in hand, we can parse the MVT schema. The full schema
(from the [Mapbox Vector Tile specification](https://github.com/mapbox/vector-tile-spec)):

```protobuf
message Tile {
  repeated Layer layers = 3;

  message Layer {
    required string  name     = 1;
    repeated Feature features = 2;
    repeated string  keys     = 3;
    repeated Value   values   = 4;
    optional uint32  extent   = 5;  // default 4096
    required uint32  version  = 15;
  }

  message Feature {
    optional uint64   id       = 1;
    repeated uint32   tags     = 2;  // packed
    optional GeomType type     = 3;
    repeated uint32   geometry = 4;  // packed
  }

  message Value {
    optional string  string_value = 1;
    optional float   float_value  = 2;
    optional double  double_value = 3;
    optional int64   int_value    = 4;
    optional uint64  uint_value   = 5;
    optional sint64  sint_value   = 6;
    optional bool    bool_value   = 7;
  }

  enum GeomType {
    UNKNOWN    = 0;
    POINT      = 1;
    LINESTRING = 2;
    POLYGON    = 3;
  }
}
```

### Parsing Order Matters

There's a subtlety in the Layer message: `keys` (field 3) and `values` (field 4) are
lookup tables that Feature `tags` reference by index. But protobuf doesn't guarantee
that fields arrive in ascending field-number order (only that repeated fields of the
same number preserve order). In practice MVT encoders often interleave features with
keys/values.

Our solution: buffer the raw feature bytes on the first pass, then parse them after
all keys and values have been collected:

```cpp
// VectorTile.cpp:271-317 - ParseLayer()

struct RawFeature { const uint8_t* data; size_t length; };
std::vector<RawFeature> rawFeatures;

while (ReadField(data, length, offset, field))
{
  switch (field.fieldNumber)
  {
    case 1:  layer.name = string(field.data, field.length);  break;
    case 2:  rawFeatures.push_back({field.data, field.length}); break;
    case 3:  keys.emplace_back(field.data, field.length);    break;
    case 4:  values.push_back(ParseValue(field.data, field.length)); break;
    case 5:  layer.extent = field.varintVal;                 break;
    case 15: layer.version = field.varintVal;                break;
  }
}

// Now parse features with complete key/value tables
for (auto& rf : rawFeatures)
  layer.features.push_back(ParseFeature(rf.data, rf.length, keys, values));
```

### Feature Properties

Each feature carries a `tags` array: pairs of indices into the layer's `keys` and
`values` tables. For example, `tags = [0, 3, 2, 1]` means:

```
property 0: keys[0] = values[3]
property 1: keys[2] = values[1]
```

```cpp
// VectorTile.cpp:252-261
for (size_t i = 0; i + 1 < tags.size(); i += 2)
{
  uint32_t keyIdx = tags[i];
  uint32_t valIdx = tags[i + 1];
  feature.properties.emplace_back(keys[keyIdx], values[valIdx].ToString());
}
```

This shared string table is one of MVT's key size optimizations - a layer with 500
buildings doesn't repeat "building" 500 times.

## 6. Step 4: Decoding Geometry Commands

This is the most interesting part of the format. MVT encodes geometry as a sequence
of **command integers** - a tiny virtual machine instruction set with just three opcodes.

### Command Integer Format

Each command integer packs an opcode and a repeat count:

```
command_integer = (count << 3) | command_id

command_id:
  1 = MoveTo    (start a new sub-path)
  2 = LineTo    (draw a line from the current position)
  7 = ClosePath (close the current ring)
```

After a MoveTo or LineTo command, `count` pairs of coordinate deltas follow.

### Delta and Zigzag Encoding

Coordinates are **delta-encoded**: each point is relative to the previous one, not
absolute. The deltas are then **zigzag-encoded** (see Section 4) so small negative
values stay compact.

The decoder maintains a **cursor** that accumulates deltas:

```cpp
// VectorTile.cpp:104-150
static void DecodeGeometry(const std::vector<uint32_t>& geom, MVTFeature& feature)
{
  int32_t cursorX = 0, cursorY = 0;
  size_t i = 0;

  while (i < geom.size())
  {
    uint32_t cmdInt  = geom[i++];
    uint32_t cmdId   = cmdInt & 0x7;     // lower 3 bits
    uint32_t cmdCount = cmdInt >> 3;      // upper bits

    if (cmdId == 1)  // MoveTo
    {
      for (uint32_t j = 0; j < cmdCount; j++)
      {
        cursorX += ZigzagDecode(geom[i++]);
        cursorY += ZigzagDecode(geom[i++]);
        feature.rings.push_back({});   // start new ring
        feature.rings.back().push_back(vec2(cursorX, cursorY));
      }
    }
    else if (cmdId == 2)  // LineTo
    {
      for (uint32_t j = 0; j < cmdCount; j++)
      {
        cursorX += ZigzagDecode(geom[i++]);
        cursorY += ZigzagDecode(geom[i++]);
        feature.rings.back().push_back(vec2(cursorX, cursorY));
      }
    }
    else if (cmdId == 7)  // ClosePath
    {
      feature.rings.back().push_back(feature.rings.back().front());
    }
  }
}
```

### Worked Example

Consider a triangle-shaped building feature with geometry type POLYGON:

```
Raw geometry array: [9, 4, 4, 18, 0, 16, 16, 0, 15]
```

Decoding step by step:

```
Cursor starts at (0, 0)

[9]  -> command_id = 9 & 7 = 1 (MoveTo), count = 9 >> 3 = 1
  [4, 4] -> dx = zigzag(4) = 2, dy = zigzag(4) = 2
  cursor = (0+2, 0+2) = (2, 2)
  Start new ring: [(2, 2)]

[18] -> command_id = 18 & 7 = 2 (LineTo), count = 18 >> 3 = 2
  [0, 16] -> dx = zigzag(0) = 0, dy = zigzag(16) = 8
  cursor = (2+0, 2+8) = (2, 10)
  Ring: [(2,2), (2,10)]

  [16, 0] -> dx = zigzag(16) = 8, dy = zigzag(0) = 0
  cursor = (2+8, 10+0) = (10, 10)
  Ring: [(2,2), (2,10), (10,10)]

[15] -> command_id = 15 & 7 = 7 (ClosePath), count = 15 >> 3 = 1
  Append first point to close: ring = [(2,2), (2,10), (10,10), (2,2)]
```

Result: a right triangle with vertices at (2,2), (2,10), (10,10).

### Geometry Types

The commands are interpreted differently based on the feature's geometry type:

| Type       | Structure                         | Commands Used         |
|------------|-----------------------------------|-----------------------|
| POINT      | Individual points                 | MoveTo only           |
| LINESTRING | Open polylines                    | MoveTo + LineTo       |
| POLYGON    | Closed rings (exterior + holes)   | MoveTo + LineTo + ClosePath |

For polygons, the first ring is the exterior boundary (counterclockwise winding),
and subsequent rings are holes (clockwise winding). ClosePath connects the last
point back to the first point of the current ring.

## 7. Step 5: Building Renderable Geometry

After parsing, we have an `MVTTile` containing layers of features, each with decoded
rings of 2D points. To render these, we convert rings into GPU-friendly line segments.

### Vertex Format

Each vertex carries a position and a color:

```cpp
// VectorTileRenderer.h
struct TileVertex
{
  glm::vec3 position;  // 12 bytes
  glm::vec4 color;     // 16 bytes
};                      // 28 bytes per vertex
```

### Line Segment Generation

For each consecutive pair of points in a ring, we emit two vertices (one line segment).
This is the `LineList` primitive topology - every two vertices form an independent line:

```cpp
// VectorTileRenderer.cpp:235-270 - BuildGeometry()
for (const auto& layer : tile.layers)
{
  glm::vec4 color = GetLayerColor(layer.name);  // color by layer type

  for (const auto& feature : layer.features)
  {
    if (feature.type == MVTGeomType::Point)
      continue;  // skip points for wireframe rendering

    for (const auto& ring : feature.rings)
    {
      for (size_t i = 0; i + 1 < ring.size(); i++)
      {
        // ring[i] -> ring[i+1] becomes one line segment = 2 vertices
        vertices.push_back({toWorldSpace(ring[i]),     color});
        vertices.push_back({toWorldSpace(ring[i + 1]), color});
      }
    }
  }
}
```

For a polygon with 4 points (a closed rectangle), this generates 4 line segments
(8 vertices): edges AB, BC, CD, DA.

For a linestring with 3 points, this generates 2 line segments (4 vertices): AB, BC.

### Layer Coloring

Different layers get distinct colors so the map is readable:

```cpp
// VectorTileRenderer.cpp:94-105
if (layerName == "water")          return {0.2, 0.4, 0.8, 1.0};  // blue
if (layerName == "transportation") return {0.7, 0.7, 0.7, 1.0};  // gray
if (layerName == "building")       return {0.9, 0.5, 0.2, 1.0};  // orange
if (layerName == "landuse")        return {0.3, 0.7, 0.3, 1.0};  // green
if (layerName == "boundary")       return {0.7, 0.3, 0.7, 1.0};  // purple
```

## 8. Step 6: Coordinate Mapping

### Tile Coordinate System

MVT coordinates live in a `[0, extent)` grid. The `extent` field (default 4096) defines
the resolution of the tile's internal coordinate space. All geometry coordinates are
integers within this range:

```
(0, 0)              (extent, 0)
  +--------------------+
  |                    |
  |    Tile content    |
  |                    |
  +--------------------+
(0, extent)          (extent, extent)
```

Note: Y increases downward (screen convention), and coordinates can extend slightly
beyond `[0, extent)` for geometry that overlaps tile borders (to prevent rendering
gaps between adjacent tiles).

### Mapping to World Space

For our 3D engine (Y-up coordinate system), we map tile coordinates to the XZ ground
plane:

```
tile X  ->  world X
tile Y  ->  world -Z  (flip because tile Y is down, world Z is towards camera)
            world Y = 0  (flat on the ground)
```

We center the tile at the world origin and scale it to a 100-unit square:

```cpp
// VectorTileRenderer.cpp:254-263
float extent = (float)layer.extent;  // typically 4096
float scale  = 100.0f / extent;      // maps [0,4096] to [0,100]

glm::vec3 worldPos = {
    (tileX - extent * 0.5f) * scale,   // center X at origin
    0.0f,                                // flat on ground
    (extent * 0.5f - tileY) * scale     // center Z, flip Y
};
```

This gives us coordinates in `[-50, +50]` on both X and Z axes.

### What About Map Projections?

For a single-tile POC, we don't need real cartographic projection. But for a full
map renderer, the tile coordinates relate to the world through the **Web Mercator**
projection (EPSG:3857):

```
longitude = tile_x / 2^zoom * 360 - 180
latitude  = atan(sinh(pi * (1 - 2 * tile_y / 2^zoom))) * 180 / pi
```

Within a single tile at zoom 14, the distortion from treating Mercator as a flat
plane is negligible (the tile covers only ~2.4km at Monaco's latitude). This is why
our simple linear mapping works well for a POC.

## 9. The Complete Pipeline

Putting it all together, the full data flow from `.pbf` file to pixels on screen:

```
14/8529/5974.pbf  (99 KB on disk)
        |
        | ReadBinaryFile()
        v
  Gzip-compressed bytes
        |
        | DecompressGzip()    -- skip gzip header, raw DEFLATE inflate
        v
  Raw protobuf bytes  (~195 KB)
        |
        | ParseMVTFromMemory()
        |   ReadField() loop  -- decode varint tags + wire types
        v
  MVTTile { layers[] }
        |
        | ParseLayer() for each layer
        |   Collect keys[], values[]
        |   ParseFeature() for each feature
        |     DecodePackedUint32() for tags and geometry
        |     DecodeGeometry()  -- MoveTo/LineTo/ClosePath state machine
        v
  Structured data: layers -> features -> rings of vec2 points
        |
        | BuildGeometry()
        |   For each ring: emit consecutive point pairs as line vertices
        |   Color by layer name
        |   Map tile coords to world XZ plane
        v
  std::vector<TileVertex>  (position + color per vertex)
        |
        | GPUAllocator::GAlloc() + SetData()
        v
  GPU vertex buffer
        |
        | wgpuRenderPassEncoderDraw() with LineList topology
        v
  Colored wireframe map on screen
```

### Data Size at Each Stage

For the Monaco z14 tile (`14/8529/5974.pbf`):

| Stage                    | Size        |
|--------------------------|-------------|
| Gzip compressed          | ~99 KB      |
| Decompressed protobuf    | ~195 KB     |
| Parsed features          | ~3000 features across ~10 layers |
| Generated line segments  | ~50K+ segments |
| GPU vertex buffer        | ~2.7 MB (at 28 bytes/vertex) |

The ~27x expansion from disk to GPU buffer is typical - binary compression is
extremely effective on the repetitive integer sequences in protobuf, and each
compressed coordinate expands into two full 28-byte vertices.
