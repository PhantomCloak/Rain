#pragma once
#include <glm/glm.hpp>

enum class EventType
{
  MouseScroll = 0,
  MouseMove = 1,
  MousePress = 2,
  MousePressed = 3
};

class Event
{
 public:
  ~Event() {};
  EventType Type;

  virtual EventType GetEventType() const = 0;
};

class MouseMoveEvent : public Event
{
 public:
  MouseMoveEvent(float x, float y) : m_X(x), m_Y(y) {}
  ~MouseMoveEvent() {};

  glm::vec2 GetPosition() { return glm::vec2(m_X, m_Y); }

  virtual EventType GetEventType() const override
  {
    return EventType::MouseMove;
  }

 private:
  float m_X = 0.0f;
  float m_Y = 0.0f;
};

class MouseScroll : public Event
{
 public:
  MouseScroll(float xOffset, float yOffset) : m_XOffset(xOffset), m_YOffset(yOffset) {}
  ~MouseScroll() {};
  glm::vec2 GetOffset() { return glm::vec2(m_XOffset, m_YOffset); }

  virtual EventType GetEventType() const override
  {
    return EventType::MouseScroll;
  }

 private:
  float m_XOffset = 0.0f;
  float m_YOffset = 0.0f;
};
