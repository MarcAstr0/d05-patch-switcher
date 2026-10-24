#include "core/PresetTable.h"

#include <QFile>
#include <QTextStream>

namespace d05 {

int presetRowCount()
{
    QFile presets(QStringLiteral(":/data/d05-presets.tsv"));
    if (!presets.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;
    int rows = -1; // minus the header
    QTextStream ts(&presets);
    while (!ts.readLine().isNull())
        ++rows;
    return rows;
}

} // namespace d05
