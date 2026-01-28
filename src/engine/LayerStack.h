#pragma once
#include <vector>
#include "engine/Layer.h"

namespace Rain
{
  class LayerStack
  {
   public:
    void PushLayer(Layer* Layer)
    {
      m_Layers.push_back(Layer);
    }

    size_t Size() const { return m_Layers.size(); }

    Layer* operator[](size_t index)
    {
      return m_Layers[index];
    }

    const Layer* operator[](size_t index) const
    {
      return m_Layers[index];
    }
    std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
    std::vector<Layer*>::iterator end() { return m_Layers.end(); }

   private:
    std::vector<Layer*> m_Layers;
  };
}  // namespace Rain
