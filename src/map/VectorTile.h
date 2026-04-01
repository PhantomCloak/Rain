#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace WebEngine
{
  enum class MVTGeomType : uint8_t
  {
    Unknown = 0,
    Point = 1,
    LineString = 2,
    Polygon = 3
  };

  struct MVTValue
  {
    enum Type { String, Float, Double, Int, UInt, SInt, Bool } type;
    std::string stringVal;
    double numVal = 0;
    bool boolVal = false;

    std::string ToString() const;
  };

  struct MVTFeature
  {
    uint64_t id = 0;
    MVTGeomType type = MVTGeomType::Unknown;
    std::vector<std::vector<glm::vec2>> rings;
    std::vector<std::pair<std::string, std::string>> properties;
  };

  struct MVTLayer
  {
    std::string name;
    uint32_t version = 0;
    uint32_t extent = 4096;
    std::vector<MVTFeature> features;
  };

  struct MVTTile
  {
    std::vector<MVTLayer> layers;
  };

  MVTTile ParseMVTFile(const std::string& path);
  MVTTile ParseMVTFromMemory(const uint8_t* data, size_t size);
}
