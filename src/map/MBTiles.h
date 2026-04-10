#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

namespace WebEngine
{
  // Thin read-only wrapper around an .mbtiles SQLite database. Exposes random
  // access to tiles by slippy-map (XYZ) coordinates and the archive metadata.
  //
  // MBTiles stores rows in TMS (origin bottom-left); ReadTile transparently
  // converts from the XYZ scheme the rest of the engine uses.
  class MBTilesReader
  {
   public:
    MBTilesReader() = default;
    ~MBTilesReader();

    MBTilesReader(const MBTilesReader&) = delete;
    MBTilesReader& operator=(const MBTilesReader&) = delete;
    MBTilesReader(MBTilesReader&& other) noexcept;
    MBTilesReader& operator=(MBTilesReader&& other) noexcept;

    bool Open(const std::string& path);
    void Close();
    bool IsOpen() const { return m_Db != nullptr; }
    const std::string& Path() const { return m_Path; }

    // Returns the raw blob stored for (zoom, x, y), or an empty vector if the
    // tile is absent. The blob is whatever is on disk — typically gzipped
    // protobuf — and is suitable to pass to ParseMVTFromBytes.
    std::vector<uint8_t> ReadTile(int zoom, int x, int y) const;

    // Sorted ascending; queries the tiles table once per call.
    std::vector<int> GetAvailableZooms() const;

    const std::unordered_map<std::string, std::string>& Metadata() const { return m_Metadata; }
    std::string MetadataValue(const std::string& key, const std::string& fallback = {}) const;

   private:
    sqlite3* m_Db = nullptr;
    // Prepared once on Open(), reused per ReadTile call. mutable because
    // ReadTile is logically const (no visible state change) even though sqlite
    // needs to reset+rebind the statement.
    mutable sqlite3_stmt* m_TileStmt = nullptr;
    std::string m_Path;
    std::unordered_map<std::string, std::string> m_Metadata;
  };
}  // namespace WebEngine
