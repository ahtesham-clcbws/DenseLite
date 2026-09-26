#pragma once
#include "model.hpp"
#include "model_registry.hpp"
#include <string>

class ModelPool;

class ModelLease {
public:
    ModelLease();
    ModelLease(ModelPool* pool, const std::string& model_id, DenseModel* model_ptr, DevicePlacement placement);
    ~ModelLease();

    // Move-only semantics (RAII lease)
    ModelLease(ModelLease&& other) noexcept;
    ModelLease& operator=(ModelLease&& other) noexcept;
    ModelLease(const ModelLease&) = delete;
    ModelLease& operator=(const ModelLease&) = delete;

    bool is_valid() const;
    const ModelConfig& config() const;
    DenseModel& model() const;
    DevicePlacement placement() const;
    const std::string& model_id() const;

private:
    ModelPool* pool_ = nullptr;
    std::string model_id_;
    DenseModel* model_ptr_ = nullptr;
    DevicePlacement placement_ = DevicePlacement::CPU_RAM;
};
