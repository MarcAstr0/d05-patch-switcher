#include "core/Model.h"

namespace d05 {

const ModelInfo &modelInfo(Model m)
{
    static const ModelInfo infos[] = {
        {Model::D05, QStringLiteral("d05"), QStringLiteral("D-05"), 896, true, {}},
        {Model::JP08, QStringLiteral("jp08"), QStringLiteral("JP-08"), 64, false, {}},
        {Model::JU06A, QStringLiteral("ju06a"), QStringLiteral("JU-06A"), 128, false, {60, 106}},
    };
    return infos[static_cast<int>(m)];
}

std::optional<Model> modelFromId(const QString &id)
{
    for (Model m : allModels())
        if (modelInfo(m).id == id)
            return m;
    return std::nullopt;
}

}  // namespace d05
