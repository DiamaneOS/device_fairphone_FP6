// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#include "c2hwjail_store.h"

#include <utility>
#include <vector>

namespace c2hwjail {
namespace {

class FilteringStore final : public C2ComponentStore {
  public:
    FilteringStore(std::shared_ptr<C2ComponentStore> store, std::set<std::string> allowed,
                   std::set<std::string> callerChecked, CallerCheck check)
        : mStore(std::move(store)), mAllowed(std::move(allowed)), mCheck(std::move(check)) {
        if (mStore == nullptr || mAllowed.empty()) {
            mAllowed.clear();
            return;
        }
        // A listed alias creates the same component, so it is allowed with its
        // component, and checked like it. Nothing else is added.
        for (const auto& traits : mStore->listComponents()) {
            if (traits != nullptr && mAllowed.count(traits->name) != 0) {
                const bool checked = callerChecked.count(traits->name) != 0;
                mListed.push_back({traits, checked});
                std::set<std::string>& names = checked ? mCheckedNames : mNames;
                names.insert(traits->name);
                names.insert(traits->aliases.begin(), traits->aliases.end());
            }
        }
    }

    C2String getName() const override { return mStore != nullptr ? mStore->getName() : ""; }

    c2_status_t createComponent(C2String name,
                                std::shared_ptr<C2Component>* const component) override {
        if (!offers(name, Use::kComponent)) {
            return C2_NOT_FOUND;
        }
        return mStore->createComponent(std::move(name), component);
    }

    c2_status_t createInterface(C2String name,
                                std::shared_ptr<C2ComponentInterface>* const interface) override {
        if (!offers(name, Use::kInterface)) {
            return C2_NOT_FOUND;
        }
        return mStore->createInterface(std::move(name), interface);
    }

    std::vector<std::shared_ptr<const C2Component::Traits>> listComponents() override {
        std::vector<std::shared_ptr<const C2Component::Traits>> listed;
        bool checked = false;
        bool callerMay = false;
        for (const auto& [traits, isChecked] : mListed) {
            if (isChecked && !checked) {
                callerMay = callerMayUse(Use::kList);
                checked = true;
            }
            if (!isChecked || callerMay) {
                listed.push_back(traits);
            }
        }
        return listed;
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
    bool callerMayUse(Use use) const { return mCheck != nullptr && mCheck(use); }

    // The name is an allowed component or alias, and the caller may use it.
    bool offers(const std::string& name, Use use) const {
        if (mNames.count(name) != 0) {
            return true;
        }
        return mCheckedNames.count(name) != 0 && callerMayUse(use);
    }

    const std::shared_ptr<C2ComponentStore> mStore;
    std::set<std::string> mAllowed;
    const CallerCheck mCheck;
    // The allowed components' names and aliases: offered to every caller, or
    // only to the callers mCheck allows.
    std::set<std::string> mNames;
    std::set<std::string> mCheckedNames;
    // The allowed components' traits, and whether each is checked per caller.
    std::vector<std::pair<std::shared_ptr<const C2Component::Traits>, bool>> mListed;
};

}  // namespace

std::shared_ptr<C2ComponentStore> FilterStore(std::shared_ptr<C2ComponentStore> store,
                                              std::set<std::string> allowed,
                                              std::set<std::string> callerChecked,
                                              CallerCheck check) {
    return std::make_shared<FilteringStore>(std::move(store), std::move(allowed),
                                            std::move(callerChecked), std::move(check));
}

}  // namespace c2hwjail
