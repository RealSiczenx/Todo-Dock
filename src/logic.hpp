#pragma once
#include <optional>
namespace taskdock {
// Missing targets are never complete, including inverted conditions.
inline bool completed(std::optional<bool> observed, bool inverted)
{
    return observed.has_value() && (*observed != inverted);
}
inline int movedIndex(int index, int delta, int count)
{
    if (index < 0 || index >= count) return -1;
    const int target = index + delta;
    return target >= 0 && target < count ? target : index;
}
}
