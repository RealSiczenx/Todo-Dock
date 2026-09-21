#include "../src/logic.hpp"
#include <iostream>
int main()
{
    int errors = 0;
    const auto check = [&](bool pass, const char *name) {
        if (!pass) { std::cerr << "FAIL: " << name << '\n'; ++errors; }
    };
    using taskdock::completed;
    check(completed(true, false), "active condition checks");
    check(!completed(false, false), "inactive condition unchecks");
    check(!completed(true, true), "inverted active condition unchecks");
    check(completed(false, true), "inverted inactive condition checks");
    check(!completed(std::nullopt, false), "missing target does not check");
    check(!completed(std::nullopt, true), "missing inverted target does not check");
    using taskdock::movedIndex;
    check(movedIndex(-1, 1, 0) == -1, "empty selection");
    check(movedIndex(0, -1, 3) == 0, "cannot move above first");
    check(movedIndex(2, 1, 3) == 2, "cannot move below last");
    check(movedIndex(1, -1, 3) == 0, "move up");
    check(movedIndex(1, 1, 3) == 2, "move down");
    check(movedIndex(3, -1, 3) == -1, "invalid selection");
    if (!errors) std::cout << "12 logic checks passed\n";
    return errors ? 1 : 0;
}
