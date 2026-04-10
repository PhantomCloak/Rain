#include "map/MBTiles.h"

#include <sqlite3.h>
#include <utility>

#include "core/Log.h"

namespace WebEngine
{
  MBTilesReader::~MBTilesReader()
  {
    Close();
  }

  MBTilesReader::MBTilesReader(MBTilesReader&& other) noexcept
      : m_Db(other.m_Db),
        m_TileStmt(other.m_TileStmt),
        m_Path(std::move(other.m_Path)),
        m_Metadata(std::move(other.m_Metadata))
  {
    other.m_Db = nullptr;
    other.m_TileStmt = nullptr;
  }

  MBTilesReader& MBTilesReader::operator=(MBTilesReader&& other) noexcept
  {
    if (this != &other)
    {
      Close();
      m_Db = other.m_Db;
      m_TileStmt = other.m_TileStmt;
      m_Path = std::move(other.m_Path);
      m_Metadata = std::move(other.m_Metadata);
      other.m_Db = nullptr;
      other.m_TileStmt = nullptr;
    }
    return *this;
  }

  bool MBTilesReader::Open(const std::string& path)
  {
    Close();

    int rc = sqlite3_open_v2(path.c_str(), &m_Db, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK)
    {
      RN_LOG_ERR("MBTilesReader: failed to open '{}': {}", path,
                 m_Db ? sqlite3_errmsg(m_Db) : "unknown error");
      if (m_Db)
      {
        sqlite3_close(m_Db);
        m_Db = nullptr;
      }
      return false;
    }

    // Prepare the hot tile lookup once; reused across ReadTile calls.
    static const char* kTileSql =
        "SELECT tile_data FROM tiles "
        "WHERE zoom_level = ?1 AND tile_column = ?2 AND tile_row = ?3 LIMIT 1";
    rc = sqlite3_prepare_v2(m_Db, kTileSql, -1, &m_TileStmt, nullptr);
    if (rc != SQLITE_OK)
    {
      RN_LOG_ERR("MBTilesReader: failed to prepare tile statement: {}",
                 sqlite3_errmsg(m_Db));
      sqlite3_close(m_Db);
      m_Db = nullptr;
      return false;
    }

    // Slurp metadata (it's tiny and frequently accessed).
    m_Metadata.clear();
    sqlite3_stmt* mdStmt = nullptr;
    if (sqlite3_prepare_v2(m_Db, "SELECT name, value FROM metadata", -1, &mdStmt, nullptr) == SQLITE_OK)
    {
      while (sqlite3_step(mdStmt) == SQLITE_ROW)
      {
        const unsigned char* key = sqlite3_column_text(mdStmt, 0);
        const unsigned char* val = sqlite3_column_text(mdStmt, 1);
        if (key && val)
        {
          m_Metadata.emplace(reinterpret_cast<const char*>(key),
                             reinterpret_cast<const char*>(val));
        }
      }
      sqlite3_finalize(mdStmt);
    }

    m_Path = path;
    RN_LOG("MBTilesReader: opened '{}' ({} metadata entries)", path, m_Metadata.size());
    return true;
  }

  void MBTilesReader::Close()
  {
    if (m_TileStmt)
    {
      sqlite3_finalize(m_TileStmt);
      m_TileStmt = nullptr;
    }
    if (m_Db)
    {
      sqlite3_close(m_Db);
      m_Db = nullptr;
    }
    m_Metadata.clear();
    m_Path.clear();
  }

  std::vector<uint8_t> MBTilesReader::ReadTile(int zoom, int x, int y) const
  {
    if (!m_Db || !m_TileStmt)
      return {};

    // Convert slippy-map Y (origin top-left) to TMS row (origin bottom-left).
    int tmsRow = (1 << zoom) - 1 - y;

    sqlite3_reset(m_TileStmt);
    sqlite3_clear_bindings(m_TileStmt);
    sqlite3_bind_int(m_TileStmt, 1, zoom);
    sqlite3_bind_int(m_TileStmt, 2, x);
    sqlite3_bind_int(m_TileStmt, 3, tmsRow);

    int rc = sqlite3_step(m_TileStmt);
    if (rc != SQLITE_ROW)
      return {};

    const void* blob = sqlite3_column_blob(m_TileStmt, 0);
    int byteCount = sqlite3_column_bytes(m_TileStmt, 0);
    if (!blob || byteCount <= 0)
      return {};

    const uint8_t* bytes = static_cast<const uint8_t*>(blob);
    return std::vector<uint8_t>(bytes, bytes + byteCount);
  }

  std::vector<int> MBTilesReader::GetAvailableZooms() const
  {
    std::vector<int> out;
    if (!m_Db)
      return out;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_Db,
                           "SELECT DISTINCT zoom_level FROM tiles ORDER BY zoom_level",
                           -1, &stmt, nullptr) != SQLITE_OK)
    {
      return out;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
      out.push_back(sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return out;
  }

  std::string MBTilesReader::MetadataValue(const std::string& key, const std::string& fallback) const
  {
    auto it = m_Metadata.find(key);
    return it != m_Metadata.end() ? it->second : fallback;
  }
}  // namespace WebEngine
