#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

#include "core/util/Uuid.h"

namespace dante::core {

struct CellSpan {
    int cols{1};
    int rows{1};
};

struct SplitLayout {
    int cols{1};
    int rows{1};
};

struct SplitWorkspace {
    Id id;
    QVector<Id> tabIds;          // length = cols * rows; null UUID = empty slot
    SplitLayout layout;
    QHash<int, CellSpan> spans;  // index -> span for merged cells
    QSet<int> coveredSlots;      // derived: indices covered by another's span

    static SplitWorkspace empty(int cols, int rows);
};

}  // namespace dante::core
