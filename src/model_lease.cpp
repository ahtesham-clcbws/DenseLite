#include "model_lease.hpp"
#include "model_pool.hpp"
#include <utility>

ModelLease::ModelLease()
    : pool_(nullptr), model_id_(""), model_ptr_(nullptr), placement_(DevicePlacement::CPU_RAM) {}

ModelLease::ModelLease(ModelPool* pool, const std::string& model_id, DenseModel* model_ptr, DevicePlacement placement)
    : pool_(pool), model_id_(model_id), model_ptr_(model_ptr), placement_(placement) {}

ModelLease::~ModelLease() {
    if (pool_ && !model_id_.empty()) {
        pool_->release_lease(model_id_);
        pool_ = nullptr;
        model_ptr_ = nullptr;
    }
}

ModelLease::ModelLease(ModelLease&& other) noexcept
    : pool_(other.pool_),
      model_id_(std::move(other.model_id_)),
      model_ptr_(other.model_ptr_),
      placement_(other.placement_) {
    other.pool_ = nullptr;
    other.model_ptr_ = nullptr;
}

ModelLease& ModelLease::operator=(ModelLease&& other) noexcept {
    if (this != &other) {
        if (pool_ && !model_id_.empty()) {
            pool_->release_lease(model_id_);
        }
        pool_ = other.pool_;
        model_id_ = std::move(other.model_id_);
        model_ptr_ = other.model_ptr_;
        placement_ = other.placement_;

        other.pool_ = nullptr;
        other.model_ptr_ = nullptr;
    }
    return *this;
}

bool ModelLease::is_valid() const {
    return (pool_ != nullptr && model_ptr_ != nullptr);
}

const ModelConfig& ModelLease::config() const {
    return model_ptr_->config;
}

DenseModel& ModelLease::model() const {
    return *model_ptr_;
}

DevicePlacement ModelLease::placement() const {
    return placement_;
}

const std::string& ModelLease::model_id() const {
    return model_id_;
}
