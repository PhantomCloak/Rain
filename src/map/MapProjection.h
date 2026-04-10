#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace WebEngine
{
  struct MapProjection
  {
    static constexpr float TILE_WORLD_SIZE = 10.0f;

    // Equirectangular projection: lat/lon (degrees) to slippy map tile coordinates
    static glm::ivec2 LatLonToTile(double lat, double lon, int zoom)
    {
      double n = (double)(1 << zoom);
      int x = (int)std::floor((lon + 180.0) / 360.0 * n);
      double latRad = lat * glm::pi<double>() / 180.0;
      int y = (int)std::floor(
          (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / glm::pi<double>()) / 2.0 * n);
      x = glm::clamp(x, 0, (int)n - 1);
      y = glm::clamp(y, 0, (int)n - 1);
      return {x, y};
    }

    // Tile coordinates (center of tile) to lat/lon (degrees)
    static glm::dvec2 TileCenterToLatLon(int x, int y, int zoom)
    {
      double n = (double)(1 << zoom);
      double lon = ((double)x + 0.5) / n * 360.0 - 180.0;
      double latRad = std::atan(
          std::sinh(glm::pi<double>() * (1.0 - 2.0 * ((double)y + 0.5) / n)));
      double lat = latRad * 180.0 / glm::pi<double>();
      return {lat, lon};
    }

    // World-space camera offset to tile grid offset
    // World +X = east, World +Z = north
    // Tile +X = east, Tile +Y = south
    static glm::ivec2 WorldToTileOffset(float worldX, float worldZ)
    {
      int dx = (int)std::round(worldX / TILE_WORLD_SIZE);
      int dy = (int)std::round(-worldZ / TILE_WORLD_SIZE);
      return {dx, dy};
    }
  };
}
