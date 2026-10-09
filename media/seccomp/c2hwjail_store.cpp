// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#include "c2hwjail_store.h"

#include <utility>
#include <vector>

namespace c2hwjail {
namespace {

class FilteringStore final : public C2ComponentStore {
  public:
    FilteringStore(std::shared_ptr<C2ComponentStore> store, std::set<std::string> allowed)
        : mStore(std::move(store)), mAllowed(std::move(allowed)) {
        if (mStore == nullptr || mAllowed.empty()) {
            mAllowed.clear();
            return;
        }
        // A listed alias creates the same component, so it is allowed with its
        // component; nothing else is added.
        for (const auto& traits : mStore->listComponents()) {
            if (traits != nullptr && mAllowed.count(traits->name) != 0) {
                mListed.push_back(traits);
                mNames.insert(traits->name);
                mNames.insert(traits->aliases.begin(), traits->aliases.end());
            }
        }
    }

    C2String getName() const override { return mStore != nullptr ? mStore->getName() : ""; }

    c2_status_t createComponent(C2String name,
                                std::shared_ptr<C2Component>* const component) override {
        if (mNames.count(name) == 0) {
            return C2_NOT_FOUND;
        }
        return mStore->createComponent(std::move(name), component);
    }

    c2_status_t createInterface(C2String name,
                                std::shared_ptr<C2ComponentInterface>* const interface) override {
        if (mNames.count(name) == 0) {
            return C2_NOT_FOUND;
        }
        return mStore->createInterface(std::move(name), interface);
    }

    std::vector<std::shared_ptr<const C2Component::Traits>> listComponents() override {
        return mListed;
    }

    c2_status_t copyBuffer(std::shared_ptr<C2GraphicBuffer> src,
                           std::shared_ptr<C2GraphicBuffer> dst) override {
        if (mStore == nullptr) {
            return C2_OMITTED;
        }
        return mStore->copyBuffer(std::move(src), std::move(dst));
    }

    c2_status_t query_sm(const std::vector<C2Param*>& stackParams,
                         const std::vector<C2Param::Index>& heapParamIndices,
                         std::vector<std::unique_ptr<C2Param>>* const heapParams) const override {
        if (mStore == nullptr) {
            return C2_OMITTED;
        }
        return mStore->query_sm(stackParams, heapParamIndices, heapParams);
    }

    c2_status_t config_sm(const std::vector<C2Param*>& params,
                          std::vector<std::unique_ptr<C2SettingResult>>* const failures) override {
        if (mStore == nullptr) {
            return C2_OMITTED;
        }
        return mStore->config_sm(params, failures);
    }

    std::shared_ptr<C2ParamReflector> getParamReflector() const override {
        return mStore != nullptr ? mStore->getParamReflector() : nullptr;
    }

    c2_status_t querySupportedParams_nb(
            std::vector<std::shared_ptr<C2ParamDescriptor>>* const params) const override {
        if (mStore == nullptr) {
            return C2_OK;
        }
        return mStore->querySupportedParams_nb(params);
    }

    c2_status_t querySupportedValues_sm(
            std::vector<C2FieldSupportedValuesQuery>& fields) const override {
        if (mStore == nullptr) {
            return C2_OK;
        }
        return mStore->querySupportedValues_sm(fields);
    }

  private:
    const std::shared_ptr<C2ComponentStore> mStore;
    std::set<std::string> mAllowed;
    // The allowed components' names and aliases, and their traits.
    std::set<std::string> mNames;
    std::vector<std::shared_ptr<const C2Component::Traits>> mListed;
};

}  // namespace

std::shared_ptr<C2ComponentStore> FilterStore(std::shared_ptr<C2ComponentStore> store,
                                              std::set<std::string> allowed) {
    return std::make_shared<FilteringStore>(std::move(store), std::move(allowed));
}

}  // namespace c2hwjail
