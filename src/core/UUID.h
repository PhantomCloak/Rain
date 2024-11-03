#pragma once
#include "Base.h"
typedef unsigned int uint;

class UUID {
 public:
  UUID();
  UUID(uint64_t uuid);
  UUID(const UUID& other);

  operator uint64_t() { return m_UUID; }
  operator const uint64_t() const { return m_UUID; }

 private:
  uint64_t m_UUID;
};


namespace std {
  template <>
  struct hash<UUID> {
    size_t operator()(const UUID& uuid) const {
      // uuid is already a randomly generated number, and is suitable as a hash key as-is.
      // this may change in future, in which case return hash<uint64_t>{}(uuid); might be more appropriate
			return hash<uint64_t>()((uint64_t)uuid);
      //return uuid;
    }
  };
}
