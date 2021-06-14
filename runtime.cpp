#include "runtime.h"

#include <cassert>
#include <iostream>
#include <optional>
#include <sstream>

using namespace std;

namespace {
    const string SELF_OBJECT = "self"s;
    const string STR_METHOD = "__str__"s;
    const string EQ_METHOD = "__eq__"s;
    const string LT_METHOD = "__lt__"s;
} // namespace

namespace runtime {

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
        : data_(std::move(data)) {
    }

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
        return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
        return ObjectHolder();
    }

    Object& ObjectHolder::operator*() const {
        AssertIsValid();
        return *Get();
    }

    Object* ObjectHolder::operator->() const {
        AssertIsValid();
        return Get();
    }

    Object* ObjectHolder::Get() const {
        return data_.get();
    }

    ObjectHolder::operator bool() const {
        return Get() != nullptr;
    }

    bool IsTrue(const ObjectHolder& object) {
        if (!object) {
            return false;
        }

        auto ptr_b = object.TryAs<runtime::Bool>();
        if (ptr_b != nullptr) {
            return ptr_b->GetValue();
        }

        auto ptr_n = object.TryAs<runtime::Number>();
        if (ptr_n != nullptr) {
            return ptr_n->GetValue() != 0;
        }

        auto ptr_s = object.TryAs<runtime::String>();
        if (ptr_s != nullptr) {
            if (ptr_s->GetValue() == ""s) {
                return false;
            } else {
                return true;
            }
        }

        auto ptr_vo_bool = object.TryAs<runtime::ValueObject<bool>>();
        if (ptr_vo_bool != nullptr) {
            return ptr_vo_bool->GetValue();
        }

        return false;
    }

    void ClassInstance::Print(std::ostream& os, Context& context) {
        auto method_ptr = this->cls_.GetMethod(STR_METHOD);

        if (method_ptr != nullptr) {
            auto res = Call(method_ptr->name, {}, context);
            res->Print(os, context);
        } else {
            os << this;
        }
    }

    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {

        auto ptr = cls_.GetMethod(method);

        if (ptr != nullptr && ptr->formal_params.size() == argument_count) {
            return true;
        }

        return false;
    }

    Closure& ClassInstance::Fields() {
        return fields_;
    }

    const Closure& ClassInstance::Fields() const {
        return fields_;
    }

    ClassInstance::ClassInstance(const Class& cls)
        : cls_(cls) {
    }

    ObjectHolder ClassInstance::Call(const std::string& method,
                                     const std::vector<ObjectHolder>& actual_args,
                                     Context& context) {

        if (!this->HasMethod(method, actual_args.size())) {
            throw std::runtime_error("No method found");
        }

        auto* method_ptr = cls_.GetMethod(method);

        Closure cl;

        cl[SELF_OBJECT] = ObjectHolder::Share(*this);

        size_t params_size = method_ptr->formal_params.size();

        for (size_t i = 0; i < params_size; ++i) {
            auto arg = method_ptr->formal_params[i];
            cl[arg] = actual_args[i];
        }

        return method_ptr->body->Execute(cl, context);
    }

    Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
        : name_(std::move(name))
        , methods_(std::move(methods))
        , parent_(parent) {

        if (parent_ != nullptr) {
            for (const auto& method : parent_->methods_) {
                name_to_method_[method.name] = &method;
            }
        }

        for (const auto& method : methods_) {
            name_to_method_[method.name] = &method;
        }
    }

    const Class* Class::GetParent() const {
        return parent_;
    }

    const Method* Class::GetMethod(const std::string& name) const {

        auto it = name_to_method_.find(name);

        if (it != name_to_method_.end()) {
            return it->second;
        }

        return nullptr;
    }

    const std::string& Class::GetName() const {
        return this->name_;
    }

    void Class::Print(ostream& os, Context& /* context */) {
        os << "Class "sv;
        os << this->GetName();
    }

    void Bool::Print(std::ostream& os, Context& /* context */) {
        os << (GetValue() ? "True"sv : "False"sv);
    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        if (!lhs && !rhs) {
            return true;
        }

        if (!lhs) {
            throw std::runtime_error("Cannot compare objects for equality"s);
        }

        auto l_ptr_n = lhs.TryAs<runtime::Number>();
        auto r_ptr_n = rhs.TryAs<runtime::Number>();

        if (l_ptr_n != nullptr && r_ptr_n != nullptr) {
            return l_ptr_n->GetValue() == r_ptr_n->GetValue();
        }

        auto l_ptr_s = lhs.TryAs<runtime::String>();
        auto r_ptr_s = rhs.TryAs<runtime::String>();

        if (l_ptr_s != nullptr && r_ptr_s != nullptr) {
            return l_ptr_s->GetValue() == r_ptr_s->GetValue();
        }

        auto l_ptr_b = lhs.TryAs<runtime::Bool>();
        auto r_ptr_b = rhs.TryAs<runtime::Bool>();

        if (l_ptr_b != nullptr && r_ptr_b != nullptr) {
            return l_ptr_b->GetValue() == r_ptr_b->GetValue();
        }

        auto l_ptr_class_inst = lhs.TryAs<runtime::ClassInstance>();

        constexpr int EQ_METHOD_ARGS_COUNT = 1;
        if (l_ptr_class_inst != nullptr && l_ptr_class_inst->HasMethod(EQ_METHOD, EQ_METHOD_ARGS_COUNT)) {
            auto res = l_ptr_class_inst->Call(EQ_METHOD, { rhs }, context);
            return res.TryAs<Bool>()->GetValue();
        }

        throw std::runtime_error("Cannot compare objects for equality"s);
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {

        if (!lhs) {
            throw std::runtime_error("Cannot compare objects for equality"s);
        }

        auto l_ptr_n = lhs.TryAs<runtime::Number>();
        auto r_ptr_n = rhs.TryAs<runtime::Number>();

        if (l_ptr_n != nullptr && r_ptr_n != nullptr) {
            return l_ptr_n->GetValue() < r_ptr_n->GetValue();
        }

        auto l_ptr_s = lhs.TryAs<runtime::String>();
        auto r_ptr_s = rhs.TryAs<runtime::String>();

        if (l_ptr_s != nullptr && r_ptr_s != nullptr) {
            return l_ptr_s->GetValue() < r_ptr_s->GetValue();
        }

        auto l_ptr_b = lhs.TryAs<runtime::Bool>();
        auto r_ptr_b = rhs.TryAs<runtime::Bool>();

        if (l_ptr_b != nullptr && r_ptr_b != nullptr) {
            return l_ptr_b->GetValue() < r_ptr_b->GetValue();
        }

        auto l_ptr_class_inst = lhs.TryAs<runtime::ClassInstance>();

        constexpr int LT_METHOD_ARGS_COUNT = 1;
        if (l_ptr_class_inst != nullptr && l_ptr_class_inst->HasMethod(LT_METHOD, LT_METHOD_ARGS_COUNT)) {
            auto res = l_ptr_class_inst->Call(LT_METHOD, { rhs }, context);
            return res.TryAs<Bool>()->GetValue();
        }

        throw std::runtime_error("Cannot compare objects for less"s);
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context);
    }

} // namespace runtime