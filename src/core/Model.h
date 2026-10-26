#pragma once

#include <QString>
#include <array>
#include <optional>
#include <vector>

namespace d05 {

// The three supported instruments (docs/SPEC-2.md 3.1).
enum class Model { D05, JP08, JU06A };

struct ModelInfo {
    Model model;
    QString id;            // stable key: "d05", "jp08", "ju06a"
    QString name;          // UI name: "D-05", "JP-08", "JU-06A"
    int catalogueSize;     // 896, 64, 128
    bool hasGroups;        // U1..U8 / P1..P6 (D-05 only)
    std::vector<int> engines;  // empty, or {60, 106}
};

constexpr std::array<Model, 3> allModels() { return {Model::D05, Model::JP08, Model::JU06A}; }

const ModelInfo &modelInfo(Model m);
std::optional<Model> modelFromId(const QString &id);

}  // namespace d05
