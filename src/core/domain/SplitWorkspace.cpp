#include "SplitWorkspace.h"

namespace dante::core {

SplitWorkspace SplitWorkspace::empty(int cols, int rows) {
    SplitWorkspace w;
    w.id = newId();
    w.layout = {cols, rows};
    w.tabIds.reserve(cols * rows);
    for (int i = 0; i < cols * rows; ++i) {
        w.tabIds.append(Id());  // null UUID = empty slot (cf. lesson §3.4.4)
    }
    return w;
}

}  // namespace dante::core
